#include "rkit/Core/Endian.h"

namespace anox { namespace data
{
	struct CompressedNormal
	{
		// Compressed normal format:
		// 0: Add primary to X instead of Z
		// 1: Add secondary to Y instead of Z
		// 2: Negate
		// 3: Unused
		// 4..17: primary
		// 18..31: secondary
		//
		// Decode process:
		// X = (primary * bit[0]) - 8191
		// Y = (secondary * bit[1]) - 8191
		// Z = (primary * (1 - bit[0]) + (secondary * (1 - bit[1])) - 8191
		// X,Y,Z = X,Y,Z * (1 - (bit[3] * 2))
		// X,Y,Z = normalize(X,Y,Z)
		rkit::endian::LittleUInt32_t m_compressedNormal;
	};
} } // anox::data

#include <math.h>

namespace anox { namespace data
{
	inline CompressedNormal CompressNormal(float x, float y, float z)
	{
		const uint32_t kXIsPrimary = 1;
		const uint32_t kYIsSecondary = 2;
		const uint32_t kNegate = 4;

		uint32_t channelBits = 0;

		float primary = 0.f;
		float secondary = 0.f;
		float magnitude = 0.f;

		const float magX = fabsf(x);
		const float magY = fabsf(y);
		const float magZ = fabsf(z);

		if (magX > magZ)
		{
			if (magX > magY)
			{
				// X is highest
				primary = z;
				secondary = y;
				magnitude = x;
				channelBits = kYIsSecondary;
			}
			else
			{
				// Y is highest
				primary = x;
				secondary = z;
				magnitude = y;
				channelBits = kXIsPrimary;
			}
		}
		else
		{
			if (magZ > magY)
			{
				// Z is highest
				primary = x;
				secondary = y;
				magnitude = z;
				channelBits = (kXIsPrimary | kYIsSecondary);
			}
			else
			{
				// Y is highest
				primary = x;
				secondary = z;
				magnitude = y;
				channelBits = kXIsPrimary;
			}
		}

		if (magnitude > 0)
			channelBits |= kNegate;

		primary /= -magnitude;
		secondary /= -magnitude;

		if (!(primary >= 0.f))
			primary = 0.f;
		else if (primary > 1.0f)
			primary = 1.0f;

		if (!(secondary >= 0.f))
			secondary = 0.f;
		else if (secondary > 1.0f)
			secondary = 1.0f;

		const uint32_t primaryAdjusted = static_cast<uint32_t>(floorf(primary * 8191.0f + 8191.5f));
		const uint32_t secondaryAdjusted = static_cast<uint32_t>(floorf(secondary * 8191.0f + 8191.5f));

		const uint32_t packedBits = (primaryAdjusted << 4) | (secondaryAdjusted << 18) | channelBits;

		CompressedNormal result;
		result.m_compressedNormal = packedBits;

		return result;
	}
} } // anox::data
