#pragma once

#include "rkit/Core/Opaque.h"
#include "rkit/Core/Result.h"

#include "rkit/Audio/AudioFileFormat.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	class TypelessRCPtr;

	template<class T>
	class Span;
}

namespace anox
{
	class AudioSubsystem;
}

namespace anox::game
{
	struct SoundEmitterProperties;

	class GameAudioManagerImpl;

	class GameAudioManager final : public rkit::Opaque<GameAudioManagerImpl>
	{
	public:
		explicit GameAudioManager(AudioSubsystem &audioSubsystem);

		rkit::Result CreateSoundSourceFromBytes(uint32_t &outSourceID, rkit::TypelessRCPtr &&keepalive, rkit::Span<const uint8_t> contents, rkit::audio::AudioContainerFormat containerFormat);
		void DestroySoundSource(uint32_t sourceID);

		rkit::Result CreateEmitterFromSource(uint32_t &outEmitterID, uint32_t sourceID, const SoundEmitterProperties &emitterProperties);
		void DestroyEmitter(uint32_t emitterID);

		rkit::Result PlayEmitter(uint32_t emitterID);

		static rkit::Result Create(rkit::UniquePtr<GameAudioManager> &resManager, AudioSubsystem& audioSubsystem);
	};
}
