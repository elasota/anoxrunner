#pragma once

#include "rkit/Core/Drivers.h"
#include "rkit/Core/Result.h"

namespace rkit
{
	template<class T>
	class UniquePtr;
}

namespace rkit::mp3
{
	static constexpr unsigned int kMaxSamplesPerFrame = 1152;
	static constexpr unsigned int kMaxBytesPerFrame = 144 * 320000 / 48000 + 1;

	struct DecodedFrameInfo
	{

		size_t m_bytesConsumed = 0;
		uint16_t m_sampleRate = 0;
		uint8_t m_channels = 0;
		uint16_t m_samplesProduced = 0;
	};

	struct IMP3Decoder
	{
		virtual ~IMP3Decoder() {}

		RKIT_NODISCARD virtual bool DecodeFrame(void *outputData, DecodedFrameInfo &outInfo, const void *data, size_t availableData) = 0;
	};


	struct IMP3Driver : public ICustomDriver
	{
		virtual rkit::Result CreateDecoder(rkit::UniquePtr<IMP3Decoder> &outDecoder) = 0;
	};
} // rkit::mp3
