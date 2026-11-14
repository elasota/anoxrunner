#pragma once

#include <cstddef>

#include "rkit/Core/StringProto.h"
#include "PipelineLibraryItemProtos.h"

namespace rkit::render
{
	struct IInternalPipelineLayout;

	struct IInternalGraphicsPipeline
	{
		virtual ~IInternalGraphicsPipeline() {}

		virtual IInternalPipelineLayout &GetPipelineLayout() const = 0;
	};
} // rkit::render
