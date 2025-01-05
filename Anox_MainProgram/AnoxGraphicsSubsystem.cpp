#include "AnoxGraphicsResourceManager.h"

#include "anox/AnoxGraphicsSubsystem.h"

#include "rkit/Render/DeviceCaps.h"
#include "rkit/Render/DisplayManager.h"
#include "rkit/Render/RenderDefs.h"
#include "rkit/Render/RenderDriver.h"
#include "rkit/Render/RenderDevice.h"
#include "rkit/Render/PipelineLibraryLoader.h"

#include "rkit/Utilities/ThreadPool.h"

#include "rkit/Data/DataDriver.h"
#include "rkit/Data/RenderDataHandler.h"

#include "rkit/Core/Algorithm.h"
#include "rkit/Core/BoolVector.h"
#include "rkit/Core/BufferStream.h"
#include "rkit/Core/DriverModuleInitParams.h"
#include "rkit/Core/Endian.h"
#include "rkit/Core/HashTable.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Mutex.h"
#include "rkit/Core/MutexLock.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/NoCopy.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/UniquePtr.h"
#include "rkit/Core/UtilitiesDriver.h"
#include "rkit/Core/Vector.h"

#include "AnoxRenderedWindow.h"
#include "AnoxGraphicsSettings.h"



namespace anox
{
	class GraphicsSubsystem final : public IGraphicsSubsystem, public rkit::NoCopy
	{
	public:
		explicit GraphicsSubsystem(rkit::data::IDataDriver &dataDriver, rkit::utils::IThreadPool &threadPool, anox::RenderBackend desiredBackend);
		~GraphicsSubsystem();

		rkit::Result Initialize();

		void SetDesiredRenderBackend(RenderBackend renderBackend) override;

		void SetDesiredDisplayMode(rkit::render::DisplayMode displayMode) override;
		rkit::Result TransitionDisplayState() override;

		void MarkSetupStepCompleted();
		void MarkSetupStepFailed();

		rkit::render::IRenderDevice *GetDevice() const;
		rkit::data::IDataDriver &GetDataDriver() const;
		const anox::GraphicsSettings &GetGraphicsSettings() const;

		void SetSplashProgress(uint64_t progress);

	private:
		template<class T>
		struct PipelineConfigResolution
		{
			uint64_t m_keyIndex = 0;
			T m_resolution = T();
		};

		class RenderDataConfigurator final : public rkit::data::IRenderDataConfigurator, public rkit::render::IPipelineLibraryConfigValidator
		{
		public:
			explicit RenderDataConfigurator(const anox::RestartRequiringGraphicsSettings &gfxSettings);

			rkit::Result GetEnumConfigKey(size_t configKeyIndex, const rkit::StringView &keyName, rkit::data::RenderRTTIMainType expectedMainType, unsigned int &outValue) override;
			rkit::Result GetFloatConfigKey(size_t configKeyIndex, const rkit::StringView &keyName, double &outValue) override;
			rkit::Result GetSIntConfigKey(size_t configKeyIndex, const rkit::StringView &keyName, int64_t &outValue) override;
			rkit::Result GetUIntConfigKey(size_t configKeyIndex, const rkit::StringView &keyName, uint64_t &outValue) override;

			rkit::Result GetShaderStaticPermutation(size_t stringIndex, const rkit::StringView &permutationName, bool &outIsStatic, int32_t &outStaticValue) override;

			rkit::Result CheckConfig(rkit::IReadStream &stream, bool &isConfigMatched) override;
			rkit::Result WriteConfig(rkit::IWriteStream &stream) const override;

		private:
			struct TypedEnumValue
			{
				rkit::data::RenderRTTIMainType m_mainType;
				unsigned int m_value;
			};

			template<class T>
			static bool FindExistingConfigResolution(rkit::Vector<PipelineConfigResolution<T> > &resolutions, uint64_t keyIndex, T &value);

			template<class T>
			static rkit::Result AddConfigResolution(rkit::Vector<PipelineConfigResolution<T> > &resolutions, uint64_t keyIndex, const T &value);

			static rkit::Result WriteValueToConfig(rkit::IWriteStream &stream, const TypedEnumValue &value);
			static rkit::Result WriteValueToConfig(rkit::IWriteStream &stream, int64_t value);
			static rkit::Result WriteValueToConfig(rkit::IWriteStream &stream, uint64_t value);
			static rkit::Result WriteValueToConfig(rkit::IWriteStream &stream, double value);
			static rkit::Result WriteValueToConfig(rkit::IWriteStream &stream, int value);

			template<class T>
			static rkit::Result WriteResolutionsToConfig(rkit::IWriteStream &stream, const rkit::Vector<PipelineConfigResolution<T>> &resolution);

			const anox::RestartRequiringGraphicsSettings &m_gfxSettings;

			rkit::Vector<PipelineConfigResolution<TypedEnumValue> > m_configEnums;
			rkit::Vector<PipelineConfigResolution<int64_t> > m_configSInts;
			rkit::Vector<PipelineConfigResolution<uint64_t> > m_configUInts;
			rkit::Vector<PipelineConfigResolution<double> > m_configFloats;

			rkit::Vector<PipelineConfigResolution<int> > m_permutations;
		};

		struct LivePipelineSets
		{
			struct LivePipeline
			{
				size_t m_pipelineIndex;
				size_t m_variationIndex;
			};

			rkit::Vector<LivePipeline> m_graphicsPipelines;
		};

		class CheckPipelinesJob final : public rkit::IJobRunner
		{
		public:
			explicit CheckPipelinesJob(GraphicsSubsystem &graphicsSubsystem, const rkit::StringView &pipelinesFileName, const rkit::StringView &pipelinesCacheFileName, bool canUpdateShadowFile);

			rkit::Result Run() override;

		private:
			rkit::Result EvaluateLivePipelineSets(const rkit::data::IRenderDataPackage &package, RenderDataConfigurator &configurator, LivePipelineSets &lpSets);
			rkit::Result RecursiveEvaluatePermutationTree(const rkit::data::IRenderDataPackage &package, RenderDataConfigurator &configurator, rkit::Vector<LivePipelineSets::LivePipeline> &pipelines,
				rkit::HashMap<size_t, rkit::Optional<int32_t>> &staticResolutions, size_t pipelineIndex, size_t base, const rkit::render::ShaderPermutationTree *tree);

			GraphicsSubsystem &m_graphicsSubsystem;
			rkit::StringView m_pipelinesFileName;
			rkit::StringView m_pipelinesCacheFileName;
			bool m_canUpdateShadowFile;
		};

		class CreateNewIndividualCacheJob final : public rkit::IJobRunner
		{
		public:
			explicit CreateNewIndividualCacheJob(GraphicsSubsystem &graphicsSubsystem, const rkit::StringView &pipelinesCacheFileName);

			rkit::Result Run() override;

		private:
			GraphicsSubsystem &m_graphicsSubsystem;
			rkit::StringView m_pipelinesCacheFileName;
		};

		class LoadOneGraphicsPipelineJob final : public rkit::IJobRunner
		{
		public:
			explicit LoadOneGraphicsPipelineJob(GraphicsSubsystem &graphicsSubsystem, size_t index);

			rkit::Result Run() override;

		private:
			GraphicsSubsystem &m_graphicsSubsystem;
			size_t m_index;
		};

		class CompileOneGraphicsPipelineJob final : public rkit::IJobRunner
		{
		public:
			explicit CompileOneGraphicsPipelineJob(GraphicsSubsystem &graphicsSubsystem, size_t index);

			rkit::Result Run() override;

		private:
			GraphicsSubsystem &m_graphicsSubsystem;
			size_t m_index;
		};

		class DoneCompilingPipelinesJob final : public rkit::IJobRunner
		{
		public:
			explicit DoneCompilingPipelinesJob(GraphicsSubsystem &graphicsSubsystem);

			rkit::Result Run() override;

		private:
			GraphicsSubsystem &m_graphicsSubsystem;
		};

		class MergePipelineLibraryJob final : public rkit::IJobRunner
		{
		public:
			explicit MergePipelineLibraryJob(GraphicsSubsystem &graphicsSubsystem);

			rkit::Result Run() override;

		private:
			GraphicsSubsystem &m_graphicsSubsystem;
		};

		enum class DeviceSetupStep
		{
			kOpenPipelinePackage,
			kFirstTryLoadingPipelines,
			kCompilingPipelines,
			kMergingPipelines,
			kSecondTryLoadingPipelines,
			kFinished,
		};

		rkit::Result TransitionDisplayMode();
		rkit::Result TransitionBackend();

		rkit::Result KickOffNextSetupStep();
		rkit::Result KickOffMergedPipelineLoad();

		void SetPipelineLibraryLoader(rkit::UniquePtr<rkit::render::IPipelineLibraryLoader> &&loader, rkit::UniquePtr<LivePipelineSets> &&livePipelineSets, bool haveExistingMergedCache);

		rkit::Result CreateGameDisplayAndDevice(const rkit::StringView &renderModule, const rkit::StringView &pipelinesFile, const rkit::StringView &pipelinesCacheFile, bool canUpdatePipelineCache);

		void WaitForRenderingTasks();

		rkit::Optional<rkit::render::DisplayMode> m_currentDisplayMode;
		rkit::render::DisplayMode m_desiredDisplayMode = rkit::render::DisplayMode::kSplash;

		rkit::Optional<anox::RenderBackend> m_backend;
		anox::RenderBackend m_desiredBackend;

		anox::GraphicsSettings m_graphicsSettings;
		rkit::Optional<anox::GraphicsSettings> m_desiredGraphicsSettings;

		rkit::UniquePtr<RenderedWindowBase> m_gameWindow;

		rkit::UniquePtr<rkit::render::IRenderDevice> m_renderDevice;

		rkit::UniquePtr<anox::IGraphicsResourceManager> m_resourceManager;

		rkit::utils::IThreadPool &m_threadPool;

		rkit::StringView m_pipelinesCacheFileName;

		rkit::UniquePtr<rkit::IMutex> m_setupMutex;
		DeviceSetupStep m_setupStep = DeviceSetupStep::kFinished;
		uint64_t m_setupProgress = 0;

		bool m_stepCompleted = false;
		bool m_stepFailed = false;

		rkit::UniquePtr<rkit::render::IPipelineLibrary> m_pipelineLibrary;
		rkit::UniquePtr<rkit::render::IPipelineLibraryLoader> m_pipelineLibraryLoader;
		rkit::UniquePtr<LivePipelineSets> m_livePipelineSets;
		bool m_haveExistingMergedCache = false;

		rkit::data::IDataDriver &m_dataDriver;
	};


	GraphicsSubsystem::CheckPipelinesJob::CheckPipelinesJob(GraphicsSubsystem &graphicsSubsystem, const rkit::StringView &pipelinesFileName, const rkit::StringView &pipelinesCacheFileName, bool canUpdateShadowFile)
		: m_graphicsSubsystem(graphicsSubsystem)
		, m_pipelinesFileName(pipelinesFileName)
		, m_pipelinesCacheFileName(pipelinesCacheFileName)
		, m_canUpdateShadowFile(canUpdateShadowFile)
	{
	}

	rkit::Result GraphicsSubsystem::CheckPipelinesJob::Run()
	{
		rkit::ISystemDriver *sysDriver = rkit::GetDrivers().m_systemDriver;
		rkit::IUtilitiesDriver *utilsDriver = rkit::GetDrivers().m_utilitiesDriver;
		rkit::data::IDataDriver *dataDriver = &m_graphicsSubsystem.GetDataDriver();

		rkit::UniquePtr<rkit::ISeekableReadStream> pipelinesFile = sysDriver->OpenFileRead(rkit::FileLocation::kGameDirectory, m_pipelinesFileName.GetChars());

		if (!pipelinesFile.IsValid())
		{
			rkit::log::Error("Failed to open pipeline package");
			return rkit::ResultCode::kFileOpenError;
		}

		rkit::data::IRenderDataHandler *rdh = dataDriver->GetRenderDataHandler();

		rkit::UniquePtr<RenderDataConfigurator> configurator;
		RKIT_CHECK(rkit::New<RenderDataConfigurator>(configurator, m_graphicsSubsystem.GetGraphicsSettings().m_restartRequired));

		rkit::UniquePtr<rkit::data::IRenderDataPackage> package;
		RKIT_CHECK(rdh->LoadPackage(*pipelinesFile, false, configurator.Get(), package, nullptr));

		rkit::UniquePtr<LivePipelineSets> pipelineSets;

		RKIT_CHECK(rkit::New<LivePipelineSets>(pipelineSets));
		RKIT_CHECK(EvaluateLivePipelineSets(*package, *configurator, *pipelineSets));

		rkit::UniquePtr<rkit::ISeekableReadStream> cacheReadStream;

		// Try opening the cache itself
		cacheReadStream = sysDriver->OpenFileRead(rkit::FileLocation::kUserSettingsDirectory, m_pipelinesCacheFileName.GetChars());

		rkit::FilePos_t binaryContentStart = pipelinesFile->Tell();

		rkit::UniquePtr<rkit::render::IPipelineLibraryLoader> loader;
		RKIT_CHECK(m_graphicsSubsystem.GetDevice()->CreatePipelineLibraryLoader(loader, std::move(configurator), std::move(package), std::move(pipelinesFile), binaryContentStart));

		RKIT_CHECK(loader->LoadObjectsFromPackage());

		loader->SetMergedLibraryStream(std::move(cacheReadStream), nullptr);

		rkit::Result openMergedResult = loader->OpenMergedLibrary();

		m_graphicsSubsystem.SetPipelineLibraryLoader(std::move(loader), std::move(pipelineSets), openMergedResult.IsOK());

		if (openMergedResult.IsOK())
			m_graphicsSubsystem.MarkSetupStepCompleted();
		else
			m_graphicsSubsystem.MarkSetupStepFailed();

		return rkit::ResultCode::kOK;
	}

	rkit::Result GraphicsSubsystem::CheckPipelinesJob::EvaluateLivePipelineSets(const rkit::data::IRenderDataPackage &package, RenderDataConfigurator &configurator, LivePipelineSets &lpSets)
	{
		rkit::data::IRenderRTTIListBase *graphicsPipelineList = package.GetIndexable(rkit::data::RenderRTTIIndexableStructType::GraphicsPipelineDesc);
		size_t numGraphicsPipelines = graphicsPipelineList->GetCount();

		rkit::HashMap<size_t, rkit::Optional<int32_t>> staticResolutions;

		for (size_t i = 0; i < numGraphicsPipelines; i++)
		{
			const rkit::render::GraphicsPipelineDesc *pipeline = static_cast<const rkit::render::GraphicsPipelineDesc *>(graphicsPipelineList->GetElementPtr(i));

			RKIT_CHECK(RecursiveEvaluatePermutationTree(package, configurator, lpSets.m_graphicsPipelines, staticResolutions, i, 0, pipeline->m_permutationTree));
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result GraphicsSubsystem::CheckPipelinesJob::RecursiveEvaluatePermutationTree(const rkit::data::IRenderDataPackage &package, RenderDataConfigurator &configurator, rkit::Vector<LivePipelineSets::LivePipeline> &pipelines,
		rkit::HashMap<size_t, rkit::Optional<int32_t>> &staticResolutions, size_t pipelineIndex, size_t base, const rkit::render::ShaderPermutationTree *tree)
	{
		if (tree)
		{
			size_t keyIndex = tree->m_keyName.GetIndex();

			rkit::Optional<int32_t> keyResolution;

			rkit::HashMap<size_t, rkit::Optional<int32_t>>::ConstIterator_t it = staticResolutions.Find(keyIndex);
			if (it == staticResolutions.end())
			{
				// FIXME: Fix the key name handling here
				int32_t value = 0;
				bool isStatic = false;
				RKIT_CHECK(configurator.GetShaderStaticPermutation(keyIndex, package.GetString(keyIndex), isStatic, value));

				if (isStatic)
					keyResolution = value;

				RKIT_CHECK(staticResolutions.Set(keyIndex, keyResolution));
			}
			else
				keyResolution = it.Value();

			const size_t numBranches = tree->m_branches.Count();
			size_t branchPermutationOffset = 0;
			const rkit::render::ShaderPermutationTreeBranch *foundBranch = nullptr;

			for (size_t i = 0; i < numBranches; i++)
			{
				const rkit::render::ShaderPermutationTreeBranch *branch = tree->m_branches[i];

				bool useThisBranch = false;
				if (keyResolution.IsSet())
				{
					if (tree->m_branches[i]->m_keyValue == keyResolution.Get())
					{
						if (foundBranch)
						{
							rkit::log::Error("A shader permutation selector had multiple valid permutations for the same value");
							return rkit::ResultCode::kMalformedFile;
						}

						foundBranch = branch;
						useThisBranch = true;
					}
				}
				else
					useThisBranch = true;

				if (useThisBranch)
				{
					RKIT_CHECK(RecursiveEvaluatePermutationTree(package, configurator, pipelines, staticResolutions, pipelineIndex, base + branchPermutationOffset, branch->m_subTree));
				}

				const rkit::render::ShaderPermutationTree *subTree = branch->m_subTree;
				if (subTree == nullptr)
				{
					RKIT_CHECK(rkit::SafeAdd(branchPermutationOffset, branchPermutationOffset, static_cast<size_t>(1)));
				}
				else
				{
					RKIT_CHECK(rkit::SafeAdd(branchPermutationOffset, branchPermutationOffset, subTree->m_width));
				}
			}

			if (keyResolution.IsSet() && !foundBranch)
			{
				rkit::log::Error("A shader permutation selector could not match the specified value");
				return rkit::ResultCode::kMalformedFile;
			}

			if (branchPermutationOffset != tree->m_width)
			{
				rkit::log::Error("A shader permutation tree's branch set was misaligned");
				return rkit::ResultCode::kMalformedFile;
			}

			return rkit::ResultCode::kOK;
		}
		else
		{
			LivePipelineSets::LivePipeline lp;
			lp.m_pipelineIndex = pipelineIndex;
			lp.m_variationIndex = base;

			return pipelines.Append(lp);
		}
	}

	GraphicsSubsystem::CreateNewIndividualCacheJob::CreateNewIndividualCacheJob(GraphicsSubsystem &graphicsSubsystem, const rkit::StringView &pipelinesCacheFileName)
		: m_graphicsSubsystem(graphicsSubsystem)
		, m_pipelinesCacheFileName(pipelinesCacheFileName)
	{
	}

	rkit::Result GraphicsSubsystem::CreateNewIndividualCacheJob::Run()
	{
		rkit::ISystemDriver *sysDriver = rkit::GetDrivers().m_systemDriver;

		m_graphicsSubsystem.m_pipelineLibraryLoader->CloseMergedLibrary(true, true);

		rkit::UniquePtr<rkit::ISeekableReadWriteStream> pipelinesFile = sysDriver->OpenFileReadWrite(rkit::FileLocation::kUserSettingsDirectory, m_graphicsSubsystem.m_pipelinesCacheFileName.GetChars(), true, true, true);
		rkit::ISeekableReadWriteStream *writeStream = pipelinesFile.Get();

		if (!pipelinesFile.IsValid())
			return rkit::ResultCode::kOperationFailed;

		m_graphicsSubsystem.m_pipelineLibraryLoader->SetMergedLibraryStream(std::move(pipelinesFile), writeStream);

		return rkit::ResultCode::kOK;
	}

	GraphicsSubsystem::LoadOneGraphicsPipelineJob::LoadOneGraphicsPipelineJob(GraphicsSubsystem &graphicsSubsystem, size_t index)
		: m_graphicsSubsystem(graphicsSubsystem)
		, m_index(index)
	{
	}

	rkit::Result GraphicsSubsystem::LoadOneGraphicsPipelineJob::Run()
	{
		if (m_index == m_graphicsSubsystem.m_livePipelineSets->m_graphicsPipelines.Count())
		{
			m_graphicsSubsystem.m_pipelineLibraryLoader->CloseMergedLibrary(false, true);
			m_graphicsSubsystem.MarkSetupStepCompleted();
			return rkit::ResultCode::kOK;
		}

		const LivePipelineSets::LivePipeline &lp = m_graphicsSubsystem.m_livePipelineSets->m_graphicsPipelines[m_index];

		rkit::Result loadPipelineResult = m_graphicsSubsystem.m_pipelineLibraryLoader->LoadGraphicsPipelineFromMergedLibrary(lp.m_pipelineIndex, lp.m_variationIndex);

		if (!loadPipelineResult.IsOK())
		{
			m_graphicsSubsystem.m_pipelineLibraryLoader->CloseMergedLibrary(true, true);
			m_graphicsSubsystem.MarkSetupStepFailed();
			return rkit::ResultCode::kOK;
		}

		rkit::UniquePtr<rkit::IJobRunner> jobRunner;
		RKIT_CHECK(rkit::New<LoadOneGraphicsPipelineJob>(jobRunner, m_graphicsSubsystem, m_index + 1));

		RKIT_CHECK(m_graphicsSubsystem.m_threadPool.GetJobQueue()->CreateJob(nullptr, rkit::JobType::kIO, std::move(jobRunner), nullptr));
		return rkit::ResultCode::kOK;
	}

	GraphicsSubsystem::CompileOneGraphicsPipelineJob::CompileOneGraphicsPipelineJob(GraphicsSubsystem &graphicsSubsystem, size_t index)
		: m_graphicsSubsystem(graphicsSubsystem)
		, m_index(index)
	{
	}

	rkit::Result GraphicsSubsystem::CompileOneGraphicsPipelineJob::Run()
	{
		const LivePipelineSets::LivePipeline &lp = m_graphicsSubsystem.m_livePipelineSets->m_graphicsPipelines[m_index];

		return m_graphicsSubsystem.m_pipelineLibraryLoader->CompileUnmergedGraphicsPipeline(lp.m_pipelineIndex, lp.m_variationIndex);
	}

	GraphicsSubsystem::RenderDataConfigurator::RenderDataConfigurator(const anox::RestartRequiringGraphicsSettings &gfxSettings)
		: m_gfxSettings(gfxSettings)
	{
	}

	GraphicsSubsystem::DoneCompilingPipelinesJob::DoneCompilingPipelinesJob(GraphicsSubsystem &graphicsSubsystem)
		: m_graphicsSubsystem(graphicsSubsystem)
	{
	}

	rkit::Result GraphicsSubsystem::DoneCompilingPipelinesJob::Run()
	{
		m_graphicsSubsystem.MarkSetupStepCompleted();
		return rkit::ResultCode::kOK;
	}

	GraphicsSubsystem::MergePipelineLibraryJob::MergePipelineLibraryJob(GraphicsSubsystem &graphicsSubsystem)
		: m_graphicsSubsystem(graphicsSubsystem)
	{
	}

	rkit::Result GraphicsSubsystem::MergePipelineLibraryJob::Run()
	{
		uint64_t numPipelinesCompiled = 0;
		for (const LivePipelineSets::LivePipeline &lp : m_graphicsSubsystem.m_livePipelineSets->m_graphicsPipelines)
		{
			RKIT_CHECK(m_graphicsSubsystem.m_pipelineLibraryLoader->AddMergedPipeline(lp.m_pipelineIndex, lp.m_variationIndex));

			m_graphicsSubsystem.SetSplashProgress(++numPipelinesCompiled);
		}

		RKIT_CHECK(m_graphicsSubsystem.m_pipelineLibraryLoader->SaveMergedPipeline());

		m_graphicsSubsystem.MarkSetupStepCompleted();

		return rkit::ResultCode::kOK;
	}

	rkit::Result GraphicsSubsystem::RenderDataConfigurator::GetEnumConfigKey(size_t configKeyIndex, const rkit::StringView &keyName, rkit::data::RenderRTTIMainType expectedMainType, unsigned int &outValue)
	{
		TypedEnumValue resolution;

		resolution.m_mainType = rkit::data::RenderRTTIMainType::Invalid;
		resolution.m_value = 0;

		if (!FindExistingConfigResolution(m_configEnums, configKeyIndex, resolution))
		{
			if (!m_gfxSettings.ResolveConfigEnum(keyName, resolution.m_mainType, resolution.m_value))
			{
				rkit::log::ErrorFmt("Enum config key '%s' in the pipeline cache was unresolvable", keyName.GetChars());
				return rkit::ResultCode::kConfigInvalid;
			}

			if (resolution.m_mainType != expectedMainType)
			{
				rkit::log::ErrorFmt("Enum config key '%s' in the pipeline cache was the wrong type", keyName.GetChars());
				return rkit::ResultCode::kConfigInvalid;
			}

			RKIT_CHECK(AddConfigResolution(m_configEnums, configKeyIndex, resolution));
		}

		outValue = resolution.m_value;

		return rkit::ResultCode::kOK;
	}

	rkit::Result GraphicsSubsystem::RenderDataConfigurator::GetFloatConfigKey(size_t configKeyIndex, const rkit::StringView &keyName, double &outValue)
	{
		if (FindExistingConfigResolution(m_configFloats, configKeyIndex, outValue))
			return rkit::ResultCode::kOK;

		if (!m_gfxSettings.ResolveConfigFloat(keyName, outValue))
		{
			rkit::log::ErrorFmt("Float config key '%s' in the pipeline cache was unresolvable", keyName.GetChars());
			return rkit::ResultCode::kConfigInvalid;
		}

		RKIT_CHECK(AddConfigResolution(m_configFloats, configKeyIndex, outValue));

		return rkit::ResultCode::kOK;
	}

	rkit::Result GraphicsSubsystem::RenderDataConfigurator::GetSIntConfigKey(size_t configKeyIndex, const rkit::StringView &keyName, int64_t &outValue)
	{
		if (FindExistingConfigResolution(m_configSInts, configKeyIndex, outValue))
			return rkit::ResultCode::kOK;

		if (!m_gfxSettings.ResolveConfigSInt(keyName, outValue))
		{
			rkit::log::ErrorFmt("SInt config key '%s' in the pipeline cache was unresolvable", keyName.GetChars());
			return rkit::ResultCode::kConfigInvalid;
		}

		RKIT_CHECK(AddConfigResolution(m_configSInts, configKeyIndex, outValue));

		return rkit::ResultCode::kOK;
	}

	rkit::Result GraphicsSubsystem::RenderDataConfigurator::GetUIntConfigKey(size_t configKeyIndex, const rkit::StringView &keyName, uint64_t &outValue)
	{
		if (FindExistingConfigResolution(m_configUInts, configKeyIndex, outValue))
			return rkit::ResultCode::kOK;

		if (!m_gfxSettings.ResolveConfigUInt(keyName, outValue))
		{
			rkit::log::ErrorFmt("UInt config key '%s' in the pipeline cache was unresolvable", keyName.GetChars());
			return rkit::ResultCode::kConfigInvalid;
		}

		RKIT_CHECK(AddConfigResolution(m_configUInts, configKeyIndex, outValue));

		return rkit::ResultCode::kOK;
	}

	rkit::Result GraphicsSubsystem::RenderDataConfigurator::GetShaderStaticPermutation(size_t stringIndex, const rkit::StringView &permutationName, bool &outIsStatic, int32_t &outStaticValue)
	{
		outIsStatic = false;
		outStaticValue = 0;

		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result GraphicsSubsystem::RenderDataConfigurator::CheckConfig(rkit::IReadStream &stream, bool &isConfigMatched)
	{
		rkit::BufferStream bufferStream;
		RKIT_CHECK(WriteConfig(bufferStream));

		const rkit::Vector<uint8_t> &expectedBuffer = bufferStream.GetBuffer();

		rkit::Vector<uint8_t> streamVersion;
		RKIT_CHECK(streamVersion.Resize(bufferStream.GetSize()));

		size_t amountRead = 0;
		RKIT_CHECK(stream.ReadPartial(streamVersion.GetBuffer(), expectedBuffer.Count(), amountRead));

		if (amountRead != expectedBuffer.Count())
		{
			isConfigMatched = false;
			return rkit::ResultCode::kOK;
		}

		isConfigMatched = rkit::CompareSpansEqual(expectedBuffer.ToSpan(), streamVersion.ToSpan());

		return rkit::ResultCode::kOK;
	}

	rkit::Result GraphicsSubsystem::RenderDataConfigurator::WriteConfig(rkit::IWriteStream &stream) const
	{
		RKIT_CHECK(WriteResolutionsToConfig(stream, m_configEnums));
		RKIT_CHECK(WriteResolutionsToConfig(stream, m_configSInts));
		RKIT_CHECK(WriteResolutionsToConfig(stream, m_configUInts));
		RKIT_CHECK(WriteResolutionsToConfig(stream, m_configFloats));
		RKIT_CHECK(WriteResolutionsToConfig(stream, m_permutations));

		return rkit::ResultCode::kOK;
	}

	template<class T>
	bool GraphicsSubsystem::RenderDataConfigurator::FindExistingConfigResolution(rkit::Vector<PipelineConfigResolution<T> > &resolutions, uint64_t keyIndex, T &value)
	{
		for (const PipelineConfigResolution<T> &resolution : resolutions)
		{
			if (resolution.m_keyIndex == keyIndex)
			{
				value = resolution.m_resolution;
				return true;
			}
		}

		return false;
	}

	template<class T>
	rkit::Result GraphicsSubsystem::RenderDataConfigurator::AddConfigResolution(rkit::Vector<PipelineConfigResolution<T> > &resolutions, uint64_t keyIndex, const T &value)
	{
		PipelineConfigResolution<T> resolution;
		resolution.m_keyIndex = keyIndex;
		resolution.m_resolution = value;

		return resolutions.Append(resolution);
	}

	rkit::Result GraphicsSubsystem::RenderDataConfigurator::WriteValueToConfig(rkit::IWriteStream &stream, const TypedEnumValue &value)
	{
		// Don't need to write the type since it's only used for checks
		return WriteValueToConfig(stream, static_cast<int>(value.m_value));
	}

	rkit::Result GraphicsSubsystem::RenderDataConfigurator::WriteValueToConfig(rkit::IWriteStream &stream, int64_t value)
	{
		return stream.WriteAll(&value, sizeof(value));
	}

	rkit::Result GraphicsSubsystem::RenderDataConfigurator::WriteValueToConfig(rkit::IWriteStream &stream, uint64_t value)
	{
		return stream.WriteAll(&value, sizeof(value));
	}

	rkit::Result GraphicsSubsystem::RenderDataConfigurator::WriteValueToConfig(rkit::IWriteStream &stream, double value)
	{
		return stream.WriteAll(&value, sizeof(value));
	}

	rkit::Result GraphicsSubsystem::RenderDataConfigurator::WriteValueToConfig(rkit::IWriteStream &stream, int value)
	{
		return stream.WriteAll(&value, sizeof(value));
	}

	template<class T>
	rkit::Result GraphicsSubsystem::RenderDataConfigurator::WriteResolutionsToConfig(rkit::IWriteStream &stream, const rkit::Vector<PipelineConfigResolution<T>> &resolutions)
	{
		// We don't need to write key indexes because they're deterministic by package
		for (const PipelineConfigResolution<T> &resolution : resolutions)
		{
			RKIT_CHECK(WriteValueToConfig(stream, resolution.m_resolution));
		}

		return rkit::ResultCode::kOK;
	}


	GraphicsSubsystem::GraphicsSubsystem(rkit::data::IDataDriver &dataDriver, rkit::utils::IThreadPool &threadPool, anox::RenderBackend desiredBackend)
		: m_threadPool(threadPool)
		, m_dataDriver(dataDriver)
		, m_desiredBackend(desiredBackend)
	{
	}

	GraphicsSubsystem::~GraphicsSubsystem()
	{
		WaitForRenderingTasks();
		m_gameWindow.Reset();
	}

	rkit::Result GraphicsSubsystem::Initialize()
	{
		rkit::ISystemDriver *sysDriver = rkit::GetDrivers().m_systemDriver;

		RKIT_CHECK(sysDriver->CreateMutex(m_setupMutex));

		return rkit::ResultCode::kOK;
	}

	rkit::Result GraphicsSubsystem::CreateGameDisplayAndDevice(const rkit::StringView &renderModuleName, const rkit::StringView &pipelinesFile, const rkit::StringView &pipelinesCacheFile, bool canUpdatePipelineCache)
	{
		rkit::render::IDisplayManager *displayManager = rkit::GetDrivers().m_systemDriver->GetDisplayManager();

		m_currentDisplayMode = rkit::render::DisplayMode::kSplash;

		rkit::UniquePtr<rkit::render::IDisplay> display;
		RKIT_CHECK(displayManager->CreateDisplay(display, m_currentDisplayMode.Get()));

		RKIT_CHECK(RenderedWindowBase::Create(m_gameWindow, std::move(display), nullptr, 0));

		rkit::render::IProgressMonitor *progressMonitor = m_gameWindow->GetDisplay().GetProgressMonitor();

		if (progressMonitor)
		{
			// FIXME: Localize
			RKIT_CHECK(progressMonitor->SetText("Starting graphics system..."));
		}

		// Create render driver
		rkit::render::RenderDriverInitProperties driverParams = {};

#if RKIT_IS_DEBUG
		driverParams.m_validationLevel = rkit::render::ValidationLevel::kAggressive;
		driverParams.m_enableLogging = true;
#endif

		rkit::DriverModuleInitParameters moduleParams = {};
		moduleParams.m_driverInitParams = &driverParams;

		rkit::IModule *renderModule = rkit::GetDrivers().m_moduleDriver->LoadModule(rkit::IModuleDriver::kDefaultNamespace, renderModuleName.GetChars(), &moduleParams);
		if (!renderModule)
			return rkit::ResultCode::kModuleLoadFailed;

		rkit::render::IRenderDriver *renderDriver = static_cast<rkit::render::IRenderDriver *>(rkit::GetDrivers().FindDriver(rkit::IModuleDriver::kDefaultNamespace, renderModuleName));

		if (!renderDriver)
		{
			rkit::log::Error("Missing render driver");
			return rkit::ResultCode::kModuleLoadFailed;
		}

		rkit::Vector<rkit::UniquePtr<rkit::render::IRenderAdapter>> adapters;
		RKIT_CHECK(renderDriver->EnumerateAdapters(adapters));

		if (adapters.Count() == 0)
		{
			rkit::log::Error("No available adapters");
			return rkit::ResultCode::kOperationFailed;
		}

		rkit::Vector<rkit::render::CommandQueueTypeRequest> queueRequests;

		float fOne = 1.0f;
		float fHalf = 0.5f;

		{
			rkit::render::CommandQueueTypeRequest rq;
			rq.m_numQueues = 1;
			rq.m_queuePriorities = &fOne;
			rq.m_type = rkit::render::CommandQueueType::kGraphicsCompute;

			RKIT_CHECK(queueRequests.Append(rq));

			rq.m_type = rkit::render::CommandQueueType::kCopy;
			RKIT_CHECK(queueRequests.Append(rq));
		}

		rkit::render::RenderDeviceCaps requiredCaps;
		rkit::render::RenderDeviceCaps optionalCaps;

		rkit::UniquePtr<rkit::render::IRenderDevice> device;
		RKIT_CHECK(renderDriver->CreateDevice(device, queueRequests.ToSpan(), requiredCaps, optionalCaps, *adapters[0]));

		m_renderDevice = std::move(device);

		// FIXME: Localize
		RKIT_CHECK(progressMonitor->SetText("Loading shader package..."));

		m_pipelinesCacheFileName = pipelinesCacheFile;
		m_setupStep = DeviceSetupStep::kOpenPipelinePackage;
		m_stepCompleted = false;

		rkit::UniquePtr<rkit::IJobRunner> jobRunner;
		RKIT_CHECK(rkit::New<CheckPipelinesJob>(jobRunner, *this, pipelinesFile, pipelinesCacheFile, canUpdatePipelineCache));

		RKIT_CHECK(m_threadPool.GetJobQueue()->CreateJob(nullptr, rkit::JobType::kIO, std::move(jobRunner), nullptr));

		return rkit::ResultCode::kOK;
	}

	void GraphicsSubsystem::WaitForRenderingTasks()
	{
	}

	rkit::Result GraphicsSubsystem::TransitionDisplayMode()
	{
		if (m_currentDisplayMode.IsSet())
		{
			WaitForRenderingTasks();
			m_gameWindow.Reset();
		}

		m_currentDisplayMode = m_desiredDisplayMode;

		rkit::render::IDisplayManager *displayManager = rkit::GetDrivers().m_systemDriver->GetDisplayManager();

		rkit::UniquePtr<rkit::render::IDisplay> display;
		RKIT_CHECK(displayManager->CreateDisplay(display, m_currentDisplayMode.Get()));

		RKIT_CHECK(RenderedWindowBase::Create(m_gameWindow, std::move(display), m_renderDevice.Get(), 1));

		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result GraphicsSubsystem::TransitionBackend()
	{
		// TODO: Wait for all tasks to complete
		if (m_resourceManager.IsValid())
			m_resourceManager->UnloadAll();

		WaitForRenderingTasks();

		m_gameWindow.Reset();

		m_renderDevice.Reset();

		m_currentDisplayMode.Reset();
		m_desiredDisplayMode = rkit::render::DisplayMode::kSplash;

		m_backend = m_desiredBackend;

		rkit::StringView backendModule;
		rkit::StringView pipelinesFile;
		rkit::StringView pipelinesCacheFile;
		bool canUpdatePipelineCache = false;

		switch (m_backend.Get())
		{
		case anox::RenderBackend::kVulkan:
			backendModule = "Render_Vulkan";
			pipelinesFile = "pipelines_vk.rkp";
			pipelinesCacheFile = "pipeline_cache_vk.rsf";
			canUpdatePipelineCache = true;
			break;

		default:
			return rkit::ResultCode::kInternalError;
		}

		return CreateGameDisplayAndDevice(backendModule, pipelinesFile, pipelinesCacheFile, canUpdatePipelineCache);
	}

	rkit::Result GraphicsSubsystem::KickOffMergedPipelineLoad()
	{
		size_t totalPipelines = m_livePipelineSets->m_graphicsPipelines.Count();

		rkit::render::IProgressMonitor *progressMonitor = m_gameWindow->GetDisplay().GetProgressMonitor();
		if (progressMonitor)
		{
			RKIT_CHECK(progressMonitor->SetRange(0, totalPipelines));
			m_setupProgress = 0;
		}

		rkit::UniquePtr<rkit::IJobRunner> jobRunner;
		RKIT_CHECK(rkit::New<LoadOneGraphicsPipelineJob>(jobRunner, *this, 0));

		RKIT_CHECK(m_threadPool.GetJobQueue()->CreateJob(nullptr, rkit::JobType::kIO, std::move(jobRunner), nullptr));

		return rkit::ResultCode::kOK;
	}

	rkit::Result GraphicsSubsystem::KickOffNextSetupStep()
	{
		if (m_setupStep == DeviceSetupStep::kOpenPipelinePackage)
		{
			m_setupStep = DeviceSetupStep::kFirstTryLoadingPipelines;

			if (m_haveExistingMergedCache)
			{
				RKIT_CHECK(KickOffMergedPipelineLoad());
				return rkit::ResultCode::kOK;
			}
			else
			{
				m_setupStep = DeviceSetupStep::kFirstTryLoadingPipelines;
				m_stepFailed = true;

				// Fall through
			}
		}

		if (m_setupStep == DeviceSetupStep::kFirstTryLoadingPipelines)
		{
			if (m_stepFailed)
			{
				m_setupStep = DeviceSetupStep::kCompilingPipelines;
				m_setupProgress = 0;

				rkit::render::IProgressMonitor *progressMonitor = m_gameWindow->GetDisplay().GetProgressMonitor();
				if (progressMonitor)
				{
					RKIT_CHECK(progressMonitor->SetText("Optimizing shaders..."));
				}

				rkit::RCPtr<rkit::Job> initJobRC;

				{
					rkit::UniquePtr<rkit::IJobRunner> jobRunner;
					RKIT_CHECK(rkit::New<CreateNewIndividualCacheJob>(jobRunner, *this, m_pipelinesCacheFileName));

					RKIT_CHECK(m_threadPool.GetJobQueue()->CreateJob(&initJobRC, rkit::JobType::kNormalPriority, std::move(jobRunner), nullptr));
				}

				rkit::Vector<rkit::RCPtr<rkit::Job>> jobs;

				for (size_t i = 0; i < m_livePipelineSets->m_graphicsPipelines.Count(); i++)
				{
					rkit::UniquePtr<rkit::IJobRunner> jobRunner;
					RKIT_CHECK(rkit::New<CompileOneGraphicsPipelineJob>(jobRunner, *this, i));

					// FIXME: Change these to low-priority
					rkit::RCPtr<rkit::Job> job;
					RKIT_CHECK(m_threadPool.GetJobQueue()->CreateJob(&job, rkit::JobType::kNormalPriority, std::move(jobRunner), rkit::Span<rkit::RCPtr<rkit::Job>>(&initJobRC, 1).ToValueISpan()));

					RKIT_CHECK(jobs.Append(std::move(job)));
				}

				rkit::UniquePtr<rkit::IJobRunner> finishedJobRunner;
				RKIT_CHECK(rkit::New<DoneCompilingPipelinesJob>(finishedJobRunner, *this));

				RKIT_CHECK(m_threadPool.GetJobQueue()->CreateJob(nullptr, rkit::JobType::kNormalPriority, std::move(finishedJobRunner), jobs.ToSpan().ToValueISpan()));

				return rkit::ResultCode::kOK;
			}
			else
			{
				m_setupStep = DeviceSetupStep::kSecondTryLoadingPipelines;
				m_setupProgress = 0;

				// Fall through
			}
		}

		if (m_setupStep == DeviceSetupStep::kCompilingPipelines)
		{
			m_setupStep = DeviceSetupStep::kMergingPipelines;
			m_setupProgress = 0;

			const size_t numGraphicsPipelines = m_livePipelineSets->m_graphicsPipelines.Count();
			const size_t numTotalPipelines = numGraphicsPipelines;

			rkit::render::IProgressMonitor *progressMonitor = m_gameWindow->GetDisplay().GetProgressMonitor();
			if (progressMonitor)
			{
				RKIT_CHECK(progressMonitor->SetText("Building shader cache..."));
				RKIT_CHECK(progressMonitor->SetRange(0, numTotalPipelines));
			}

			rkit::UniquePtr<rkit::IJobRunner> jobRunner;
			RKIT_CHECK(rkit::New<MergePipelineLibraryJob>(jobRunner, *this));

			RKIT_CHECK(m_threadPool.GetJobQueue()->CreateJob(nullptr, rkit::JobType::kNormalPriority, std::move(jobRunner), nullptr));

			return rkit::ResultCode::kOK;
		}

		if (m_setupStep == DeviceSetupStep::kMergingPipelines)
		{
			m_setupStep = DeviceSetupStep::kSecondTryLoadingPipelines;
			m_setupProgress = 0;

			m_haveExistingMergedCache = true;

			rkit::render::IProgressMonitor *progressMonitor = m_gameWindow->GetDisplay().GetProgressMonitor();
			if (progressMonitor)
			{
				RKIT_CHECK(progressMonitor->SetText("Reloading shaders..."));
			}

			RKIT_CHECK(KickOffMergedPipelineLoad());

			return rkit::ResultCode::kOK;
		}

		if (m_setupStep == DeviceSetupStep::kSecondTryLoadingPipelines)
		{
			if (m_stepFailed)
			{
				return rkit::ResultCode::kOperationFailed;
			}
			else
			{
				m_setupStep = DeviceSetupStep::kFinished;
				m_setupProgress = 0;

				m_pipelineLibrary = m_pipelineLibraryLoader->GetFinishedPipeline();
				m_pipelineLibraryLoader.Reset();

				m_desiredDisplayMode = rkit::render::DisplayMode::kBorderlessFullscreen;

				return rkit::ResultCode::kOK;
			}
		}

		return rkit::ResultCode::kNotYetImplemented;
	}

	void GraphicsSubsystem::SetDesiredRenderBackend(RenderBackend renderBackend)
	{
		m_desiredBackend = renderBackend;
	}

	void GraphicsSubsystem::SetDesiredDisplayMode(rkit::render::DisplayMode displayMode)
	{
		m_desiredDisplayMode = displayMode;
	}

	rkit::Result GraphicsSubsystem::TransitionDisplayState()
	{
		if (m_setupStep != DeviceSetupStep::kFinished)
		{
			if (m_stepCompleted)
			{
				m_stepCompleted = false;
				RKIT_CHECK(KickOffNextSetupStep());
			}

			rkit::GetDrivers().m_systemDriver->SleepMSec(1000u / 60u);

			rkit::render::IProgressMonitor *progressMonitor = m_gameWindow->GetDisplay().GetProgressMonitor();
			if (progressMonitor)
			{
				uint64_t progress = 0;
				{
					rkit::MutexLock lock(*m_setupMutex);
					progress = m_setupProgress;
				}

				RKIT_CHECK(progressMonitor->SetValue(progress));
				progressMonitor->FlushEvents();
			}

			RKIT_CHECK(m_threadPool.GetJobQueue()->CheckFault());

			return rkit::ResultCode::kOK;
		}

		if (!m_backend.IsSet() || m_backend.Get() != m_desiredBackend)
			return TransitionBackend();

		if (!m_currentDisplayMode.IsSet() || m_currentDisplayMode.Get() != m_desiredDisplayMode)
			return TransitionDisplayMode();

		return rkit::ResultCode::kOK;
	}

	rkit::render::IRenderDevice *GraphicsSubsystem::GetDevice() const
	{
		return m_renderDevice.Get();
	}

	rkit::data::IDataDriver &GraphicsSubsystem::GetDataDriver() const
	{
		return m_dataDriver;
	}

	const anox::GraphicsSettings &GraphicsSubsystem::GetGraphicsSettings() const
	{
		return m_graphicsSettings;
	}

	void GraphicsSubsystem::SetSplashProgress(uint64_t progress)
	{
		rkit::MutexLock lock(*m_setupMutex);
		m_setupProgress = progress;
	}


	void GraphicsSubsystem::SetPipelineLibraryLoader(rkit::UniquePtr<rkit::render::IPipelineLibraryLoader> &&loader, rkit::UniquePtr<LivePipelineSets> &&livePipelineSets, bool haveExistingMergedCache)
	{
		rkit::MutexLock lock(*m_setupMutex);
		m_pipelineLibraryLoader = std::move(loader);
		m_livePipelineSets = std::move(livePipelineSets);
		m_haveExistingMergedCache = haveExistingMergedCache;
	}

	void GraphicsSubsystem::MarkSetupStepCompleted()
	{
		rkit::MutexLock lock(*m_setupMutex);
		m_stepCompleted = true;
		m_stepFailed = false;
	}

	void GraphicsSubsystem::MarkSetupStepFailed()
	{
		rkit::MutexLock lock(*m_setupMutex);
		m_stepCompleted = true;
		m_stepFailed = true;
	}
}

rkit::Result anox::IGraphicsSubsystem::Create(rkit::UniquePtr<IGraphicsSubsystem> &outSubsystem, rkit::data::IDataDriver &dataDriver, rkit::utils::IThreadPool &threadPool, anox::RenderBackend defaultBackend)
{
	rkit::UniquePtr<anox::GraphicsSubsystem> subsystem;
	RKIT_CHECK(rkit::New<anox::GraphicsSubsystem>(subsystem, dataDriver, threadPool, defaultBackend));

	RKIT_CHECK(subsystem->Initialize());

	outSubsystem = std::move(subsystem);

	return rkit::ResultCode::kOK;
}
