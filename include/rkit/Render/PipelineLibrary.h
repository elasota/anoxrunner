#pragma once

#include <cstddef>

#include "rkit/Core/StringProto.h"
#include "PipelineLibraryItemProtos.h"

namespace rkit::render
{
	template<class TDesc, class TCompiled>
	class PipelineLibraryItemRef;

	struct IPipelineLibrary
	{
		virtual ~IPipelineLibrary() {}

		virtual RenderPassRef_t FindRenderPass(const StringSliceView &name) = 0;
	};
}
