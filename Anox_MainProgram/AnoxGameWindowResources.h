#pragma once

#include "rkit/Core/UniquePtr.h"
#include "rkit/Core/Vector.h"

#include "AnoxRenderedWindow.h"

namespace rkit
{
	namespace render
	{
		struct IRenderPassInstance;
		struct IImageResource;
	}
}

namespace anox
{
	struct GameWindowSwapChainFrameResources
	{
		rkit::UniquePtr<rkit::render::IRenderPassInstance> m_simpleColorTargetRPI;
		rkit::render::IImageResource *m_colorTargetImage;
	};

	struct GameWindowResources final : public RenderedWindowResources
	{
		rkit::Vector<GameWindowSwapChainFrameResources> m_swapChainFrameResources;
	};
}
