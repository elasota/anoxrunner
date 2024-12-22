#include "AnoxDataBuilder.h"

#include "anox/AnoxModule.h"
#include "anox/AFSArchive.h"
#include "anox/UtilitiesDriver.h"

#include "rkit/BuildSystem/BuildSystem.h"
#include "rkit/BuildSystem/DependencyGraph.h"
#include "rkit/BuildSystem/PackageBuilder.h"

#include "rkit/Core/DirectoryScan.h"
#include "rkit/Core/Drivers.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Module.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/SystemDriver.h"

namespace anox::utils
{
	class AnoxDataBuilder final : public IDataBuilder
	{
	public:
		AnoxDataBuilder(anox::IUtilitiesDriver *utils);

		rkit::Result Run(const rkit::StringView &targetName, const rkit::StringView &sourceDir, const rkit::StringView &intermedDir, const rkit::StringView &dataDir, rkit::render::BackendType backendType) override;

	private:
		class ExportPipelinesCheckRunner final : public rkit::buildsystem::IBuildSystemAction
		{
		public:
			ExportPipelinesCheckRunner(AnoxDataBuilder &dataBuilder, rkit::buildsystem::IBuildSystemInstance &bsi);

			rkit::Result Run() override;

		private:
			AnoxDataBuilder &m_dataBuilder;
			rkit::buildsystem::IBuildSystemInstance &m_bsi;
		};

		rkit::Result ExportPipelineLibraries(rkit::buildsystem::IBuildSystemInstance &bsi);

		rkit::buildsystem::IBuildSystemDriver *m_bsDriver;
		anox::IUtilitiesDriver *m_utils;
	};

	class AnoxFileSystem final : public rkit::buildsystem::IBuildFileSystem
	{
	public:
		AnoxFileSystem();

		rkit::Result Load(anox::IUtilitiesDriver *utils, const rkit::StringView &sourceDir, const rkit::StringView &intermedDir, const rkit::StringView &dataDir);

		rkit::Result ResolveFileStatusIfExists(rkit::buildsystem::BuildFileLocation inputFileLocation, const rkit::StringView &identifier, void *userdata, ApplyFileStatusCallback_t applyStatus) override;

	private:
		struct MountedArchive
		{
			rkit::String m_archiveName;
			rkit::UniquePtr<anox::afs::IArchive> m_archive;
		};

		static rkit::Result StripTrailingPathSeparators(rkit::String &str);

		rkit::String m_sourceDir;
		rkit::String m_intermedDir;
		rkit::String m_dataDir;

		rkit::Vector<MountedArchive> m_afsArchives;
	};

	AnoxDataBuilder::AnoxDataBuilder(anox::IUtilitiesDriver *utils)
		: m_bsDriver(nullptr)
		, m_utils(utils)
	{
	}

	rkit::Result AnoxDataBuilder::ExportPipelineLibraries(rkit::buildsystem::IBuildSystemInstance &bsi)
	{
		rkit::Vector<rkit::buildsystem::IDependencyNode *> rplNodes;

		bool rebuiltAnyPipelines = false;
		for (rkit::buildsystem::IDependencyNode *node : bsi.GetBuildRelevantNodes())
		{
			uint32_t nodeNamespace = node->GetDependencyNodeNamespace();
			uint32_t nodeType = node->GetDependencyNodeType();

			if (nodeNamespace == rkit::buildsystem::kDefaultNamespace && nodeType == rkit::buildsystem::kRenderPipelineLibraryNodeID)
			{
				RKIT_CHECK(rplNodes.Append(node));

				if (node->WasCompiled())
					rebuiltAnyPipelines = true;
			}
		}

		if (!rebuiltAnyPipelines)
			return rkit::ResultCode::kOK;

		rkit::log::LogInfo("Combining pipeline libraries...");

		rkit::UniquePtr<rkit::buildsystem::IPipelineLibraryCombiner> combiner;
		RKIT_CHECK(m_bsDriver->CreatePipelineLibraryCombiner(combiner));

		for (rkit::buildsystem::IDependencyNode *node : rplNodes)
		{
			for (const rkit::buildsystem::FileStatusView &product : node->GetCompileProducts())
			{
				rkit::UniquePtr<rkit::ISeekableReadStream> stream;
				RKIT_CHECK(bsi.TryOpenFileRead(product.m_location, product.m_filePath, stream));

				if (!stream.IsValid())
				{
					rkit::log::ErrorFmt("Failed to open pipeline '%s' for merge", product.m_filePath.GetChars());
					return rkit::ResultCode::kOperationFailed;
				}

				RKIT_CHECK(combiner->AddInput(*stream));
			}
		}

		rkit::UniquePtr<rkit::ISeekableReadWriteStream> outStream;
		RKIT_CHECK(bsi.OpenFileWrite(rkit::buildsystem::BuildFileLocation::kOutputDir, "pipelines_vk.rkp", outStream));

		RKIT_CHECK(combiner->WritePackage(*outStream));

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxDataBuilder::Run(const rkit::StringView &targetName, const rkit::StringView &sourceDir, const rkit::StringView &intermedDir, const rkit::StringView &dataDir, rkit::render::BackendType backendType)
	{
		rkit::IModule *buildModule = rkit::GetDrivers().m_moduleDriver->LoadModule(rkit::IModuleDriver::kDefaultNamespace, "Build");
		if (!buildModule)
		{
			rkit::log::Error("Couldn't load build module");
			return rkit::ResultCode::kModuleLoadFailed;
		}

		m_bsDriver = static_cast<rkit::buildsystem::IBuildSystemDriver *>(rkit::GetDrivers().FindDriver(rkit::IModuleDriver::kDefaultNamespace, "BuildSystem"));
		if (!m_bsDriver)
		{
			rkit::log::Error("Couldn't find build system driver");
			return rkit::ResultCode::kModuleLoadFailed;
		}

		AnoxFileSystem fs;
		RKIT_CHECK(fs.Load(m_utils, sourceDir, intermedDir, dataDir));

		rkit::UniquePtr<rkit::buildsystem::IBuildSystemInstance> instance;
		RKIT_CHECK(m_bsDriver->CreateBuildSystemInstance(instance));

		RKIT_CHECK(instance->Initialize(targetName, sourceDir, intermedDir, dataDir));

		rkit::StringView renderAddOnDriverName;
		switch (backendType)
		{
		case rkit::render::BackendType::Vulkan:
			renderAddOnDriverName = "Build_Vulkan";
			break;
		default:
			return rkit::ResultCode::kInternalError;
		}

		rkit::IModule *renderBuildModule = rkit::GetDrivers().m_moduleDriver->LoadModule(rkit::IModuleDriver::kDefaultNamespace, renderAddOnDriverName.GetChars());
		if (!renderBuildModule)
		{
			rkit::log::Error("Couldn't load render build add-on module");
			return rkit::ResultCode::kModuleLoadFailed;
		}

		rkit::buildsystem::IBuildSystemAddOnDriver *addOnDriver = static_cast<rkit::buildsystem::IBuildSystemAddOnDriver *>(rkit::GetDrivers().FindDriver(rkit::IModuleDriver::kDefaultNamespace, renderAddOnDriverName));
		if (!addOnDriver)
		{
			rkit::log::Error("Couldn't load render build add-on driver");
			return rkit::ResultCode::kModuleLoadFailed;
		}

		RKIT_CHECK(addOnDriver->RegisterBuildSystemAddOn(instance.Get()));

		RKIT_CHECK(instance->LoadCache());

		rkit::buildsystem::IDependencyGraphFactory *graphFactory = instance->GetDependencyGraphFactory();

		rkit::buildsystem::IDependencyNode *rootDepsNode;
		RKIT_CHECK(instance->FindOrCreateNode(rkit::buildsystem::kDefaultNamespace, rkit::buildsystem::kDepsNodeID, rkit::buildsystem::BuildFileLocation::kSourceDir, "rootfiles.deps", rootDepsNode));

		RKIT_CHECK(instance->AddRootNode(rootDepsNode));

		ExportPipelinesCheckRunner exportPipelinesCheck(*this, *instance);
		RKIT_CHECK(instance->AddPostBuildAction(&exportPipelinesCheck));

		RKIT_CHECK(instance->Build(&fs));

		return rkit::ResultCode::kOK;
	}

	AnoxDataBuilder::ExportPipelinesCheckRunner::ExportPipelinesCheckRunner(AnoxDataBuilder &dataBuilder, rkit::buildsystem::IBuildSystemInstance &bsi)
		: m_dataBuilder(dataBuilder)
		, m_bsi(bsi)
	{
	}

	rkit::Result AnoxDataBuilder::ExportPipelinesCheckRunner::Run()
	{
		return m_dataBuilder.ExportPipelineLibraries(m_bsi);
	}

	AnoxFileSystem::AnoxFileSystem()
	{
	}

	rkit::Result AnoxFileSystem::ResolveFileStatusIfExists(rkit::buildsystem::BuildFileLocation inputFileLocation, const rkit::StringView &identifier, void *userdata, ApplyFileStatusCallback_t applyStatus)
	{
		rkit::ISystemDriver *sysDriver = rkit::GetDrivers().m_systemDriver;

		rkit::String tempPath;
		rkit::StringView path = identifier;
		rkit::FileLocation fileLocation = rkit::FileLocation::kGameDirectory;

		if (inputFileLocation == rkit::buildsystem::BuildFileLocation::kSourceDir)
		{
			path = identifier;
			fileLocation = rkit::FileLocation::kDataSourceDirectory;
		}
		else if (inputFileLocation == rkit::buildsystem::BuildFileLocation::kIntermediateDir)
		{
			RKIT_CHECK(tempPath.Set(m_intermedDir));
			RKIT_CHECK(tempPath.Append(sysDriver->GetPathSeparator()));
			RKIT_CHECK(tempPath.Append(identifier));

			path = tempPath;
			fileLocation = rkit::FileLocation::kAbsolute;
		}
		else
			return rkit::ResultCode::kNotYetImplemented;

		// Try to import file from the root directory
		rkit::FileAttributes attribs;
		bool exists = false;

		RKIT_CHECK(sysDriver->GetFileAttributes(fileLocation, path.GetChars(), exists, attribs));

		if (exists)
		{
			if (attribs.m_isDirectory)
				return rkit::ResultCode::kOK;

			rkit::buildsystem::FileStatusView fsView;
			fsView.m_filePath = identifier;
			fsView.m_fileSize = attribs.m_fileSize;
			fsView.m_fileTime = attribs.m_fileTime;
			fsView.m_location = inputFileLocation;

			return applyStatus(userdata, fsView);
		}

		return rkit::ResultCode::kOK;
	}


	rkit::Result AnoxFileSystem::StripTrailingPathSeparators(rkit::String &str)
	{
		size_t numTrailingSeparators = 0;

		while (numTrailingSeparators < str.Length())
		{
			char c = str[str.Length() - 1 - numTrailingSeparators];
			if (c != '/')
				break;

			numTrailingSeparators++;
		}

		if (numTrailingSeparators > 0)
		{
			RKIT_CHECK(str.Set(str.SubString(0, str.Length() - numTrailingSeparators)));
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxFileSystem::Load(anox::IUtilitiesDriver *utils, const rkit::StringView &sourceDir, const rkit::StringView &intermedDir, const rkit::StringView &dataDir)
	{
		RKIT_CHECK(m_sourceDir.Set(sourceDir));
		RKIT_CHECK(m_intermedDir.Set(intermedDir));
		RKIT_CHECK(m_dataDir.Set(dataDir));

		RKIT_CHECK(StripTrailingPathSeparators(m_sourceDir));
		RKIT_CHECK(StripTrailingPathSeparators(m_intermedDir));
		RKIT_CHECK(StripTrailingPathSeparators(m_dataDir));

		rkit::ISystemDriver *sysDriver = rkit::GetDrivers().m_systemDriver;

		rkit::UniquePtr<rkit::IDirectoryScan> dirScan;
		RKIT_CHECK(sysDriver->OpenDirectoryScan(rkit::FileLocation::kAbsolute, m_sourceDir.CStr(), dirScan));

		if (dirScan.Get())
		{
			for (;;)
			{
				bool haveItem = false;
				rkit::DirectoryScanItem scanItem;
				RKIT_CHECK(dirScan->GetNext(haveItem, scanItem));

				if (!haveItem)
					break;

				if (scanItem.m_attribs.m_isDirectory == false && scanItem.m_fileName.Length() >= 5 && scanItem.m_fileName.EndsWithNoCase(".dat"))
				{
					rkit::String archivePath;
					RKIT_CHECK(archivePath.Set(m_sourceDir));
					RKIT_CHECK(archivePath.Append(sysDriver->GetPathSeparator()));
					RKIT_CHECK(archivePath.Append(scanItem.m_fileName));

					rkit::UniquePtr<rkit::ISeekableReadStream> archiveStream = sysDriver->OpenFileRead(rkit::FileLocation::kAbsolute, archivePath.CStr());

					if (!archiveStream.Get())
					{
						rkit::log::ErrorFmt("Failed to open archive %s", scanItem.m_fileName.GetChars());
						return rkit::ResultCode::kFileOpenError;
					}

					MountedArchive mountedArchive;
					RKIT_CHECK(mountedArchive.m_archiveName.Set(scanItem.m_fileName.SubString(0, scanItem.m_fileName.Length() - 4)));
					RKIT_CHECK(mountedArchive.m_archiveName.MakeLower());
					RKIT_CHECK(utils->OpenAFSArchive(std::move(archiveStream), mountedArchive.m_archive));

					RKIT_CHECK(m_afsArchives.Append(std::move(mountedArchive)));
				}
			}
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result IDataBuilder::Create(anox::IUtilitiesDriver *utils, rkit::UniquePtr<IDataBuilder> &outDataBuilder)
	{
		return rkit::New<AnoxDataBuilder>(outDataBuilder, utils);
	}
}
