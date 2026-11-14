#pragma once

#include <cstddef>

#include "rkit/Core/StringProto.h"
#include "PipelineLibraryItemProtos.h"

namespace rkit::render
{
	struct IInternalPipelineLayout
	{
		virtual ~IInternalPipelineLayout() {}

		virtual uint8_t GetConstantBindingCount() const = 0;
	};
} // rkit::render
