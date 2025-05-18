#pragma once

namespace rkit
{
	template<class T>
	class EnumMask;
}

namespace rkit { namespace render
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
} } // rkit::render
