#pragma once

#include <stdint.h>

namespace rkit { namespace extops {
	void XMulU64(uint64_t a, uint64_t b, uint64_t &outLow, uint64_t &outHigh);

#if defined(_MSC_VER)
#endif


} } // rkit::extops

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace rkit { namespace extops {
#if defined(_MSC_VER)
	inline void XMulU64(uint64_t a, uint64_t b, uint64_t &outLow, uint64_t &outHigh)
	{
		unsigned long long highProduct = 0;
		outLow = _umul128(a, b, &highProduct);
		outHigh = highProduct;
	}
#endif


} } // rkit::extops
