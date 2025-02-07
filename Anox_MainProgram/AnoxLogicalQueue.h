#pragma once

namespace anox
{
	enum class LogicalQueueType
	{
		kPresentation,
		kDMA,
		kGraphics,
		kPrimaryCompute,
		kAsyncCompute,

		kCount,
	};
}
