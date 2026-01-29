#include "rkit/Core/Endian.h"

namespace anox { namespace data
{
	struct CompressedNormal32
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
		rkit::endian::LittleUInt32_t m_value;
	};

	struct CompressedNormal64NoNegate
	{
		rkit::endian::LittleUInt32_t m_part0;
		rkit::endian::LittleUInt32_t m_part1;
	};

	struct CompressedNormal64
	{
		rkit::endian::LittleUInt32_t m_part0;
		rkit::endian::LittleUInt32_t m_part1;
	};
} } // anox::data

#include <math.h>

#include "rkit/Math/Functions.h"

namespace anox { namespace data { namespace priv {
	template<class TPartial, class THighPrecisionInt, class TFloat, int TBits>
	inline void DecompressNormalTemplate(float &outX, float &outY, float &outZ, TPartial primary, TPartial secondary, bool primaryX, bool secondaryY, bool negate)
	{
		const TPartial offsetAmount = static_cast<TPartial>(-((1 << (TBits - 1)) - 1));
		TPartial x = offsetAmount;
		TPartial y = offsetAmount;
		TPartial z = offsetAmount;

		if (primaryX)
			x += primary;
		else
			z += primary;

		if (secondaryY)
			y += secondary;
		else
			z += secondary;

		const THighPrecisionInt xsq = static_cast<THighPrecisionInt>(x) * x;
		const THighPrecisionInt ysq = static_cast<THighPrecisionInt>(y) * y;
		const THighPrecisionInt zsq = static_cast<THighPrecisionInt>(z) * z;

		if (negate)
		{
			x = -x;
			y = -y;
			z = -z;
		}

		const TFloat len = rkit::math::SamePrecisionSqrt(static_cast<TFloat>(xsq + ysq + zsq));

		outX = static_cast<float>(x / len);
		outY = static_cast<float>(y / len);
		outZ = static_cast<float>(z / len);
	}

	template<class TPartial, class TFloat, int TBits>
	void CompressNormalTemplate(TPartial &outPrimary, TPartial &outSecondary, bool &outPrimaryX, bool &outSecondaryY, bool &outNegate, float x, float y, float z)
	{
		uint32_t channelBits = 0;

		TFloat primary = 0.f;
		TFloat secondary = 0.f;
		TFloat magnitude = 0.f;

		const TFloat magX = rkit::math::SamePrecisionAbs(x);
		const TFloat magY = rkit::math::SamePrecisionAbs(y);
		const TFloat magZ = rkit::math::SamePrecisionAbs(z);

		bool secondaryY = false;
		bool primaryX = false;
		bool negate = false;

		if (magX > magZ)
		{
			if (magX > magY)
			{
				// X is highest
				primary = z;
				secondary = y;
				magnitude = x;
				secondaryY = true;
			}
			else
			{
				// Y is highest
				primary = x;
				secondary = z;
				magnitude = y;
				primaryX = true;
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
				primaryX = true;
				secondaryY = true;
			}
			else
			{
				// Y is highest
				primary = x;
				secondary = z;
				magnitude = y;
				primaryX = true;
			}
		}

		if (magnitude > 0)
			negate = true;

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

		const uint32_t intHalf = (static_cast<uint32_t>(1) << (TBits - 1)) - 1u;
		const TFloat floatHalf = static_cast<TFloat>(intHalf);
		const TFloat floatHalfPlusOneHalf = floatHalf + 0.5f;

		const uint32_t primaryAdjusted = static_cast<uint32_t>(rkit::math::SamePrecisionFloor<TFloat>(primary * floatHalf + floatHalfPlusOneHalf));
		const uint32_t secondaryAdjusted = static_cast<uint32_t>(rkit::math::SamePrecisionFloor<TFloat>(secondary * floatHalf + floatHalfPlusOneHalf));

		outPrimary = static_cast<TPartial>(primaryAdjusted);
		outSecondary = static_cast<TPartial>(secondaryAdjusted);

		outPrimaryX = primaryX;
		outSecondaryY = secondaryY;
		outNegate = negate;
	}
} } } // anox::data::priv

namespace anox { namespace data {
	inline void DecompressNormal32(uint32_t bits, float &outX, float &outY, float &outZ)
	{
		const int16_t primary = (bits >> 4) & 0x3fff;
		const int16_t secondary = (bits >> 18) & 0x3fff;

		const bool primaryX = ((bits & 1) != 0);
		const bool secondaryY = ((bits & 2) != 0);
		const bool negate = ((bits & 4) != 0);

		return priv::DecompressNormalTemplate<int16_t, int32_t, float, 14>(outX, outY, outZ, primary, secondary, primaryX, secondaryY, negate);
	}

	inline uint32_t CompressNormal32(float x, float y, float z)
	{
		uint32_t primary = 0;
		uint32_t secondary = 0;
		bool primaryX = false;
		bool secondaryY = false;
		bool negate = false;
		priv::CompressNormalTemplate<uint32_t, float, 14>(primary, secondary, primaryX, secondaryY, negate, x, y, z);

		uint32_t packed = (primary << 4) | (secondary << 18);
		if (primaryX)
			packed |= 1;
		if (secondaryY)
			packed |= 2;
		if (negate)
			packed |= 4;

		return packed;
	}

	inline void DecompressNormal64(uint32_t part0, uint32_t part1, float &outX, float &outY, float &outZ)
	{
		const int32_t primary = static_cast<int32_t>(part0 & 0x3fffffffu);
		const int32_t secondary = static_cast<int32_t>(part1 & 0x3fffffffu);

		const bool primaryX = ((part0 & 0x40000000u) != 0);
		const bool secondaryY = ((part1 & 0x40000000u) != 0);
		const bool negate = ((part0 & 0x80000000u) != 0);

		return priv::DecompressNormalTemplate<int32_t, int64_t, double, 30>(outX, outY, outZ, primary, secondary, primaryX, secondaryY, negate);
	}

	inline void DecompressNormal64NoNegate(uint32_t part0, uint32_t part1, float &outX, float &outY, float &outZ)
	{
		const int32_t primary = static_cast<int32_t>(part0 & 0x7fffffffu);
		const int32_t secondary = static_cast<int32_t>(part1 & 0x7fffffffu);

		const bool primaryX = ((part0 & 0x80000000u) != 0);
		const bool secondaryY = ((part1 & 0x80000000u) != 0);

		return priv::DecompressNormalTemplate<int32_t, int64_t, double, 31>(outX, outY, outZ, primary, secondary, primaryX, secondaryY, false);
	}

	inline void CompressNormal64(uint32_t &outPart0, uint32_t &outPart1, float x, float y, float z)
	{
		uint32_t primary = 0;
		uint32_t secondary = 0;
		bool primaryX = false;
		bool secondaryY = false;
		bool negate = false;
		priv::CompressNormalTemplate<uint32_t, double, 31>(primary, secondary, primaryX, secondaryY, negate, x, y, z);

		uint32_t part0 = primary;
		uint32_t part1 = secondary;
		if (primaryX)
			part0 |= 0x40000000u;
		if (secondaryY)
			part1 |= 0x40000000u;
		if (negate)
			part0 |= 0x80000000u;

		outPart0 = part0;
		outPart1 = part1;
	}

	inline void CompressNormal64NoNegate(uint32_t &outPart0, uint32_t &outPart1, bool &outWasNegated, float x, float y, float z)
	{
		uint32_t primary = 0;
		uint32_t secondary = 0;
		bool primaryX = false;
		bool secondaryY = false;
		bool negate = false;
		priv::CompressNormalTemplate<uint32_t, double, 31>(primary, secondary, primaryX, secondaryY, negate, x, y, z);

		uint32_t part0 = primary;
		uint32_t part1 = secondary;
		if (primaryX)
			part0 |= 0x80000000u;
		if (secondaryY)
			part1 |= 0x80000000u;

		outPart0 = part0;
		outPart1 = part1;
		outWasNegated = negate;
	}
} } // anox::data
