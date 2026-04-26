#include "rkit/Core/CoreLib.h"

namespace rkit::utils
{
	::rkit::HashValue_t RKIT_CORELIB_API ComputeHash(::rkit::HashValue_t baseHash, const void *data, size_t size)
	{
		const uint8_t *bytes = static_cast<const uint8_t *>(data);

		// TODO: Improve this
		HashValue_t hash = baseHash;
		for (size_t i = 0; i < size; i++)
			hash = hash * 223u + bytes[i] * 4447u;

		return hash;
	}
}

