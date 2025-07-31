#pragma once

#include <stdint.h>

namespace rkit
{
	template<char C0, char C1, char C2, char C3>
	struct FourCCValue
	{
		static const uint32_t kValue = static_cast<uint32_t>(
			(static_cast<uint32_t>(C0) << 24) |
			(static_cast<uint32_t>(C1) << 16) |
			(static_cast<uint32_t>(C2) << 8) |
			(static_cast<uint32_t>(C3) << 0)
			);
	};
}

namespace rkit { namespace utils
{
	inline uint32_t ComputeFourCC(char c0, char c1, char c2, char c3)
	{
		return static_cast<uint32_t>(
			(static_cast<uint32_t>(static_cast<uint8_t>(c0)) << 24) |
			(static_cast<uint32_t>(static_cast<uint8_t>(c1)) << 16) |
			(static_cast<uint32_t>(static_cast<uint8_t>(c2)) << 8) |
			(static_cast<uint32_t>(static_cast<uint8_t>(c3)) << 0)
			);
	}

	inline void ExtractFourCC(uint32_t fourCC, char &outC0, char &outC1, char &outC2, char &outC3)
	{
		outC0 = static_cast<char>(static_cast<uint8_t>((fourCC >> 24) & 0xff));
		outC1 = static_cast<char>(static_cast<uint8_t>((fourCC >> 16) & 0xff));
		outC2 = static_cast<char>(static_cast<uint8_t>((fourCC >> 8 ) & 0xff));
		outC3 = static_cast<char>(static_cast<uint8_t>((fourCC >> 0 ) & 0xff));
	}
} } // rkit::utils

#define RKIT_FOURCC(a, b, c, d) (::rkit::FourCCValue<a, b, c, d>::kValue)
