#include "rkit/Core/HashValue.h"

#include <stdint.h>

namespace anox { namespace buildsystem
{
	struct ModelProtoVert
	{
		ModelProtoVert() = default;
		ModelProtoVert(uint32_t uBits, uint32_t vBits, uint32_t xyzIndex);

		uint32_t m_uBits = 0;
		uint32_t m_vBits = 0;
		uint32_t m_xyzIndex = 0;

		rkit::HashValue_t ComputeHash() const;
		bool operator==(const ModelProtoVert &other) const;
		bool operator!=(const ModelProtoVert &other) const;
	};

	struct PrecompiledModelHeader
	{
		uint32_t m_numTextures = 0;

		//uint32_t m_textureCounts[m_numTextures]
		//ModelProtoVert m_textureVerts[m_numTextures][...]
	};


} } // anox::buildsystem

#include "rkit/Core/UtilitiesDriver.h"


namespace anox { namespace buildsystem
{
	inline ModelProtoVert::ModelProtoVert(uint32_t uBits, uint32_t vBits, uint32_t xyzIndex)
		: m_uBits(uBits)
		, m_vBits(vBits)
		, m_xyzIndex(xyzIndex)
	{
	}

	inline rkit::HashValue_t ModelProtoVert::ComputeHash() const
	{
		return m_uBits * 13 + m_vBits * 17 + m_xyzIndex * 23;
	}

	inline bool ModelProtoVert::operator==(const ModelProtoVert &other) const
	{
		return m_uBits == other.m_uBits && m_vBits == other.m_vBits && m_xyzIndex == other.m_xyzIndex;
	}

	inline bool ModelProtoVert::operator!=(const ModelProtoVert &other) const
	{
		return !((*this) == other);
	}
} }
