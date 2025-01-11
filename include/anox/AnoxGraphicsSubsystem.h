#pragma once

#include "rkit/Render/DisplayManager.h"

namespace rkit
{
	struct Result;
}

namespace rkit::utils
{
	struct IThreadPool;
}

namespace rkit::data
{
	struct IDataDriver;
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

		virtual rkit::Result BeginFrame() = 0;

		virtual rkit::Result EndFrame() = 0;

		static rkit::Result Create(rkit::UniquePtr<IGraphicsSubsystem> &outSubsystem, rkit::data::IDataDriver &dataDriver, rkit::utils::IThreadPool &threadPool, anox::RenderBackend defaultBackend);
	};
}
