#include "AnoxDataReader.h"

#include "anox/Label.h"

#include "rkit/Core/String.h"

namespace anox
{
	rkit::Result DataReader::ReadCheckFloat(float &outFloat, const rkit::endian::LittleFloat32_t &inFloat, int highestExpectedExponent)
	{
		const uint32_t floatBits = inFloat.GetBits();

		const int exponent = static_cast<int>((floatBits >> 23) & 0xff) - 0x7f;

		if (exponent > highestExpectedExponent)
			return rkit::ResultCode::kDataError;

		outFloat = inFloat.Get();
		return rkit::ResultCode::kOK;
	}

	rkit::Result DataReader::ReadCheckUInt(uint64_t &outUInt, const rkit::endian::LittleUInt64_t &inUInt, uint64_t expectedMax)
	{
		return ReadCheckUIntImpl(outUInt, inUInt.Get(), expectedMax);
	}

	rkit::Result DataReader::ReadCheckUInt(uint32_t &outUInt, const rkit::endian::LittleUInt32_t &inUInt, uint32_t expectedMax)
	{
		return ReadCheckUIntImpl(outUInt, inUInt.Get(), expectedMax);
	}

	rkit::Result DataReader::ReadCheckUInt(uint16_t &outUInt, const rkit::endian::LittleUInt16_t &inUInt, uint16_t expectedMax)
	{
		return ReadCheckUIntImpl(outUInt, inUInt.Get(), expectedMax);
	}

	rkit::Result DataReader::ReadCheckUInt(uint8_t &outUInt, uint8_t inUInt, uint8_t expectedMax)
	{
		return ReadCheckUIntImpl(outUInt, inUInt, expectedMax);
	}

	rkit::Result DataReader::ReadCheckLabel(Label &outLabel, const rkit::endian::LittleUInt32_t &inLabel)
	{
		outLabel = Label::FromRawValue(inLabel.Get());
		return rkit::ResultCode::kOK;
	}

	rkit::Result DataReader::ReadCheckUTF8String(rkit::String &outString, const rkit::Span<const char> &chars)
	{
		rkit::StringSliceView view(chars);
		if (!view.Validate())
			return rkit::ResultCode::kDataError;

		return rkit::ResultCode::kOK;
	}

	template<class TIntegral>
	rkit::Result DataReader::ReadCheckUIntImpl(TIntegral &outUInt, TIntegral value, TIntegral expectedMax)
	{
		if (value > expectedMax)
			return rkit::ResultCode::kDataError;

		outUInt = value;
		return rkit::ResultCode::kOK;
	}
}
