#include "rkit/MP3/MP3Driver.h"

#include "rkit/Core/DriverModuleStub.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/ModuleGlue.h"
#include "rkit/Core/Stream.h"

#include "rkit/Utilities/Image.h"

#include "minimp3.h"

namespace rkit::mp3
{
	class MP3Decoder final : public IMP3Decoder
	{
	public:
		MP3Decoder();

		bool DecodeFrame(void *outputData, DecodedFrameInfo &outInfo, const void *data, size_t availableData) override;

	private:
		mp3dec_t m_mp3dec;
	};

	class MP3Driver final : public IMP3Driver
	{
	public:
		rkit::Result CreateDecoder(rkit::UniquePtr<IMP3Decoder> &outDecoder) override;

	private:
		rkit::Result InitDriver(const rkit::DriverInitParameters *) override;
		void ShutdownDriver() override;

		uint32_t GetDriverNamespaceID() const override { return rkit::IModuleDriver::kDefaultNamespace; }
		rkit::StringView GetDriverName() const override { return u8"MP3"; }
	};

	typedef rkit::CustomDriverModuleStub<MP3Driver> MP3Module;

	MP3Decoder::MP3Decoder()
	{
		mp3dec_init(&m_mp3dec);
	}

	bool MP3Decoder::DecodeFrame(void *outputData, DecodedFrameInfo &outInfo, const void *data, size_t availableData)
	{
		if (availableData > static_cast<size_t>(std::numeric_limits<int>::max()))
			availableData = static_cast<size_t>(std::numeric_limits<int>::max());

		mp3dec_frame_info_t frameInfo = {};
		int samplesDecoded = mp3dec_decode_frame(&m_mp3dec, static_cast<const uint8_t *>(data), static_cast<int>(availableData), static_cast<int16_t *>(outputData), &frameInfo);

		if (samplesDecoded == 0)
			return false;

		outInfo.m_bytesConsumed = frameInfo.frame_bytes;
		outInfo.m_channels = frameInfo.channels;
		outInfo.m_sampleRate = frameInfo.hz;
		outInfo.m_samplesProduced = samplesDecoded;

		return true;
	}

	rkit::Result MP3Driver::CreateDecoder(rkit::UniquePtr<IMP3Decoder> &outDecoder)
	{
		return rkit::New<MP3Decoder>(outDecoder);
	}

	rkit::Result MP3Driver::InitDriver(const rkit::DriverInitParameters *)
	{
		RKIT_RETURN_OK;
	}

	void MP3Driver::ShutdownDriver()
	{
	}

} // rkit::mp3


RKIT_IMPLEMENT_MODULE(RKit, MP3, ::rkit::mp3::MP3Module)
