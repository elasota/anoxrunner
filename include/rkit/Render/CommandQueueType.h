#pragma once

namespace rkit
{
	namespace render
	{
		enum class CommandQueueType
		{
			kGraphics,
			kAsyncCompute,
			kGraphicsCompute,
			kCopy,

			kCount,
		};

		bool IsQueueTypeCompatible(CommandQueueType from, CommandQueueType to);
	}
}

namespace rkit { namespace render //
{
	inline bool IsQueueTypeCompatible(CommandQueueType from, CommandQueueType to)
	{
		if (from == to)
			return true;

		switch (to)
		{
		case CommandQueueType::kGraphics:
			return from == CommandQueueType::kGraphicsCompute;
		case CommandQueueType::kAsyncCompute:
			return from == CommandQueueType::kGraphicsCompute;
		case CommandQueueType::kCopy:
			return from == CommandQueueType::kGraphics || from == CommandQueueType::kGraphicsCompute || from == CommandQueueType::kAsyncCompute;
		case CommandQueueType::kGraphicsCompute:
		default:
			return false;
		}
	}
} } // rkit::render
