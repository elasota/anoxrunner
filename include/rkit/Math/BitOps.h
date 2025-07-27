#pragma once

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include <stdint.h>

namespace rkit { namespace bitops {

#if defined(_MSC_VER)
	inline int8_t FindLowestSet(uint32_t value)
	{
		unsigned long index = 0;
		if (_BitScanForward(&index, value))
			return static_cast<int8_t>(index);
		return -1;
	}

	inline int8_t FindHighestSet(uint32_t value)
	{
		unsigned long index = 0;
		if (_BitScanReverse(&index, value))
			return static_cast<int8_t>(index);
		return -1;
	}
#endif

#if defined(_MSC_VER) && defined(_M_AMD64)
	inline int8_t FindLowestSet(uint64_t value)
	{
		unsigned long index = 0;
		if (_BitScanForward64(&index, value))
			return static_cast<int8_t>(index);
		return -1;
	}

	inline int8_t FindHighestSet(uint64_t value)
	{
		unsigned long index = 0;
		if (_BitScanReverse64(&index, value))
			return static_cast<int8_t>(index);
		return -1;
	}
#else
	inline int8_t FindLowestSet(uint64_t value)
	{
		const uint32_t lowBits = (value & 0xffffffffu);

		if (lowBits == 0)
		{
			const uint32_t highBits = ((value >> 32) & 0xffffffffu);
			if (highBits == 0)
				return -1;
			else
				return FindLowestSet(highBits) + 32;
		}

		return FindLowestSet(lowBits);
	}

	inline int8_t FindHighestSet(uint64_t value)
	{
		const uint32_t highBits = ((value >> 32) & 0xffffffffu);

		if (highBits == 0)
		{
			const uint32_t lowBits = (value & 0xffffffffu);
			return FindHighestSet(highBits);
		}

		return FindHighestSet(highBits) + 32;
	}
#endif


} } // rkit::math
