#include "AnoxGameAudioManager.h"

#include "AnoxAudioSubsystem.h"

#include "AnoxSandboxTrackedObjectList.h"

#include "rkit/Audio/AudioFileFormat.h"

#include "rkit/MP3/MP3Driver.h"

#include "rkit/Core/LogDriver.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/RefCounted.h"
#include "rkit/Core/Vector.h"

namespace anox::game
{
	class GameSoundDataSource
	{
	public:
		virtual ~GameSoundDataSource() {}

		virtual size_t ReadData(rkit::Span<uint8_t> dest) = 0;
		virtual bool IsExhausted() const = 0;
	};

	class GameSoundBytesDataSource final : public GameSoundDataSource
	{
	public:
		explicit GameSoundBytesDataSource(rkit::TypelessRCPtr keepalive, rkit::Span<const uint8_t> contents);

		size_t ReadData(rkit::Span<uint8_t> dest) override;
		bool IsExhausted() const override;

	private:
		rkit::TypelessRCPtr m_keepalive;
		const rkit::Span<const uint8_t> m_contents;

		size_t m_readOffset = 0;
	};

	class GameSoundMP3Source final : public rkit::RefCounted, public IAudioSource
	{
	public:
		explicit GameSoundMP3Source(rkit::UniquePtr<GameSoundDataSource> dataSource);

		rkit::Result Initialize(rkit::mp3::IMP3Driver &mp3Driver);

		const AudioFrame *GetCurrentAudioFrame() override;
		void DiscardAudioFrame() override;

		rkit::audio::AudioFormat GetAudioFormat() const override;

	private:
		static constexpr size_t kMaxSamples = rkit::mp3::kMaxSamplesPerFrame;
		static constexpr size_t kMaxBytes = rkit::mp3::kMaxBytesPerFrame * 3;

		rkit::StaticArray<int16_t, 2 * kMaxSamples> m_samplesArray;
		rkit::StaticArray<uint8_t, kMaxBytes> m_dataStorage;
		rkit::ConstSpan<uint8_t> m_dataSpan;

		rkit::mp3::DecodedFrameInfo m_decodedFrameInfo = {};
		rkit::audio::AudioFormat m_audioFormat;

		rkit::Optional<AudioFrame> m_currentFrame;
		bool m_exhausted = false;

		rkit::UniquePtr<rkit::mp3::IMP3Decoder> m_decoder;
		rkit::UniquePtr<GameSoundDataSource> m_dataSource;
	};

	class GameAudioManagerImpl final : public rkit::OpaqueImplementation<GameAudioManager>
	{
	public:
		explicit GameAudioManagerImpl(AudioSubsystem &audioSubsystem);

		rkit::Result Initialize();

		rkit::Result CreateSoundSourceFromBytes(uint32_t &outSourceID, rkit::TypelessRCPtr &&keepalive, rkit::Span<const uint8_t> contents, rkit::audio::AudioContainerFormat containerFormat);
		void DestroySoundSource(uint32_t sourceID);

		rkit::Result CreateEmitterFromSource(uint32_t &outEmitterID, uint32_t sourceID, const SoundEmitterProperties &emitterProperties);
		void DestroyEmitter(uint32_t emitterID);

		void PlayEmitter(uint32_t emitterID);

	private:
		struct AudioEmitterState
		{
			AudioEmitter *m_mixerEmitter = nullptr;
			rkit::RCPtr<IAudioSource> m_audioSource;
		};

		class AudioEmitterDisposer
		{
		public:
			explicit AudioEmitterDisposer(GameAudioManagerImpl &owner);

			void DisposeObject(const AudioEmitterState &emitterState) const;

		private:
			GameAudioManagerImpl &m_owner;
		};

		rkit::Result CreateAudioSourceFromDataSource(uint32_t &outSourceID, rkit::UniquePtr<GameSoundDataSource> dataSource, rkit::audio::AudioContainerFormat containerFormat);

		AudioSubsystem &m_audioSubsystem;
		rkit::mp3::IMP3Driver *m_mp3Driver = nullptr;

		TrackedObjectList<rkit::RCPtr<IAudioSource>> m_sources;
		TrackedObjectList<AudioEmitterState, AudioEmitterDisposer> m_emitters;
	};


	GameSoundBytesDataSource::GameSoundBytesDataSource(rkit::TypelessRCPtr keepalive, rkit::Span<const uint8_t> contents)
		: m_keepalive(std::move(keepalive))
		, m_contents(contents)
	{
	}

	size_t GameSoundBytesDataSource::ReadData(rkit::Span<uint8_t> dest)
	{
		const size_t bytesAvailable = m_contents.Count() - m_readOffset;
		const size_t sizeToRead = rkit::Min(bytesAvailable, dest.Count());

		rkit::CopySpan(dest.SubSpan(0, sizeToRead), m_contents.SubSpan(m_readOffset, sizeToRead));
		m_readOffset += sizeToRead;

		return sizeToRead;
	}

	bool GameSoundBytesDataSource::IsExhausted() const
	{
		return m_readOffset == m_contents.Count();
	}

	GameSoundMP3Source::GameSoundMP3Source(rkit::UniquePtr<GameSoundDataSource> dataSource)
		: m_dataSource(std::move(dataSource))
	{
	}


	rkit::Result GameSoundMP3Source::Initialize(rkit::mp3::IMP3Driver &mp3Driver)
	{
		RKIT_CHECK(mp3Driver.CreateDecoder(m_decoder));

		(void)GetCurrentAudioFrame();

		RKIT_RETURN_OK;
	}

	const AudioFrame *GameSoundMP3Source::GetCurrentAudioFrame()
	{
		if (!m_currentFrame.IsSet() && !m_exhausted)
		{
			bool haveDecodedFrame = false;

			if (m_dataSpan.Count() < rkit::mp3::kMaxBytesPerFrame)
			{
				rkit::Span<uint8_t> startSpan = m_dataStorage.ToSpan().SubSpan(0, m_dataSpan.Count());
				rkit::Span<uint8_t> remainderSpan = m_dataStorage.ToSpan().SubSpan(m_dataSpan.Count());

				rkit::CopySpan(startSpan, m_dataSpan);

				const size_t bytesAvailable = m_dataStorage.Count() - startSpan.Count();
				const size_t bytesRead = m_dataSource->ReadData(remainderSpan);

				m_dataSpan = m_dataStorage.ToSpan().SubSpan(0, startSpan.Count() + bytesRead);
			}

			if (m_dataSpan.Count() > 0)
			{
				haveDecodedFrame = m_decoder->DecodeFrame(m_samplesArray.GetBuffer(), m_decodedFrameInfo, m_dataSpan.Ptr(), m_dataSpan.Count());
				if (haveDecodedFrame)
					m_dataSpan = m_dataSpan.SubSpan(m_decodedFrameInfo.m_bytesConsumed);
			}

			if (haveDecodedFrame)
			{
				rkit::audio::AudioFormat audioFormat;
				audioFormat.m_sampleRate = m_decodedFrameInfo.m_sampleRate;
				audioFormat.m_sampleType = rkit::audio::SampleType::kSInt16;
				if (m_decodedFrameInfo.m_channels == 1)
					audioFormat.m_speakers.Set(rkit::audio::SpeakerPosition::kFrontCenter, true);
				else
				{
					audioFormat.m_speakers.Set(rkit::audio::SpeakerPosition::kFrontLeft, true);
					audioFormat.m_speakers.Set(rkit::audio::SpeakerPosition::kFrontRight, true);
				}

				if (audioFormat != m_audioFormat)
				{
					if (m_audioFormat.m_sampleRate == 0)
						m_audioFormat = audioFormat;
					else
						haveDecodedFrame = false;
				}
			}

			if (haveDecodedFrame)
			{
				AudioFrame frame;
				frame.m_data = m_samplesArray.GetBuffer();
				frame.m_numSamples = m_decodedFrameInfo.m_samplesProduced;

				m_currentFrame = frame;
			}

			m_exhausted = !haveDecodedFrame;
		}

		if (m_currentFrame.IsSet())
			return &m_currentFrame.Get();
		else
			return nullptr;
	}

	rkit::audio::AudioFormat GameSoundMP3Source::GetAudioFormat() const
	{
		return m_audioFormat;
	}

	void GameSoundMP3Source::DiscardAudioFrame()
	{
		m_currentFrame.Reset();
	}

	GameAudioManagerImpl::AudioEmitterDisposer::AudioEmitterDisposer(GameAudioManagerImpl &owner)
		: m_owner(owner)
	{
	}

	void GameAudioManagerImpl::AudioEmitterDisposer::DisposeObject(const AudioEmitterState &emitterState) const
	{
		if (emitterState.m_mixerEmitter)
			m_owner.m_audioSubsystem.DestroyEmitter(emitterState.m_mixerEmitter);
	}

	GameAudioManagerImpl::GameAudioManagerImpl(AudioSubsystem &audioSubsystem)
		: m_audioSubsystem(audioSubsystem)
		, m_emitters(AudioEmitterDisposer(*this))
	{
	}

	rkit::Result GameAudioManagerImpl::Initialize()
	{
		if (!rkit::GetDrivers().m_moduleDriver->LoadModule(rkit::IModuleDriver::kDefaultNamespace, u8"MP3"))
		{
			rkit::log::Error(u8"MP3 module missing");
			RKIT_THROW(rkit::ResultCode::kModuleLoadFailed);
		}

		rkit::ICustomDriver *mp3Driver = rkit::GetDrivers().FindDriver(rkit::IModuleDriver::kDefaultNamespace, u8"MP3");

		if (!mp3Driver)
		{
			rkit::log::Error(u8"MP3 driver failed to load");
			RKIT_THROW(rkit::ResultCode::kModuleLoadFailed);
		}

		m_mp3Driver = static_cast<rkit::mp3::IMP3Driver *>(mp3Driver);

		RKIT_RETURN_OK;
	}

	rkit::Result GameAudioManagerImpl::CreateSoundSourceFromBytes(uint32_t &outSourceID, rkit::TypelessRCPtr &&keepalive, rkit::Span<const uint8_t> contents, rkit::audio::AudioContainerFormat containerFormat)
	{
		rkit::UniquePtr<GameSoundDataSource> dataSrc;
		rkit::New<GameSoundBytesDataSource>(dataSrc, std::move(keepalive), contents);

		return CreateAudioSourceFromDataSource(outSourceID, std::move(dataSrc), containerFormat);
	}

	rkit::Result GameAudioManagerImpl::CreateAudioSourceFromDataSource(uint32_t &outSourceID, rkit::UniquePtr<GameSoundDataSource> dataSource, rkit::audio::AudioContainerFormat containerFormat)
	{
		rkit::audio::AudioFileDataFormat dataFormat = rkit::audio::AudioFileDataFormat::kInvalid;

		rkit::RCPtr<IAudioSource> src;
		switch (containerFormat)
		{
		case rkit::audio::AudioContainerFormat::kMPEGLayer3:
			{
				rkit::RCPtr<GameSoundMP3Source> mp3Src;
				RKIT_CHECK(rkit::New<GameSoundMP3Source>(mp3Src, std::move(dataSource)));
				RKIT_CHECK(mp3Src->Initialize(*m_mp3Driver));

				src = std::move(mp3Src);
			}
			break;
		default:
			RKIT_THROW(rkit::ResultCode::kInvalidParameter);
		}

		return m_sources.RegisterObject(outSourceID, std::move(src));
	}

	void GameAudioManagerImpl::DestroySoundSource(uint32_t sourceID)
	{
		m_sources.DestroyObject(sourceID);
	}

	rkit::Result GameAudioManagerImpl::CreateEmitterFromSource(uint32_t &outEmitterID, uint32_t sourceID, const SoundEmitterProperties &emitterProperties)
	{
		rkit::RCPtr<IAudioSource> *source = m_sources.TryGetObject(sourceID);
		if (!source)
			RKIT_THROW(rkit::ResultCode::kInvalidParameter);

		AudioEmitterState emitterState;
		emitterState.m_audioSource = *source;

		uint32_t emitterID = 0;
		RKIT_CHECK(m_emitters.RegisterObject(emitterID, std::move(emitterState)));

		// If this succeeds, the source ID is invalidated, so this must only happen on complete success
		m_sources.DestroyObject(sourceID);
		outEmitterID = emitterID;

		RKIT_RETURN_OK;
	}

	void GameAudioManagerImpl::DestroyEmitter(uint32_t emitterID)
	{
		m_emitters.DestroyObject(emitterID);
	}

	rkit::Result GameAudioManagerImpl::PlayEmitter(uint32_t emitterID)
	{
		AudioEmitterState *emitterState = m_emitters.TryGetObject(emitterID);
		if (!emitterState)
			return;

		if (emitterState->m_mixerEmitter == nullptr)
		{
			RKIT_CHECK(m_audioSubsystem.CreateEmitter(emitterState->m_mixerEmitter, emitterState->m_audioSource));
		}

		RKIT_CHECK(m_audioSubsystem.PlayEmitter(emitterState->m_mixerEmitter));

		RKIT_RETURN_OK;
	}

	GameAudioManager::GameAudioManager(AudioSubsystem &audioSubsystem)
		: rkit::Opaque<GameAudioManagerImpl>(audioSubsystem)
	{
	}

	rkit::Result GameAudioManager::CreateSoundSourceFromBytes(uint32_t &outSourceID, rkit::TypelessRCPtr &&keepalive, rkit::Span<const uint8_t> contents, rkit::audio::AudioContainerFormat containerFormat)
	{
		return Impl().CreateSoundSourceFromBytes(outSourceID, std::move(keepalive), contents, containerFormat);
	}

	void GameAudioManager::DestroySoundSource(uint32_t sourceID)
	{
		return Impl().DestroySoundSource(sourceID);
	}

	rkit::Result GameAudioManager::CreateEmitterFromSource(uint32_t &outEmitterID, uint32_t sourceID, const SoundEmitterProperties &emitterProperties)
	{
		return Impl().CreateEmitterFromSource(outEmitterID, sourceID, emitterProperties);
	}

	void GameAudioManager::DestroyEmitter(uint32_t emitterID)
	{
		Impl().DestroyEmitter(emitterID);
	}

	rkit::Result GameAudioManager::PlayEmitter(uint32_t emitterID)
	{
		return Impl().PlayEmitter(emitterID);
	}

	rkit::Result GameAudioManager::Create(rkit::UniquePtr<GameAudioManager> &outAudioManager, AudioSubsystem& audioSubsystem)
	{
		rkit::UniquePtr<GameAudioManager> audioManager;
		RKIT_CHECK(rkit::New<GameAudioManager>(audioManager, audioSubsystem));

		RKIT_CHECK(audioManager->Impl().Initialize());

		outAudioManager = std::move(audioManager);

		RKIT_RETURN_OK;
	}
}

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(anox::game::GameAudioManagerImpl)
