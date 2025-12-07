#include "AnoxDataBuilder.h"

#include "anox/AnoxModule.h"
#include "anox/AFSArchive.h"
#include "anox/AnoxUtilitiesDriver.h"

#include "rkit/BuildSystem/BuildSystem.h"
#include "rkit/BuildSystem/DependencyGraph.h"
#include "rkit/BuildSystem/PackageBuilder.h"

#include "rkit/Core/DirectoryScan.h"
#include "rkit/Core/Drivers.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Module.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/StaticArray.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/SystemDriver.h"

namespace anox { namespace utils
{
	class AnoxDataBuilder final : public IDataBuilder
	{
	public:
		AnoxDataBuilder(anox::IUtilitiesDriver *utils);

		rkit::Result Run(const rkit::StringView &targetName, const rkit::OSAbsPathView &sourceDir, const rkit::OSAbsPathView &intermedDir, const rkit::OSAbsPathView &dataDir, const rkit::OSAbsPathView &dataSourceDir, rkit::render::BackendType backendType) override;

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

		rkit::Result Load(anox::IUtilitiesDriver *utils, const rkit::OSAbsPathView &sourceDir, const rkit::OSAbsPathView &intermedDir, const rkit::OSAbsPathView &dataFilesDir, const rkit::OSAbsPathView &dataContentDir, const rkit::OSAbsPathView &dataSourceDir);

		rkit::Result ResolveFileStatusIfExists(rkit::buildsystem::BuildFileLocation inputFileLocation, const rkit::CIPathView &path, bool allowDirectories, void *userdata, ApplyFileStatusCallback_t applyStatus) override;
		rkit::Result TryOpenFileRead(rkit::buildsystem::BuildFileLocation inputFileLocation, const rkit::CIPathView &path, rkit::UniquePtr<rkit::ISeekableReadStream> &outStream) override;
		rkit::Result EnumerateDirectory(rkit::buildsystem::BuildFileLocation inputFileLocation, const rkit::CIPathView &path, bool listFiles, bool listDirectories, void *userdata, ApplyFileStatusCallback_t callback) override;

	private:
		struct MountedArchive
		{
			rkit::String m_archiveName;
			rkit::UniquePtr<anox::afs::IArchive> m_archive;
			rkit::FileAttributes m_fileAttribs;
		};

		static rkit::Result StripTrailingPathSeparators(rkit::String &str);

		bool FindFileInArchive(const rkit::CIPathView &path, bool allowDirectories, const MountedArchive *&outArchive, afs::FileHandle &outFileHandle);

		rkit::OSAbsPath m_exeDir;
		rkit::OSAbsPath m_sourceDir;
		rkit::OSAbsPath m_intermedDir;
		rkit::OSAbsPath m_dataFilesDir;
		rkit::OSAbsPath m_dataContentDir;
		rkit::OSAbsPath m_dataSourceDir;

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
					rkit::log::ErrorFmt("Failed to open pipeline '{}' for merge", product.m_filePath.GetChars());
					return rkit::ResultCode::kOperationFailed;
				}

				RKIT_CHECK(combiner->AddInput(*stream));
			}
		}

		rkit::UniquePtr<rkit::ISeekableReadWriteStream> outStream;
		RKIT_CHECK(bsi.OpenFileWrite(rkit::buildsystem::BuildFileLocation::kOutputFiles, "pipelines_vk.rkp", outStream));

		RKIT_CHECK(combiner->WritePackage(*outStream));

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxDataBuilder::Run(const rkit::StringView &targetName, const rkit::OSAbsPathView &sourceDir, const rkit::OSAbsPathView &intermedDir, const rkit::OSAbsPathView &dataDir, const rkit::OSAbsPathView &dataSourceDir, rkit::render::BackendType backendType)
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

		rkit::OSAbsPath dataFilesDir;
		rkit::OSAbsPath dataContentDir;

		{
			rkit::CIPathView filesSubDir("files");
			rkit::CIPathView contentSubDir("content");

			rkit::OSRelPath osFilesSubDir;
			RKIT_CHECK(osFilesSubDir.ConvertFrom(filesSubDir));

			rkit::OSRelPath osContentSubDir;
			RKIT_CHECK(osContentSubDir.ConvertFrom(contentSubDir));

			RKIT_CHECK(dataFilesDir.Set(dataDir));
			RKIT_CHECK(dataFilesDir.Append(osFilesSubDir));

			RKIT_CHECK(dataContentDir.Set(dataDir));
			RKIT_CHECK(dataContentDir.Append(osContentSubDir));
		}

		AnoxFileSystem fs;
		RKIT_CHECK(fs.Load(m_utils, sourceDir, intermedDir, dataFilesDir, dataContentDir, dataSourceDir));

		rkit::UniquePtr<rkit::buildsystem::IBuildSystemInstance> instance;
		RKIT_CHECK(m_bsDriver->CreateBuildSystemInstance(instance));

		RKIT_CHECK(instance->Initialize(targetName, sourceDir, intermedDir, dataFilesDir, dataContentDir));

		rkit::StringView renderAddOnDriverName;
		switch (backendType)
		{
		case rkit::render::BackendType::Vulkan:
			renderAddOnDriverName = "Build_Vulkan";
			break;
		default:
			return rkit::ResultCode::kInternalError;
		}

		// Add render add-on
		{
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
		}

		// Add Anox add-on
		{
			rkit::IModule *renderBuildModule = rkit::GetDrivers().m_moduleDriver->LoadModule(anox::kAnoxNamespaceID, "Build");
			if (!renderBuildModule)
			{
				rkit::log::Error("Couldn't load game build add-on module");
				return rkit::ResultCode::kModuleLoadFailed;
			}

			rkit::buildsystem::IBuildSystemAddOnDriver *addOnDriver = static_cast<rkit::buildsystem::IBuildSystemAddOnDriver *>(rkit::GetDrivers().FindDriver(anox::kAnoxNamespaceID, "Build"));
			if (!addOnDriver)
			{
				rkit::log::Error("Couldn't load game build add-on driver");
				return rkit::ResultCode::kModuleLoadFailed;
			}

			RKIT_CHECK(addOnDriver->RegisterBuildSystemAddOn(instance.Get()));
		}

		RKIT_CHECK(instance->LoadCache());

		rkit::buildsystem::IDependencyGraphFactory *graphFactory = instance->GetDependencyGraphFactory();

		rkit::buildsystem::IDependencyNode *rootDepsNode;
		RKIT_CHECK(instance->FindOrCreateNamedNode(rkit::buildsystem::kDefaultNamespace, rkit::buildsystem::kDepsNodeID, rkit::buildsystem::BuildFileLocation::kSourceDir, "rootfiles.deps", rootDepsNode));

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

	rkit::Result AnoxFileSystem::ResolveFileStatusIfExists(rkit::buildsystem::BuildFileLocation inputFileLocation, const rkit::CIPathView &path, bool allowDirectories, void *userdata, ApplyFileStatusCallback_t applyStatus)
	{
		rkit::ISystemDriver *sysDriver = rkit::GetDrivers().m_systemDriver;

		rkit::FileAttributes attribs;
		bool exists = false;

		bool isInArchive = false;

		if (inputFileLocation == rkit::buildsystem::BuildFileLocation::kSourceDir)
		{
			afs::FileHandle fileHandle;
			const MountedArchive *archive = nullptr;
			if (FindFileInArchive(path, allowDirectories, archive, fileHandle))
			{
				attribs = archive->m_fileAttribs;
				attribs.m_isDirectory = fileHandle.IsDirectory();
				exists = true;
				isInArchive = true;
			}
			else
			{
				rkit::OSRelPath relPath;
				RKIT_CHECK(relPath.ConvertFrom(path));

				rkit::OSAbsPath osPath = m_dataSourceDir;
				RKIT_CHECK(osPath.Append(relPath));

				RKIT_CHECK(sysDriver->GetFileAttributesAbs(osPath, exists, attribs));

				if (!exists)
				{
					osPath = m_sourceDir;
					RKIT_CHECK(osPath.Append(relPath));

					RKIT_CHECK(sysDriver->GetFileAttributesAbs(osPath, exists, attribs));
				}
			}
		}
		else
		{
			rkit::OSAbsPath osPath;

			switch (inputFileLocation)
			{
			case rkit::buildsystem::BuildFileLocation::kIntermediateDir:
				osPath = m_intermedDir;
				break;
			case rkit::buildsystem::BuildFileLocation::kOutputFiles:
				osPath = m_dataFilesDir;
				break;
			case rkit::buildsystem::BuildFileLocation::kOutputContent:
				osPath = m_dataContentDir;
				break;
			case rkit::buildsystem::BuildFileLocation::kAuxDir0:
				osPath = m_exeDir;
				break;
			default:
				return rkit::ResultCode::kNotYetImplemented;
			}

			{
				rkit::OSRelPath relPath;
				RKIT_CHECK(relPath.ConvertFrom(path));

				RKIT_CHECK(osPath.Append(relPath));
			}

			RKIT_CHECK(sysDriver->GetFileAttributesAbs(osPath, exists, attribs));
		}

		// Try to import file from the root directory
		if (exists)
		{
			if (attribs.m_isDirectory && !allowDirectories)
				return rkit::ResultCode::kOK;

			rkit::buildsystem::FileStatusView fsView;
			fsView.m_filePath = path;
			fsView.m_fileSize = attribs.m_fileSize;
			fsView.m_fileTime = attribs.m_fileTime;
			fsView.m_location = inputFileLocation;
			fsView.m_isDirectory = attribs.m_isDirectory;

			return applyStatus(userdata, fsView);
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxFileSystem::TryOpenFileRead(rkit::buildsystem::BuildFileLocation inputFileLocation, const rkit::CIPathView &path, rkit::UniquePtr<rkit::ISeekableReadStream> &outStream)
	{
		rkit::ISystemDriver *sysDriver = rkit::GetDrivers().m_systemDriver;

		rkit::FileLocation fileLocation = rkit::FileLocation::kGameDirectory;

		if (inputFileLocation == rkit::buildsystem::BuildFileLocation::kSourceDir)
		{
			afs::FileHandle fileHandle;
			const MountedArchive *archive;
			if (FindFileInArchive(path, false, archive, fileHandle))
				return fileHandle.Open(outStream);

			rkit::OSRelPath relPath;
			RKIT_CHECK(relPath.ConvertFrom(path));

			rkit::OSAbsPath osPath = m_dataSourceDir;
			RKIT_CHECK(osPath.Append(relPath));

			rkit::Result openResult = sysDriver->OpenFileReadAbs(outStream, osPath);
			if (rkit::utils::GetResultCode(openResult) == rkit::ResultCode::kFileOpenError)
			{
				osPath = m_sourceDir;
				RKIT_CHECK(osPath.Append(relPath));

				openResult = sysDriver->OpenFileReadAbs(outStream, osPath);

				if (rkit::utils::GetResultCode(openResult) == rkit::ResultCode::kFileOpenError)
					return rkit::ResultCode::kOK;
			}

			return openResult;
		}
		else
		{
			rkit::OSAbsPath osPath;

			switch (inputFileLocation)
			{
			case rkit::buildsystem::BuildFileLocation::kIntermediateDir:
				osPath = m_intermedDir;
				break;
			case rkit::buildsystem::BuildFileLocation::kAuxDir0:
				osPath = m_exeDir;
				break;
			default:
				return rkit::ResultCode::kNotYetImplemented;
			}

			{
				rkit::OSRelPath relPath;
				RKIT_CHECK(relPath.ConvertFrom(path));

				RKIT_CHECK(osPath.Append(relPath));
			}

			rkit::Result openResult = sysDriver->OpenFileReadAbs(outStream, osPath);
			if (rkit::utils::GetResultCode(openResult) == rkit::ResultCode::kFileOpenError)
				return rkit::ResultCode::kOK;
		}


		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxFileSystem::EnumerateDirectory(rkit::buildsystem::BuildFileLocation inputFileLocation, const rkit::CIPathView &path, bool listFiles, bool listDirectories, void *userdata, ApplyFileStatusCallback_t callback)
	{
		if (!listFiles && !listDirectories)
			return rkit::ResultCode::kOK;

		rkit::OSRelPath osRelPath;
		RKIT_CHECK(osRelPath.ConvertFrom(path));

		rkit::OSAbsPath osPath;

		if (inputFileLocation == rkit::buildsystem::BuildFileLocation::kSourceDir)
		{
			rkit::StaticArray<rkit::OSAbsPath, 2> directories;
			directories[0] = m_dataSourceDir;
			directories[1] = m_sourceDir;

			for (rkit::OSAbsPath &directory : directories)
			{
				RKIT_CHECK(directory.Append(osRelPath));
			}

			rkit::ISystemDriver *sysDriver = rkit::GetDrivers().m_systemDriver;

			for (size_t srcIndex = 0; srcIndex < directories.Count(); srcIndex++)
			{
				const rkit::OSAbsPathView &dirPath = directories[srcIndex];

				bool directoryExists = false;
				rkit::FileAttributes dirAttribs;
				RKIT_CHECK(sysDriver->GetFileAttributesAbs(dirPath, directoryExists, dirAttribs));

				// See if this actually exists, otherwise blank it out so future scans ignore it
				if (!directoryExists || !dirAttribs.m_isDirectory)
				{
					directories[srcIndex] = rkit::OSAbsPath();
					continue;
				}

				rkit::UniquePtr<rkit::IDirectoryScan> dirScan;
				RKIT_CHECK(sysDriver->OpenDirectoryScanAbs(directories[srcIndex], dirScan));

				for (;;)
				{
					bool haveItem = false;
					rkit::DirectoryScanItem item;
					RKIT_CHECK(dirScan->GetNext(haveItem, item));

					if (!haveItem)
						break;

					const bool isDirectory = item.m_attribs.m_isDirectory;

					if (isDirectory && !listDirectories)
						continue;

					if (!isDirectory && !listFiles)
						continue;

					bool haveCollision = false;

					for (size_t prevIndex = 0; prevIndex < srcIndex; prevIndex++)
					{
						rkit::OSAbsPath altPath = directories[prevIndex];
						if (altPath.NumComponents() == 0)
							continue;

						RKIT_CHECK(altPath.Append(item.m_fileName));

						bool altExists = false;
						rkit::FileAttributes altAttribs;
						RKIT_CHECK(sysDriver->GetFileAttributesAbs(altPath, altExists, altAttribs));

						if (altExists && altAttribs.m_isDirectory == isDirectory)
						{
							haveCollision = true;
							break;
						}
					}

					if (!haveCollision)
					{
						rkit::CIPath fileNameCI;
						RKIT_CHECK(fileNameCI.ConvertFrom(item.m_fileName));

						rkit::buildsystem::FileStatus fileStatus;
						RKIT_CHECK(fileStatus.m_filePath.Set(path));
						RKIT_CHECK(fileStatus.m_filePath.Append(fileNameCI));
						fileStatus.m_fileSize = item.m_attribs.m_fileSize;
						fileStatus.m_fileTime = item.m_attribs.m_fileTime;
						fileStatus.m_isDirectory = isDirectory;
						fileStatus.m_location = inputFileLocation;

						RKIT_CHECK(callback(userdata, fileStatus.ToView()));
					}
				}
			}

			if (path.NumComponents() >= 1)
			{
				rkit::StringSliceView firstComponent = path[0];

				for (const MountedArchive &archive : m_afsArchives)
				{
					if (archive.m_archiveName == firstComponent)
					{
						afs::FileHandle dirHandle;
						if (path.NumComponents() == 1)
							dirHandle = archive.m_archive->GetRootDirectory();
						else
						{
							rkit::StringSliceView remainder = path.ToStringView().SubString(firstComponent.Length() + 1);
							afs::FileHandle dirHandle = archive.m_archive->FindFile(remainder, true);
						}

						if (dirHandle.IsValid() && dirHandle.IsDirectory())
						{
							rkit::CIPath basePath;
							RKIT_CHECK(basePath.Set(firstComponent));

							if (listFiles)
							{
								const uint32_t numFiles = dirHandle.GetNumFiles();
								for (uint32_t i = 0; i < numFiles; i++)
								{
									afs::FileHandle subHandle = dirHandle.GetFileByIndex(i);
									RKIT_ASSERT(subHandle.IsValid() && !subHandle.IsDirectory());

									rkit::CIPath subPath;
									RKIT_CHECK(subPath.Set(subHandle.GetFilePath()));

									rkit::buildsystem::FileStatus fileStatus;

									fileStatus.m_filePath = basePath;
									RKIT_CHECK(fileStatus.m_filePath.Append(subPath));

									fileStatus.m_fileSize = subHandle.GetFileSize();
									fileStatus.m_fileTime = archive.m_fileAttribs.m_fileTime;
									fileStatus.m_isDirectory = false;
									fileStatus.m_location = inputFileLocation;

									RKIT_CHECK(callback(userdata, fileStatus.ToView()));
								}
							}
							if (listDirectories)
							{
								const uint32_t numDirs = dirHandle.GetNumSubdirectories();
								for (uint32_t i = 0; i < numDirs; i++)
								{
									afs::FileHandle subHandle = dirHandle.GetSubdirectoryByIndex(i);
									RKIT_ASSERT(subHandle.IsValid() && subHandle.IsDirectory());

									rkit::buildsystem::FileStatus fileStatus;
									RKIT_CHECK(fileStatus.m_filePath.Set(subHandle.GetFilePath()));
									fileStatus.m_fileSize = 0;
									fileStatus.m_fileTime = archive.m_fileAttribs.m_fileTime;
									fileStatus.m_isDirectory = true;
									fileStatus.m_location = inputFileLocation;

									RKIT_CHECK(callback(userdata, fileStatus.ToView()));
								}
							}
						}

						break;
					}
				}
			}
		}

		return rkit::ResultCode::kOK;
	}

	bool AnoxFileSystem::FindFileInArchive(const rkit::CIPathView &path, bool allowDirectories, const MountedArchive *&outArchive, afs::FileHandle &outFileHandle)
	{
		rkit::Optional<size_t> firstSlashPos;

		rkit::StringView pathStr = path.ToStringView();

		// FIXME: Improve this

		for (size_t i = 0; i < pathStr.Length(); i++)
		{
			if (pathStr[i] == '/')
			{
				firstSlashPos = i;
				break;
			}
		}

		if (firstSlashPos.IsSet())
		{
			const rkit::StringSliceView archiveName = pathStr.SubString(0, firstSlashPos.Get());
			const rkit::StringSliceView itemName = pathStr.SubString(firstSlashPos.Get() + 1);

			for (const MountedArchive &archive : m_afsArchives)
			{
				if (archive.m_archiveName == archiveName)
				{
					outFileHandle = archive.m_archive->FindFile(itemName, allowDirectories);
					if (outFileHandle.IsValid())
					{
						outArchive = &archive;
						return true;
					}

					break;
				}
			}
		}
		else
		{
			if (allowDirectories)
			{
				for (const MountedArchive &archive : m_afsArchives)
				{
					if (archive.m_archiveName == pathStr)
					{
						outFileHandle = afs::FileHandle(archive.m_archive.Get(), 0, true);
						outArchive = &archive;
						return true;
					}
				}
			}
		}

		return false;
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

	rkit::Result AnoxFileSystem::Load(anox::IUtilitiesDriver *utils, const rkit::OSAbsPathView &sourceDir, const rkit::OSAbsPathView &intermedDir, const rkit::OSAbsPathView &dataFilesDir, const rkit::OSAbsPathView &dataContentDir, const rkit::OSAbsPathView &dataSourceDir)
	{
		RKIT_CHECK(m_exeDir.Set(sourceDir));

		m_sourceDir = m_exeDir;

		{
			rkit::OSRelPath anoxdataComponent;
			RKIT_CHECK(anoxdataComponent.SetFromUTF8("anoxdata"));
			RKIT_CHECK(m_sourceDir.Append(anoxdataComponent));
		}
		RKIT_CHECK(m_intermedDir.Set(intermedDir));
		RKIT_CHECK(m_dataFilesDir.Set(dataFilesDir));
		RKIT_CHECK(m_dataContentDir.Set(dataContentDir));
		RKIT_CHECK(m_dataSourceDir.Set(dataSourceDir));

		rkit::ISystemDriver *sysDriver = rkit::GetDrivers().m_systemDriver;

		rkit::UniquePtr<rkit::IDirectoryScan> dirScan;
		RKIT_CHECK(sysDriver->OpenDirectoryScanAbs(m_sourceDir, dirScan));

		if (dirScan.Get())
		{
			for (;;)
			{
				bool haveItem = false;
				rkit::DirectoryScanItem scanItem;
				RKIT_CHECK(dirScan->GetNext(haveItem, scanItem));

				if (!haveItem)
					break;

				if (scanItem.m_attribs.m_isDirectory == false && scanItem.m_fileName.Length() >= 5 && scanItem.m_fileName.ToStringView().EndsWithNoCase(RKIT_OS_PATH_LITERAL(".dat")))
				{
					rkit::OSAbsPath archivePath = m_sourceDir;
					RKIT_CHECK(archivePath.Append(scanItem.m_fileName));

					rkit::UniquePtr<rkit::ISeekableReadStream> archiveStream;
					rkit::Result openResult = sysDriver->OpenFileReadAbs(archiveStream, archivePath);

					if (!rkit::utils::ResultIsOK(openResult))
					{
						rkit::log::ErrorFmt("Failed to open archive");
						return openResult;
					}

					rkit::CIPath fileNameCIPath;
					RKIT_CHECK(fileNameCIPath.ConvertFrom(scanItem.m_fileName));

					const rkit::String &fileName = fileNameCIPath.ToString();

					MountedArchive mountedArchive;
					mountedArchive.m_fileAttribs = scanItem.m_attribs;
					RKIT_CHECK(mountedArchive.m_archiveName.Set(fileName.SubString(0, fileName.Length() - 4)));
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
} } // anox::utils
