#pragma once

#include "rkit/Core/CoreDefs.h"

namespace rkit
{
	template<class T>
	class RCPtr;

	template<class T>
	class UniquePtr;
}

namespace anox
{
	struct PerFrameResources;
	struct PerFramePerDisplayResources;
	struct IGraphicsSubsystem;
	class RenderedWindowBase;

	struct IFrameDrawer
	{
		virtual ~IFrameDrawer() {}

		virtual rkit::Result DrawFrame(IGraphicsSubsystem &graphicsSubsystem, const rkit::RCPtr<PerFrameResources> &perFrameResources, RenderedWindowBase &renderedWindow) = 0;

		static rkit::Result Create(rkit::UniquePtr<IFrameDrawer> &outFrameDrawer);
	};
}
