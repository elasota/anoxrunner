#pragma once

namespace rkit::render
{
	enum class PipelineStage
	{
		kTopOfPipe,
		kColorOutput,
		kBottomOfPipe,

		kCount,
	};
}
