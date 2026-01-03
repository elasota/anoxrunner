#include "AnoxGraphicsResourceManager.h"

#include "AnoxLogicalQueue.h"
#include "AnoxFrameDrawer.h"
#include "AnoxPeriodicResources.h"
#include "AnoxRecordJobRunner.h"
#include "AnoxSubmitJobRunner.h"

#include "anox/AnoxFileSystem.h"
#include "anox/AnoxGraphicsSubsystem.h"

#include "rkit/Render/Barrier.h"
#include "rkit/Render/BufferSpec.h"
#include "rkit/Render/BufferImageFootprint.h"
#include "rkit/Render/BufferResource.h"
#include "rkit/Render/CommandAllocator.h"
#include "rkit/Render/CommandBatch.h"
#include "rkit/Render/CommandEncoder.h"
#include "rkit/Render/CommandQueue.h"
#include "rkit/Render/DeviceCaps.h"
#include "rkit/Render/DisplayManager.h"
#include "rkit/Render/HeapSpec.h"
#include "rkit/Render/ImageResource.h"
#include "rkit/Render/ImageSpec.h"
#include "rkit/Render/Memory.h"
#include "rkit/Render/PipelineLibrary.h"
#include "rkit/Render/PipelineLibraryItem.h"
#include "rkit/Render/PipelineLibraryItemProtos.h"
#include "rkit/Render/PipelineLibraryLoader.h"
#include "rkit/Render/RenderDefs.h"
#include "rkit/Render/RenderDriver.h"
#include "rkit/Render/RenderDevice.h"
#include "rkit/Render/RenderPassInstance.h"
#include "rkit/Render/RenderPassResources.h"
#include "rkit/Render/SwapChain.h"

#include "rkit/Utilities/ThreadPool.h"

#include "rkit/Data/DataDriver.h"
#include "rkit/Data/DDSFile.h"
#include "rkit/Data/RenderDataHandler.h"

#include "rkit/Core/Algorithm.h"
#include "rkit/Core/BoolVector.h"
#include "rkit/Core/BufferStream.h"
#include "rkit/Core/DriverModuleInitParams.h"
#include "rkit/Core/Endian.h"
#include "rkit/Core/EnumEnumerator.h"
#include "rkit/Core/Event.h"
#include "rkit/Core/Future.h"
#include "rkit/Core/HashTable.h"
#include "rkit/Core/HybridVector.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/MemoryStream.h"
#include "rkit/Core/Mutex.h"
#include "rkit/Core/MutexLock.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/NoCopy.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/Path.h"
#include "rkit/Core/RefCounted.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/StaticBoolArray.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/UniquePtr.h"
#include "rkit/Core/UtilitiesDriver.h"
#include "rkit/Core/Vector.h"

#include "AnoxGameWindowResources.h"
#include "AnoxRenderedWindow.h"
#include "AnoxTexture.h"
#include "AnoxGraphicsSettings.h"
#include "AnoxGraphicTimelinedResource.h"

namespace anox
{
	struct IGameDataFileSystem;

	class Texture final : public ITexture, public GraphicTimelinedResource
	{
	public:
		explicit Texture(IGraphicsSubsystem &subsystem);

		rkit::render::IImageResource *GetRenderResource() const;
		void SetRenderResources(rkit::UniquePtr<rkit::render::IImageResource> &&resource, rkit::UniquePtr<rkit::render::IMemoryHeap> &&heap);

	private:
		rkit::UniquePtr<rkit::render::IMemoryHeap> m_heap;
		rkit::UniquePtr<rkit::render::IImageResource> m_resource;
	};

	class GraphicsSubsystem final : public IGraphicsSubsystem, public rkit::NoCopy
	{
	public:
		explicit GraphicsSubsystem(IGameDataFileSystem &fileSystem, rkit::data::IDataDriver &dataDriver, rkit::utils::IThreadPool &threadPool, anox::RenderBackend desiredBackend);
		~GraphicsSubsystem();

		rkit::Result Initialize();

		void SetDesiredRenderBackend(RenderBackend renderBackend) override;

		void SetDesiredDisplayMode(rkit::render::DisplayMode displayMode) override;
		rkit::Result TransitionDisplayState() override;

		rkit::Result RetireOldestFrame() override;
		rkit::Result PumpAsyncUploads() override;
		rkit::Result StartRendering() override;
		rkit::Result DrawFrame() override;
		rkit::Result EndFrame() override;

		rkit::Optional<rkit::render::DisplayMode> GetDisplayMode() const override;

		rkit::Result CreateAndQueueRecordJob(rkit::RCPtr<rkit::Job> *outJob, LogicalQueueType queueType, rkit::UniquePtr<IRecordJobRunner> &&jobRunner, const rkit::JobDependencyList &dependencies) override;
		rkit::Result CreateAndQueueSubmitJob(rkit::RCPtr<rkit::Job> *outJob, LogicalQueueType queueType, rkit::UniquePtr<ISubmitJobRunner> &&jobRunner, const rkit::JobDependencyList &dependencies) override;

		rkit::Result CreateAsyncCreateTextureJob(rkit::RCPtr<rkit::Job> *outJob, rkit::RCPtr<ITexture> &outTexture, const rkit::RCPtr<rkit::Vector<uint8_t>> &textureData, const rkit::JobDependencyList &dependencies) override;

		void CondemnTimelinedResource(GraphicTimelinedResource &timelinedResource) override;

		void MarkSetupStepCompleted();
		void MarkSetupStepFailed();

		rkit::render::IRenderDevice *GetDevice() const;
		rkit::data::IDataDriver &GetDataDriver() const;
		const anox::GraphicsSettings &GetGraphicsSettings() const;

		void SetSplashProgress(uint64_t progress);

	private:
		struct UploadActionSet;
		struct UploadTask;
		struct FrameSyncPoint;

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

		struct FrameSyncPointCommandListHandler
		{
			rkit::UniquePtr<rkit::render::IBaseCommandAllocator> m_commandAllocator;
			rkit::render::IBaseCommandQueue* m_commandQueue = nullptr;
		};

		class CheckPipelinesJob final : public rkit::IJobRunner
		{
		public:
			explicit CheckPipelinesJob(GraphicsSubsystem &graphicsSubsystem, const rkit::Future<rkit::UniquePtr<rkit::ISeekableReadStream>> &stream, const rkit::CIPathView &pipelinesCacheFileName);

			rkit::Result Run() override;

		private:
			rkit::Result EvaluateLivePipelineSets(const rkit::data::IRenderDataPackage &package, RenderDataConfigurator &configurator, LivePipelineSets &lpSets);
			rkit::Result RecursiveEvaluatePermutationTree(const rkit::data::IRenderDataPackage &package, RenderDataConfigurator &configurator, rkit::Vector<LivePipelineSets::LivePipeline> &pipelines,
				rkit::HashMap<size_t, rkit::Optional<int32_t>> &staticResolutions, size_t pipelineIndex, size_t base, const rkit::render::ShaderPermutationTree *tree);

			GraphicsSubsystem &m_graphicsSubsystem;
			rkit::Future<rkit::UniquePtr<rkit::ISeekableReadStream>> m_stream;
			rkit::CIPathView m_pipelinesCacheFileName;
		};

		class CreateNewIndividualCacheJob final : public rkit::IJobRunner
		{
		public:
			explicit CreateNewIndividualCacheJob(GraphicsSubsystem &graphicsSubsystem, const rkit::CIPathView &pipelinesCacheFileName);

			rkit::Result Run() override;

		private:
			GraphicsSubsystem &m_graphicsSubsystem;
			rkit::CIPathView m_pipelinesCacheFileName;
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

		class ResetCommandAllocatorJob final : public rkit::IJobRunner
		{
		public:
			explicit ResetCommandAllocatorJob(FrameSyncPointCommandListHandler &cmdListHandler);

			rkit::Result Run() override;

		private:
			FrameSyncPointCommandListHandler &m_cmdListHandler;
		};

		class RunRecordJobRunner final : public rkit::IJobRunner
		{
		public:
			explicit RunRecordJobRunner(rkit::UniquePtr<IRecordJobRunner> &&recordJob, rkit::render::IBaseCommandAllocator &cmdAlloc);

			rkit::Result Run() override;

		private:
			rkit::UniquePtr<IRecordJobRunner> m_recordJob;
			rkit::render::IBaseCommandAllocator &m_cmdAlloc;
		};

		class RunSubmitJobRunner final : public rkit::IJobRunner
		{
		public:
			explicit RunSubmitJobRunner(rkit::UniquePtr<ISubmitJobRunner> &&submitJob, rkit::render::IBaseCommandQueue &cmdQueue);

			rkit::Result Run() override;

		private:
			rkit::UniquePtr<ISubmitJobRunner> m_submitJob;
			rkit::render::IBaseCommandQueue &m_cmdQueue;
		};

		class AllocateTextureStorageAndPostCopyJobRunner final : public rkit::IJobRunner
		{
		public:
			explicit AllocateTextureStorageAndPostCopyJobRunner(GraphicsSubsystem &graphicsSubsystem,
				const rkit::RCPtr<Texture> &texture,
				const rkit::RCPtr<rkit::Vector<uint8_t>> &textureData,
				const rkit::RCPtr<rkit::JobSignaler> &doneCopyingSignaler);

			rkit::Result Run() override;

		private:
			GraphicsSubsystem &m_graphicsSubsystem;
			const rkit::RCPtr<Texture> m_texture;
			const rkit::RCPtr<rkit::Vector<uint8_t>> m_textureData;
			rkit::RCPtr<rkit::JobSignaler> m_doneCopyingSignaler;
		};

		class StripedMemCopyJobRunner final : public rkit::IJobRunner
		{
		public:
			StripedMemCopyJobRunner(GraphicsSubsystem &graphicsSubsystem, uint8_t syncPointIndex);

			rkit::Result Run() override;

		private:
			GraphicsSubsystem &m_graphicsSubsystem;
			uint8_t m_syncPointIndex;
		};

		class StripedMemCopyCleanupJobRunner final : public rkit::IJobRunner
		{
		public:
			explicit StripedMemCopyCleanupJobRunner(GraphicsSubsystem &graphicsSubsystem, uint8_t syncPointIndex);

			rkit::Result Run() override;

		private:
			GraphicsSubsystem &m_graphicsSubsystem;
			uint8_t m_syncPointIndex;
		};

		class AsyncDisposeResourceJobRunner final : public rkit::IJobRunner
		{
		public:
			explicit AsyncDisposeResourceJobRunner(GraphicTimelinedResourceStack &&stack);

			rkit::Result Run() override;

		private:
			GraphicTimelinedResourceStack m_stack;
		};

		class CloseFrameRecordRunner final : public IGraphicsRecordJobRunner_t
		{
		public:
			explicit CloseFrameRecordRunner(rkit::render::IBaseCommandBatch *&outBatchPtr, rkit::render::IBinaryGPUWaitableFence *asyncUploadFence);

			rkit::Result RunRecord(rkit::render::IGraphicsCommandAllocator &cmdAlloc) override;

		private:
			rkit::render::IBaseCommandBatch *&m_outBatchPtr;
			rkit::render::IBinaryGPUWaitableFence *m_asyncUploadFence;
		};


		class AsyncUploadPrepareTargetsSubmitRunner final : public ICopySubmitJobRunner_t
		{
		public:
			explicit AsyncUploadPrepareTargetsSubmitRunner();

			rkit::Result RunSubmit(rkit::render::ICopyCommandQueue &commandQueue) override;

			rkit::render::ICopyCommandBatch **GetCmdBatchRef();

		private:
			rkit::render::ICopyCommandBatch *m_cmdBatch = nullptr;
		};

		class AsyncUploadPrepareTargetsRecordRunner final : public ICopyRecordJobRunner_t
		{
		public:
			explicit AsyncUploadPrepareTargetsRecordRunner(rkit::render::ICopyCommandBatch **cmdBatchRef, FrameSyncPoint &syncPoint);

			rkit::Result RunRecord(rkit::render::ICopyCommandAllocator &cmdAlloc) override;

		private:
			rkit::render::ICopyCommandBatch **m_cmdBatchRef;
			FrameSyncPoint &m_syncPoint;
		};

		class CopyAsyncUploadsSubmitRunner final : public ICopySubmitJobRunner_t
		{
		public:
			explicit CopyAsyncUploadsSubmitRunner(FrameSyncPoint &syncPoint);

			rkit::Result RunSubmit(rkit::render::ICopyCommandQueue &commandQueue) override;

			rkit::render::ICopyCommandBatch **GetCmdBatchRef();

		private:
			FrameSyncPoint &m_syncPoint;
			rkit::render::ICopyCommandBatch *m_cmdBatch = nullptr;
		};

		class CopyAsyncUploadsRecordRunner final : public ICopyRecordJobRunner_t
		{
		public:
			explicit CopyAsyncUploadsRecordRunner(rkit::render::ICopyCommandBatch **cmdBatchRef, FrameSyncPoint &syncPoint, uint64_t globalSyncPoint,
				rkit::render::IBinaryGPUWaitableFence *fence);

			rkit::Result RunRecord(rkit::render::ICopyCommandAllocator &cmdAlloc) override;

		private:
			rkit::render::IBinaryGPUWaitableFence *m_fence;
			rkit::render::ICopyCommandBatch **m_cmdBatchRef;
			FrameSyncPoint &m_syncPoint;
			uint64_t m_globalSyncPoint;
		};

		class CloseFrameSubmitRunner final : public IGraphicsSubmitJobRunner_t
		{
		public:
			explicit CloseFrameSubmitRunner(rkit::render::IBaseCommandBatch *&frameEndBatchPtr);

			rkit::Result RunSubmit(rkit::render::IGraphicsCommandQueue &commandQueue) override;

			rkit::render::IBaseCommandBatch **GetLastBatchRef();

		private:
			rkit::render::IBaseCommandBatch *m_lastBatch = nullptr;
			rkit::render::IBaseCommandBatch *&m_frameEndBatchPtr;
		};

		class SyncPointFenceFactory final : public IBinaryGPUWaitableFenceFactory
		{
		public:
			SyncPointFenceFactory(GraphicsSubsystem &subsystem, uint8_t syncPoint);

			rkit::Result CreateFence(rkit::render::IBinaryGPUWaitableFence *&outFence) override;

		private:
			GraphicsSubsystem &m_subsystem;
			uint8_t m_syncPoint;
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

		static const size_t kNumLogicalQueueTypes = static_cast<size_t>(LogicalQueueType::kCount);

		struct LogicalQueueBase
		{
			rkit::RCPtr<rkit::Job> m_lastRecordJob;
			rkit::RCPtr<rkit::Job> m_lastSubmitJob;

			virtual rkit::render::IBaseCommandQueue *GetBaseCommandQueue() const = 0;
			virtual rkit::Result CreateCommandAllocator(rkit::render::IRenderDevice &renderDevice, rkit::UniquePtr<rkit::render::IBaseCommandAllocator> &cmdAlloc, bool isBundle) = 0;
		};

		template<class TCommandQueueType, class TCommandAllocatorType,
			rkit::Result (TCommandQueueType::*TCommandAllocCreationMethod)(rkit::UniquePtr<TCommandAllocatorType>&, bool),
			TCommandQueueType *(rkit::render::IBaseCommandQueue::*TQueueConversionMethod)()
		>
		struct LogicalQueue final : public LogicalQueueBase
		{
			TCommandQueueType *m_commandQueue = nullptr;

			inline TCommandQueueType *GetCommandQueue() const
			{
				return m_commandQueue;
			}

			inline TCommandAllocatorType *GetCommandAllocator() const
			{
				return static_cast<TCommandAllocatorType *>(this->m_commandAllocator.Get());
			}

			rkit::render::IBaseCommandQueue *GetBaseCommandQueue() const override
			{
				return m_commandQueue;
			}

			rkit::Result CreateCommandAllocator(rkit::render::IRenderDevice &renderDevice, rkit::UniquePtr<rkit::render::IBaseCommandAllocator> &cmdAlloc, bool isBundle) override
			{
				rkit::UniquePtr<TCommandAllocatorType> alloc;
				RKIT_CHECK((m_commandQueue->*TCommandAllocCreationMethod)(alloc, isBundle));

				cmdAlloc = std::move(alloc);

				return rkit::ResultCode::kOK;
			}
		};

		struct UploadTask : rkit::RefCounted
		{
			virtual ~UploadTask() {}

			virtual rkit::Result StartTask(UploadActionSet &actionSet, GraphicsSubsystem &subsystem) = 0;
			virtual rkit::Result PumpTask(bool &isCompleted, UploadActionSet &actionSet, GraphicsSubsystem &subsystem) = 0;
			virtual void OnCopyCompleted() = 0;
			virtual void OnUploadCompleted() = 0;
		};

		struct BufferStripedMemCopyAction
		{
			const uint8_t *m_start = nullptr;
			uint32_t m_rowSizeBytes = 0;
			uint32_t m_rowInPitch = 0;
			uint32_t m_rowOutPitch = 0;
			uint32_t m_rowCount = 0;

			rkit::render::MemoryPosition m_destPosition;
		};

		struct PrepareImageForTransferAction
		{
			rkit::render::IImageResource *m_image = nullptr;
		};

		struct BufferToTextureCopyAction
		{
			rkit::render::IBufferResource *m_buffer = nullptr;
			rkit::RCPtr<Texture> m_texture;

			rkit::render::BufferImageFootprint m_footprint = {};

			rkit::render::ImageRect3D m_destRect = {};

			rkit::render::ImageLayout m_imageLayout = rkit::render::ImageLayout::Count;
			uint32_t m_mipLevel = 0;
			uint32_t m_arrayElement = 0;
			rkit::render::ImagePlane m_imagePlane = rkit::render::ImagePlane::kCount;
		};

		struct UploadActionSet
		{
			// NOTE: While m_stripedMemCpy and m_bufferToTextureCopy are always 1:1,
			// m_prepareImageForTransfer is not, because it is only run once for
			// each target texture.
			rkit::Vector<PrepareImageForTransferAction> m_prepareImageForTransfer;
			rkit::Vector<BufferStripedMemCopyAction> m_stripedMemCpy;
			rkit::Vector<BufferToTextureCopyAction> m_bufferToTextureCopy;
			rkit::Vector<rkit::RCPtr<UploadTask>> m_retiredUploadTasks;

			void ClearActions();

			rkit::RCPtr<rkit::Job> m_memCopyJob;
			rkit::RCPtr<rkit::Job> m_cleanupJob;
		};

		struct FrameSyncPoint
		{
			rkit::render::IBaseCommandBatch *m_frameEndBatch = nullptr;
			rkit::RCPtr<rkit::Job> m_frameEndJob;

			GraphicTimelinedResourceStack m_condemnedResources;

			UploadActionSet m_asyncUploadActionSet;
			uint32_t m_asyncUploadHeapFinalHighMark = 0;

			rkit::Vector<rkit::UniquePtr<rkit::render::IBinaryGPUWaitableFence>> m_gpuWaitableFences;
			size_t m_usedGPUWaitableFences = 0;

			rkit::StaticArray<FrameSyncPointCommandListHandler, static_cast<size_t>(rkit::render::CommandQueueType::kCount)> m_commandListHandlers;

			void Reset();
		};

		struct TextureUploadTask final : public UploadTask
		{
			rkit::RCPtr<Texture> m_texture;
			rkit::RCPtr<rkit::Vector<uint8_t>> m_textureData;
			size_t m_textureDataOffset = 0;
			rkit::RCPtr<rkit::JobSignaler> m_doneSignaler;

			rkit::render::ImageSpec m_spec = {};

			uint32_t m_arrayElement = 0;
			uint32_t m_mipLevel = 0;

			uint32_t m_blockWidth = 1;
			uint32_t m_blockHeight = 1;
			uint32_t m_blockDepth = 1;
			uint32_t m_blockSizeBytes = 4;

			rkit::Result StartTask(UploadActionSet &actionSet, GraphicsSubsystem &subsystem) override;
			rkit::Result PumpTask(bool &isCompleted, UploadActionSet &actionSet, GraphicsSubsystem &subsystem) override;
			void OnCopyCompleted() override;
			void OnUploadCompleted() override;
		};

		rkit::Result CreateAndQueueJob(rkit::RCPtr<rkit::Job> *outJob, LogicalQueueType queueType, rkit::UniquePtr<rkit::IJobRunner> &&jobRunner, const rkit::JobDependencyList &dependencies, rkit::RCPtr<rkit::Job> (LogicalQueueBase::*queueMember));

		rkit::Result TransitionDisplayMode();
		rkit::Result TransitionBackend();

		rkit::Result KickOffNextSetupStep();
		rkit::Result KickOffMergedPipelineLoad();

		void SetPipelineLibraryLoader(rkit::UniquePtr<rkit::render::IPipelineLibraryLoader> &&loader, rkit::UniquePtr<LivePipelineSets> &&livePipelineSets, bool haveExistingMergedCache);

		rkit::Result CreateGameDisplayAndDevice(const rkit::StringView &renderModule, const rkit::CIPathView &pipelinesFile, const rkit::CIPathView &pipelinesCacheFile, bool canUpdatePipelineCache);

		rkit::Result WaitForRenderingTasks();

		rkit::Result PostAsyncUploadTask(rkit::RCPtr<UploadTask> &&uploadTask);
		rkit::Result PumpActiveAsyncUploadTask(UploadActionSet &actionSet);

		void GetAsyncUploadHeapStats(uint32_t &outContiguousLow, uint32_t &outContiguousHigh) const;
		void ConsumeAsyncUploadSpace(uint32_t space);

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

		rkit::CIPathView m_pipelinesCacheFileName;

		rkit::UniquePtr<rkit::IMutex> m_setupMutex;
		DeviceSetupStep m_setupStep = DeviceSetupStep::kFinished;
		uint64_t m_setupProgress = 0;

		bool m_stepCompleted = false;
		bool m_stepFailed = false;

		rkit::UniquePtr<rkit::render::IPipelineLibrary> m_pipelineLibrary;
		rkit::UniquePtr<rkit::render::IPipelineLibraryLoader> m_pipelineLibraryLoader;
		rkit::UniquePtr<LivePipelineSets> m_livePipelineSets;
		bool m_haveExistingMergedCache = false;

		IGameDataFileSystem &m_fileSystem;
		rkit::data::IDataDriver &m_dataDriver;

		uint8_t m_numSyncPoints = 3;
		uint8_t m_currentSyncPoint = 0;
		uint64_t m_currentGlobalSyncPoint = 0;

		rkit::render::IGraphicsCommandQueue *m_graphicsQueue = nullptr;
		rkit::render::IGraphicsComputeCommandQueue *m_graphicsComputeQueue = nullptr;
		rkit::render::IComputeCommandQueue *m_asyncComputeQueue = nullptr;
		rkit::render::ICopyCommandQueue *m_dmaQueue = nullptr;

		rkit::Vector<FrameSyncPoint> m_syncPoints;
		rkit::UniquePtr<rkit::render::ICPUFenceWaiter> m_fenceWaiter;
		GraphicTimelinedResourceStack m_unsortedCondemnedResources;

		LogicalQueue<rkit::render::ICopyCommandQueue, rkit::render::ICopyCommandAllocator,
			&rkit::render::ICopyCommandQueue::CreateCopyCommandAllocator,
			&rkit::render::IBaseCommandQueue::ToCopyCommandQueue
		> m_dmaLogicalQueue;

		LogicalQueue<rkit::render::IGraphicsComputeCommandQueue, rkit::render::IGraphicsComputeCommandAllocator,
			&rkit::render::IGraphicsComputeCommandQueue::CreateGraphicsComputeCommandAllocator,
			&rkit::render::IBaseCommandQueue::ToGraphicsComputeCommandQueue
		> m_graphicsComputeLogicalQueue;

		LogicalQueue<rkit::render::IGraphicsCommandQueue, rkit::render::IGraphicsCommandAllocator,
			&rkit::render::IGraphicsCommandQueue::CreateGraphicsCommandAllocator,
			&rkit::render::IBaseCommandQueue::ToGraphicsCommandQueue
		> m_graphicsLogicalQueue;

		LogicalQueue<rkit::render::IComputeCommandQueue, rkit::render::IComputeCommandAllocator,
			&rkit::render::IComputeCommandQueue::CreateComputeCommandAllocator,
			&rkit::render::IBaseCommandQueue::ToComputeCommandQueue
		> m_asyncComputeLogicalQueue;

		rkit::StaticArray<LogicalQueueBase*, kNumLogicalQueueTypes> m_logicalQueues;

		rkit::RCPtr<PerFrameResources> m_currentFrameResources;

		rkit::UniquePtr<rkit::IEvent> m_shutdownJoinEvent;
		rkit::UniquePtr<rkit::IEvent> m_shutdownTerminateEvent;

		rkit::UniquePtr<rkit::IEvent> m_prevFrameWaitWakeEvent;
		rkit::UniquePtr<rkit::IEvent> m_prevFrameWaitTerminateEvent;

		rkit::UniquePtr<IFrameDrawer> m_frameDrawer;

		bool m_enableAsyncUpload = false;

		rkit::UniquePtr<rkit::IMutex> m_asyncUploadMutex;
		rkit::Vector<rkit::RCPtr<UploadTask>> m_asyncUploadWaitingTasks;

		rkit::RCPtr<UploadTask> m_asyncUploadActiveTask;
		rkit::UniquePtr<rkit::render::IMemoryHeap> m_asyncUploadHeap;
		rkit::UniquePtr<rkit::render::IBufferResource> m_asyncUploadBuffer;

		uint32_t m_asyncUploadHeapLowMark = 0;
		uint32_t m_asyncUploadHeapHighMark = 0;
		uint32_t m_asyncUploadHeapSize = 0;
		uint32_t m_asyncUploadHeapAlignment = 0;
		bool m_asyncUploadHeapIsFull = false;
	};


	Texture::Texture(IGraphicsSubsystem &subsystem)
		: GraphicTimelinedResource(subsystem)
	{
	}

	rkit::render::IImageResource *Texture::GetRenderResource() const
	{
		return m_resource.Get();
	}

	void Texture::SetRenderResources(rkit::UniquePtr<rkit::render::IImageResource> &&resource, rkit::UniquePtr<rkit::render::IMemoryHeap> &&heap)
	{
		m_resource = std::move(resource);
		m_heap = std::move(heap);
	}

	GraphicsSubsystem::CheckPipelinesJob::CheckPipelinesJob(GraphicsSubsystem &graphicsSubsystem, const rkit::Future<rkit::UniquePtr<rkit::ISeekableReadStream>> &stream, const rkit::CIPathView &pipelinesCacheFileName)
		: m_graphicsSubsystem(graphicsSubsystem)
		, m_stream(stream)
		, m_pipelinesCacheFileName(pipelinesCacheFileName)
	{
	}

	rkit::Result GraphicsSubsystem::CheckPipelinesJob::Run()
	{
		rkit::ISystemDriver *sysDriver = rkit::GetDrivers().m_systemDriver;
		rkit::IUtilitiesDriver *utilsDriver = rkit::GetDrivers().m_utilitiesDriver;
		rkit::data::IDataDriver *dataDriver = &m_graphicsSubsystem.GetDataDriver();

		rkit::UniquePtr<rkit::ISeekableReadStream> pipelinesFile = std::move(m_stream.GetResult());

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
		rkit::Result openResult = sysDriver->OpenFileRead(cacheReadStream, rkit::FileLocation::kUserSettingsDirectory, m_pipelinesCacheFileName);
		if (!rkit::utils::ResultIsOK(openResult))
		{
			rkit::log::Error("Failed to open pipeline cache");
			return openResult;
		}

		rkit::FilePos_t binaryContentStart = pipelinesFile->Tell();

		rkit::UniquePtr<rkit::render::IPipelineLibraryLoader> loader;
		RKIT_CHECK(m_graphicsSubsystem.GetDevice()->CreatePipelineLibraryLoader(loader, std::move(configurator), std::move(package), std::move(pipelinesFile), binaryContentStart));

		RKIT_CHECK(loader->LoadObjectsFromPackage());

		loader->SetMergedLibraryStream(std::move(cacheReadStream), nullptr);

		rkit::Result openMergedResult = loader->OpenMergedLibrary();

		m_graphicsSubsystem.SetPipelineLibraryLoader(std::move(loader), std::move(pipelineSets), rkit::utils::ResultIsOK(openMergedResult));

		if (rkit::utils::ResultIsOK(openMergedResult))
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

	GraphicsSubsystem::CreateNewIndividualCacheJob::CreateNewIndividualCacheJob(GraphicsSubsystem &graphicsSubsystem, const rkit::CIPathView &pipelinesCacheFileName)
		: m_graphicsSubsystem(graphicsSubsystem)
		, m_pipelinesCacheFileName(pipelinesCacheFileName)
	{
	}

	rkit::Result GraphicsSubsystem::CreateNewIndividualCacheJob::Run()
	{
		rkit::ISystemDriver *sysDriver = rkit::GetDrivers().m_systemDriver;

		m_graphicsSubsystem.m_pipelineLibraryLoader->CloseMergedLibrary(true, true);

		rkit::UniquePtr<rkit::ISeekableReadWriteStream> pipelinesFile;
		RKIT_CHECK(sysDriver->OpenFileReadWrite(pipelinesFile, rkit::FileLocation::kUserSettingsDirectory, m_graphicsSubsystem.m_pipelinesCacheFileName, true, true, true));

		rkit::ISeekableReadWriteStream *writeStream = pipelinesFile.Get();

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

		if (!rkit::utils::ResultIsOK(loadPipelineResult))
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

	GraphicsSubsystem::ResetCommandAllocatorJob::ResetCommandAllocatorJob(FrameSyncPointCommandListHandler &cmdListHandler)
		: m_cmdListHandler(cmdListHandler)
	{
	}

	rkit::Result GraphicsSubsystem::ResetCommandAllocatorJob::Run()
	{
		RKIT_CHECK(m_cmdListHandler.m_commandAllocator->ResetCommandAllocator(false));

		return rkit::ResultCode::kOK;
	}

	GraphicsSubsystem::RunRecordJobRunner::RunRecordJobRunner(rkit::UniquePtr<IRecordJobRunner> &&recordJob, rkit::render::IBaseCommandAllocator &cmdAlloc)
		: m_recordJob(std::move(recordJob))
		, m_cmdAlloc(cmdAlloc)
	{
	}

	rkit::Result GraphicsSubsystem::RunRecordJobRunner::Run()
	{
		return m_recordJob->RunBase(m_cmdAlloc);
	}

	GraphicsSubsystem::RunSubmitJobRunner::RunSubmitJobRunner(rkit::UniquePtr<ISubmitJobRunner> &&submitJob, rkit::render::IBaseCommandQueue &cmdQueue)
		: m_submitJob(std::move(submitJob))
		, m_cmdQueue(cmdQueue)
	{
	}

	rkit::Result GraphicsSubsystem::RunSubmitJobRunner::Run()
	{
		return m_submitJob->RunBase(m_cmdQueue);
	}


	GraphicsSubsystem::AllocateTextureStorageAndPostCopyJobRunner::AllocateTextureStorageAndPostCopyJobRunner(
		GraphicsSubsystem &graphicsSubsystem,
		const rkit::RCPtr<Texture> &texture,
		const rkit::RCPtr<rkit::Vector<uint8_t>> &textureData,
		const rkit::RCPtr<rkit::JobSignaler> &doneCopyingSignaler)
		: m_graphicsSubsystem(graphicsSubsystem)
		, m_texture(texture)
		, m_textureData(textureData)
		, m_doneCopyingSignaler(doneCopyingSignaler)
	{
	}

	rkit::Result GraphicsSubsystem::AllocateTextureStorageAndPostCopyJobRunner::Run()
	{
		rkit::render::ImageSpec spec = {};

		rkit::data::DDSHeader ddsHeader = {};
		rkit::data::DDSExtendedHeader extHeader = {};

		rkit::ReadOnlyMemoryStream memStream(this->m_textureData.Get()->ToSpan());

		RKIT_CHECK(memStream.ReadAll(&ddsHeader, sizeof(ddsHeader)));

		const bool isExtended =
			(ddsHeader.m_ddsFlags.Get() & rkit::data::DDSFlags::kPixelFormat)
			&& (ddsHeader.m_pixelFormat.m_pixelFormatFlags.Get() & rkit::data::DDSPixelFormatFlags::kFourCC)
			&& (ddsHeader.m_pixelFormat.m_fourCC.Get() == rkit::data::DDSFourCCs::kExtended);

		if (isExtended)
		{
			RKIT_CHECK(memStream.ReadAll(&extHeader, sizeof(extHeader)));
		}

		const size_t headerSize = static_cast<size_t>(memStream.Tell());

		if (ddsHeader.m_magic.Get() != rkit::data::DDSHeader::kExpectedMagic
			|| ddsHeader.m_headerSizeMinus4.Get() != (sizeof(ddsHeader) - 4u))
			return rkit::ResultCode::kDataError;

		const uint32_t ddsFlags = ddsHeader.m_ddsFlags.Get();
		const uint32_t width = (ddsFlags & rkit::data::DDSFlags::kWidth) ? ddsHeader.m_width.Get() : 1;
		const uint32_t height = (ddsFlags & rkit::data::DDSFlags::kHeight) ? ddsHeader.m_height.Get() : 1;
		const uint32_t depth = (ddsFlags & rkit::data::DDSFlags::kDepth) ? ddsHeader.m_depth.Get() : 1;
		const uint32_t levels = (ddsFlags & rkit::data::DDSFlags::kMipMapCount) ? ddsHeader.m_depth.Get() : 1;
		const uint32_t pitchOrLinearSize = ddsHeader.m_pitchOrLinearSize.Get();

		if (width == 0 || height == 0 || depth == 0)
			return rkit::ResultCode::kDataError;

		const uint32_t maxLevels = static_cast<uint32_t>(rkit::Max(rkit::FindHighestSetBit(width), rkit::FindHighestSetBit(height)) + 1);
		if (levels > maxLevels)
			return rkit::ResultCode::kDataError;

		const uint32_t caps1 = ddsHeader.m_caps.Get();
		const uint32_t caps2 = ddsHeader.m_caps2.Get();
		const uint32_t caps3 = ddsHeader.m_caps3.Get();
		const uint32_t caps4 = ddsHeader.m_caps4.Get();

		rkit::render::TextureFormat textureFormat = rkit::render::TextureFormat::Count;

		uint32_t arraySize = 1;
		uint32_t cubeSides = 1;

		uint32_t pixelBlockWidth = 1;
		uint32_t pixelBlockHeight = 1;
		uint32_t pixelBlockSizeBytes = 0;

		uint32_t maxDimension = 0;
		uint32_t maxLayers = 1;

		if (cubeSides == 1)
		{
			if (depth == 1)
				maxDimension = m_graphicsSubsystem.GetDevice()->GetCaps().GetUInt32Cap(rkit::render::RenderDeviceUInt32Cap::kMaxTexture2DSize);
			else
				maxDimension = m_graphicsSubsystem.GetDevice()->GetCaps().GetUInt32Cap(rkit::render::RenderDeviceUInt32Cap::kMaxTexture3DSize);
		}
		else
		{
			maxDimension = m_graphicsSubsystem.GetDevice()->GetCaps().GetUInt32Cap(rkit::render::RenderDeviceUInt32Cap::kMaxTextureCubeSize);
		}

		if (ddsFlags & rkit::data::DDSFlags::kPixelFormat)
		{
			const rkit::data::DDSPixelFormat &pixelFormat = ddsHeader.m_pixelFormat;

			const uint32_t pixelFormatFlags = (pixelFormat.m_pixelFormatFlags.Get());

			if (pixelFormatFlags & rkit::data::DDSPixelFormatFlags::kFourCC)
			{
				return rkit::ResultCode::kNotYetImplemented;
			}
			else
			{
				const uint32_t rMask = pixelFormat.m_rBitMask.Get();
				const uint32_t gMask = pixelFormat.m_gBitMask.Get();
				const uint32_t bMask = pixelFormat.m_bBitMask.Get();
				const uint32_t aMask = pixelFormat.m_aBitMask.Get();

				const uint32_t bitCount = pixelFormat.m_rgbBitCount.Get();

				const uint32_t rgbaFlags = (rkit::data::DDSPixelFormatFlags::kRGB | rkit::data::DDSPixelFormatFlags::kAlphaPixels);
				const uint32_t rgbaLumaFlags = (rgbaFlags | rkit::data::DDSPixelFormatFlags::kLuminance);
				if ((pixelFormatFlags & rgbaLumaFlags) == rgbaFlags
					&& bitCount == 32
					&& rMask == 0x000000ff
					&& gMask == 0x0000ff00
					&& bMask == 0x00ff0000
					&& aMask == 0xff000000)
				{
					textureFormat = rkit::render::TextureFormat::RGBA_UNorm8;
				}
				else if ((pixelFormatFlags & rgbaLumaFlags) == rkit::data::DDSPixelFormatFlags::kRGB
					&& bitCount == 16
					&& rMask == 0x00ff
					&& gMask == 0xff00
					&& bMask == 0
					&& aMask == 0)
				{
					textureFormat = rkit::render::TextureFormat::RG_UNorm8;
				}
				else if ((pixelFormatFlags & rgbaLumaFlags) == rkit::data::DDSPixelFormatFlags::kRGB
					&& bitCount == 8
					&& rMask == 0xff
					&& gMask == 0
					&& bMask == 0
					&& aMask == 0)
				{
					textureFormat = rkit::render::TextureFormat::R_UNorm8;
				}

				if (textureFormat != rkit::render::TextureFormat::Count && (ddsFlags & rkit::data::DDSFlags::kPitch))
				{
					pixelBlockSizeBytes = bitCount / 8u;
					if (pitchOrLinearSize % pixelBlockSizeBytes != 0 || pitchOrLinearSize / pixelBlockSizeBytes != width)
						return rkit::ResultCode::kDataError;
				}
			}
		}

		if (textureFormat == rkit::render::TextureFormat::Count)
			return rkit::ResultCode::kDataError;

		if (width > maxDimension || height > maxDimension || depth > maxDimension || arraySize > maxLayers)
		{
			return rkit::ResultCode::kDataError;
		}

		rkit::render::ImageSpec textureSpec = {};
		textureSpec.m_format = textureFormat;
		textureSpec.m_width = width;
		textureSpec.m_height = height;
		textureSpec.m_depth = depth;
		textureSpec.m_mipLevels = levels;
		textureSpec.m_arrayLayers = arraySize;

		size_t totalBytesRequired = 0;

		for (uint32_t level = 0; level < levels; level++)
		{
			const uint32_t levelWidth = rkit::Max<uint32_t>(width >> level, 1);
			const uint32_t levelHeight = rkit::Max<uint32_t>(height >> level, 1);
			const uint32_t levelDepth = rkit::Max<uint32_t>(depth >> level, 1);

			const uint32_t levelBlockCols = (levelWidth + pixelBlockWidth - 1) / pixelBlockWidth;
			const uint32_t levelBlockRows = (levelHeight + pixelBlockHeight - 1) / pixelBlockHeight;

			const uint32_t multipliers[] =
			{
				levelBlockCols,
				levelBlockRows,
				pixelBlockSizeBytes,
				levelDepth,
				arraySize,
				cubeSides,
			};

			size_t levelBytesRequired = 1;
			if (level == 0)
			{
				for (uint32_t multiplier : multipliers)
				{
					if (std::numeric_limits<size_t>::max() / levelBytesRequired < multiplier)
						return rkit::ResultCode::kDataError;

					levelBytesRequired *= multiplier;
				}
			}
			else
			{
				for (uint32_t multiplier : multipliers)
					levelBytesRequired *= multiplier;
			}

			RKIT_CHECK(rkit::SafeAdd<size_t>(totalBytesRequired, levelBytesRequired, totalBytesRequired));
		}

		const size_t textureDataSize = m_textureData->Count() - static_cast<size_t>(headerSize);
		if (textureDataSize < totalBytesRequired)
			return rkit::ResultCode::kDataError;

		rkit::render::ImageResourceSpec resSpec = {};
		resSpec.m_usage.Add({ rkit::render::TextureUsageFlag::kCopyDest, rkit::render::TextureUsageFlag::kSampled });

		rkit::UniquePtr<rkit::render::IImagePrototype> prototype;
		RKIT_CHECK(m_graphicsSubsystem.m_renderDevice->CreateImagePrototype(prototype, textureSpec, resSpec));

		// Find a good heap
		rkit::render::HeapSpec heapSpec = {};
		heapSpec.m_allowBuffers = false;
		heapSpec.m_allowNonRTDSImages = true;
		heapSpec.m_heapType = rkit::render::HeapType::kDefault;

		const rkit::render::MemoryRequirementsView memReqs = prototype->GetMemoryRequirements();

		rkit::render::GPUMemorySize_t memSize = memReqs.Size();

		rkit::Optional<rkit::render::HeapKey> heapKey = memReqs.FindSuitableHeap(heapSpec);
		if (!heapKey.IsSet())
			return rkit::ResultCode::kInternalError;

		// FIXME FIXME FIXME
		rkit::UniquePtr<rkit::render::IMemoryHeap> memHeap;
		RKIT_CHECK(m_graphicsSubsystem.m_renderDevice->CreateMemoryHeap(memHeap, heapKey.Get(), memSize));

		rkit::ConstSpan<uint8_t> initialDataSpan;
		if (m_graphicsSubsystem.m_renderDevice->SupportsInitialTextureData())
		{
			initialDataSpan = m_textureData->ToSpan().SubSpan(headerSize);
		}

		rkit::UniquePtr<rkit::render::IImageResource> image;
		RKIT_CHECK(m_graphicsSubsystem.m_renderDevice->CreateImage(image, std::move(prototype), memHeap->GetRegion(), initialDataSpan));

		m_texture->SetRenderResources(std::move(image), std::move(memHeap));

		if (m_graphicsSubsystem.m_renderDevice->SupportsInitialTextureData())
		{
			if (m_doneCopyingSignaler)
				m_doneCopyingSignaler->SignalDone(rkit::ResultCode::kOK);
		}
		else
		{
			rkit::RCPtr<TextureUploadTask> textureUploadTask;
			RKIT_CHECK(rkit::New<TextureUploadTask>(textureUploadTask));

			textureUploadTask->m_doneSignaler = m_doneCopyingSignaler;
			textureUploadTask->m_texture = m_texture;
			textureUploadTask->m_textureData = m_textureData;
			textureUploadTask->m_textureDataOffset = headerSize;
			textureUploadTask->m_spec = textureSpec;
			textureUploadTask->m_blockWidth = pixelBlockWidth;
			textureUploadTask->m_blockHeight = pixelBlockHeight;
			textureUploadTask->m_blockSizeBytes = pixelBlockSizeBytes;

			RKIT_CHECK(m_graphicsSubsystem.PostAsyncUploadTask(std::move(textureUploadTask)));
		}

		return rkit::ResultCode::kOK;
	}

	GraphicsSubsystem::StripedMemCopyJobRunner::StripedMemCopyJobRunner(GraphicsSubsystem &graphicsSubsystem, uint8_t syncPointIndex)
		: m_graphicsSubsystem(graphicsSubsystem)
		, m_syncPointIndex(syncPointIndex)
	{
	}

	rkit::Result GraphicsSubsystem::StripedMemCopyJobRunner::Run()
	{
		FrameSyncPoint &syncPoint = m_graphicsSubsystem.m_syncPoints[m_syncPointIndex];

		for (const BufferStripedMemCopyAction &memCopyAction : syncPoint.m_asyncUploadActionSet.m_stripedMemCpy)
		{
			const uint8_t *srcStart = memCopyAction.m_start;
			const uint32_t rowSizeBytes = memCopyAction.m_rowSizeBytes;
			const uint32_t rowInPitch = memCopyAction.m_rowInPitch;
			const uint32_t rowOutPitch = memCopyAction.m_rowOutPitch;
			const uint32_t rowCount = memCopyAction.m_rowCount;

			rkit::render::IMemoryAllocation *memAllocation = memCopyAction.m_destPosition.GetAllocation();
			rkit::render::GPUMemoryOffset_t memOffset = memCopyAction.m_destPosition.GetOffset();

			uint8_t *destStart = static_cast<uint8_t *>(memAllocation->GetCPUPtr()) + memOffset;

			if (rowInPitch == rowOutPitch)
				memcpy(destStart, srcStart, rowInPitch * rowCount);
			else
			{
				for (uint32_t i = 0; i < rowCount; i++)
				{
					memcpy(destStart, srcStart, rowSizeBytes);
					srcStart += rowInPitch;
					destStart += rowOutPitch;
				}
			}
		}

		return rkit::ResultCode::kOK;
	}

	GraphicsSubsystem::StripedMemCopyCleanupJobRunner::StripedMemCopyCleanupJobRunner(GraphicsSubsystem &graphicsSubsystem, uint8_t syncPointIndex)
		: m_graphicsSubsystem(graphicsSubsystem)
		, m_syncPointIndex(syncPointIndex)
	{
	}

	rkit::Result GraphicsSubsystem::StripedMemCopyCleanupJobRunner::Run()
	{
		FrameSyncPoint &syncPoint = m_graphicsSubsystem.m_syncPoints[m_syncPointIndex];
		for (const rkit::RCPtr<UploadTask> &retiredUploadTask : syncPoint.m_asyncUploadActionSet.m_retiredUploadTasks)
		{
			retiredUploadTask->OnCopyCompleted();
		}

		return rkit::ResultCode::kOK;
	}

	GraphicsSubsystem::AsyncDisposeResourceJobRunner::AsyncDisposeResourceJobRunner(GraphicTimelinedResourceStack &&stack)
		: m_stack(std::move(stack))
	{
	}

	rkit::Result GraphicsSubsystem::AsyncDisposeResourceJobRunner::Run()
	{
		m_stack = GraphicTimelinedResourceStack();
		return rkit::ResultCode::kOK;
	}

	GraphicsSubsystem::CloseFrameRecordRunner::CloseFrameRecordRunner(rkit::render::IBaseCommandBatch *&outBatchPtr, rkit::render::IBinaryGPUWaitableFence *asyncUploadFence)
		: m_outBatchPtr(outBatchPtr)
		, m_asyncUploadFence(asyncUploadFence)
	{
	}

	rkit::Result GraphicsSubsystem::CloseFrameRecordRunner::RunRecord(rkit::render::IGraphicsCommandAllocator &cmdAlloc)
	{
		rkit::render::IGraphicsCommandBatch *batch = nullptr;
		RKIT_CHECK(cmdAlloc.OpenGraphicsCommandBatch(batch, true));

		if (m_asyncUploadFence)
		{
			RKIT_CHECK(batch->AddWaitForFence(*m_asyncUploadFence, rkit::render::PipelineStageMask_t({ rkit::render::PipelineStage::kTopOfPipe })));
		}

		RKIT_CHECK(batch->CloseBatch());

		m_outBatchPtr = batch;

		return rkit::ResultCode::kOK;
	}

	GraphicsSubsystem::AsyncUploadPrepareTargetsSubmitRunner::AsyncUploadPrepareTargetsSubmitRunner()
	{
	}

	rkit::Result GraphicsSubsystem::AsyncUploadPrepareTargetsSubmitRunner::RunSubmit(rkit::render::ICopyCommandQueue &commandQueue)
	{
		RKIT_CHECK(m_cmdBatch->Submit());
		return rkit::ResultCode::kOK;
	}

	rkit::render::ICopyCommandBatch **GraphicsSubsystem::AsyncUploadPrepareTargetsSubmitRunner::GetCmdBatchRef()
	{
		return &m_cmdBatch;
	}

	GraphicsSubsystem::AsyncUploadPrepareTargetsRecordRunner::AsyncUploadPrepareTargetsRecordRunner(rkit::render::ICopyCommandBatch **cmdBatchRef, FrameSyncPoint &syncPoint)
		: m_cmdBatchRef(cmdBatchRef)
		, m_syncPoint(syncPoint)
	{
	}

	rkit::Result GraphicsSubsystem::AsyncUploadPrepareTargetsRecordRunner::RunRecord(rkit::render::ICopyCommandAllocator &cmdAlloc)
	{
		RKIT_CHECK(cmdAlloc.OpenCopyCommandBatch(*m_cmdBatchRef, false));

		rkit::render::ICopyCommandBatch *cmdBatch = *m_cmdBatchRef;

		rkit::render::BarrierGroup barrierGroup;

		rkit::HybridVector<rkit::render::ImageMemoryBarrier, 16> imageBarriers;

		RKIT_CHECK(imageBarriers.Reserve(m_syncPoint.m_asyncUploadActionSet.m_prepareImageForTransfer.Count()));

		for (const PrepareImageForTransferAction &action : m_syncPoint.m_asyncUploadActionSet.m_prepareImageForTransfer)
		{
			rkit::render::ImageMemoryBarrier barrier = {};
			barrier.m_priorStages.Add({ rkit::render::PipelineStage::kTopOfPipe });
			barrier.m_subsequentStages.Add({ rkit::render::PipelineStage::kCopy });
			barrier.m_subsequentAccess.Add({ rkit::render::ResourceAccess::kCopyDest });

			barrier.m_image = action.m_image;

			barrier.m_priorLayout = rkit::render::ImageLayout::Undefined;
			barrier.m_subsequentLayout = rkit::render::ImageLayout::CopyDst;

			RKIT_CHECK(imageBarriers.Append(barrier));
		}

		barrierGroup.m_imageMemoryBarriers = imageBarriers.ToSpan();

		rkit::render::ICopyCommandEncoder *cmdEncoder = nullptr;
		RKIT_CHECK(cmdBatch->OpenCopyCommandEncoder(cmdEncoder));
		RKIT_CHECK(cmdEncoder->PipelineBarrier(barrierGroup));

		RKIT_CHECK(cmdBatch->CloseBatch());

		return rkit::ResultCode::kOK;
	}

	GraphicsSubsystem::CopyAsyncUploadsSubmitRunner::CopyAsyncUploadsSubmitRunner(FrameSyncPoint &syncPoint)
		: m_syncPoint(syncPoint)
	{
	}

	rkit::Result GraphicsSubsystem::CopyAsyncUploadsSubmitRunner::RunSubmit(rkit::render::ICopyCommandQueue &commandQueue)
	{
		RKIT_CHECK(m_cmdBatch->Submit());

		return rkit::ResultCode::kOK;
	}

	rkit::render::ICopyCommandBatch **GraphicsSubsystem::CopyAsyncUploadsSubmitRunner::GetCmdBatchRef()
	{
		return &m_cmdBatch;
	}

	GraphicsSubsystem::CopyAsyncUploadsRecordRunner::CopyAsyncUploadsRecordRunner(rkit::render::ICopyCommandBatch **cmdBatchRef, FrameSyncPoint &syncPoint, uint64_t globalSyncPoint, rkit::render::IBinaryGPUWaitableFence *fence)
		: m_cmdBatchRef(cmdBatchRef)
		, m_syncPoint(syncPoint)
		, m_globalSyncPoint(globalSyncPoint)
		, m_fence(fence)
	{
	}

	rkit::Result GraphicsSubsystem::CopyAsyncUploadsRecordRunner::RunRecord(rkit::render::ICopyCommandAllocator &cmdAlloc)
	{
		RKIT_CHECK(cmdAlloc.OpenCopyCommandBatch(*m_cmdBatchRef, false));

		rkit::render::ICopyCommandBatch *cmdBatch = *m_cmdBatchRef;

		rkit::render::ICopyCommandEncoder *cmdEncoder = nullptr;
		RKIT_CHECK(cmdBatch->OpenCopyCommandEncoder(cmdEncoder));

		for (const BufferToTextureCopyAction &copyAction : m_syncPoint.m_asyncUploadActionSet.m_bufferToTextureCopy)
		{
			Texture *texture = copyAction.m_texture;
			texture->Touch(m_globalSyncPoint);

			RKIT_CHECK(cmdEncoder->CopyBufferToImage(*texture->GetRenderResource(), copyAction.m_destRect, *copyAction.m_buffer, copyAction.m_footprint,
				copyAction.m_imageLayout, copyAction.m_mipLevel, copyAction.m_arrayElement, copyAction.m_imagePlane));
		}

		RKIT_CHECK(cmdBatch->AddSignalFence(*m_fence));

		RKIT_CHECK(cmdBatch->CloseBatch());

		return rkit::ResultCode::kOK;
	}

	GraphicsSubsystem::CloseFrameSubmitRunner::CloseFrameSubmitRunner(rkit::render::IBaseCommandBatch *&frameEndBatchPtr)
		: m_frameEndBatchPtr(frameEndBatchPtr)
	{
	}

	rkit::Result GraphicsSubsystem::CloseFrameSubmitRunner::RunSubmit(rkit::render::IGraphicsCommandQueue &commandQueue)
	{
		RKIT_CHECK(m_lastBatch->Submit());

		m_frameEndBatchPtr = m_lastBatch;

		return rkit::ResultCode::kOK;
	}

	rkit::render::IBaseCommandBatch **GraphicsSubsystem::CloseFrameSubmitRunner::GetLastBatchRef()
	{
		return &m_lastBatch;
	}

	GraphicsSubsystem::SyncPointFenceFactory::SyncPointFenceFactory(GraphicsSubsystem &subsystem, uint8_t syncPoint)
		: m_subsystem(subsystem)
		, m_syncPoint(syncPoint)
	{
	}

	rkit::Result GraphicsSubsystem::SyncPointFenceFactory::CreateFence(rkit::render::IBinaryGPUWaitableFence *&outFence)
	{
		FrameSyncPoint &syncPoint = m_subsystem.m_syncPoints[m_syncPoint];

		if (syncPoint.m_usedGPUWaitableFences < syncPoint.m_gpuWaitableFences.Count())
			outFence = syncPoint.m_gpuWaitableFences[syncPoint.m_usedGPUWaitableFences].Get();
		else
		{
			rkit::UniquePtr<rkit::render::IBinaryGPUWaitableFence> fence;
			RKIT_CHECK(m_subsystem.m_renderDevice->CreateBinaryGPUWaitableFence(fence));

			rkit::render::IBinaryGPUWaitableFence *fencePtr = fence.Get();
			RKIT_CHECK(syncPoint.m_gpuWaitableFences.Append(std::move(fence)));

			outFence = fencePtr;
		}

		syncPoint.m_usedGPUWaitableFences++;
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
				rkit::log::ErrorFmt("Enum config key '{}' in the pipeline cache was unresolvable", keyName.GetChars());
				return rkit::ResultCode::kConfigInvalid;
			}

			if (resolution.m_mainType != expectedMainType)
			{
				rkit::log::ErrorFmt("Enum config key '{}' in the pipeline cache was the wrong type", keyName.GetChars());
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
			rkit::log::ErrorFmt("Float config key '{}' in the pipeline cache was unresolvable", keyName.GetChars());
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
			rkit::log::ErrorFmt("SInt config key '{}' in the pipeline cache was unresolvable", keyName.GetChars());
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
			rkit::log::ErrorFmt("UInt config key '{}' in the pipeline cache was unresolvable", keyName.GetChars());
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

	void GraphicsSubsystem::UploadActionSet::ClearActions()
	{
		m_bufferToTextureCopy.ShrinkToSize(0);
		m_prepareImageForTransfer.ShrinkToSize(0);
		m_retiredUploadTasks.ShrinkToSize(0);
		m_stripedMemCpy.ShrinkToSize(0);
	}

	void GraphicsSubsystem::FrameSyncPoint::Reset()
	{
		for (FrameSyncPointCommandListHandler &cmdListHandler : m_commandListHandlers)
		{
			cmdListHandler.m_commandAllocator.Reset();
		}

		m_frameEndBatch = nullptr;
	}

	rkit::Result GraphicsSubsystem::TextureUploadTask::StartTask(UploadActionSet &actionSet, GraphicsSubsystem &subsystem)
	{
		PrepareImageForTransferAction action = {};
		action.m_image = m_texture->GetRenderResource();

		return actionSet.m_prepareImageForTransfer.Append(action);
	}


	rkit::Result GraphicsSubsystem::TextureUploadTask::PumpTask(bool &isCompleted, UploadActionSet &actionSet, GraphicsSubsystem &subsystem)
	{
		isCompleted = false;

		const uint32_t blockSizeBytes = m_blockSizeBytes;
		const uint32_t arrayCount = m_spec.m_arrayLayers * (m_spec.m_cubeMap ? 6 : 1);

		const uint8_t *textureDataStart = m_textureData->GetBuffer();

		for (;;)
		{
			uint32_t lowContiguous = 0;
			uint32_t highContiguous = 0;
			subsystem.GetAsyncUploadHeapStats(lowContiguous, highContiguous);

			if (lowContiguous == 0 && highContiguous == 0)
			{
				// Totally out of space
				return rkit::ResultCode::kOK;
			}

			const uint32_t mipLevel = m_mipLevel;

			const uint32_t levelWidth = rkit::Max<uint32_t>(1, m_spec.m_width >> mipLevel);
			const uint32_t levelHeight = rkit::Max<uint32_t>(1, m_spec.m_height >> mipLevel);
			const uint32_t levelDepth = rkit::Max<uint32_t>(1, m_spec.m_depth >> mipLevel);

			const uint32_t levelBlockCols = rkit::DivideRoundUp(levelWidth, m_blockWidth);
			const uint32_t levelBlockRows = rkit::DivideRoundUp(levelHeight, m_blockHeight);
			const uint32_t levelBlockDepthSlices = rkit::DivideRoundUp(levelDepth, m_blockDepth);

			const uint32_t paddedLevelRowPitch = rkit::AlignUp<uint32_t>(levelBlockCols * blockSizeBytes, subsystem.m_asyncUploadHeapAlignment);

			const uint32_t paddedDepthSliceSize = levelBlockCols * paddedLevelRowPitch;
			const uint32_t paddedLevelSize = paddedDepthSliceSize * levelBlockDepthSlices;

			// Vulkan is only guaranteed to support whole-mip-level copies, so we just do that exclusively
			if (paddedLevelSize > lowContiguous)
			{
				if (paddedLevelSize > highContiguous)
					return rkit::ResultCode::kOK;	// Not enough space
				else
				{
					subsystem.ConsumeAsyncUploadSpace(lowContiguous);
					lowContiguous = highContiguous;
					highContiguous = 0;
				}
			}

			BufferStripedMemCopyAction memCpyAction = {};
			BufferToTextureCopyAction texCopyAction = {};

			memCpyAction.m_start = textureDataStart + m_textureDataOffset;
			memCpyAction.m_rowInPitch = levelBlockCols * blockSizeBytes;
			memCpyAction.m_rowOutPitch = paddedLevelRowPitch;
			memCpyAction.m_rowCount = levelBlockRows * levelBlockDepthSlices;
			memCpyAction.m_rowSizeBytes = levelBlockCols * blockSizeBytes;
			memCpyAction.m_destPosition = subsystem.m_asyncUploadHeap->GetRegion().GetPosition() + subsystem.m_asyncUploadHeapHighMark;

			texCopyAction.m_buffer = subsystem.m_asyncUploadBuffer.Get();
			texCopyAction.m_texture = m_texture;
			texCopyAction.m_footprint.m_format = m_spec.m_format;
			texCopyAction.m_destRect.m_x = 0;
			texCopyAction.m_destRect.m_y = 0;
			texCopyAction.m_destRect.m_z = 0;
			texCopyAction.m_arrayElement = m_arrayElement;
			texCopyAction.m_mipLevel = m_mipLevel;
			texCopyAction.m_imageLayout = rkit::render::ImageLayout::CopyDst;
			texCopyAction.m_imagePlane = rkit::render::ImagePlane::kColor;
			texCopyAction.m_footprint.m_bufferOffset = subsystem.m_asyncUploadHeapHighMark;
			texCopyAction.m_footprint.m_width = levelBlockCols * m_blockWidth;
			texCopyAction.m_footprint.m_height = levelBlockRows * m_blockHeight;
			texCopyAction.m_footprint.m_depth = levelBlockDepthSlices * m_blockDepth;
			texCopyAction.m_footprint.m_rowPitch = paddedLevelRowPitch;
			texCopyAction.m_destRect.m_width = rkit::Min(texCopyAction.m_footprint.m_width, levelWidth);
			texCopyAction.m_destRect.m_height = rkit::Min(texCopyAction.m_footprint.m_height, levelHeight);
			texCopyAction.m_destRect.m_depth = rkit::Min(texCopyAction.m_footprint.m_depth, levelDepth);

			subsystem.ConsumeAsyncUploadSpace(memCpyAction.m_rowOutPitch * memCpyAction.m_rowCount);
			m_textureDataOffset += memCpyAction.m_rowInPitch * memCpyAction.m_rowCount;

			RKIT_CHECK(actionSet.m_bufferToTextureCopy.Append(texCopyAction));
			RKIT_CHECK(actionSet.m_stripedMemCpy.Append(memCpyAction));

			m_mipLevel++;

			if (m_mipLevel == m_spec.m_mipLevels)
			{
				m_mipLevel = 0;
				m_arrayElement++;
			}

			if (m_arrayElement == arrayCount)
			{
				isCompleted = true;
				return rkit::ResultCode::kOK;
			}
		}

		isCompleted = false;
		return rkit::ResultCode::kOK;
	}

	void GraphicsSubsystem::TextureUploadTask::OnCopyCompleted()
	{
		// Can destroy the texture data, but not the texture itself
		m_textureData.Reset();
	}

	void GraphicsSubsystem::TextureUploadTask::OnUploadCompleted()
	{
		if (m_doneSignaler.IsValid())
			m_doneSignaler->SignalDone(rkit::ResultCode::kOK);
	}


	GraphicsSubsystem::GraphicsSubsystem(IGameDataFileSystem &fileSystem, rkit::data::IDataDriver &dataDriver, rkit::utils::IThreadPool &threadPool, anox::RenderBackend desiredBackend)
		: m_fileSystem(fileSystem)
		, m_threadPool(threadPool)
		, m_dataDriver(dataDriver)
		, m_desiredBackend(desiredBackend)
	{
		for (LogicalQueueBase *&logicalQueue : m_logicalQueues)
			logicalQueue = nullptr;
	}

	GraphicsSubsystem::~GraphicsSubsystem()
	{
		if (m_renderDevice.IsValid())
		{
			(void)m_renderDevice->WaitForDeviceIdle();
		}

		m_asyncUploadBuffer.Reset();
		m_asyncUploadHeap.Reset();

		m_syncPoints.Reset();
		m_fenceWaiter.Reset();
		m_gameWindow.Reset();

		m_asyncUploadActiveTask.Reset();
		m_asyncUploadWaitingTasks.Reset();
		m_unsortedCondemnedResources = GraphicTimelinedResourceStack();

		m_pipelineLibraryLoader.Reset();
		m_pipelineLibrary.Reset();

		m_renderDevice.Reset();
	}

	rkit::Result GraphicsSubsystem::Initialize()
	{
		rkit::ISystemDriver *sysDriver = rkit::GetDrivers().m_systemDriver;

		RKIT_CHECK(sysDriver->CreateMutex(m_setupMutex));
		RKIT_CHECK(sysDriver->CreateMutex(m_asyncUploadMutex));

		RKIT_CHECK(sysDriver->CreateEvent(m_shutdownJoinEvent, true, false));
		RKIT_CHECK(sysDriver->CreateEvent(m_shutdownTerminateEvent, true, false));
		RKIT_CHECK(sysDriver->CreateEvent(m_prevFrameWaitWakeEvent, true, false));
		RKIT_CHECK(sysDriver->CreateEvent(m_prevFrameWaitTerminateEvent, true, false));

		RKIT_CHECK(IFrameDrawer::Create(m_frameDrawer));

		return rkit::ResultCode::kOK;
	}

	rkit::Result GraphicsSubsystem::CreateGameDisplayAndDevice(const rkit::StringView &renderModuleName, const rkit::CIPathView &pipelinesFile, const rkit::CIPathView &pipelinesCacheFile, bool canUpdatePipelineCache)
	{
		rkit::render::IDisplayManager *displayManager = rkit::GetDrivers().m_systemDriver->GetDisplayManager();

		m_currentDisplayMode = rkit::render::DisplayMode::kSplash;

		rkit::UniquePtr<rkit::render::IDisplay> display;
		RKIT_CHECK(displayManager->CreateDisplay(display, m_currentDisplayMode.Get()));

		RKIT_CHECK(RenderedWindowBase::Create(m_gameWindow, std::move(display), nullptr, nullptr, m_renderDevice.Get(), nullptr, 0, 0));

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

		const float fOne = 1.0f;
		const float fHalf = 0.5f;

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

		requiredCaps.SetUInt32Cap(rkit::render::RenderDeviceUInt32Cap::kMaxTexture2DSize, 1024);

		rkit::UniquePtr<rkit::render::IRenderDevice> device;
		RKIT_CHECK(renderDriver->CreateDevice(device, queueRequests.ToSpan(), requiredCaps, optionalCaps, *adapters[0]));

		m_renderDevice = std::move(device);

		m_syncPoints.Reset();

		RKIT_CHECK(m_syncPoints.Resize(m_numSyncPoints));
		m_currentSyncPoint = 0;

		RKIT_CHECK(m_renderDevice->CreateCPUFenceWaiter(m_fenceWaiter));

		// FIXME: Localize
		RKIT_CHECK(progressMonitor->SetText("Loading shader package..."));

		m_pipelinesCacheFileName = pipelinesCacheFile;
		m_setupStep = DeviceSetupStep::kOpenPipelinePackage;
		m_stepCompleted = false;

		rkit::RCPtr<rkit::Job> openPipelineCacheJob;

		rkit::FutureContainerPtr<rkit::UniquePtr<rkit::ISeekableReadStream>> pipelineStreamFutureContainer;
		RKIT_CHECK(rkit::New<rkit::FutureContainer<rkit::UniquePtr<rkit::ISeekableReadStream>>>(pipelineStreamFutureContainer));

		RKIT_CHECK(m_fileSystem.OpenNamedFileBlocking(openPipelineCacheJob, pipelineStreamFutureContainer, pipelinesFile));

		rkit::Future<rkit::UniquePtr<rkit::ISeekableReadStream>> pipelineStreamFuture(pipelineStreamFutureContainer);

		rkit::UniquePtr<rkit::IJobRunner> jobRunner;
		RKIT_CHECK(rkit::New<CheckPipelinesJob>(jobRunner, *this, pipelineStreamFuture, pipelinesCacheFile));

		RKIT_CHECK(m_threadPool.GetJobQueue()->CreateJob(nullptr, rkit::JobType::kIO, std::move(jobRunner), openPipelineCacheJob));

		m_enableAsyncUpload = (!m_renderDevice->SupportsInitialBufferData() && !m_renderDevice->SupportsInitialTextureData());

		if (m_enableAsyncUpload)
		{
			rkit::render::BufferSpec uploadBufferSpec = {};
			m_asyncUploadHeapSize = 64 * 1024 * 1024;
			m_asyncUploadHeapAlignment = m_renderDevice->GetUploadHeapAlignment();
			uploadBufferSpec.m_size = m_asyncUploadHeapSize;

			rkit::render::BufferResourceSpec uploadBufferResSpec = {};
			uploadBufferResSpec.m_usage.Add({
				rkit::render::BufferUsageFlag::kCopySrc
				});

			rkit::UniquePtr<rkit::render::IBufferPrototype> prototype;
			RKIT_CHECK(m_renderDevice->CreateBufferPrototype(prototype, uploadBufferSpec, uploadBufferResSpec));

			rkit::render::HeapSpec heapSpec = {};
			heapSpec.m_cpuAccessible = true;
			heapSpec.m_allowBuffers = true;
			heapSpec.m_heapType = rkit::render::HeapType::kUpload;

			rkit::Optional<rkit::render::HeapKey> uploadHeapKey = prototype->GetMemoryRequirements().FindSuitableHeap(heapSpec);
			if (!uploadHeapKey.IsSet())
			{
				rkit::log::Error("No heap available for use as upload heap");
				return rkit::ResultCode::kInternalError;
			}

			RKIT_CHECK(m_renderDevice->CreateMemoryHeap(m_asyncUploadHeap, uploadHeapKey.Get(), prototype->GetMemoryRequirements().Size()));

			RKIT_CHECK(m_renderDevice->CreateBuffer(m_asyncUploadBuffer, std::move(prototype), m_asyncUploadHeap->GetRegion(), rkit::ConstSpan<uint8_t>()));

			m_asyncUploadHeapLowMark = 0;
			m_asyncUploadHeapHighMark = 0;
			m_asyncUploadHeapIsFull = false;
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result GraphicsSubsystem::WaitForRenderingTasks()
	{
		if (m_renderDevice.IsValid())
		{
			RKIT_CHECK(m_renderDevice->WaitForDeviceIdle());
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result GraphicsSubsystem::PostAsyncUploadTask(rkit::RCPtr<UploadTask> &&uploadTaskRef)
	{
		rkit::RCPtr<UploadTask> uploadTask = std::move(uploadTaskRef);

		rkit::MutexLock lock(*m_asyncUploadMutex);

		RKIT_CHECK(m_asyncUploadWaitingTasks.Append(std::move(uploadTask)));

		return rkit::ResultCode::kOK;
	}

	rkit::Result GraphicsSubsystem::PumpActiveAsyncUploadTask(UploadActionSet &actionSet)
	{
		bool isCompleted = false;
		RKIT_CHECK(m_asyncUploadActiveTask->PumpTask(isCompleted, actionSet, *this));

		if (isCompleted)
		{
			RKIT_CHECK(actionSet.m_retiredUploadTasks.Append(std::move(m_asyncUploadActiveTask)));
			m_asyncUploadActiveTask.Reset();
		}

		return rkit::ResultCode::kOK;
	}

	void GraphicsSubsystem::GetAsyncUploadHeapStats(uint32_t &outContiguousLow, uint32_t &outContiguousHigh) const
	{
		if (m_asyncUploadHeapHighMark < m_asyncUploadHeapLowMark)
		{
			outContiguousLow = m_asyncUploadHeapLowMark - m_asyncUploadHeapHighMark;
			outContiguousHigh = 0;
		}
		else if (m_asyncUploadHeapHighMark > m_asyncUploadHeapLowMark)
		{
			outContiguousLow = m_asyncUploadHeapSize - m_asyncUploadHeapHighMark;
			outContiguousHigh = m_asyncUploadHeapLowMark;
		}
		else
		{
			outContiguousLow = m_asyncUploadHeapIsFull ? 0 : m_asyncUploadHeapSize;
			outContiguousHigh = 0;
		}
	}

	void GraphicsSubsystem::ConsumeAsyncUploadSpace(uint32_t space)
	{
		if (space == 0)
			return;

		RKIT_ASSERT(m_asyncUploadHeapIsFull == false);

		if (m_asyncUploadHeapLowMark <= m_asyncUploadHeapHighMark)
		{
			RKIT_ASSERT(space <= m_asyncUploadHeapSize - m_asyncUploadHeapHighMark);
			m_asyncUploadHeapHighMark += space;

			if (m_asyncUploadHeapHighMark == m_asyncUploadHeapSize)
				m_asyncUploadHeapHighMark = 0;
		}
		else
		{
			RKIT_ASSERT(space <= m_asyncUploadHeapLowMark - m_asyncUploadHeapHighMark);
			m_asyncUploadHeapHighMark += space;
		}

		m_asyncUploadHeapIsFull = (m_asyncUploadHeapHighMark == m_asyncUploadHeapLowMark);
	}

	rkit::Result GraphicsSubsystem::CreateAndQueueJob(rkit::RCPtr<rkit::Job> *outJob, LogicalQueueType queueType, rkit::UniquePtr<rkit::IJobRunner> &&jobRunnerRef, const rkit::JobDependencyList &dependencies, rkit::RCPtr<rkit::Job>(LogicalQueueBase:: *queueMember))
	{
		rkit::UniquePtr<rkit::IJobRunner> jobRunner = std::move(jobRunnerRef);

		LogicalQueueBase *logicalQueue = m_logicalQueues[static_cast<size_t>(queueType)];

		rkit::RCPtr<rkit::Job> &jobRef = (logicalQueue->*queueMember);

		rkit::Job *job = jobRef.Get();

		rkit::Span<rkit::Job *> jobSpan;
		rkit::SpanToISpanValueWrapper<rkit::Job *> jobISpan;

		const rkit::ISpan<rkit::Job *> *jobSpansArray[2] = {};

		rkit::Span<const rkit::ISpan<rkit::Job *> *> jobSpans;

		rkit::ConcatenatedSpanISpan<rkit::Job *> concatenatedSpan;

		const rkit::ISpan<rkit::Job *> *spanToPass = nullptr;

		if (jobRef.IsValid())
		{
			jobSpan = rkit::Span<rkit::Job *>(&job, 1);
			jobISpan = jobSpan.ToValueISpan();

			if (dependencies.GetSpan().Count() > 0)
			{
				jobSpansArray[0] = &jobISpan;
				jobSpansArray[1] = &dependencies.GetSpan();

				jobSpans = rkit::Span<const rkit::ISpan<rkit::Job *> *>(jobSpansArray, 2);
				concatenatedSpan = rkit::ConcatenatedSpanISpan<rkit::Job *>(jobSpans);

				spanToPass = &concatenatedSpan;
			}
			else
				spanToPass = &jobISpan;
		}
		else
			spanToPass = &dependencies.GetSpan();

		rkit::RCPtr<rkit::Job> newJob;
		RKIT_CHECK(m_threadPool.GetJobQueue()->CreateJob(&newJob, rkit::JobType::kNormalPriority, std::move(jobRunner), *spanToPass));


		jobRef = newJob;

		if (outJob)
			*outJob = newJob;

		return rkit::ResultCode::kOK;
	}

	rkit::Result GraphicsSubsystem::TransitionDisplayMode()
	{
		if (m_currentDisplayMode.IsSet())
		{
			RKIT_CHECK(WaitForRenderingTasks());
			m_gameWindow.Reset();
		}

		m_currentDisplayMode = m_desiredDisplayMode;

		rkit::render::IDisplayManager *displayManager = rkit::GetDrivers().m_systemDriver->GetDisplayManager();

		rkit::UniquePtr<rkit::render::IDisplay> display;
		RKIT_CHECK(displayManager->CreateDisplay(display, m_currentDisplayMode.Get()));

		rkit::render::IBaseCommandQueue *swapChainQueue = nullptr;

		for (rkit::render::IGraphicsComputeCommandQueue *queue : m_renderDevice->GetGraphicsComputeQueues())
		{
			m_graphicsComputeQueue = queue;
			break;
		}

		for (rkit::render::IGraphicsCommandQueue *queue : m_renderDevice->GetGraphicsQueues())
		{
			m_graphicsQueue = queue;
			break;
		}

		for (rkit::render::ICopyCommandQueue *queue : m_renderDevice->GetCopyQueues())
		{
			m_dmaQueue = queue;
			break;
		}

		for (rkit::render::IComputeCommandQueue *queue : m_renderDevice->GetComputeQueues())
		{
			m_asyncComputeQueue = queue;
			break;
		}

		if (m_graphicsComputeQueue)
		{
			m_graphicsComputeLogicalQueue.m_commandQueue = m_graphicsComputeQueue;
			m_logicalQueues[static_cast<size_t>(LogicalQueueType::kGraphics)] = &m_graphicsComputeLogicalQueue;
			m_logicalQueues[static_cast<size_t>(LogicalQueueType::kPrimaryCompute)] = &m_graphicsComputeLogicalQueue;
			m_logicalQueues[static_cast<size_t>(LogicalQueueType::kAsyncCompute)] = &m_graphicsComputeLogicalQueue;
		}

		if (m_graphicsQueue)
		{
			m_graphicsLogicalQueue.m_commandQueue = m_graphicsQueue;
			m_logicalQueues[static_cast<size_t>(LogicalQueueType::kGraphics)] = &m_graphicsLogicalQueue;
		}

		if (m_asyncComputeQueue)
		{
			m_asyncComputeLogicalQueue.m_commandQueue = m_asyncComputeQueue;
			m_logicalQueues[static_cast<size_t>(LogicalQueueType::kAsyncCompute)] = &m_asyncComputeLogicalQueue;

			if (!m_graphicsComputeQueue)
				m_logicalQueues[static_cast<size_t>(LogicalQueueType::kPrimaryCompute)] = &m_asyncComputeLogicalQueue;
		}

		if (m_dmaQueue)
		{
			m_dmaLogicalQueue.m_commandQueue = m_dmaQueue;
			m_logicalQueues[static_cast<size_t>(LogicalQueueType::kDMA)] = &m_dmaLogicalQueue;
		}

		if (!m_dmaQueue || !m_logicalQueues[static_cast<size_t>(LogicalQueueType::kGraphics)])
		{
			rkit::log::Error("Missing a required graphics API queue type");
			return rkit::ResultCode::kOperationFailed;
		}

		rkit::UniquePtr<rkit::render::ISwapChainPrototype> swapChainPrototype;
		RKIT_CHECK(m_renderDevice->CreateSwapChainPrototype(swapChainPrototype, *display));

		LogicalQueueBase *graphicsLogicalQueue = m_logicalQueues[static_cast<size_t>(LogicalQueueType::kGraphics)];

		bool isGraphicsQueueCompatible = false;
		RKIT_CHECK(swapChainPrototype->CheckQueueCompatibility(isGraphicsQueueCompatible, *graphicsLogicalQueue->GetBaseCommandQueue()));

		if (!isGraphicsQueueCompatible)
		{
			rkit::log::Error("Graphics queue wasn't capable of presenting to the desired display");
			return rkit::ResultCode::kOperationFailed;
		}

		LogicalQueueBase *&presentationLogicalQueueRef = m_logicalQueues[static_cast<size_t>(LogicalQueueType::kPresentation)];

		presentationLogicalQueueRef = graphicsLogicalQueue;

		rkit::UniquePtr<GameWindowResources> gameWindowResourcesUniquePtr;
		RKIT_CHECK(rkit::New<GameWindowResources>(gameWindowResourcesUniquePtr));

		GameWindowResources &gameWindowResources = *gameWindowResourcesUniquePtr.Get();

		const uint8_t numSwapChainFrames = 3;
		RKIT_CHECK(RenderedWindowBase::Create(m_gameWindow, std::move(display), std::move(swapChainPrototype), std::move(gameWindowResourcesUniquePtr), m_renderDevice.Get(), presentationLogicalQueueRef->GetBaseCommandQueue(), numSwapChainFrames, m_numSyncPoints));

		RKIT_CHECK(gameWindowResources.m_swapChainFrameResources.Resize(numSwapChainFrames));

		for (size_t i = 0; i < numSwapChainFrames; i++)
		{
			GameWindowSwapChainFrameResources &scfr = gameWindowResources.m_swapChainFrameResources[i];

			scfr.m_colorTargetImage = m_gameWindow->GetSwapChain()->GetImageForFrame(i);

			{
				rkit::render::RenderPassRef_t simpleColorTargetRP = m_pipelineLibrary->FindRenderPass("RP_SimpleColorTarget");

				if (!simpleColorTargetRP.IsValid())
				{
					rkit::log::Error("RP_SimpleColorTarget render pass is missing");
					return rkit::ResultCode::kDataError;
				}

				rkit::render::RenderPassResources resources;

				resources.m_arraySize = 1;

				rkit::render::IRenderTargetView *rtvs[] =
				{
					m_gameWindow->GetSwapChain()->GetRenderTargetViewForFrame(i),
				};

				m_gameWindow->GetSwapChain()->GetExtents(resources.m_width, resources.m_height);
				resources.m_renderTargetViews = rkit::Span<rkit::render::IRenderTargetView *>(rtvs);
				resources.m_renderArea.m_width = resources.m_width;
				resources.m_renderArea.m_height = resources.m_height;

				RKIT_CHECK(m_renderDevice->CreateRenderPassInstance(scfr.m_simpleColorTargetRPI, simpleColorTargetRP, resources));
			}
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result GraphicsSubsystem::TransitionBackend()
	{
		if (m_resourceManager.IsValid())
			m_resourceManager->UnloadAll();

		RKIT_CHECK(WaitForRenderingTasks());

		m_gameWindow.Reset();

		for (LogicalQueueBase *&logicalQueue : m_logicalQueues)
			logicalQueue = nullptr;

		for (FrameSyncPoint &frameSyncPoint : m_syncPoints)
			frameSyncPoint.Reset();

		m_renderDevice.Reset();

		m_currentDisplayMode.Reset();
		m_desiredDisplayMode = rkit::render::DisplayMode::kSplash;

		m_backend = m_desiredBackend;

		rkit::StringView backendModule;
		rkit::CIPathView pipelinesFile;
		rkit::CIPathView pipelinesCacheFile;
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

				RKIT_CHECK(m_pipelineLibraryLoader->GetFinishedPipeline(m_pipelineLibrary));
				m_pipelineLibraryLoader.Reset();

				m_desiredDisplayMode = rkit::render::DisplayMode::kWindowed;

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

				if (m_setupStep == DeviceSetupStep::kFinished)
					return rkit::ResultCode::kOK;	// Bail out since there will be no main-thread jobs to run
			}

			rkit::GetDrivers().m_systemDriver->SleepMSec(1000u / 60u);

			{
				rkit::RCPtr<rkit::Job> jobToRun = m_threadPool.GetJobQueue()->WaitForWork(m_threadPool.GetMainThreadJobTypes(), false, nullptr, nullptr);
				if (jobToRun.IsValid())
					jobToRun->Run();
			}

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

	rkit::Result GraphicsSubsystem::RetireOldestFrame()
	{
		if (!m_currentDisplayMode.IsSet() || m_currentDisplayMode.Get() == rkit::render::DisplayMode::kSplash)
			return rkit::ResultCode::kOK;

		FrameSyncPoint &syncPoint = m_syncPoints[m_currentSyncPoint];

		if (syncPoint.m_frameEndJob.IsValid())
		{
			m_threadPool.GetJobQueue()->WaitForJob(*syncPoint.m_frameEndJob, m_threadPool.GetMainThreadJobTypes(), m_prevFrameWaitWakeEvent.Get(), m_prevFrameWaitTerminateEvent.Get());
			RKIT_CHECK(m_threadPool.GetJobQueue()->CheckFault());

			RKIT_ASSERT(syncPoint.m_frameEndBatch != nullptr);
			RKIT_CHECK(syncPoint.m_frameEndBatch->WaitForCompletion(*m_fenceWaiter));
		}

		if (syncPoint.m_asyncUploadActionSet.m_cleanupJob.IsValid())
		{
			m_threadPool.GetJobQueue()->WaitForJob(*syncPoint.m_asyncUploadActionSet.m_cleanupJob, m_threadPool.GetMainThreadJobTypes(), m_prevFrameWaitWakeEvent.Get(), m_prevFrameWaitTerminateEvent.Get());
			RKIT_CHECK(m_threadPool.GetJobQueue()->CheckFault());
			syncPoint.m_asyncUploadActionSet.m_cleanupJob.Reset();
		}

		// When the frame is retired, all of the uploads have been pushed,
		// so the buffer can't be full.
		m_asyncUploadHeapLowMark = syncPoint.m_asyncUploadHeapFinalHighMark;
		m_asyncUploadHeapIsFull = false;

		// If the buffer is completely emptied, reset to the zero point
		if (m_asyncUploadHeapLowMark == m_asyncUploadHeapHighMark)
		{
			m_asyncUploadHeapLowMark = 0;
			m_asyncUploadHeapHighMark = 0;
		}

		for (rkit::RCPtr<UploadTask> &uploadTaskPtr : syncPoint.m_asyncUploadActionSet.m_retiredUploadTasks)
		{
			uploadTaskPtr->OnUploadCompleted();
			uploadTaskPtr.Reset();
		}

		syncPoint.m_usedGPUWaitableFences = 0;

		// Handle timelined resource disposal
		{
			GraphicTimelinedResourceStack disposeNow = std::move(syncPoint.m_condemnedResources);

			while (GraphicTimelinedResource *resource = m_unsortedCondemnedResources.Pop())
			{
				const uint64_t globalSyncPoint = resource->GetGlobalSyncPoint();
				const uint64_t framesAgo = m_currentGlobalSyncPoint - globalSyncPoint;

				if (framesAgo >= m_numSyncPoints)
					disposeNow.PushUnsafe(resource);
				else
				{
					const uint8_t binIntoSyncPoint = (m_currentSyncPoint + m_numSyncPoints - static_cast<uint8_t>(framesAgo)) % m_numSyncPoints;
					m_syncPoints[binIntoSyncPoint].m_condemnedResources.PushUnsafe(resource);
				}
			}

			if (!disposeNow.IsEmptyUnsafe())
			{
				rkit::UniquePtr<AsyncDisposeResourceJobRunner> disposeRunner;
				RKIT_CHECK(rkit::New<AsyncDisposeResourceJobRunner>(disposeRunner, std::move(disposeNow)));

				RKIT_CHECK(m_threadPool.GetJobQueue()->CreateJob(nullptr, rkit::JobType::kNormalPriority, std::move(disposeRunner), rkit::JobDependencyList()));
			}
		}


		rkit::StaticBoolArray<static_cast<size_t>(rkit::render::CommandQueueType::kCount)> cmdQueueReset;

		for (size_t logicalQueueTypeInt = 0; logicalQueueTypeInt < kNumLogicalQueueTypes; logicalQueueTypeInt++)
		{
			LogicalQueueType logicalQueueType = static_cast<LogicalQueueType>(logicalQueueTypeInt);
			LogicalQueueBase *logicalQueue = m_logicalQueues[logicalQueueTypeInt];

			if (!logicalQueue)
				continue;

			rkit::render::CommandQueueType queueType = logicalQueue->GetBaseCommandQueue()->GetCommandQueueType();

			FrameSyncPointCommandListHandler &cmdListHandler = syncPoint.m_commandListHandlers[static_cast<size_t>(queueType)];

			if (!cmdListHandler.m_commandAllocator.IsValid())
			{
				RKIT_CHECK(logicalQueue->CreateCommandAllocator(*m_renderDevice, cmdListHandler.m_commandAllocator, false));
			}

			if (cmdListHandler.m_commandQueue == nullptr)
			{
				cmdListHandler.m_commandQueue = logicalQueue->GetBaseCommandQueue();
			}

			if (!cmdQueueReset.Get(static_cast<size_t>(queueType)))
			{
				// Queue a command allocator reset on each record queue
				rkit::UniquePtr<rkit::IJobRunner> resetJobRunner;
				RKIT_CHECK(rkit::New<ResetCommandAllocatorJob>(resetJobRunner, cmdListHandler));

				RKIT_CHECK(CreateAndQueueJob(nullptr, logicalQueueType, std::move(resetJobRunner), rkit::Span<rkit::Job *>().ToValueISpan(), &LogicalQueueBase::m_lastRecordJob));
			}

			cmdQueueReset.Set(static_cast<size_t>(queueType), true);
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result GraphicsSubsystem::PumpAsyncUploads()
	{
		if (!m_enableAsyncUpload)
			return rkit::ResultCode::kOK;

		FrameSyncPoint &syncPoint = m_syncPoints[m_currentSyncPoint];

		UploadActionSet &actionSet = syncPoint.m_asyncUploadActionSet;
		actionSet.ClearActions();

		for (;;)
		{
			if (!m_asyncUploadActiveTask.IsValid())
			{
				{
					rkit::MutexLock lock(*m_asyncUploadMutex);

					const size_t numWaitingTasks = m_asyncUploadWaitingTasks.Count();
					if (numWaitingTasks == 0)
						break;

					m_asyncUploadActiveTask = std::move(m_asyncUploadWaitingTasks[numWaitingTasks - 1]);
					m_asyncUploadWaitingTasks.ShrinkToSize(numWaitingTasks - 1);
				}


				RKIT_CHECK(m_asyncUploadActiveTask->StartTask(actionSet, *this));
			}

			RKIT_CHECK(PumpActiveAsyncUploadTask(actionSet));

			if (m_asyncUploadActiveTask.IsValid())
				break;	// Still working on this one
		}

		actionSet.m_memCopyJob.Reset();
		syncPoint.m_asyncUploadHeapFinalHighMark = m_asyncUploadHeapHighMark;

		SyncPointFenceFactory fenceFactory(*this, m_currentSyncPoint);

		// If anything was uploaded, post the appropriate jobs.
		// Async uploads have several jobs:
		// - Initial layout transition jobs (run here)
		// - Striped memcopy jobs (run here)
		// - Buffer-to-image copy jobs
		if (actionSet.m_prepareImageForTransfer.Count() > 0)
		{
			rkit::UniquePtr<AsyncUploadPrepareTargetsSubmitRunner> prepareSubmitRunner;
			RKIT_CHECK(rkit::New< AsyncUploadPrepareTargetsSubmitRunner>(prepareSubmitRunner));

			rkit::UniquePtr<AsyncUploadPrepareTargetsRecordRunner> prepareRecordRunner;
			RKIT_CHECK(rkit::New<AsyncUploadPrepareTargetsRecordRunner>(prepareRecordRunner, prepareSubmitRunner->GetCmdBatchRef(), syncPoint));

			rkit::RCPtr<rkit::Job> recordJob;
			RKIT_CHECK(CreateAndQueueRecordJob(&recordJob, LogicalQueueType::kDMA, std::move(prepareRecordRunner), rkit::JobDependencyList()));

			RKIT_CHECK(CreateAndQueueSubmitJob(nullptr, LogicalQueueType::kDMA, std::move(prepareSubmitRunner), recordJob));
		}

		if (actionSet.m_stripedMemCpy.Count() > 0)
		{
			rkit::UniquePtr<StripedMemCopyJobRunner> copyJobRunner;
			RKIT_CHECK(rkit::New<StripedMemCopyJobRunner>(copyJobRunner, *this, m_currentSyncPoint));

			rkit::RCPtr<rkit::Job> memCopyJob;
			RKIT_CHECK(m_threadPool.GetJobQueue()->CreateJob(&memCopyJob, rkit::JobType::kNormalPriority, std::move(copyJobRunner), rkit::JobDependencyList()));

			m_syncPoints[m_currentSyncPoint].m_asyncUploadActionSet.m_memCopyJob = memCopyJob;

			rkit::UniquePtr<StripedMemCopyCleanupJobRunner> cleanupJobRunner;
			RKIT_CHECK(rkit::New<StripedMemCopyCleanupJobRunner>(cleanupJobRunner, *this, m_currentSyncPoint));

			rkit::RCPtr<rkit::Job> cleanupJob;
			RKIT_CHECK(m_threadPool.GetJobQueue()->CreateJob(&cleanupJob, rkit::JobType::kNormalPriority, std::move(cleanupJobRunner), memCopyJob));

			m_syncPoints[m_currentSyncPoint].m_asyncUploadActionSet.m_cleanupJob = cleanupJob;
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result GraphicsSubsystem::StartRendering()
	{
		FrameSyncPoint &syncPoint = m_syncPoints[m_currentSyncPoint];

		RKIT_CHECK(rkit::New<PerFrameResources>(m_currentFrameResources));

		m_currentFrameResources->m_frameEndBatchPtr = &syncPoint.m_frameEndBatch;
		m_currentFrameResources->m_frameEndJobPtr = &syncPoint.m_frameEndJob;

		RKIT_CHECK(m_gameWindow->BeginFrame(*this));

		return rkit::ResultCode::kOK;
	}

	rkit::Result GraphicsSubsystem::DrawFrame()
	{
		if (!m_currentDisplayMode.IsSet() || m_currentDisplayMode.Get() == rkit::render::DisplayMode::kSplash)
			return rkit::ResultCode::kOK;

		RKIT_CHECK(m_frameDrawer->DrawFrame(*this, m_currentFrameResources, *m_gameWindow));

		return rkit::ResultCode::kOK;
	}

	rkit::Result GraphicsSubsystem::EndFrame()
	{
		if (!m_currentDisplayMode.IsSet() || m_currentDisplayMode.Get() == rkit::render::DisplayMode::kSplash)
			return rkit::ResultCode::kOK;

		RKIT_CHECK(m_gameWindow->EndFrame(*this));

		FrameSyncPoint &syncPoint = m_syncPoints[m_currentSyncPoint];

		rkit::RCPtr<rkit::Job> asyncUploadMemCopyJob = syncPoint.m_asyncUploadActionSet.m_memCopyJob;

		SyncPointFenceFactory fenceFactory(*this, m_currentSyncPoint);

		rkit::StaticArray<rkit::RCPtr<rkit::Job>, 1> frameEndRecordDeps;
		size_t numFrameEndRecordDeps = 0;

		rkit::render::IBinaryGPUWaitableFence *asyncUploadGPUFence = nullptr;

		if (asyncUploadMemCopyJob.IsValid())
		{
			RKIT_CHECK(fenceFactory.CreateFence(asyncUploadGPUFence));

			// Post async upload submits to the end of the frame
			rkit::UniquePtr<CopyAsyncUploadsSubmitRunner> submitRunner;
			RKIT_CHECK(rkit::New<CopyAsyncUploadsSubmitRunner>(submitRunner, syncPoint));

			rkit::UniquePtr<CopyAsyncUploadsRecordRunner> recordRunner;
			RKIT_CHECK(rkit::New<CopyAsyncUploadsRecordRunner>(recordRunner, submitRunner->GetCmdBatchRef(), syncPoint, m_currentGlobalSyncPoint, asyncUploadGPUFence));

			rkit::RCPtr<rkit::Job> recordJob;
			RKIT_CHECK(CreateAndQueueRecordJob(&recordJob, LogicalQueueType::kDMA, std::move(recordRunner), asyncUploadMemCopyJob));

			rkit::RCPtr<rkit::Job> submitJob;
			RKIT_CHECK(CreateAndQueueSubmitJob(&submitJob, LogicalQueueType::kDMA, std::move(submitRunner), recordJob));

			frameEndRecordDeps[numFrameEndRecordDeps++] = submitJob;
		}

		rkit::UniquePtr<CloseFrameSubmitRunner> closeFrameSubmitRunner;
		RKIT_CHECK(rkit::New<CloseFrameSubmitRunner>(closeFrameSubmitRunner, *m_currentFrameResources->m_frameEndBatchPtr));

		rkit::UniquePtr<CloseFrameRecordRunner> closeFrameRecordRunner;
		RKIT_CHECK(rkit::New<CloseFrameRecordRunner>(closeFrameRecordRunner, *closeFrameSubmitRunner->GetLastBatchRef(), asyncUploadGPUFence));

		rkit::RCPtr<rkit::Job> closeFrameRecordJob;
		RKIT_CHECK(CreateAndQueueRecordJob(&closeFrameRecordJob, LogicalQueueType::kGraphics, std::move(closeFrameRecordRunner),
			frameEndRecordDeps.ToSpan().SubSpan(0, numFrameEndRecordDeps)));

		rkit::RCPtr<rkit::Job> closeFrameSubmitJob;
		RKIT_CHECK(CreateAndQueueSubmitJob(&closeFrameSubmitJob, LogicalQueueType::kGraphics, std::move(closeFrameSubmitRunner), closeFrameRecordJob));

		*m_currentFrameResources->m_frameEndJobPtr = closeFrameSubmitJob;

		m_currentSyncPoint++;

		if (m_currentSyncPoint == m_numSyncPoints)
			m_currentSyncPoint = 0;

		m_currentGlobalSyncPoint++;

		return rkit::ResultCode::kOK;
	}

	rkit::Optional<rkit::render::DisplayMode> GraphicsSubsystem::GetDisplayMode() const
	{
		return m_currentDisplayMode;
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

	rkit::Result GraphicsSubsystem::CreateAndQueueRecordJob(rkit::RCPtr<rkit::Job> *outJob, LogicalQueueType queueType, rkit::UniquePtr<IRecordJobRunner> &&jobRunner, const rkit::JobDependencyList &dependencies)
	{
		FrameSyncPoint &syncPoint = m_syncPoints[m_currentSyncPoint];

		rkit::render::CommandQueueType cmdQueueType = m_logicalQueues[static_cast<size_t>(queueType)]->GetBaseCommandQueue()->GetCommandQueueType();

		FrameSyncPointCommandListHandler &cmdListHandler = syncPoint.m_commandListHandlers[static_cast<size_t>(cmdQueueType)];

		rkit::render::IBaseCommandAllocator *cmdAllocator = cmdListHandler.m_commandAllocator.Get();

		rkit::UniquePtr<RunRecordJobRunner> runRecordJobRunner;
		RKIT_CHECK(rkit::New<RunRecordJobRunner>(runRecordJobRunner, std::move(jobRunner), *cmdAllocator));

		return this->CreateAndQueueJob(outJob, queueType, std::move(runRecordJobRunner), dependencies, &LogicalQueueBase::m_lastRecordJob);
	}

	rkit::Result GraphicsSubsystem::CreateAndQueueSubmitJob(rkit::RCPtr<rkit::Job> *outJob, LogicalQueueType queueType, rkit::UniquePtr<ISubmitJobRunner> &&jobRunner, const rkit::JobDependencyList &dependencies)
	{
		FrameSyncPoint &syncPoint = m_syncPoints[m_currentSyncPoint];

		rkit::render::CommandQueueType cmdQueueType = m_logicalQueues[static_cast<size_t>(queueType)]->GetBaseCommandQueue()->GetCommandQueueType();

		FrameSyncPointCommandListHandler &cmdListHandler = syncPoint.m_commandListHandlers[static_cast<size_t>(cmdQueueType)];

		rkit::UniquePtr<RunSubmitJobRunner> runSubmitJobRunner;
		RKIT_CHECK(rkit::New<RunSubmitJobRunner>(runSubmitJobRunner, std::move(jobRunner), *cmdListHandler.m_commandQueue));

		return this->CreateAndQueueJob(outJob, queueType, std::move(runSubmitJobRunner), dependencies, &LogicalQueueBase::m_lastSubmitJob);
	}

	rkit::Result GraphicsSubsystem::CreateAsyncCreateTextureJob(rkit::RCPtr<rkit::Job> *outJob, rkit::RCPtr<ITexture> &outTexture, const rkit::RCPtr<rkit::Vector<uint8_t>> &textureData, const rkit::JobDependencyList &dependencies)
	{
		rkit::RCPtr<Texture> texture;
		RKIT_CHECK(rkit::New<Texture>(texture, *this));

		rkit::IJobQueue &jobQueue = *m_threadPool.GetJobQueue();

		rkit::RCPtr<rkit::JobSignaler> doneCopyingSignaler;
		rkit::RCPtr<rkit::Job> doneCopyingJob;
		if (outJob)
		{
			RKIT_CHECK(jobQueue.CreateSignaledJob(doneCopyingSignaler, doneCopyingJob));

			*outJob = doneCopyingJob;
		}

		rkit::UniquePtr<AllocateTextureStorageAndPostCopyJobRunner> allocStorageJobRunner;
		RKIT_CHECK(rkit::New<AllocateTextureStorageAndPostCopyJobRunner>(allocStorageJobRunner, *this, texture, textureData, doneCopyingSignaler));

		RKIT_CHECK(jobQueue.CreateJob(nullptr, rkit::JobType::kNormalPriority, std::move(allocStorageJobRunner), dependencies));

		return rkit::ResultCode::kOK;
	}

	void GraphicsSubsystem::CondemnTimelinedResource(GraphicTimelinedResource &timelinedResource)
	{
		m_unsortedCondemnedResources.Push(&timelinedResource);
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

rkit::Result anox::IGraphicsSubsystem::Create(rkit::UniquePtr<IGraphicsSubsystem> &outSubsystem, IGameDataFileSystem &fileSystem, rkit::data::IDataDriver &dataDriver, rkit::utils::IThreadPool &threadPool, anox::RenderBackend defaultBackend)
{
	rkit::UniquePtr<anox::GraphicsSubsystem> subsystem;
	RKIT_CHECK(rkit::New<anox::GraphicsSubsystem>(subsystem, fileSystem, dataDriver, threadPool, defaultBackend));

	RKIT_CHECK(subsystem->Initialize());

	outSubsystem = std::move(subsystem);

	return rkit::ResultCode::kOK;
}
