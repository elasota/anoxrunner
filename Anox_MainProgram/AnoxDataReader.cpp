#include "AnoxDataReader.h"

#include "anox/Label.h"

#include "rkit/Core/String.h"

namespace anox
{
	rkit::Result DataReader::ReadCheckFloats(const rkit::Span<float> &outFloats, const rkit::Span<const rkit::endian::LittleFloat32_t> &inFloats, int highestExpectedExponent)
	{
		RKIT_ASSERT(outFloats.Count() == inFloats.Count());

		const size_t numFloats = rkit::Min(outFloats.Count(), inFloats.Count());

		const rkit::endian::LittleFloat32_t *inFloatsPtr = inFloats.Ptr();
		float *outFloatsPtr = outFloats.Ptr();

		bool isOK = true;
		for (size_t i = 0; i < numFloats; i++)
		{
			const uint32_t floatBits = inFloatsPtr[i].GetBits();

			const int exponent = static_cast<int>((floatBits >> 23) & 0xff) - 0x7f;

			// Don't use shortcutting here so compiler can possibly vectorize this loop
			isOK &= (exponent <= highestExpectedExponent);
		}

		if (!isOK)
			return rkit::ResultCode::kDataError;

		for (size_t i = 0; i < numFloats; i++)
			outFloatsPtr[i] = inFloatsPtr[i].Get();

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
