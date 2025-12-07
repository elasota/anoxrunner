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
		// X,Y,Z = X,Y,Z * (1 - (bit[2] * 2))
		// X,Y,Z = normalize(X,Y,Z)
		rkit::endian::LittleUInt32_t m_compressedNormal;
	};
} } // anox::data

#include <math.h>

namespace anox { namespace data
{
	inline void DecompressNormal(const CompressedNormal &cn, float &outX, float &outY, float &outZ)
	{
		const uint32_t bits = cn.m_compressedNormal.GetBits();

		const int16_t primary = (bits >> 4) & 0x3fff;
		const int16_t secondary = (bits >> 18) & 0x3fff;

		int16_t x = -8191;
		int16_t y = -8191;
		int16_t z = -8191;

		if (bits & 1)
			x += primary;
		else
			z += primary;

		if (bits & 2)
			y += secondary;
		else
			z += secondary;

		if (bits & 4)
		{
			x = -x;
			y = -y;
			z = -z;
		}

		const int32_t xsq = static_cast<int32_t>(x) * x;
		const int32_t ysq = static_cast<int32_t>(y) * y;
		const int32_t zsq = static_cast<int32_t>(z) * z;

		const double len = sqrt(xsq + ysq + zsq);

		outX = static_cast<float>(x / len);
		outY = static_cast<float>(y / len);
		outZ = static_cast<float>(z / len);
	}

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

		if (!(primary >= -1.f))
			primary = -1.f;
		else if (primary > 1.0f)
			primary = 1.0f;

		if (!(secondary >= -1.f))
			secondary = -1.f;
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
