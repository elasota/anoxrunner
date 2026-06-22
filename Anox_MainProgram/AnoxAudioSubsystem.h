#pragma once

#include "rkit/Core/Opaque.h"
#include "rkit/Core/Result.h"

#include "rkit/Audio/AudioDriver.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	template<class T>
	class RCPtr;

	struct IJobQueue;
}

namespace anox
{
	class AudioSubsystemImpl;
	class AudioEmitterImpl;

	class AudioEmitter
	{
	public:
		virtual ~AudioEmitter() {}
	};

	struct AudioFrame
	{
		const void *m_data = nullptr;
		size_t m_numSamples = 0;
	};

	struct IAudioSource
	{
		static constexpr size_t kMaxSourceChannels = 6;

		virtual ~IAudioSource() {}

		virtual const AudioFrame *GetCurrentAudioFrame() = 0;
		virtual rkit::audio::AudioFormat GetAudioFormat() const = 0;
		virtual void DiscardAudioFrame() = 0;
	};

	class AudioSubsystem final : public rkit::Opaque<AudioSubsystemImpl>
	{
	public:
		explicit AudioSubsystem(rkit::IJobQueue &jobQueue);

		rkit::Result CreateEmitter(AudioEmitter *&outEmitter, rkit::RCPtr<IAudioSource> inSource);
		void DestroyEmitter(AudioEmitter *emitter);

		rkit::Result PlayEmitter(AudioEmitter *emitter);

		rkit::Result Update();

		static rkit::Result Create(rkit::UniquePtr<AudioSubsystem> &outSubsystem, rkit::IJobQueue& jobQueue);
	};
}
