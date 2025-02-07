#pragma once

#include "rkit/Core/RefCounted.h"

namespace rkit::render
{
	struct ISwapChainSyncPoint;
	struct IBinaryCPUWaitableFence;
}

namespace anox
{
	struct PerFrameResources final : public rkit::RefCounted
	{
		rkit::render::IBinaryCPUWaitableFence *m_frameEndFence = nullptr;
	};

	struct PerFramePerDisplayResources final : public rkit::RefCounted
	{
		rkit::render::ISwapChainSyncPoint *m_swapChainSyncPoint = nullptr;
	};
}
