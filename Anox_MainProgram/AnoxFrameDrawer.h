#pragma once

namespace rkit
{
	struct Result;

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

	struct IFrameDrawer
	{
		virtual ~IFrameDrawer() {}

		virtual rkit::Result DrawFrame(IGraphicsSubsystem &graphicsSubsystem, const rkit::RCPtr<PerFrameResources> &perFrameResources, const rkit::RCPtr<PerFramePerDisplayResources> &perDisplayResources) = 0;

		static rkit::Result Create(rkit::UniquePtr<IFrameDrawer> &outFrameDrawer);
	};
}
