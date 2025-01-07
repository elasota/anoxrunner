#pragma once

#include "rkit/Core/StringProto.h"

#include <cstdint>

namespace rkit
{
	struct Result;

	template<class T>
	class UniquePtr;
}

namespace rkit::render
{
	enum class DisplayMode
	{
		kSplash,
		kWindowed,
		kFullScreen,
		kBorderlessFullscreen,
	};

	struct IProgressMonitor
	{
		virtual Result SetText(const StringView &text) = 0;
		virtual Result SetRange(uint64_t minimum, uint64_t maximum) = 0;
		virtual Result SetValue(uint64_t value) = 0;

		virtual void FlushEvents() = 0;
	};

	struct IDisplay
	{
		virtual ~IDisplay() {}

		virtual DisplayMode GetDisplayMode() const = 0;
		virtual bool CanChangeToDisplayMode(DisplayMode mode) const = 0;
		virtual Result ChangeToDisplayMode(DisplayMode mode) = 0;

		virtual IProgressMonitor *GetProgressMonitor() = 0;
	};

	struct IDisplayManager
	{
		virtual ~IDisplayManager() {}

		virtual Result CreateDisplay(UniquePtr<IDisplay> &display, DisplayMode displayMode, uint32_t width, uint32_t height) = 0;
		virtual Result CreateDisplay(UniquePtr<IDisplay> &display, DisplayMode displayMode) = 0;
	};
}
