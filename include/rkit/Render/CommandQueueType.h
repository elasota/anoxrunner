#pragma once

namespace rkit::render
{
	enum class CommandQueueType
	{
		kGraphics,
		kAsyncCompute,
		kGraphicsCompute,
		kCopy,

		kCount,
	};
}
