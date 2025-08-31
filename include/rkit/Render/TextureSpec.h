#pragma once

#include "rkit/Render/RenderEnums.h"

namespace rkit { namespace render {
	struct TextureSpec
	{
		TextureFormat m_format = TextureFormat::Count;
		bool m_cubeMap = false;
		bool m_linearLayout = false;

		uint32_t m_width = 0;
		uint32_t m_height = 0;
		uint32_t m_depth = 0;

		uint32_t m_mipLevels = 1;
		uint32_t m_arrayLayers = 1;
	};
} }
