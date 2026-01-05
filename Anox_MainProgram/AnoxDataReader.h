#pragma once

#include "rkit/Core/Endian.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/StringProto.h"

#include <stddef.h>

namespace rkit
{
	template<class TEnum>
	class EnumMask;

	template<class T>
	class Span;
}

namespace rkit { namespace math {
	template<class TNumber, size_t TSize>
	class Vec;
} }

namespace anox { namespace priv {
	template<class TEnum, class TIntegral, bool TNumberIsIntegral>
	struct ReadCheckEnumHelper
	{
	};

	template<class TEnum, class TIntegral>
	struct ReadCheckEnumHelper<TEnum, TIntegral, true>
	{
		static rkit::Result ReadCheckEnum(TEnum &outValue, TIntegral inUInt);
		static rkit::Result ReadCheckEnumMask(rkit::EnumMask<TEnum> &outValue, TIntegral inUInt);
	};

	template<class TEnum, class TIntegral>
	struct ReadCheckEnumHelper<TEnum, TIntegral, false>
	{
		static rkit::Result ReadCheckEnum(TEnum &outValue, const TIntegral &inUInt);
		static rkit::Result ReadCheckEnumMask(rkit::EnumMask<TEnum> &outValue, const TIntegral &inUInt);
	};
} }

namespace anox
{
	class Label;

	class DataReader
	{
	public:
		static rkit::Result ReadCheckFloat(float &outFloat, const rkit::endian::LittleFloat32_t &inFloat, int highestExpectedExponent);
		static rkit::Result ReadCheckUInt(uint64_t &outUInt, const rkit::endian::LittleUInt64_t &inUInt, uint64_t expectedMax);
		static rkit::Result ReadCheckUInt(uint32_t &outUInt, const rkit::endian::LittleUInt32_t &inUInt, uint32_t expectedMax);
		static rkit::Result ReadCheckUInt(uint16_t &outUInt, const rkit::endian::LittleUInt16_t &inUInt, uint16_t expectedMax);
		static rkit::Result ReadCheckUInt(uint8_t &outUInt, uint8_t inUInt, uint8_t expectedMax);
		static rkit::Result ReadCheckBool(bool &outBool, uint8_t inBool);
		static rkit::Result ReadCheckLabel(Label &outLabel, const rkit::endian::LittleUInt32_t &inLabel);
		static rkit::Result ReadCheckUTF8String(rkit::String &outString, const rkit::Span<const char> &chars);

		template<class TEnum, class TIntegral>
		static rkit::Result ReadCheckEnum(TEnum &outValue, const TIntegral &inUInt);

		template<class TEnum, class TIntegral>
		static rkit::Result ReadCheckEnumMask(rkit::EnumMask<TEnum> &outValue, const TIntegral &inUInt);

		template<size_t TSize>
		static rkit::Result ReadCheckVec(rkit::math::Vec<float, TSize> &outVec, const rkit::endian::LittleFloat32_t (&inVec)[TSize], int highestExpectedExponent);

	private:
		template<class TIntegral>
		static rkit::Result ReadCheckUIntImpl(TIntegral &outUInt, TIntegral inUInt, TIntegral expectedMax);
	};
}

#include "rkit/Math/Vec.h"

namespace anox { namespace priv {
	template<class TEnum, class TIntegral>
	rkit::Result ReadCheckEnumHelper<TEnum, TIntegral, true>::ReadCheckEnum(TEnum &outValue, TIntegral num)
	{
		if (num >= static_cast<uintmax_t>(TEnum::kCount))
			return rkit::ResultCode::kDataError;

		outValue = static_cast<TEnum>(num);
		return rkit::ResultCode::kOK;
	}

	template<class TEnum, class TIntegral>
	rkit::Result ReadCheckEnumHelper<TEnum, TIntegral, true>::ReadCheckEnumMask(rkit::EnumMask<TEnum> &outValue, TIntegral num)
	{
		static_assert(sizeof(TIntegral) * 8u >= static_cast<size_t>(TEnum::kCount), "Storage too small for enum");
		static_assert(static_cast<size_t>(TEnum::kCount) != 0, "Enum is empty");

		const uintmax_t emax = static_cast<uintmax_t>(TEnum::kCount);

		rkit::EnumMask<TEnum> result;
		for (size_t i = 0; i < emax; i++)
		{
			if ((static_cast<TIntegral>(1 << i) & num) != 0)
				result.Set(static_cast<TEnum>(i), true);
		}

		// Ensure no extraneous flags
		if (sizeof(TIntegral) * 8 < static_cast<size_t>(TEnum::kCount)
			&& (num >> static_cast<int>(TEnum::kCount)) != 0)
			return rkit::ResultCode::kDataError;

		outValue = result;
		return rkit::ResultCode::kOK;
	}

	template<class TEnum, class TIntegral>
	rkit::Result ReadCheckEnumHelper<TEnum, TIntegral, false>::ReadCheckEnum(TEnum &outValue, const TIntegral &inUInt)
	{
		typedef typename TIntegral::Number_t Number_t;
		const Number_t num = inUInt.Get();

		return ReadCheckEnumHelper<TEnum, Number_t, true>::ReadCheckEnum(outValue, num);
	}

	template<class TEnum, class TIntegral>
	rkit::Result ReadCheckEnumHelper<TEnum, TIntegral, false>::ReadCheckEnumMask(rkit::EnumMask<TEnum> &outValue, const TIntegral &inUInt)
	{
		typedef typename TIntegral::Number_t Number_t;
		const Number_t num = inUInt.Get();

		return ReadCheckEnumHelper<TEnum, Number_t, true>::ReadCheckEnumMask(outValue, num);
	}
} }

namespace anox
{
	template<size_t TSize>
	rkit::Result DataReader::ReadCheckVec(rkit::math::Vec<float, TSize> &outVec, const rkit::endian::LittleFloat32_t(&inVec)[TSize], int highestExpectedExponent)
	{
		float floats[TSize] = {};
		for (size_t i = 0; i < TSize; i++)
		{
			RKIT_CHECK(ReadCheckFloat(floats[i], inVec[i], highestExpectedExponent));
		}

		outVec = rkit::math::Vec<float, TSize>::FromArray(floats);

		return rkit::ResultCode::kOK;
	}

	template<class TEnum, class TIntegral>
	rkit::Result DataReader::ReadCheckEnum(TEnum &outValue, const TIntegral &inUInt)
	{
		return priv::ReadCheckEnumHelper<TEnum, TIntegral, std::is_integral<TIntegral>::value>::ReadCheckEnum(outValue, inUInt);
	}

	template<class TEnum, class TIntegral>
	rkit::Result DataReader::ReadCheckEnumMask(rkit::EnumMask<TEnum> &outValue, const TIntegral &inUInt)
	{
		return priv::ReadCheckEnumHelper<TEnum, TIntegral, std::is_integral<TIntegral>::value>::ReadCheckEnumMask(outValue, inUInt);
	}

	inline rkit::Result DataReader::ReadCheckBool(bool &outBool, uint8_t inBool)
	{
		uint8_t b = 0;
		RKIT_CHECK(ReadCheckUInt(b, inBool, 1));
		outBool = (b != 0);
		return rkit::ResultCode::kOK;
	}
}
