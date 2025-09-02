#pragma once

#include "MemoryProtos.h"
#include "RenderEnums.h"

namespace rkit { namespace render {
	struct BufferImageFootprint
	{
		GPUMemoryOffset_t m_bufferOffset = 0;
		TextureFormat m_format = TextureFormat::Count;
		uint32_t m_width = 0;
		uint32_t m_height = 0;
		uint32_t m_depth = 0;
		uint32_t m_rowPitch = 0;
	};
} }
