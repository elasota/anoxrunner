#pragma once

#include "rkit/Core/EnumMask.h"

#include "rkit/Render/MemoryProtos.h"
#include "rkit/Render/RenderEnums.h"

namespace rkit { namespace render {
	struct BufferSpec
	{
		GPUMemorySize_t m_size = 0;
	};

	struct BufferResourceSpec
	{
		EnumMask<BufferUsageFlag> m_usage;
	};
} }
