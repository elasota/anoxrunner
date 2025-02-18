#pragma once

#include "rkit/Core/UniquePtr.h"
#include "rkit/Core/Vector.h"

#include "AnoxRenderedWindow.h"

namespace rkit::render
{
	struct IRenderPassInstance;
}

namespace anox
{
	struct GameWindowSwapChainFrameResources
	{
		rkit::UniquePtr<rkit::render::IRenderPassInstance> m_simpleColorTargetRPI;
	};

	struct GameWindowResources final : public RenderedWindowResources
	{
		rkit::Vector<GameWindowSwapChainFrameResources> m_swapChainFrameResources;
	};
}
