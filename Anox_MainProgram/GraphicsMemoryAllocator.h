#pragma once

#include "rkit/Core/Result.h"
#include "rkit/Render/MemoryProtos.h"

namespace rkit { namespace render {
	struct IMemoryHeap;
} }


namespace anox
{
	class GraphicsMemoryRegion final
	{
	public:
		GraphicsMemoryRegion();
		GraphicsMemoryRegion(rkit::render::IMemoryHeap *memHeap, rkit::render::GPUMemoryOffset_t offset, rkit::render::GPUMemorySize_t size);
		GraphicsMemoryRegion(const GraphicsMemoryRegion &other) = default;

	private:
		rkit::render::IMemoryHeap *m_memHeap = nullptr;
		rkit::render::GPUMemoryOffset_t m_offset = 0;
		rkit::render::GPUMemorySize_t m_size = 0;
	};


	class GraphicsMemoryAllocation final
	{
	public:
		GraphicsMemoryAllocation(rkit::render::IMemoryHeap *memHeap, rkit::render::GPUMemoryOffset_t offset);

	};

	struct IGraphicsMemoryAllocator
	{
		virtual ~IGraphicsMemoryAllocator() {}

		virtual rkit::Result AllocateMemory()
	};
}
