#include "AnoxDataBuilder.h"

#include "anox/AnoxModule.h"
#include "anox/AFSArchive.h"
#include "anox/UtilitiesDriver.h"

#include "rkit/BuildSystem/BuildSystem.h"
#include "rkit/BuildSystem/DependencyGraph.h"

#include "rkit/Core/DirectoryScan.h"
#include "rkit/Core/Drivers.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Module.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Stream.h"

namespace anox::utils
{
	class AnoxDataBuilder final : public IDataBuilder
	{
	public:
		AnoxDataBuilder(anox::IUtilitiesDriver *utils);

		rkit::Result Run(const rkit::StringView &targetName, const rkit::StringView &sourceDir, const rkit::StringView &intermedDir, const rkit::StringView &dataDir) override;

	private:
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

	rkit::Result AnoxDataBuilder::Run(const rkit::StringView &targetName, const rkit::StringView &sourceDir, const rkit::StringView &intermedDir, const rkit::StringView &dataDir)
	{
		rkit::IModule *buildModule = rkit::GetDrivers().m_moduleDriver->LoadModule(rkit::IModuleDriver::kDefaultNamespace, "Build");
		if (!buildModule)
		{
			rkit::log::Error("Couldn't load build module");
			return rkit::ResultCode::kModuleLoadFailed;
		}

		RKIT_CHECK(buildModule->Init(nullptr));

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

		rkit::buildsystem::IDependencyGraphFactory *graphFactory = instance->GetDependencyGraphFactory();

		rkit::buildsystem::IDependencyNode *rootDepsNode;
		RKIT_CHECK(instance->FindOrCreateNode(rkit::buildsystem::kDefaultNamespace, rkit::buildsystem::kDepsNodeID, rkit::buildsystem::BuildFileLocation::kSourceDir, "RootFiles.deps", rootDepsNode));

		RKIT_CHECK(instance->AddRootNode(rootDepsNode));

		RKIT_CHECK(instance->Build(&fs));

		return rkit::ResultCode::kOK;
	}

	AnoxFileSystem::AnoxFileSystem()
	{
	}

	rkit::Result AnoxFileSystem::ResolveFileStatusIfExists(rkit::buildsystem::BuildFileLocation inputFileLocation, const rkit::StringView &identifier, void *userdata, ApplyFileStatusCallback_t applyStatus)
	{
		return rkit::ResultCode::kNotYetImplemented;
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
