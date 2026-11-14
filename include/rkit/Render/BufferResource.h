#pragma once

#include "rkit/Core/NoCopy.h"

#include "rkit/Render/MemoryProtos.h"

namespace rkit
{
	template<class T>
	class Span;
}

namespace rkit::render
{
	class MemoryRequirementsView;

	struct TypelessBufferViewSpec
	{
		uint64_t m_firstElement = 0;
		uint32_t m_numElements = 0;
		uint32_t m_structureElementStride = 0;
	};

	struct StructuredBufferViewSpec
	{
		uint64_t m_firstElement = 0;
		uint32_t m_numElements = 0;
		uint32_t m_structureElementStride = 0;
	};

	struct IBufferResource : public NoCopy
	{
		virtual ~IBufferResource() {}

		// Only works if the buffer is CPU-visible
		virtual void CopyDataToBuffer(GPUMemoryOffset_t destOffset, Span<const uint8_t> data) const = 0;
	};

	struct IBufferView : public NoCopy
	{
		virtual ~IBufferView() {}
	};

	struct IRWBufferView : public NoCopy
	{
		virtual ~IRWBufferView() {}
	};

	struct IBufferPrototype : public NoCopy
	{
		virtual ~IBufferPrototype() {}

		virtual MemoryRequirementsView GetMemoryRequirements() const = 0;
	};
} // rkit::render
