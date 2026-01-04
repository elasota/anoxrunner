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

namespace anox
{
	class Label;

	class DataReader
	{
	public:
		static rkit::Result ReadCheckFloat(float &outFloat, const rkit::endian::LittleFloat32_t &inFloat, int highestExpectedExponent);
		static rkit::Result ReadCheckUInt(uint32_t &outUInt, const rkit::endian::LittleUInt32_t &inUInt, uint32_t expectedMax);
		static rkit::Result ReadCheckLabel(Label &outLabel, const rkit::endian::LittleUInt32_t &inLabel);
		static rkit::Result ReadCheckUTF8String(rkit::String &outString, const rkit::Span<const char> &chars);

		template<class TEnum, class TIntegral>
		static rkit::Result ReadCheckEnum(TEnum &outValue, const TIntegral &inUInt);

		template<class TEnum, class TIntegral>
		static rkit::Result ReadCheckEnumMask(rkit::EnumMask<TEnum> &outValue, const TIntegral &inUInt);

		template<size_t TSize>
		static rkit::Result ReadCheckVec(rkit::math::Vec<float, TSize> &outVec, const rkit::endian::LittleFloat32_t (&inVec)[TSize], int highestExpectedExponent);
	};
}

#include "rkit/Math/Vec.h"

namespace anox
{
	template<class TEnum, class TIntegral>
	rkit::Result DataReader::ReadCheckEnum(TEnum &outValue, const TIntegral &inUInt)
	{
		typedef typename TIntegral::Number_t Number_t;

		const uintmax_t emax = static_cast<uintmax_t>(TEnum::kCount);
		const Number_t num = inUInt.Get();

		if (num >= static_cast<Number_t>(TEnum::kCount))
			return rkit::ResultCode::kDataError;

		outValue = static_cast<TEnum>(num);
		return rkit::ResultCode::kOK;
	}

	template<class TEnum, class TIntegral>
	rkit::Result DataReader::ReadCheckEnumMask(rkit::EnumMask<TEnum> &outValue, const TIntegral &inUInt)
	{
		typedef typename TIntegral::Number_t Number_t;

		const uintmax_t emax = static_cast<uintmax_t>(TEnum::kCount);
		Number_t num = inUInt.Get();

		rkit::EnumMask<TEnum> result;
		for (uintmax_t i = 0; i < emax; i++)
		{
			if (i < sizeof(Number_t) * 8u && ((num >> i) & 1) == 1)
			{
				result.Set(i, true);
				num -= static_cast<Number_t>(static_cast<Number_t>(1) << i);
			}
		}

		// Ensure no extraneous flags
		if (num != 0)
			return rkit::ResultCode::kDataError;

		outValue = result;
		return rkit::ResultCode::kOK;
	}

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
}
