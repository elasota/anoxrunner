#pragma once

#include "Algorithm.h"
#include "Endian.h"

#include <string.h>

namespace rkit
{
	template<class T>
	class Span;
}

namespace rkit { namespace sanitizers {
	inline float SanitizeClampFloat(const endian::LittleFloat32_t &f, int maxMagnitude)
	{
		const uint32_t signFracMask = 0x807fffffu;
		const uint32_t expBitsMax = (maxMagnitude + 0x7f) << 23;

		const uint32_t floatBits = f.GetBits();
		const uint32_t signFracBits = (floatBits & signFracMask);
		const uint32_t expBits = (floatBits & ~signFracMask);

		const uint32_t adjustedExpBits = Min(expBits, expBitsMax);

		const uint32_t combined = (signFracBits | adjustedExpBits);

		float result = 0.f;
		memcpy(&result, &combined, 4);

		return result;
	}

	inline uint32_t SanitizeClampUInt(uint32_t value, uint32_t maxValue)
	{
		return Min(value, maxValue);
	}
} }
