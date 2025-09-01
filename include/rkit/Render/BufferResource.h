#pragma once

#include "rkit/Core/NoCopy.h"

namespace rkit { namespace render
{
	class MemoryRequirementsView;

	struct IBufferResource : public NoCopy
	{
		virtual ~IBufferResource() {}
	};

	struct IBufferPrototype : public NoCopy
	{
		virtual ~IBufferPrototype() {}

		virtual MemoryRequirementsView GetMemoryRequirements() const = 0;
	};
} } // rkit::render
