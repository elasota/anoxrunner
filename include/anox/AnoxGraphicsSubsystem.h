#pragma once

#include "rkit/Render/DisplayManager.h"

namespace rkit
{
	struct Result;
}

namespace anox
{
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

		static rkit::Result Create(rkit::UniquePtr<IGraphicsSubsystem> &outSubsystem, anox::RenderBackend defaultBackend);
	};
}
