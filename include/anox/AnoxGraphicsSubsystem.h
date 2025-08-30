#pragma once

#include "rkit/Render/DisplayManager.h"

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
}

namespace anox
{
	struct IRecordJobRunner;
	struct ISubmitJobRunner;
	struct IGameDataFileSystem;
	struct ITexture;

	enum class RenderBackend
	{
		kVulkan,
	};

	struct IGraphicsSubsystem
	{
		virtual ~IGraphicsSubsystem() {}

		virtual void SetDesiredRenderBackend(RenderBackend renderBackend) = 0;

		virtual void SetDesiredDisplayMode(rkit::render::DisplayMode displayMode) = 0;
		virtual rkit::Result TransitionDisplayState() = 0;

		virtual rkit::Result RetireOldestFrame() = 0;
		virtual rkit::Result StartRendering() = 0;
		virtual rkit::Result DrawFrame() = 0;
		virtual rkit::Result EndFrame() = 0;

		virtual rkit::Optional<rkit::render::DisplayMode> GetDisplayMode() const = 0;

		virtual rkit::Result CreateAndQueueRecordJob(rkit::RCPtr<rkit::Job> *outJob, LogicalQueueType queueType, rkit::UniquePtr<IRecordJobRunner> &&jobRunner, const rkit::JobDependencyList &dependencies) = 0;
		virtual rkit::Result CreateAndQueueSubmitJob(rkit::RCPtr<rkit::Job> *outJob, LogicalQueueType queueType, rkit::UniquePtr<ISubmitJobRunner> &&jobRunner, const rkit::JobDependencyList &dependencies) = 0;

		virtual rkit::Result CreateAsyncCreateTextureJob(rkit::RCPtr<rkit::Job> *outJob, rkit::RCPtr<ITexture> &outTexture, rkit::Vector<uint8_t> &&textureData, const rkit::JobDependencyList &dependencies) = 0;

		static rkit::Result Create(rkit::UniquePtr<IGraphicsSubsystem> &outSubsystem, IGameDataFileSystem &fileSystem, rkit::data::IDataDriver &dataDriver, rkit::utils::IThreadPool &threadPool, anox::RenderBackend defaultBackend);
	};
}
