#pragma once

#include "rkit/Core/RefCounted.h"

namespace rkit::render
{
	struct ISwapChainSyncPoint;
	struct IBinaryCPUWaitableFence;
}

namespace anox
{
	class RenderedWindowResources;

	struct PerFrameResources final : public rkit::RefCounted
	{
		rkit::render::IBinaryCPUWaitableFence *m_frameEndFence = nullptr;
	};

	struct PerFramePerDisplayResources final : public rkit::RefCounted
	{
		RenderedWindowResources *m_windowResources = nullptr;
		rkit::render::ISwapChainSyncPoint *m_swapChainSyncPoint = nullptr;
		size_t m_swapChainFrameIndex = 0;
	};
}
