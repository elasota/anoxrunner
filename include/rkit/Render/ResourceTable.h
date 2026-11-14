#pragma once

#include "rkit/Core/NoCopy.h"

namespace rkit::render
{
	struct IBufferView;

	struct IResourceTable : public NoCopy
	{
		virtual ~IResourceTable() {}
	};

	struct ResourceTableSpec
	{
		uint32_t m_numTextureResources = 0;
		uint32_t m_numRWTextureResources = 0;
		uint32_t m_numBufferResources = 0;
		uint32_t m_numSamplerResources = 0;
	};
} // rkit::render
