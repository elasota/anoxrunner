#pragma once

#include <cstddef>

#include "rkit/Core/StringProto.h"
#include "PipelineLibraryItemProtos.h"

namespace rkit { namespace render
{
	struct IPipelineLibrary
	{
		virtual ~IPipelineLibrary() {}

		virtual RenderPassRef_t FindRenderPass(const StringSliceView &name) const = 0;
		virtual GraphicsPipelineRef_t FindGraphicsPipeline(const StringSliceView &name) const = 0;
	};
} } // rkit::render
