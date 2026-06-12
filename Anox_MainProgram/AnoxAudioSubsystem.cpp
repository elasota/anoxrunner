#include "AnoxAudioSubsystem.h"

#include "rkit/Audio/AudioDriver.h"

#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/RefCounted.h"

#include "rkit/Win32/IncludeWindows.h"

#define _USE_MATH_DEFINES
#include <math.h>

namespace anox
{
	class AudioMixer final : public rkit::audio::IAudioOutputRenderer
	{
	public:
		bool Render(void *buffer, size_t numSamples, const rkit::audio::IAudioOutputStateQuery &outputQuery) override;

		void SetOutputFormat(const rkit::audio::AudioFormat &audioFormat);
		void SetBufferCapacity(size_t bufferCapacity);

	private:
		rkit::audio::AudioFormat m_audioFormat;
		size_t m_bufferCapacity = 0;
	};

	class AudioSubsystemImpl final : public rkit::OpaqueImplementation<AudioSubsystem>
	{
	public:
		~AudioSubsystemImpl();

		rkit::Result Init();

	private:
		rkit::Result AcquireOutput();
		void UnloadOutput();

		void Shutdown();

		void PauseOutput();
		void ResumeOutput();

		bool HasOutput() const;

		rkit::RCPtr<rkit::audio::IAudioOutputEndpoint> m_endpoint;
		rkit::UniquePtr<rkit::audio::IAudioOutputStream> m_outputStream;

		rkit::audio::IAudioDriver *m_audioDriver = nullptr;

		AudioMixer m_mixer;
	};

	bool AudioMixer::Render(void *buffer, size_t numSamples, const rkit::audio::IAudioOutputStateQuery &outputQuery)
	{
		return false;
	}

	void AudioMixer::SetOutputFormat(const rkit::audio::AudioFormat &audioFormat)
	{
		m_audioFormat = audioFormat;
	}

	void AudioMixer::SetBufferCapacity(size_t bufferCapacity)
	{
		m_bufferCapacity = bufferCapacity;
	}

	AudioSubsystemImpl::~AudioSubsystemImpl()
	{
		Shutdown();
	}

	rkit::Result AudioSubsystemImpl::Init()
	{
		// FIXME: Select appropriate audio driver
		rkit::IModule* module = rkit::GetDrivers().m_moduleDriver->LoadModule(rkit::IModuleDriver::kDefaultNamespace, u8"Audio_WASAPI");
		if (!module)
			RKIT_THROW(rkit::ResultCode::kModuleLoadFailed);

		m_audioDriver = static_cast<rkit::audio::IAudioDriver*>(rkit::GetDrivers().FindDriver(rkit::IModuleDriver::kDefaultNamespace, u8"Audio_WASAPI"));

		RKIT_CHECK(AcquireOutput());

		if (HasOutput())
			ResumeOutput();

		RKIT_RETURN_OK;
	}

	rkit::Result AudioSubsystemImpl::AcquireOutput()
	{
		rkit::RCPtr<rkit::audio::IAudioOutputEndpoint> audioOutputEndpoint;
		RKIT_CHECK(m_audioDriver->GetDefaultOutputEndpoint(audioOutputEndpoint));

		if (!audioOutputEndpoint.IsValid())
			RKIT_RETURN_OK;

		rkit::audio::AudioFormat audioFormat;
		audioFormat.m_sampleRate = 44100;
		audioFormat.m_sampleType = rkit::audio::SampleType::kSInt16;
		audioFormat.m_speakers.Set(rkit::audio::SpeakerPosition::kFrontLeft, true);
		audioFormat.m_speakers.Set(rkit::audio::SpeakerPosition::kFrontCenter, true);

		rkit::UniquePtr<rkit::audio::IAudioOutputStream> audioOutputStream;
		RKIT_CHECK(audioOutputEndpoint->TryOpenOutputStream(audioOutputStream, audioFormat, 1024, &m_mixer));
		if (!audioOutputStream.IsValid())
			RKIT_RETURN_OK;

		m_mixer.SetOutputFormat(audioFormat);
		m_mixer.SetBufferCapacity(audioOutputStream->GetBufferCapacity());

		m_endpoint = std::move(audioOutputEndpoint);
		m_outputStream = std::move(audioOutputStream);

		RKIT_RETURN_OK;
	}

	void AudioSubsystemImpl::PauseOutput()
	{
		if (m_outputStream.IsValid())
			m_outputStream->Stop();
	}

	void AudioSubsystemImpl::ResumeOutput()
	{
		if (m_outputStream.IsValid())
			m_outputStream->Start();
	}

	bool AudioSubsystemImpl::HasOutput() const
	{
		return m_outputStream.IsValid();
	}

	void AudioSubsystemImpl::UnloadOutput()
	{
		m_outputStream.Reset();
		m_endpoint.Reset();
	}

	void AudioSubsystemImpl::Shutdown()
	{
		UnloadOutput();
	}

	rkit::Result AudioSubsystem::Init()
	{
		return Impl().Init();
	}

	rkit::Result AudioSubsystem::Create(rkit::UniquePtr<AudioSubsystem> &outSubsystem)
	{
		rkit::UniquePtr<AudioSubsystem> subsystem;
		RKIT_CHECK(rkit::New<AudioSubsystem>(subsystem));

		RKIT_CHECK(subsystem->Init());

		outSubsystem = std::move(subsystem);
		RKIT_RETURN_OK;
	}
}

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(::anox::AudioSubsystemImpl)
