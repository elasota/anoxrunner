#pragma once

#include "rkit/Core/NoCopy.h"
#include "HeapSpec.h"

namespace rkit::render
{
	class MemoryRequirementsView;

	struct IImageResource : public NoCopy
	{
		virtual ~IImageResource() {}
	};

	struct IImagePrototype : public NoCopy
	{
		virtual ~IImagePrototype() {}

		virtual MemoryRequirementsView GetMemoryRequirements() const = 0;
	};
} // rkit::render
