#pragma once

namespace rkit
{
	template<class T>
	class EnumMask;
}

namespace rkit::render
{
	enum class PipelineStage
	{
		kTopOfPipe,
		kColorOutput,
		kBottomOfPipe,

		kPresentAcquire,
		kPresentSubmit,

		kCount,
	};

	typedef EnumMask<PipelineStage> PipelineStageMask_t;
}
