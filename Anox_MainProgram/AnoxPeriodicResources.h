#pragma once

#include "rkit/Core/RefCounted.h"

#include "rkit/Render/TimelinePoint.h"

namespace rkit
{
	template<class T>
	class RCPtr;

	class Job;

	namespace render
	{
		struct ISwapChainSyncPoint;
		struct IBaseCommandBatch;
	}
}


namespace anox
{
	class RenderedWindowResources;

	struct PerFrameResources final : public rkit::RefCounted
	{
		rkit::render::IBaseCommandBatch **m_frameEndBatchPtr = nullptr;
		rkit::RCPtr<rkit::Job> *m_frameEndJobPtr = nullptr;
		rkit::render::TimelinePoint_t *m_frameEndGraphicsTimelinePointPtr = nullptr;
	};

	struct PerFramePerDisplayResources final : public rkit::RefCounted
	{
		RenderedWindowResources *m_windowResources = nullptr;
		rkit::render::ISwapChainSyncPoint *m_swapChainSyncPoint = nullptr;
		size_t m_swapChainFrameIndex = 0;

		rkit::RCPtr<rkit::Job> m_acquireJob;
	};
}
