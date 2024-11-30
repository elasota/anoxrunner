#pragma once

#include <stdint.h>

namespace rkit
{
	template<char C0, char C1, char C2, char C3>
	struct FourCCValue
	{
		static const uint32_t kValue = static_cast<uint32_t>(
			(static_cast<uint32_t>(C0) << 0) |
			(static_cast<uint32_t>(C1) << 8) |
			(static_cast<uint32_t>(C2) << 16) |
			(static_cast<uint32_t>(C3) << 24)
			);
	};
}

namespace rkit::utils
{
	inline uint32_t ComputeFourCC(char c0, char c1, char c2, char c3)
	{
		return static_cast<uint32_t>(
			(static_cast<uint32_t>(c0) << 0) |
			(static_cast<uint32_t>(c1) << 8) |
			(static_cast<uint32_t>(c2) << 16) |
			(static_cast<uint32_t>(c3) << 24)
			);
	}

	inline void ExtractFourCC(uint32_t fourCC, char &outC0, char &outC1, char &outC2, char &outC3)
	{
		outC0 = static_cast<char>((fourCC >> 0) & 0xff);
		outC1 = static_cast<char>((fourCC >> 8) & 0xff);
		outC2 = static_cast<char>((fourCC >> 16) & 0xff);
		outC3 = static_cast<char>((fourCC >> 24) & 0xff);
	}
}

#define RKIT_FOURCC(a, b, c, d) (::rkit::FourCCValue<a, b, c, d>::kValue)
