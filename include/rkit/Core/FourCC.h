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

#define RKIT_FOURCC(a, b, c, d) (::rkit::FourCCValue<a, b, c, d>::kValue)
