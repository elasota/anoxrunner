#pragma once

#include "rkit/Render/DisplayManager.h"
#include "rkit/Render/BufferSpec.h"
#include "rkit/Core/Span.h"

#include "AnoxLogicalQueue.h"

namespace rkit
{
	class Job;
	struct IJobRunner;

	template<class T>
	class RCPtr;

	template<class T>
	class Optional;

	template<class T>
	struct ISpan;

	template<class T>
	class Vector;

	class JobDependencyList;

	namespace utils
	{
		struct IThreadPool;
	}

	namespace data
	{
		struct IDataDriver;
	}

	namespace render
	{
		struct IBinaryGPUWaitableFence;
		struct IRenderDeviceCaps;
		struct IRenderDeviceRequirements;
	}
}

namespace anox
{
	struct IRecordJobRunner;
	struct ISubmitJobRunner;
	struct IGameDataFileSystem;
	struct ITexture;
	struct IBuffer;
	class GraphicTimelinedResource;

	enum class RenderBackend
	{
		kVulkan,
	};

	struct IBinaryGPUWaitableFenceFactory
	{
		virtual rkit::Result CreateFence(rkit::render::IBinaryGPUWaitableFence *&outFence) = 0;
	};

	struct BufferInitializer
	{
		virtual ~BufferInitializer() {}

		struct CopyOperation
		{
			size_t m_offset = 0;
			rkit::Span<const uint8_t> m_data;
		};

		rkit::Span<const CopyOperation> m_copyOperations;
		rkit::render::BufferSpec m_spec;
		rkit::render::BufferResourceSpec m_resSpec;
	};

	struct IGraphicsSubsystem
	{
		virtual ~IGraphicsSubsystem() {}

		virtual void SetDesiredRenderBackend(RenderBackend renderBackend) = 0;

		virtual void SetDesiredDisplayMode(rkit::render::DisplayMode displayMode) = 0;
		virtual rkit::Result TransitionDisplayState() = 0;

		virtual rkit::Result RetireOldestFrame() = 0;
		virtual rkit::Result PumpAsyncUploads() = 0;
		virtual rkit::Result StartRendering() = 0;
		virtual rkit::Result DrawFrame() = 0;
		virtual rkit::Result EndFrame() = 0;

		virtual rkit::Optional<rkit::render::DisplayMode> GetDisplayMode() const = 0;

		virtual const rkit::render::IRenderDeviceCaps &GetDeviceCaps() const = 0;
		virtual const rkit::render::IRenderDeviceRequirements &GetDeviceRequirements() const = 0;

		virtual rkit::Result CreateAndQueueRecordJob(rkit::RCPtr<rkit::Job> *outJob, LogicalQueueType queueType, rkit::UniquePtr<IRecordJobRunner> &&jobRunner, const rkit::JobDependencyList &dependencies) = 0;
		virtual rkit::Result CreateAndQueueSubmitJob(rkit::RCPtr<rkit::Job> *outJob, LogicalQueueType queueType, rkit::UniquePtr<ISubmitJobRunner> &&jobRunner, const rkit::JobDependencyList &dependencies) = 0;

		virtual rkit::Result CreateAsyncCreateTextureJob(rkit::RCPtr<rkit::Job> *outJob, rkit::RCPtr<ITexture> &outTexture, const rkit::RCPtr<rkit::Vector<uint8_t>> &textureData, const rkit::JobDependencyList &dependencies) = 0;
		virtual rkit::Result CreateAsyncCreateAndFillBufferJob(rkit::RCPtr<rkit::Job> *outJob, rkit::RCPtr<IBuffer> &outBuffer, const rkit::RCPtr<BufferInitializer> &bufferInitializer, const rkit::JobDependencyList &dependencies) = 0;

		virtual void CondemnTimelinedResource(GraphicTimelinedResource &timelinedResource) = 0;

		static rkit::Result Create(rkit::UniquePtr<IGraphicsSubsystem> &outSubsystem, IGameDataFileSystem &fileSystem, rkit::data::IDataDriver &dataDriver, rkit::utils::IThreadPool &threadPool, anox::RenderBackend defaultBackend);
	};
}
