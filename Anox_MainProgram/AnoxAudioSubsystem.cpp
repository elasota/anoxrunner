#include "AnoxAudioSubsystem.h"

#include "rkit/Audio/AudioDriver.h"

#include "rkit/Core/Event.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/MutexLock.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Platform.h"
#include "rkit/Core/RefCounted.h"
#include "rkit/Core/SystemDriver.h"

#include "rkit/Core/JobQueue.h"

#include "rkit/Core/Mutex.h"

namespace anox::priv
{
	template<rkit::audio::SampleType TSampleType>
	struct SampleTypeDataResolver
	{
	};

	template<>
	struct SampleTypeDataResolver<rkit::audio::SampleType::kFloat32>
	{
		typedef float Type_t;
	};

	template<>
	struct SampleTypeDataResolver<rkit::audio::SampleType::kSInt32>
	{
		typedef int32_t Type_t;
	};

	template<>
	struct SampleTypeDataResolver<rkit::audio::SampleType::kSInt32_24bit>
	{
		typedef int32_t Type_t;
	};

	template<>
	struct SampleTypeDataResolver<rkit::audio::SampleType::kSInt16>
	{
		typedef int16_t Type_t;
	};
}

namespace anox
{
	template<rkit::audio::SampleType TSampleType>
	using AudioSampleData_t = anox::priv::SampleTypeDataResolver<TSampleType>::Type_t;

	using AudioVolumeLevel = uint16_t;

	constexpr uint8_t ConstAudioSampleTypeSize(rkit::audio::SampleType sampleType)
	{
		switch (sampleType)
		{
		case rkit::audio::SampleType::kFloat32:
		case rkit::audio::SampleType::kSInt32:
		case rkit::audio::SampleType::kSInt32_24bit:
			return 4;
		case rkit::audio::SampleType::kSInt16:
			return 2;
		//case rkit::audio::SampleType::kSInt8:
		//	return 1;
		default:
			return 0;
		}
	}

	uint8_t AudioSampleTypeSize(rkit::audio::SampleType sampleType)
	{
		const uint8_t size = ConstAudioSampleTypeSize(sampleType);
		RKIT_ASSERT(size != 0);
		return size;
	}

#if RKIT_PLATFORM_ARCH_HAVE_SSE2 != 0
	template<rkit::audio::SampleType TSampleType>
	struct AudioSSESampleUtils
	{
	};

	template<>
	struct AudioSSESampleUtils< rkit::audio::SampleType::kSInt32>
	{
		typedef int32_t PlainType_t;
		typedef __m128i RegisterType_t;
		static constexpr uint8_t kSamplesPerRegister = 4;

		static void Deinterleave(RegisterType_t(&output)[2], const RegisterType_t(&input)[2]);

		static RegisterType_t Load(const PlainType_t *src);
		static void Store(PlainType_t *dest, RegisterType_t value);
	};

	template<>
	struct AudioSSESampleUtils< rkit::audio::SampleType::kSInt32_24bit>
	{
		typedef int32_t PlainType_t;
		typedef __m128i RegisterType_t;
		static constexpr uint8_t kSamplesPerRegister = 4;

		static void Deinterleave(RegisterType_t(&output)[2], const RegisterType_t(&input)[2]);

		static RegisterType_t Load(const PlainType_t *src);
		static void Store(PlainType_t *dest, RegisterType_t value);
	};

	template<>
	struct AudioSSESampleUtils< rkit::audio::SampleType::kSInt16>
	{
		typedef int16_t PlainType_t;
		typedef __m128i RegisterType_t;
		static constexpr uint8_t kSamplesPerRegister = 8;

		static void Deinterleave(RegisterType_t(&output)[2], const RegisterType_t(&input)[2]);

		static RegisterType_t Load(const PlainType_t *src);
		static void Store(PlainType_t *dest, RegisterType_t value);
	};

	/*
	template<>
	struct AudioSSESampleUtils< rkit::audio::SampleType::kSInt8>
	{
		typedef int8_t PlainType_t;
		typedef __m128i RegisterType_t;
		static constexpr uint8_t kSamplesPerRegister = 16;

		static void Deinterleave(RegisterType_t(&output)[2], const RegisterType_t(&input)[2]);

		static RegisterType_t Load(const PlainType_t *src);
		static void Store(PlainType_t *dest, RegisterType_t value);
	};
	*/

	template<>
	struct AudioSSESampleUtils< rkit::audio::SampleType::kFloat32>
	{
		typedef float PlainType_t;
		typedef __m128 RegisterType_t;
		static constexpr uint8_t kSamplesPerRegister = 4;

		static void Deinterleave(RegisterType_t(&output)[2], const RegisterType_t(&input)[2]);

		static RegisterType_t Load(const PlainType_t *src);
		static void Store(PlainType_t *dest, RegisterType_t value);
	};

	template<rkit::audio::SampleType TDestType, rkit::audio::SampleType TSrcType>
	struct AudioSIMDDeinterleaver
	{
		static size_t Deinterleave(void *dest, int outChannelOffsetBits, const void *src);

	private:
		template<unsigned int TChannelCount>
		static size_t DeinterleaveInternal(void *dest, int outChannelOffsetBits, const void *src);
	};
#else
	template<rkit::audio::SampleType TDestType, rkit::audio::SampleType TSrcType>
	struct AudioSIMDDeinterleaver
	{
		static size_t Deinterleave(void *dest, int outChannelOffsetBits, const void *src) { return 0; }
	};
#endif

	template<rkit::audio::SampleType TDestSampleType>
	struct AudioSampleMixer
	{
	};

	template<class TSingle, class TDouble, TSingle TMin, TSingle TMax>
	struct AudioSampleIntMixer
	{
		typedef int32_t ConstantVolumeScaler_t;

		static int32_t ComputeConstantVolumeScaler(AudioVolumeLevel volumeLevel)
		{
			return volumeLevel;
		}

		static void MixUnscaled(TSingle &dest, TDouble src)
		{
			dest = static_cast<TSingle>(rkit::Max<TDouble>(rkit::Min<TDouble>(dest + src, TMax), TMin));
		}

		static void MixScaled(TSingle &dest, TSingle src, ConstantVolumeScaler_t volume)
		{
			const TDouble scaled = (static_cast<TDouble>(src) * volume) >> 16;

			MixUnscaled(dest, scaled);
		}
	};

	template<>
	struct AudioSampleMixer<rkit::audio::SampleType::kSInt32>
		: public AudioSampleIntMixer<int32_t, int64_t, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()>
	{
	};

	template<>
	struct AudioSampleMixer<rkit::audio::SampleType::kSInt32_24bit>
		: public AudioSampleIntMixer<int32_t, int64_t, (1 << 24) - 1, (1 << 24) - 1>
	{
	};

	template<>
	struct AudioSampleMixer<rkit::audio::SampleType::kSInt16>
		: public AudioSampleIntMixer<int16_t, int32_t, std::numeric_limits<int16_t>::min(), std::numeric_limits<int16_t>::max()>
	{
	};

	template<>
	struct AudioSampleMixer<rkit::audio::SampleType::kFloat32>
	{
		typedef float ConstantVolumeScaler_t;

		static float ComputeConstantVolumeScaler(AudioVolumeLevel volumeLevel)
		{
			return static_cast<float>(volumeLevel) * static_cast<float>(1.0f / static_cast<float>(std::numeric_limits<AudioVolumeLevel>::max()));;
		}

		static void MixUnscaled(float &dest, float src)
		{
			dest += src;
		}

		static void MixScaled(float &dest, float src, float volume)
		{
			dest += src * volume;
		}
	};

	template<rkit::audio::SampleType TDestSampleType, rkit::audio::SampleType TSrcSampleType>
	struct AudioSampleTranscoder
	{
	};

	template<>
	struct AudioSampleTranscoder<rkit::audio::SampleType::kSInt32, rkit::audio::SampleType::kSInt32_24bit>
	{
		static void Transcode(int32_t &dest, int32_t src)
		{
			dest = src << 8;
		}
	};

	template<>
	struct AudioSampleTranscoder<rkit::audio::SampleType::kSInt32, rkit::audio::SampleType::kSInt16>
	{
		static void Transcode(int32_t &dest, int16_t src)
		{
			dest = src << 16;
		}
	};

	template<>
	struct AudioSampleTranscoder<rkit::audio::SampleType::kSInt32, rkit::audio::SampleType::kFloat32>
	{
		static void Transcode(int32_t &dest, float src)
		{
			if (src < -1.0f)
				dest = std::numeric_limits<int32_t>::min();
			else if (src > 1.0f)
				dest = std::numeric_limits<int32_t>::max();
			else
				dest = static_cast<int32_t>(src * static_cast<float>(std::numeric_limits<int32_t>::max()));
		}
	};

	template<>
	struct AudioSampleTranscoder<rkit::audio::SampleType::kSInt32_24bit, rkit::audio::SampleType::kSInt32>
	{
		static void Transcode(int32_t &dest, int32_t src)
		{
			dest = src >> 8;
		}
	};

	template<>
	struct AudioSampleTranscoder<rkit::audio::SampleType::kSInt32_24bit, rkit::audio::SampleType::kSInt16>
	{
		static void Transcode(int32_t &dest, int16_t src)
		{
			dest = src << 8;
		}
	};

	template<>
	struct AudioSampleTranscoder<rkit::audio::SampleType::kSInt32_24bit, rkit::audio::SampleType::kFloat32>
	{
		static void Transcode(int32_t &dest, float src)
		{
			if (src < -1.0f)
				dest = (1 << 24) - 1;
			else if (src > 1.0f)
				dest = 0 - (1 << 24);
			else
				dest = static_cast<int32_t>(src * static_cast<float>((1 << 24) - 1));
		}
	};

	template<>
	struct AudioSampleTranscoder<rkit::audio::SampleType::kSInt16, rkit::audio::SampleType::kSInt32>
	{
		static void Transcode(int16_t &dest, int32_t src)
		{
			dest = src >> 16;
		}
	};

	template<>
	struct AudioSampleTranscoder<rkit::audio::SampleType::kSInt16, rkit::audio::SampleType::kSInt32_24bit>
	{
		static void Transcode(int16_t &dest, int32_t src)
		{
			dest = src >> 8;
		}
	};

	template<>
	struct AudioSampleTranscoder<rkit::audio::SampleType::kSInt16, rkit::audio::SampleType::kFloat32>
	{
		static void Transcode(int16_t &dest, float src)
		{
			if (src < -1.0f)
				dest = (1 << 16) - 1;
			else if (src > 1.0f)
				dest = 0 - (1 << 16);
			else
				dest = static_cast<int16_t>(src * static_cast<float>((1 << 16) - 1));
		}
	};


	template<>
	struct AudioSampleTranscoder<rkit::audio::SampleType::kFloat32, rkit::audio::SampleType::kSInt32>
	{
		static void Transcode(float &dest, int32_t src)
		{
			dest = static_cast<float>(src) * static_cast<float>(1.0f / static_cast<float>(std::numeric_limits<int32_t>::max()));
		}
	};

	template<>
	struct AudioSampleTranscoder<rkit::audio::SampleType::kFloat32, rkit::audio::SampleType::kSInt32_24bit>
	{
		static void Transcode(float &dest, int32_t src)
		{
			dest = static_cast<float>(src) * static_cast<float>(1.0f / static_cast<float>((1 << 24) - 1));
		}
	};

	template<>
	struct AudioSampleTranscoder<rkit::audio::SampleType::kFloat32, rkit::audio::SampleType::kSInt16>
	{
		static void Transcode(float &dest, float src)
		{
			dest = static_cast<float>(src) * static_cast<float>(1.0f / static_cast<float>(std::numeric_limits<int16_t>::max()));
		}
	};

	class AudioGarbageCollector;
	class AudioMixerEmitter;

	template<class T, bool THasPrev>
	class AudioRCListLink;

	template<class T, bool THasPrev>
	class AudioRCListDefaultLinkResolver
	{
	public:
		static AudioRCListLink<T, THasPrev> &GetRCLink(T &object);
	};

	template<class T, bool THasPrev, class TResolver>
	class AudioRCListBidirectionalBase
	{
	};

	template<class T, class TResolver>
	class AudioRCListBidirectionalBase<T, true, TResolver>
	{
	public:
		RKIT_NODISCARD rkit::RCPtr<T> DetachItem(T *item);
		void RemoveItem(T *item);

		RKIT_NODISCARD rkit::RCPtr<T> DetachLast();
		void RemoveLast();
	};

	template<class T, bool THasPrev, class TResolver = AudioRCListDefaultLinkResolver<T, THasPrev>>
	class AudioRCList : public AudioRCListBidirectionalBase<T, THasPrev, TResolver>
	{
	public:
		template<class T, bool THasPrev, class TList>
		friend class AudioRCListBidirectionalBase;

		AudioRCList() = default;
		AudioRCList(const AudioRCList &other) = delete;
		AudioRCList(AudioRCList &&other) noexcept;
		~AudioRCList();

		AudioRCList &operator=(const AudioRCList &other) = delete;
		AudioRCList &operator=(AudioRCList &&other) noexcept;

		RKIT_NODISCARD static AudioRCList ItemToRCList(rkit::RCPtr<T> &&rcPtr);

		RKIT_NODISCARD T *GetFirst() const;
		RKIT_NODISCARD T *GetLast() const;

		RKIT_NODISCARD rkit::RCPtr<T> DetachFirst();
		void RemoveFirst();

		void Append(rkit::RCPtr<T> &&other);
		void Prepend(rkit::RCPtr<T> &&other);

		void Append(AudioRCList &&other);
		void Prepend(AudioRCList &&other);

		void Clear();

	private:
		rkit::RCPtr<T> m_first;
		T *m_last = nullptr;
	};

	template<class T>
	using AudioRCDoubleList = AudioRCList<T, true>;

	template<class T>
	using AudioRCSingleList = AudioRCList<T, false>;

	template<class T, bool THasPrev>
	struct AudioRCListLinkBidirectionalBase
	{
	};

	template<class T>
	struct AudioRCListLinkBidirectionalBase<T, true>
	{
		T *m_prev = nullptr;
	};

	template<class T, bool THasPrev>
	struct AudioRCListLink : public AudioRCListLinkBidirectionalBase<T, THasPrev>
	{
		AudioRCListLink() = default;
		AudioRCListLink(const AudioRCListLink &) = delete;
		AudioRCListLink(AudioRCListLink &&) = delete;

		AudioRCListLink &operator=(const AudioRCListLink &) = delete;
		AudioRCListLink &operator=(AudioRCListLink &&) = delete;

		rkit::RCPtr<T> m_next;
	};

	template<class T>
	using AudioRCListSingleLink = AudioRCListLink<T, false>;

	template<class T>
	using AudioRCListDoubleLink = AudioRCListLink<T, true>;

	class AudioGarbageCollectable : public rkit::RefCounted
	{
	public:
		friend class AudioGarbageCollector;

		explicit AudioGarbageCollectable(AudioGarbageCollector &gc);

	private:
		AudioGarbageCollectable *DeleteAndGetNext();
		void RCTrackerZero() override;

		AudioGarbageCollector &m_gc;
		AudioGarbageCollectable *m_next = nullptr;
	};

	class AudioGarbageCollector
	{
	public:
		explicit AudioGarbageCollector(rkit::IJobQueue &jobQueue);
		~AudioGarbageCollector();

		rkit::Result Initialize();
		void Shutdown();

		void AddCollectableList(AudioGarbageCollectable &firstCollectable, AudioGarbageCollectable &lastCollectable);

		rkit::Result TryCreateGCJob(rkit::RCPtr<rkit::Job>& anOutJob);

	private:
		class GCJobRunner final : public rkit::IJobRunner
		{
		public:
			rkit::Result Run() override;

			void SetItems(AudioGarbageCollectable *items);

		private:
			AudioGarbageCollectable *m_items = nullptr;
		};

		rkit::IJobQueue &m_jobQueue;

		rkit::UniquePtr<rkit::IEvent> m_disposeWakeEvent;
		rkit::UniquePtr<rkit::IEvent> m_disposeTerminateEvent;
		rkit::RCPtr<rkit::Job> m_lastGCJob;

		std::atomic<AudioGarbageCollectable *> m_first;
	};

	enum class AudioCommandType
	{
		kInvalid,

		kPlayEmitter,
		kStopEmitter,
	};

	struct OpaqueEmitterCommand
	{
		AudioMixerEmitter *m_emitterPtr;
	};

	union AudioCommandWord
	{
		AudioCommandType m_cmdType;
		OpaqueEmitterCommand m_emitter;
		uint64_t m_uint64;
	};

	struct AudioCommandList final : public AudioGarbageCollectable
	{
	public:
		static constexpr size_t kNumRequiredParams = 1;

		explicit AudioCommandList(AudioGarbageCollector &gc);
		~AudioCommandList();

		AudioCommandWord *TryAllocCommandWords(size_t numWords);

		rkit::ConstSpan<AudioCommandWord> GetCommands() const;

		AudioRCListSingleLink<AudioCommandList> &GetRCLink();

	private:
		static constexpr size_t kMaxCommandWords = 256;

		rkit::StaticArray<AudioCommandWord, kMaxCommandWords> m_commands;
		size_t m_numCommandWords = 0;

		AudioRCListSingleLink<AudioCommandList> m_rcLink;
	};

	constexpr AudioVolumeLevel kAudioMaxVolume = std::numeric_limits<uint16_t>::max();

	class AudioChannelMap
	{
	public:
		AudioChannelMap();

		rkit::Span<AudioVolumeLevel> GetOutputMapForSourceChannel(size_t sourceChannelIndex);
		rkit::Span<const AudioVolumeLevel> GetOutputMapForSourceChannel(size_t sourceChannelIndex) const;

	public:
		typedef rkit::StaticArray<AudioVolumeLevel, static_cast<size_t>(rkit::audio::SpeakerPosition::kCount)> OutputMap_t;
		typedef rkit::StaticArray<OutputMap_t, IAudioSource::kMaxSourceChannels> ChannelMap_t;

		ChannelMap_t m_channelMap;
	};

	class AudioMixerEmitter final : public AudioGarbageCollectable, public AudioEmitter
	{
	public:
		friend class AudioMixer;

		explicit AudioMixerEmitter(AudioGarbageCollector &gc, rkit::RCPtr<IAudioSource> audioSource);

		IAudioSource &GetAudioSource() const;

		AudioRCListDoubleLink<AudioMixerEmitter> &GetRCLink();

	private:
		static constexpr size_t kMaxSourceChannels = IAudioSource::kMaxSourceChannels;
		static constexpr size_t kMaxOutputChannels = static_cast<size_t>(rkit::audio::SpeakerPosition::kCount);

		rkit::RCPtr<IAudioSource> m_source;

		AudioRCListDoubleLink<AudioMixerEmitter> m_rcLink;

		AudioMixerEmitter *m_nextRemove = nullptr;
		bool m_isActive = false;

		rkit::Optional<AudioFrame> m_currentFrame;
		rkit::audio::AudioFormat m_currentFrameFormat;
		size_t m_frameReadOffset = 0;

		AudioChannelMap m_channelMap;
	};

	class AudioScratchBufferInstance
	{
	public:
		explicit AudioScratchBufferInstance(uint8_t *baseMem, int offsetShiftBits, uint32_t numChannels);

		void *GetChannelMem(size_t channel) const;
		int GetOffsetShiftBits() const;
		uint32_t GetNumChannels() const;

		rkit::Span<uint8_t> GetAllMem() const;

	private:
		uint8_t *m_baseMem = 0;
		uint32_t m_numChannels = 0;
		int m_offsetShiftBits = 0;
	};

	enum class AudioScratchBufferSet
	{
		kTempCount = 2,

		kMix = 0,
		kTemp0,

		kCount = kTemp0 + kTempCount,
	};

	class AudioScratchBuffers : public rkit::NoCopy
	{
	public:
		AudioScratchBuffers() = default;
		~AudioScratchBuffers();

		rkit::Result Allocate(size_t minimumCapacity, uint32_t numChannels);

		AudioScratchBufferInstance GetBufferSet(AudioScratchBufferSet bufferSet) const;

	private:
		static constexpr size_t kNumBufferSets = static_cast<size_t>(AudioScratchBufferSet::kCount);

		uint8_t *m_mem = nullptr;
		rkit::IMallocDriver *m_alloc = nullptr;
		int m_offsetShiftBits = 0;

		rkit::StaticArray<size_t, kNumBufferSets> m_bufferSetChannelOffset;
		rkit::StaticArray<uint32_t, kNumBufferSets> m_bufferSetChannelCounts;
	};

	class AudioMixer final : public rkit::audio::IAudioOutputRenderer
	{
	public:
		explicit AudioMixer(AudioGarbageCollector &gc);

		bool Render(void *buffer, size_t numSamples, const rkit::audio::IAudioOutputStateQuery &outputQuery) override;
		void RunTrailingActions() override;

		void RenderEmitter(const AudioScratchBufferInstance &outputBuffers, size_t numSamples, const rkit::audio::IAudioOutputStateQuery &outputQuery, AudioMixerEmitter *emitter);

		rkit::Result Initialize(rkit::audio::IAudioDriver *audioDriver);
		void Shutdown();

		rkit::Result SetOutputLayout(const rkit::audio::AudioFormat &audioFormat, size_t bufferCapacity);

		void AddEmitter(rkit::RCPtr<AudioMixerEmitter> emitter);
		void RemoveEmitter(AudioMixerEmitter* emitter);

		void Cmd_PlayEmitter(AudioMixerEmitter *emitter);
		void Cmd_StopEmitter(AudioMixerEmitter *emitter);

	private:
		struct AudioRenderThreadState;

		class TempBufferSetHandle
		{
		public:
			TempBufferSetHandle();
			TempBufferSetHandle(TempBufferSetHandle &&other) noexcept;
			TempBufferSetHandle(const TempBufferSetHandle &other) = delete;
			~TempBufferSetHandle();

			explicit TempBufferSetHandle(AudioRenderThreadState &owner, size_t tempIndex);

			TempBufferSetHandle &operator=(TempBufferSetHandle &&other) noexcept;
			TempBufferSetHandle &operator=(const TempBufferSetHandle &other) = delete;

			AudioScratchBufferInstance GetInstance() const;

			void Release();

		private:
			size_t m_tempIndex = 0;
			AudioRenderThreadState *m_owner = nullptr;
		};

		struct AudioRenderThreadState
		{
			AudioRCDoubleList<AudioMixerEmitter> m_activeEmitters;
			AudioRCDoubleList<AudioMixerEmitter> m_inactiveEmitters;

			AudioMixerEmitter *m_firstToRemove = nullptr;
			AudioMixerEmitter *m_lastToRemove = nullptr;

			rkit::audio::U64Fraction m_startTime;		// Denominator = Timestamp denominator
			rkit::audio::U64Fraction m_exhaustionTime;	// Denominator = Timestamp denominator * SampleRate

			AudioScratchBuffers m_scratchBuffers;

			static constexpr size_t kTempScratchBufferCount = static_cast<size_t>(AudioScratchBufferSet::kTempCount);
			rkit::StaticBoolArray<kTempScratchBufferCount> m_tempBufferUse;

			TempBufferSetHandle GetTempBuffer();
			void ReleaseTempBuffer(size_t tempIndex);
		};

		template<class TFunc>
		rkit::Result PostAudioCommand(AudioCommandType cmdType, size_t numParamWords, const TFunc &func);

		template<class TFunc>
		static void LambdaThunk(const void *func, AudioCommandWord *cmdWords);

		rkit::Result PostAudioCommandWithCallback(AudioCommandType cmdType, size_t numParamWords, const void *userdata, void (*invoker)(const void *, AudioCommandWord *));

		static AudioVolumeLevel ComputeChannelVolume(const AudioMixerEmitter *emitter, rkit::audio::SpeakerPosition outPosition, rkit::audio::SpeakerPosition inPosition);

		static void DeinterleaveAndResample(size_t &outDestSamplesEmitted, size_t &outSourceSamplesConsumed,
			const AudioScratchBufferInstance &outBuffers,
			const void *inData,
			size_t inDataSamplesCount, size_t outDataSamplesCount,
			const rkit::audio::AudioFormat &destFormat, const rkit::audio::AudioFormat &sourceFormat);

		static void Deinterleave(const AudioScratchBufferInstance &outBuffers,
			const void *inData,
			size_t sampleCount,
			rkit::audio::SampleType destSampleType, rkit::audio::SampleType srcSampleType,
			uint32_t channelCount);

		template<rkit::audio::SampleType TDestSampleType>
		static void DeinterleaveTo(const AudioScratchBufferInstance &outBuffers,
			const void *inData,
			size_t sampleCount,
			rkit::audio::SampleType srcSampleType,
			uint32_t channelCount);

		template<rkit::audio::SampleType TDestSampleType, rkit::audio::SampleType TSrcSampleType>
		static void DeinterleaveFrom(const AudioScratchBufferInstance &outBuffers,
			const void *inData,
			size_t sampleCount,
			uint32_t channelCount);

		static void Mix(void *outMem, const void *inMem, rkit::audio::SampleType inSampleType,
			size_t numSamples, AudioVolumeLevel oldVolume, AudioVolumeLevel newVolume);

		template<rkit::audio::SampleType TSampleType>
		static void MixSpecialized(void *outMem, const void *inMem,
			size_t numSamples, AudioVolumeLevel oldVolume, AudioVolumeLevel newVolume);

		template<rkit::audio::SampleType TSampleType>
		static void MixConstantVolume(void *outMem, const void *inMem,
			size_t numSamples, AudioVolumeLevel volume);

		template<rkit::audio::SampleType TSampleType>
		static void MixFullVolume(void *outMem, const void *inMem, size_t numSamples);

		RKIT_NODISCARD static OpaqueEmitterCommand WriteEmitter(AudioMixerEmitter *emitter);
		RKIT_NODISCARD static AudioMixerEmitter *ReadEmitter(const OpaqueEmitterCommand &cmdWord);

		void ExecuteCommandLists(const rkit::audio::U64Fraction &timestamp);
		void FlushRemovals();

		size_t ExecuteCommand(AudioCommandType cmdType, const AudioCommandWord *params);

		rkit::audio::AudioFormat m_audioFormat;
		size_t m_bufferCapacity = 0;

		AudioGarbageCollector &m_gc;
		rkit::audio::IAudioDriver *m_audioDriver = nullptr;

		// RENDER THREAD ONLY
		AudioRenderThreadState m_renderThreadState;

		// Command lists and add/remove lists
		rkit::UniquePtr<rkit::IMutex> m_cmdListMutex;

		AudioRCDoubleList<AudioMixerEmitter> m_addEmitters;

		AudioMixerEmitter *m_firstRemoveEmitter = nullptr;
		AudioMixerEmitter *m_lastRemoveEmitter = nullptr;

		AudioRCSingleList<AudioCommandList> m_cmdLists;
	};

	class AudioSubsystemImpl final : public rkit::OpaqueImplementation<AudioSubsystem>
	{
	public:
		friend class AudioSubsystem;

		explicit AudioSubsystemImpl(rkit::IJobQueue& jobQueue);
		~AudioSubsystemImpl();

		rkit::Result Initialize();

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

		AudioGarbageCollector m_gc;
		AudioMixer m_mixer;
	};

	/////////////////////////////////////////////////////////////////////////////
	template<class T, bool THasPrev>
	AudioRCListLink<T, THasPrev> &AudioRCListDefaultLinkResolver<T, THasPrev>::GetRCLink(T &object)
	{
		return object.GetRCLink();
	}

	template<class T, class TResolver>
	rkit::RCPtr<T> AudioRCListBidirectionalBase<T, true, TResolver>::DetachItem(T *item)
	{
		RKIT_ASSERT(item != nullptr);

		AudioRCListLink<T, true> &rcLink = TResolver::GetRCLink(*item);
		AudioRCList<T, true, TResolver> *derived = static_cast<AudioRCList<T, true, TResolver> *>(this);

		rkit::RCPtr<T> next = std::move(rcLink.m_next);
		T *prev = rcLink.m_prev;

		if (next.IsValid())
		{
			AudioRCListLink<T, true> &nextRCLink = TResolver::GetRCLink(*next);
			nextRCLink.m_prev = prev;
		}
		else
		{
			RKIT_ASSERT(derived->m_last == item);
			derived->m_last = prev;
		}

		rkit::RCPtr<T> result;
		if (prev == nullptr)
		{
			RKIT_ASSERT(derived->m_first.Get() == item);

			result = std::move(derived->m_first);
			derived->m_first = std::move(next);
		}
		else
		{
			AudioRCListLink<T, true> &prevRCLink = TResolver::GetRCLink(*prev);

			result = std::move(prevRCLink.m_next);
			prevRCLink.m_next = std::move(next);
		}

		return result;
	}

	template<class T, class TResolver>
	rkit::RCPtr<T> AudioRCListBidirectionalBase<T, true, TResolver>::DetachLast()
	{
		AudioRCList<T, true, TResolver> *derived = static_cast<AudioRCList<T, true, TResolver> *>(this);

		T *itemPtr = derived->m_last;

		T *prev = itemPtr->m_prev;
		if (prev == nullptr)
		{
			derived->m_last = nullptr;
			return std::move(derived->m_first);
		}

		rkit::RCPtr<T> rcItem = std::move(prev->m_next);
		derived->m_last = prev;

		return rcItem;
	}

	template<class T, class TResolver>
	void AudioRCListBidirectionalBase<T, true, TResolver>::RemoveLast()
	{
		(void) DetachLast();
	}

	template<class T, class TResolver>
	void AudioRCListBidirectionalBase<T, true, TResolver>::RemoveItem(T *item)
	{
		(void) this->DetachItem(item);
	}

	template<class T, bool THasPrev, class TResolver>
	AudioRCList<T, THasPrev, TResolver>::AudioRCList(AudioRCList &&other) noexcept
		: m_first(std::move(other.m_first))
		, m_last(other.m_last)
	{
		other.m_last = nullptr;
	}

	template<class T, bool THasPrev, class TResolver>
	AudioRCList<T, THasPrev, TResolver>::~AudioRCList()
	{
		Clear();
	}

	template<class T, bool THasPrev, class TResolver>
	AudioRCList<T, THasPrev, TResolver> &AudioRCList<T, THasPrev, TResolver>::operator=(AudioRCList &&other) noexcept
	{
		m_first = std::move(other.m_first);
		m_last = other.m_last;

		other.m_last = nullptr;

		return *this;
	}

	template<class T, bool THasPrev, class TResolver>
	AudioRCList<T, THasPrev, TResolver> AudioRCList<T, THasPrev, TResolver>::ItemToRCList(rkit::RCPtr<T> &&rcPtr)
	{
		AudioRCList<T, THasPrev, TResolver> result;
		result.m_last = rcPtr.Get();
		result.m_first = std::move(rcPtr);

		return result;
	}

	template<class T, bool THasPrev, class TResolver>
	T *AudioRCList<T, THasPrev, TResolver>::GetFirst() const
	{
		return m_first.Get();
	}

	template<class T, bool THasPrev, class TResolver>
	T *AudioRCList<T, THasPrev, TResolver>::GetLast() const
	{
		return m_last;
	}

	template<class T, bool THasPrev, class TResolver>
	rkit::RCPtr<T> AudioRCList<T, THasPrev, TResolver>::DetachFirst()
	{
		if (m_last == nullptr)
			return rkit::RCPtr<T>();

		if (m_first.Get() == m_last)
		{
			rkit::RCPtr<T> item = std::move(m_first);
			m_last = nullptr;
			return item;
		}

		rkit::RCPtr<T> item = std::move(m_first);

		AudioRCListLink<T, THasPrev> &itemLink = TResolver::GetRCLink(*item);

		rkit::RCPtr<T> next = std::move(itemLink.m_next);

		if (next.IsValid())
		{
			if constexpr (THasPrev)
			{
				AudioRCListLink<T, THasPrev> &nextRCLink = TResolver::GetRCLink(*next);
				nextRCLink->m_prev = nullptr;
			}
		}
		else
			m_last = nullptr;

		m_first = std::move(next);

		return item;
	}

	template<class T, bool THasPrev, class TResolver>
	void AudioRCList<T, THasPrev, TResolver>::RemoveFirst()
	{
		(void)DetachFirst();
	}

	template<class T, bool THasPrev, class TResolver>
	void AudioRCList<T, THasPrev, TResolver>::Append(rkit::RCPtr<T> &&other)
	{
		return Append(ItemToRCList(std::move(other)));
	}

	template<class T, bool THasPrev, class TResolver>
	void AudioRCList<T, THasPrev, TResolver>::Prepend(rkit::RCPtr<T> &&other)
	{
		return Append(ItemToRCList(std::move(other)));
	}

	template<class T, bool THasPrev, class TResolver>
	void AudioRCList<T, THasPrev, TResolver>::Append(AudioRCList &&other)
	{
		if (m_last == nullptr)
		{
			m_first = std::move(other.m_first);

			m_last = other.m_last;
			other.m_last = nullptr;
		}
		else if (other.m_last != nullptr)
		{
			AudioRCListLink<T, THasPrev> &lastRCLink = TResolver::GetRCLink(*m_last);

			rkit::RCPtr<T> otherFirst = std::move(other.m_first);

			if constexpr (THasPrev)
			{
				AudioRCListLink<T, THasPrev> &otherFirstRCLink = TResolver::GetRCLink(*otherFirst);
				otherFirstRCLink.m_prev = m_last;
			}

			lastRCLink.m_next = std::move(otherFirst);

			m_last = other.m_last;
			other.m_last = nullptr;
		}
	}

	template<class T, bool THasPrev, class TResolver>
	void AudioRCList<T, THasPrev, TResolver>::Prepend(AudioRCList &&other)
	{
		other.Append(std::move(*this));
		(*this) = std::move(other);
	}

	template<class T, bool THasPrev, class TResolver>
	void AudioRCList<T, THasPrev, TResolver>::Clear()
	{
		rkit::RCPtr<T> item = std::move(m_first);
		while (item.IsValid())
		{
			AudioRCListLink<T, THasPrev> &rcLink = TResolver::GetRCLink(*item.Get());
			rkit::RCPtr<T> next = std::move(rcLink.m_next);
			item = std::move(next);
		}

		m_last = nullptr;
	}

	AudioGarbageCollectable::AudioGarbageCollectable(AudioGarbageCollector &gc)
		: m_gc(gc)
		, m_next(nullptr)
	{
	}

	AudioGarbageCollectable *AudioGarbageCollectable::DeleteAndGetNext()
	{
		AudioGarbageCollectable *next = m_next;
		RefCounted::RCTrackerZero();

		return next;
	}

	void AudioGarbageCollectable::RCTrackerZero()
	{
		m_gc.AddCollectableList(*this, *this);
	}


	rkit::Result AudioGarbageCollector::GCJobRunner::Run()
	{
		AudioGarbageCollectable *item = m_items;

		while (item != nullptr)
			item = item->DeleteAndGetNext();

		RKIT_RETURN_OK;
	}

	void AudioGarbageCollector::GCJobRunner::SetItems(AudioGarbageCollectable *items)
	{
		m_items = items;
	}

	AudioGarbageCollector::AudioGarbageCollector(rkit::IJobQueue &jobQueue)
		: m_first(nullptr)
		, m_jobQueue(jobQueue)
	{
	}

	AudioGarbageCollector::~AudioGarbageCollector()
	{
		Shutdown();
	}

	rkit::Result AudioGarbageCollector::Initialize()
	{
		rkit::ISystemDriver &sys = *rkit::GetDrivers().m_systemDriver;

		RKIT_CHECK(sys.CreateEvent(m_disposeWakeEvent, true, false));
		RKIT_CHECK(sys.CreateEvent(m_disposeTerminateEvent, true, false));

		RKIT_RETURN_OK;
	}

	void AudioGarbageCollector::Shutdown()
	{
		// This may be called more than once!
		if (m_lastGCJob.IsValid())
		{
			m_jobQueue.WaitForJob(*m_lastGCJob, rkit::CallbackSpan<rkit::JobType, void *>(), m_disposeWakeEvent.Get(), m_disposeTerminateEvent.Get());
			m_lastGCJob.Reset();
		}

		AudioGarbageCollectable *collectable = m_first.load(std::memory_order_acquire);
		while (collectable)
			collectable = collectable->DeleteAndGetNext();

		m_first.store(nullptr, std::memory_order_relaxed);
	}

	void AudioGarbageCollector::AddCollectableList(AudioGarbageCollectable &firstCollectable, AudioGarbageCollectable &lastCollectable)
	{
		AudioGarbageCollectable *currentHead = m_first.load(std::memory_order_relaxed);

		for (;;)
		{
			lastCollectable.m_next = currentHead;

			if (m_first.compare_exchange_weak(currentHead, &firstCollectable, std::memory_order_release))
				break;
		}
	}

	rkit::Result AudioGarbageCollector::TryCreateGCJob(rkit::RCPtr<rkit::Job> &anOutJob)
	{
		rkit::UniquePtr<GCJobRunner> runner;
		RKIT_CHECK(rkit::New<GCJobRunner>(runner));

		AudioGarbageCollectable *firstCollectable = m_first.exchange(nullptr, std::memory_order_acquire);
		if (firstCollectable)
		{
			runner->SetItems(firstCollectable);
			RKIT_TRY_CATCH_RETHROW(m_jobQueue.CreateJob(&m_lastGCJob, rkit::JobType::kNormalPriority, std::move(runner), rkit::JobDependencyList(m_lastGCJob)),
				rkit::CatchContext([this, firstCollectable]
					{
						// If creating the job fails, requeue the items that were not deleted
						AudioGarbageCollectable *lastCollectable = firstCollectable;
						while (lastCollectable->m_next != nullptr)
							lastCollectable = lastCollectable->m_next;

						this->AddCollectableList(*firstCollectable, *lastCollectable);
					})
			);
		}

		anOutJob = m_lastGCJob;

		RKIT_RETURN_OK;
	}

	rkit::Result AudioMixer::Initialize(rkit::audio::IAudioDriver *audioDriver)
	{
		rkit::ISystemDriver &sys = *rkit::GetDrivers().m_systemDriver;

		RKIT_CHECK(sys.CreateMutex(m_cmdListMutex));

		m_audioDriver = audioDriver;

		RKIT_RETURN_OK;
	}

	void AudioMixer::Shutdown()
	{
		m_cmdLists.Clear();
		m_addEmitters.Clear();

		m_renderThreadState.m_activeEmitters.Clear();
		m_renderThreadState.m_inactiveEmitters.Clear();
	}

	AudioCommandList::AudioCommandList(AudioGarbageCollector &gc)
		: AudioGarbageCollectable(gc)
	{
	}

	AudioCommandList::~AudioCommandList()
	{
	}

	AudioCommandWord *AudioCommandList::TryAllocCommandWords(size_t numWords)
	{
		if (kMaxCommandWords - m_numCommandWords < numWords)
			return nullptr;

		AudioCommandWord *cmdWords = m_commands.GetBuffer() + m_numCommandWords;

		m_numCommandWords += numWords;

		return cmdWords;
	}

	rkit::ConstSpan<AudioCommandWord> AudioCommandList::GetCommands() const
	{
		return m_commands.ToSpan().SubSpan(0, m_numCommandWords);
	}

	AudioRCListSingleLink<AudioCommandList> &AudioCommandList::GetRCLink()
	{
		return m_rcLink;
	}

	AudioChannelMap::AudioChannelMap()
	{
		for (OutputMap_t &outputMap : m_channelMap)
		{
			for (AudioVolumeLevel &volumeLevel : outputMap)
				volumeLevel = 0;
		}
	}

	rkit::Span<AudioVolumeLevel> AudioChannelMap::GetOutputMapForSourceChannel(size_t sourceChannelIndex)
	{
		return m_channelMap[sourceChannelIndex].ToSpan();
	}

	rkit::Span<const AudioVolumeLevel> AudioChannelMap::GetOutputMapForSourceChannel(size_t sourceChannelIndex) const
	{
		return const_cast<AudioChannelMap *>(this)->GetOutputMapForSourceChannel(sourceChannelIndex);
	}

	AudioMixerEmitter::AudioMixerEmitter(AudioGarbageCollector &gc, rkit::RCPtr<IAudioSource> audioSource)
		: AudioGarbageCollectable(gc)
		, m_source(std::move(audioSource))
	{
	}

	IAudioSource &AudioMixerEmitter::GetAudioSource() const
	{
		return *m_source;
	}

	AudioRCListDoubleLink<AudioMixerEmitter> &AudioMixerEmitter::GetRCLink()
	{
		return m_rcLink;
	}

	AudioScratchBufferInstance::AudioScratchBufferInstance(uint8_t *baseMem, int offsetShiftBits, uint32_t numChannels)
		: m_baseMem(baseMem)
		, m_offsetShiftBits(offsetShiftBits)
		, m_numChannels(numChannels)
	{
	}

	rkit::Span<uint8_t> AudioScratchBufferInstance::GetAllMem() const
	{
		return rkit::Span<uint8_t>(m_baseMem, static_cast<size_t>(m_numChannels) << m_offsetShiftBits);
	}

	void *AudioScratchBufferInstance::GetChannelMem(size_t channel) const
	{
		return m_baseMem + (channel << m_offsetShiftBits);
	}

	int AudioScratchBufferInstance::GetOffsetShiftBits() const
	{
		return m_offsetShiftBits;
	}

	uint32_t AudioScratchBufferInstance::GetNumChannels() const
	{
		return m_numChannels;
	}

	AudioScratchBuffers::~AudioScratchBuffers()
	{
		if (m_mem != nullptr)
			m_alloc->Free(m_mem);
	}

	rkit::Result AudioScratchBuffers::Allocate(size_t minimumCapacity, uint32_t numChannels)
	{
		size_t scratchBufferChannelSize = rkit::Max<size_t>(minimumCapacity, rkit::Max<size_t>(RKIT_SIMD_MAX_WIDTH_BYTES, RKIT_SIMD_ALIGNMENT));

		RKIT_CHECK(rkit::SafeMul<size_t>(scratchBufferChannelSize, scratchBufferChannelSize, AudioSampleTypeSize(rkit::audio::SampleType::kLargestSampleType)));

		int scratchBufferChannelBits = rkit::FindHighestSetBit(scratchBufferChannelSize - 1u) + 1;

		const size_t tempSetChannelCount = rkit::Max<size_t>(numChannels, IAudioSource::kMaxSourceChannels);

		m_bufferSetChannelCounts[static_cast<size_t>(AudioScratchBufferSet::kMix)] = numChannels;
		for (size_t i = 0; i < static_cast<size_t>(AudioScratchBufferSet::kTempCount); i++)
			m_bufferSetChannelCounts[static_cast<size_t>(AudioScratchBufferSet::kTemp0) + i] = tempSetChannelCount;

		size_t totalChannels = 0;
		for (size_t i = 0; i < m_bufferSetChannelOffset.Count(); i++)
		{
			m_bufferSetChannelOffset[i] = totalChannels;
			totalChannels += m_bufferSetChannelCounts[i];
		}

		const size_t scratchBufferTotalSize = (totalChannels << scratchBufferChannelBits);

		rkit::IMallocDriver *alloc = rkit::GetDrivers().m_mallocDriver.Get();
		void *mem = alloc->Alloc(scratchBufferTotalSize);

		if (!mem)
			RKIT_THROW(rkit::ResultCode::kOutOfMemory);

		// FIXME: Delete this
		memset(mem, 0xbb, scratchBufferTotalSize);

		if (m_mem != nullptr)
			m_alloc->Free(m_mem);

		m_mem = static_cast<uint8_t *>(mem);
		m_alloc = alloc;
		m_offsetShiftBits = scratchBufferChannelBits;

		RKIT_RETURN_OK;
	}

	AudioScratchBufferInstance AudioScratchBuffers::GetBufferSet(AudioScratchBufferSet bufferSet) const
	{
		RKIT_ASSERT(bufferSet < AudioScratchBufferSet::kCount);
		const size_t bufferSetIndex = static_cast<size_t>(bufferSet);

		return AudioScratchBufferInstance(m_mem + (m_bufferSetChannelOffset[bufferSetIndex] << m_offsetShiftBits), m_offsetShiftBits, m_bufferSetChannelCounts[bufferSetIndex]);
	}

	AudioMixer::TempBufferSetHandle::TempBufferSetHandle()
		: m_owner(nullptr)
		, m_tempIndex(0)
	{
	}

	AudioMixer::TempBufferSetHandle::TempBufferSetHandle(TempBufferSetHandle &&other) noexcept
		: m_owner(other.m_owner)
		, m_tempIndex(other.m_tempIndex)
	{
		other.m_owner = nullptr;
		other.m_tempIndex = 0;
	}

	AudioMixer::TempBufferSetHandle::~TempBufferSetHandle()
	{
		if (m_owner)
			m_owner->ReleaseTempBuffer(m_tempIndex);
	}

	AudioMixer::TempBufferSetHandle::TempBufferSetHandle(AudioRenderThreadState &owner, size_t tempIndex)
		: m_owner(&owner)
		, m_tempIndex(tempIndex)
	{
	}

	AudioMixer::TempBufferSetHandle &AudioMixer::TempBufferSetHandle::operator=(TempBufferSetHandle &&other) noexcept
	{
		if (m_owner)
			m_owner->ReleaseTempBuffer(m_tempIndex);

		m_owner = other.m_owner;
		m_tempIndex = other.m_tempIndex;

		other.m_owner = nullptr;
		other.m_tempIndex = 0;

		return *this;
	}

	AudioScratchBufferInstance AudioMixer::TempBufferSetHandle::GetInstance() const
	{
		const AudioScratchBufferSet bufferSet = static_cast<AudioScratchBufferSet>(static_cast<size_t>(AudioScratchBufferSet::kTemp0) + m_tempIndex);

		RKIT_ASSERT(m_owner != nullptr);
		return m_owner->m_scratchBuffers.GetBufferSet(bufferSet);
	}

	void AudioMixer::TempBufferSetHandle::Release()
	{
		if (m_owner)
			m_owner->ReleaseTempBuffer(m_tempIndex);

		m_owner = nullptr;
		m_tempIndex = 0;
	}

	AudioMixer::TempBufferSetHandle AudioMixer::AudioRenderThreadState::GetTempBuffer()
	{
		size_t tempIndex = 0;
		if (!m_tempBufferUse.FindFirstClear(tempIndex))
		{
			RKIT_ASSERT(false);
		}

		return TempBufferSetHandle(*this, tempIndex);
	}

	void AudioMixer::AudioRenderThreadState::ReleaseTempBuffer(size_t tempIndex)
	{
		this->m_tempBufferUse.Set(tempIndex, false);
	}

	AudioMixer::AudioMixer(AudioGarbageCollector &gc)
		: m_gc(gc)
	{
	}

	bool AudioMixer::Render(void *buffer, size_t numSamples, const rkit::audio::IAudioOutputStateQuery &outputQuery)
	{
		rkit::audio::U64Fraction timestamp = outputQuery.GetTimestamp();

		bool exhausted = false;

		if (m_renderThreadState.m_startTime.m_numerator == 0)
		{
			m_renderThreadState.m_startTime = timestamp;
			exhausted = true;
		}
		else
		{
			RKIT_ASSERT(m_renderThreadState.m_startTime.m_denominator == timestamp.m_denominator);
			timestamp.m_numerator -= m_renderThreadState.m_startTime.m_numerator;
		}

		rkit::audio::U64Fraction factoredUpCurrentTime;
		factoredUpCurrentTime.m_numerator = timestamp.m_numerator * m_audioFormat.m_sampleRate;
		factoredUpCurrentTime.m_denominator = timestamp.m_denominator * m_audioFormat.m_sampleRate;

		if (factoredUpCurrentTime.m_numerator > m_renderThreadState.m_exhaustionTime.m_numerator)
			exhausted = true;

		rkit::audio::U64Fraction factoredUpRenderDuration;
		factoredUpRenderDuration.m_numerator = numSamples * timestamp.m_denominator;
		factoredUpRenderDuration.m_denominator = m_audioFormat.m_sampleRate * timestamp.m_denominator;

		if (exhausted)
			m_renderThreadState.m_exhaustionTime = factoredUpCurrentTime;

		rkit::audio::U64Fraction renderTime = m_renderThreadState.m_exhaustionTime;

		m_renderThreadState.m_exhaustionTime.m_numerator += factoredUpRenderDuration.m_numerator;

		ExecuteCommandLists(renderTime);

		const rkit::audio::SampleType mixSampleType = m_audioFormat.m_sampleType;

		bool haveAnything = false;
		{
			AudioMixerEmitter *emitter = m_renderThreadState.m_activeEmitters.GetFirst();

			if (emitter != nullptr)
			{
				haveAnything = true;

				const size_t numChannels = m_audioFormat.m_speakers.CountSetBits();

				const AudioScratchBufferInstance mixChannelBuffers = m_renderThreadState.m_scratchBuffers.GetBufferSet(AudioScratchBufferSet::kMix);

				for (size_t ch = 0; ch < mixChannelBuffers.GetNumChannels(); ch++)
				{
					const size_t zeroSize = numSamples * AudioSampleTypeSize(mixSampleType);
					RKIT_ASSERT(zeroSize < static_cast<size_t>(1) << mixChannelBuffers.GetOffsetShiftBits());

					void *mixChannelMem = mixChannelBuffers.GetChannelMem(ch);
					memset(mixChannelMem, 0, zeroSize);
				}

				while (emitter != nullptr)
				{
					AudioMixerEmitter *next = emitter->GetRCLink().m_next.Get();
					RenderEmitter(mixChannelBuffers, numSamples, outputQuery, emitter);
					emitter = next;
				}
			}
		}

		return haveAnything;
	}

	void AudioMixer::RunTrailingActions()
	{
		ExecuteCommandLists(m_renderThreadState.m_exhaustionTime);
		FlushRemovals();
	}

	void AudioMixer::RenderEmitter(const AudioScratchBufferInstance &outputBuffersRef, size_t numSamples, const rkit::audio::IAudioOutputStateQuery &outputQuery, AudioMixerEmitter *emitter)
	{
		const AudioScratchBufferInstance outputBuffers = outputBuffersRef;

		TempBufferSetHandle deinterleaveAndResampleHandle = m_renderThreadState.GetTempBuffer();

		const rkit::audio::AudioFormat sourceFormat = emitter->GetAudioSource().GetAudioFormat();
		rkit::audio::AudioFormat deinterleaveFormat = m_audioFormat;
		deinterleaveFormat.m_speakers = sourceFormat.m_speakers;

		const size_t sourceFrameSize = AudioSampleTypeSize(sourceFormat.m_sampleType) * sourceFormat.m_speakers.CountSetBits();

		const AudioScratchBufferInstance deinterleaveAndResampleBuffers = deinterleaveAndResampleHandle.GetInstance();

		size_t samplesProduced = 0;
		{
			while (samplesProduced < numSamples)
			{
				if (!emitter->m_currentFrame.IsSet())
				{
					const AudioFrame *framePtr = emitter->GetAudioSource().GetCurrentAudioFrame();
					if (framePtr == nullptr)
						break;

					emitter->m_currentFrame = *framePtr;
					emitter->m_frameReadOffset = 0;
				}

				const AudioFrame &frame = emitter->m_currentFrame.Get();
				const size_t sourceFrameRemaining = frame.m_numSamples - emitter->m_frameReadOffset;
				const size_t destRemaining = numSamples - samplesProduced;

				size_t sourceSamplesConsumed = 0;
				size_t destSamplesEmitted = 0;
				DeinterleaveAndResample(destSamplesEmitted, sourceSamplesConsumed, deinterleaveAndResampleBuffers,
					(static_cast<const uint8_t *>(frame.m_data) + emitter->m_frameReadOffset * sourceFrameSize),
					destRemaining, sourceFrameRemaining,
					deinterleaveFormat, sourceFormat);

				emitter->m_frameReadOffset += sourceSamplesConsumed;
				samplesProduced += destSamplesEmitted;

				RKIT_ASSERT(emitter->m_frameReadOffset <= frame.m_numSamples);

				if (emitter->m_frameReadOffset == frame.m_numSamples)
				{
					emitter->m_currentFrame.Reset();
					emitter->GetAudioSource().DiscardAudioFrame();
				}
			}

			RKIT_ASSERT(samplesProduced <= numSamples);
		}

		AudioChannelMap &channelMap = emitter->m_channelMap;

		{
			size_t inChannelIndex = 0;
			for (rkit::audio::SpeakerPosition inSpeakerPosition : deinterleaveFormat.m_speakers)
			{
				rkit::Span<AudioVolumeLevel> volumes = channelMap.GetOutputMapForSourceChannel(inChannelIndex);

				size_t outChannelIndex = 0;
				for (rkit::audio::SpeakerPosition outSpeakerPosition : m_audioFormat.m_speakers)
				{
					AudioVolumeLevel &volumeRef = volumes[outChannelIndex];

					const AudioVolumeLevel oldVolume = volumeRef;
					const AudioVolumeLevel newVolume = ComputeChannelVolume(emitter, outSpeakerPosition, inSpeakerPosition);

					if (oldVolume != 0 || newVolume != 0)
					{
						Mix(outputBuffers.GetChannelMem(outChannelIndex), deinterleaveAndResampleBuffers.GetChannelMem(inChannelIndex), deinterleaveFormat.m_sampleType,
							samplesProduced, oldVolume, newVolume);
					}

					volumeRef = newVolume;
					outChannelIndex++;
				}

				inChannelIndex++;
			}
		}
	}

	rkit::Result AudioMixer::SetOutputLayout(const rkit::audio::AudioFormat &audioFormat, size_t bufferCapacity)
	{
		m_audioFormat = audioFormat;
		m_bufferCapacity = bufferCapacity;

		const size_t channelBufferBytes = bufferCapacity * audioFormat.m_speakers.CountSetBits();

		RKIT_CHECK(m_renderThreadState.m_scratchBuffers.Allocate(channelBufferBytes, audioFormat.m_speakers.CountSetBits()));

		RKIT_RETURN_OK;
	}

	void AudioMixer::AddEmitter(rkit::RCPtr<AudioMixerEmitter> emitter)
	{
		rkit::MutexLock lock(*m_cmdListMutex);

		// New emitters are added to the start of the list
		m_addEmitters.Prepend(std::move(emitter));
	}

	void AudioMixer::RemoveEmitter(AudioMixerEmitter *emitter)
	{
		rkit::MutexLock lock(*m_cmdListMutex);

		// Dead emitters are removed in FIFO order
		if (m_lastRemoveEmitter == nullptr)
			m_firstRemoveEmitter = emitter;
		else
			m_lastRemoveEmitter->m_nextRemove = emitter;

		m_lastRemoveEmitter = emitter;
	}

	void AudioMixer::Cmd_PlayEmitter(AudioMixerEmitter *emitter)
	{
		RKIT_CHECK(
			PostAudioCommand(AudioCommandType::kPlayEmitter, 1, [emitter](AudioCommandWord *cmdWords)
				{
					cmdWords[0].m_emitter = WriteEmitter(emitter);
				})
		);
	}

	void AudioMixer::Cmd_StopEmitter(AudioMixerEmitter *emitter)
	{
		RKIT_CHECK(
			PostAudioCommand(AudioCommandType::kStopEmitter, 1, [emitter](AudioCommandWord *cmdWords)
				{
					cmdWords[0].m_emitter = WriteEmitter(emitter);
				})
		);
	}

	template<class TFunc>
	rkit::Result AudioMixer::PostAudioCommand(AudioCommandType cmdType, size_t numParamWords, const TFunc &func)
	{
		PostAudioCommandWithCallback(cmdType, numParamWords, &func, LambdaThunk<TFunc>);
	}

	template<class TFunc>
	void AudioMixer::LambdaThunk(const void *funcPtr, AudioCommandWord *cmdWords)
	{
		const TFunc &func = *static_cast<const TFunc *>(funcPtr);
		func(cmdWords);
	}

	rkit::Result AudioMixer::PostAudioCommandWithCallback(AudioCommandType cmdType, size_t numParamWords, const void *userdata, void (*invoker)(const void *, AudioCommandWord *))
	{
		auto setRequiredParams = [this, cmdType](AudioCommandWord *cmdWords)
			{
				cmdWords[0].m_cmdType = cmdType;
			};

		{
			rkit::MutexLock lock(*m_cmdListMutex);
			AudioCommandList *lastCmdList = m_cmdLists.GetLast();
			if (lastCmdList != nullptr)
			{
				AudioCommandWord *cmdWords = lastCmdList->TryAllocCommandWords(numParamWords + AudioCommandList::kNumRequiredParams);
				if (cmdWords)
				{
					setRequiredParams(cmdWords);

					invoker(userdata, cmdWords + AudioCommandList::kNumRequiredParams);
					RKIT_RETURN_OK;
				}
			}
		}

		rkit::RCPtr<AudioCommandList> cmdList;
		RKIT_CHECK(rkit::New<AudioCommandList>(cmdList, this->m_gc));
		AudioCommandWord *cmdWords = cmdList->TryAllocCommandWords(numParamWords + AudioCommandList::kNumRequiredParams);

		RKIT_ASSERT(cmdWords != nullptr);
		setRequiredParams(cmdWords);

		invoker(userdata, cmdWords + AudioCommandList::kNumRequiredParams);

		{
			rkit::MutexLock lock(*m_cmdListMutex);
			m_cmdLists.Append(std::move(cmdList));
		}

		RKIT_RETURN_OK;
	}

	AudioVolumeLevel AudioMixer::ComputeChannelVolume(const AudioMixerEmitter *emitter, rkit::audio::SpeakerPosition outPosition, rkit::audio::SpeakerPosition inPosition)
	{
		if (outPosition == rkit::audio::SpeakerPosition::kFrontLeft)
		{
			if (inPosition == rkit::audio::SpeakerPosition::kFrontCenter)
				return kAudioMaxVolume;
			if (inPosition == rkit::audio::SpeakerPosition::kFrontLeft)
				return kAudioMaxVolume;
		}
		if (outPosition == rkit::audio::SpeakerPosition::kFrontRight)
		{
			if (inPosition == rkit::audio::SpeakerPosition::kFrontCenter)
				return kAudioMaxVolume;
			if (inPosition == rkit::audio::SpeakerPosition::kFrontRight)
				return kAudioMaxVolume;
		}

		return 0;
	}

	void AudioMixer::DeinterleaveAndResample(size_t &outDestSamplesEmitted, size_t &outSourceSamplesConsumed,
		const AudioScratchBufferInstance &outBuffers,
		const void *inData,
		size_t inDataSamplesCount, size_t outDataSamplesCount,
		const rkit::audio::AudioFormat &destFormat, const rkit::audio::AudioFormat &sourceFormat)
	{
		RKIT_ASSERT(sourceFormat.m_speakers == destFormat.m_speakers);

		if (sourceFormat.m_sampleRate == destFormat.m_sampleRate)
		{
			const size_t sampleCount = rkit::Min(inDataSamplesCount, outDataSamplesCount);

			Deinterleave(outBuffers, inData, sampleCount, destFormat.m_sampleType, sourceFormat.m_sampleType, destFormat.m_speakers.CountSetBits());

			outDestSamplesEmitted = sampleCount;
			outSourceSamplesConsumed = sampleCount;
		}
	}

	void AudioMixer::Deinterleave(const AudioScratchBufferInstance &outBuffers,
		const void *inData,
		size_t sampleCount,
		rkit::audio::SampleType destSampleType, rkit::audio::SampleType srcSampleType,
		uint32_t channelCount)
	{
		if (destSampleType == srcSampleType && channelCount == 1)
			memcpy(outBuffers.GetChannelMem(0), inData, AudioSampleTypeSize(destSampleType));
		else
		{
			switch (destSampleType)
			{
			case rkit::audio::SampleType::kSInt32:
				DeinterleaveTo<rkit::audio::SampleType::kSInt32>(outBuffers, inData, sampleCount, srcSampleType, channelCount);
				break;
			case rkit::audio::SampleType::kSInt32_24bit:
				DeinterleaveTo<rkit::audio::SampleType::kSInt32_24bit>(outBuffers, inData, sampleCount, srcSampleType, channelCount);
				break;
			case rkit::audio::SampleType::kSInt16:
				DeinterleaveTo<rkit::audio::SampleType::kSInt16>(outBuffers, inData, sampleCount, srcSampleType, channelCount);
				break;
			case rkit::audio::SampleType::kFloat32:
				DeinterleaveTo<rkit::audio::SampleType::kFloat32>(outBuffers, inData, sampleCount, srcSampleType, channelCount);
				break;
			default:
				break;
			}
		}
	}

	template<rkit::audio::SampleType TDestSampleType>
	void AudioMixer::DeinterleaveTo(const AudioScratchBufferInstance &outBuffers,
		const void *inData,
		size_t sampleCount,
		rkit::audio::SampleType srcSampleType,
		uint32_t channelCount)
	{
		switch (srcSampleType)
		{
		case rkit::audio::SampleType::kSInt32:
			DeinterleaveFrom<TDestSampleType, rkit::audio::SampleType::kSInt32>(outBuffers, inData, sampleCount, channelCount);
			break;
		case rkit::audio::SampleType::kSInt32_24bit:
			DeinterleaveFrom<TDestSampleType, rkit::audio::SampleType::kSInt32_24bit>(outBuffers, inData, sampleCount, channelCount);
			break;
		case rkit::audio::SampleType::kSInt16:
			DeinterleaveFrom<TDestSampleType, rkit::audio::SampleType::kSInt16>(outBuffers, inData, sampleCount, channelCount);
			break;
		case rkit::audio::SampleType::kFloat32:
			DeinterleaveFrom<TDestSampleType, rkit::audio::SampleType::kFloat32>(outBuffers, inData, sampleCount, channelCount);
			break;
		default:
			break;
		}
	}

	template<rkit::audio::SampleType TDestSampleType, rkit::audio::SampleType TSrcSampleType>
	void AudioMixer::DeinterleaveFrom(const AudioScratchBufferInstance &outBuffers,
		const void *inData,
		size_t sampleCount,
		uint32_t channelCount)
	{
		using DestSampleData_t = AudioSampleData_t<TDestSampleType>;
		using SrcSampleData_t = AudioSampleData_t<TSrcSampleType>;

		void *outMem = outBuffers.GetAllMem().Ptr();
		const int offsetShiftBits = outBuffers.GetOffsetShiftBits();

		//const size_t bulkDeinterleavedCount = AudioSIMDDeinterleaver<TDestSampleType, TSrcSampleType>::Deinterleave(outMem, offsetShiftBits, inData);
		const size_t bulkDeinterleavedCount = 0;

		sampleCount -= bulkDeinterleavedCount;

		outMem = static_cast<DestSampleData_t *>(outMem) + bulkDeinterleavedCount;
		inData = static_cast<const SrcSampleData_t *>(inData) + bulkDeinterleavedCount * channelCount;

		while (sampleCount > 0)
		{
			for (size_t i = 0; i < channelCount; i++)
			{
				DestSampleData_t &destSample = *reinterpret_cast<DestSampleData_t *>(static_cast<uint8_t *>(outMem) + (i << offsetShiftBits));
				const SrcSampleData_t &srcSample = static_cast<const SrcSampleData_t *>(inData)[i];

				if constexpr (TDestSampleType == TSrcSampleType)
					destSample = srcSample;
				else
					AudioSampleTranscoder<TDestSampleType, TSrcSampleType>::Transcode(destSample, srcSample);
			}

			outMem = static_cast<DestSampleData_t *>(outMem) + 1;
			inData = static_cast<const SrcSampleData_t *>(inData) + channelCount;

			sampleCount--;
		}
	}

	void AudioMixer::Mix(void *outMem, const void *inMem, rkit::audio::SampleType sampleType,
		size_t numSamples, AudioVolumeLevel oldVolume, AudioVolumeLevel newVolume)
	{
		switch (sampleType)
		{
		case rkit::audio::SampleType::kSInt32:
			MixSpecialized<rkit::audio::SampleType::kSInt32>(outMem, inMem, numSamples, oldVolume, newVolume);
			break;
		case rkit::audio::SampleType::kSInt32_24bit:
			MixSpecialized<rkit::audio::SampleType::kSInt32_24bit>(outMem, inMem, numSamples, oldVolume, newVolume);
			break;
		case rkit::audio::SampleType::kSInt16:
			MixSpecialized<rkit::audio::SampleType::kSInt16>(outMem, inMem, numSamples, oldVolume, newVolume);
			break;
		case rkit::audio::SampleType::kFloat32:
			MixSpecialized<rkit::audio::SampleType::kFloat32>(outMem, inMem, numSamples, oldVolume, newVolume);
			break;
		default:
			break;
		}
	}

	template<rkit::audio::SampleType TSampleType>
	void AudioMixer::MixSpecialized(void *outMem, const void *inMem,
		size_t numSamples, AudioVolumeLevel oldVolume, AudioVolumeLevel newVolume)
	{
		if (oldVolume == newVolume)
			MixConstantVolume<TSampleType>(outMem, inMem, numSamples, newVolume);
		else
			MixConstantVolume<TSampleType>(outMem, inMem, numSamples, newVolume);
	}

	template<rkit::audio::SampleType TSampleType>
	static void AudioMixer::MixConstantVolume(void *outMem, const void *inMem, size_t numSamples, AudioVolumeLevel volume)
	{
		if (volume == kAudioMaxVolume)
		{
			MixFullVolume<TSampleType>(outMem, inMem, numSamples);
			return;
		}

		using SampleData_t = AudioSampleData_t<TSampleType>;
		using VolumeScaler_t = typename AudioSampleMixer<TSampleType>::ConstantVolumeScaler_t;

		SampleData_t *outSamples = static_cast<SampleData_t *>(outMem);
		const SampleData_t *inSamples = static_cast<const SampleData_t *>(inMem);
		const VolumeScaler_t volumeScaler = AudioSampleMixer<TSampleType>::ComputeConstantVolumeScaler(volume);

		//const size_t bulkCount = AudioSIMDMixer<TSampleType>::MixConstantVolume(outSamples, inSamples, volumeScaler);
		const size_t bulkMixCount = 0;

		size_t samplesRemaining = numSamples - bulkMixCount;
		inSamples += bulkMixCount;
		outSamples += bulkMixCount;

		for (size_t i = 0; i < samplesRemaining; i++)
			AudioSampleMixer<TSampleType>::MixScaled(outSamples[i], inSamples[i], volumeScaler);
	}

	template<rkit::audio::SampleType TSampleType>
	static void AudioMixer::MixFullVolume(void *outMem, const void *inMem, size_t numSamples)
	{
		using SampleData_t = AudioSampleData_t<TSampleType>;

		//const size_t bulkCount = AudioSIMDMixer<TSampleType>::MixFullVolume(outMem, inMem);
		const size_t bulkMixCount = 0;

		size_t samplesRemaining = numSamples - bulkMixCount;
		SampleData_t *outSamples = static_cast<SampleData_t *>(outMem) + bulkMixCount;

		const SampleData_t *inSamples = static_cast<const SampleData_t *>(inMem) + bulkMixCount;

		for (size_t i = 0; i < samplesRemaining; i++)
			AudioSampleMixer<TSampleType>::MixUnscaled(outSamples[i], inSamples[i]);
	}

	OpaqueEmitterCommand AudioMixer::WriteEmitter(AudioMixerEmitter *emitter)
	{
		OpaqueEmitterCommand result = { };
		result.m_emitterPtr = emitter;
		return result;
	}

	AudioMixerEmitter *AudioMixer::ReadEmitter(const OpaqueEmitterCommand &cmdWord)
	{
		AudioMixerEmitter *emitter = cmdWord.m_emitterPtr;
		return emitter;
	}

	void AudioMixer::ExecuteCommandLists(const rkit::audio::U64Fraction &timestamp)
	{
		AudioRCDoubleList<AudioMixerEmitter> addEmitters;
		AudioRCSingleList<AudioCommandList> cmdLists;

		AudioMixerEmitter *removeListStart = nullptr;
		AudioMixerEmitter *removeListEnd = nullptr;

		AudioMixerEmitter *addListEnd = nullptr;

		{
			rkit::MutexLock lock(*m_cmdListMutex);
			addEmitters = std::move(m_addEmitters);
			cmdLists = std::move(m_cmdLists);

			removeListStart = m_firstRemoveEmitter;
			removeListEnd = m_lastRemoveEmitter;

			m_firstRemoveEmitter = nullptr;
			m_lastRemoveEmitter = nullptr;
		}

		if (addEmitters.GetFirst() != nullptr)
		{
			int n = 0;
		}

		// Add new emitters
		m_renderThreadState.m_inactiveEmitters.Prepend(std::move(addEmitters));

		// Run commands
		for (;;)
		{
			rkit::RCPtr<AudioCommandList> cmdList = cmdLists.DetachFirst();
			if (!cmdList.IsValid())
				break;

			rkit::ConstSpan<AudioCommandWord> allCmdListWords = cmdList->GetCommands();
			const AudioCommandWord *allCmdListWordStart = allCmdListWords.Ptr();

			size_t cmdListOffset = 0;
			while (cmdListOffset < allCmdListWords.Count())
			{
				const AudioCommandWord *cmdWords = allCmdListWordStart + cmdListOffset;
				const AudioCommandType cmdType = cmdWords[0].m_cmdType;

				cmdListOffset += ExecuteCommand(cmdType, cmdWords + AudioCommandList::kNumRequiredParams) + AudioCommandList::kNumRequiredParams;
			}
		}

		// Append emitter removal list
		if (removeListStart != nullptr)
		{
			if (m_renderThreadState.m_firstToRemove == nullptr)
				m_renderThreadState.m_firstToRemove = removeListStart;
			else
				m_renderThreadState.m_lastToRemove->m_nextRemove = removeListStart;

			m_renderThreadState.m_lastToRemove = removeListEnd;
		}
	}

	void AudioMixer::FlushRemovals()
	{
		AudioMixerEmitter *emitterToRemove = m_renderThreadState.m_firstToRemove;
		for (;;)
		{
			if (emitterToRemove == nullptr)
			{
				m_renderThreadState.m_lastToRemove = nullptr;
				break;
			}

			AudioMixerEmitter *nextToRemove = emitterToRemove->m_nextRemove;

			if (emitterToRemove->m_isActive)
				m_renderThreadState.m_activeEmitters.RemoveItem(emitterToRemove);
			else
				m_renderThreadState.m_inactiveEmitters.RemoveItem(emitterToRemove);

			emitterToRemove = nextToRemove;
		}

		m_renderThreadState.m_firstToRemove = emitterToRemove;

	}

	size_t AudioMixer::ExecuteCommand(AudioCommandType cmdType, const AudioCommandWord *params)
	{
		switch (cmdType)
		{
		case AudioCommandType::kPlayEmitter:
			{
				AudioMixerEmitter *emitter = ReadEmitter(params[0].m_emitter);
				if (!emitter->m_isActive)
				{
					emitter->m_isActive = true;
					rkit::RCPtr<AudioMixerEmitter> emitterRC = m_renderThreadState.m_inactiveEmitters.DetachItem(emitter);
					m_renderThreadState.m_activeEmitters.Prepend(std::move(emitterRC));
				}
			}
			return 1;
		case AudioCommandType::kStopEmitter:
			{
				AudioMixerEmitter *emitter = ReadEmitter(params[0].m_emitter);
				if (emitter->m_isActive)
				{
					emitter->m_isActive = false;
					rkit::RCPtr<AudioMixerEmitter> emitterRC = m_renderThreadState.m_activeEmitters.DetachItem(emitter);
					m_renderThreadState.m_inactiveEmitters.Append(std::move(emitterRC));
				}
			}
			return 1;
		default:
			RKIT_ASSERT(false);
			return 0;
		}
	}

	AudioSubsystemImpl::AudioSubsystemImpl(rkit::IJobQueue& jobQueue)
		: m_gc(jobQueue)
		, m_mixer(m_gc)
	{
	}

	AudioSubsystemImpl::~AudioSubsystemImpl()
	{
		Shutdown();
	}

	rkit::Result AudioSubsystemImpl::Initialize()
	{
		// FIXME: Select appropriate audio driver
		rkit::IModule *module = rkit::GetDrivers().m_moduleDriver->LoadModule(rkit::IModuleDriver::kDefaultNamespace, u8"Audio_WASAPI");
		if (!module)
			RKIT_THROW(rkit::ResultCode::kModuleLoadFailed);

		m_audioDriver = static_cast<rkit::audio::IAudioDriver *>(rkit::GetDrivers().FindDriver(rkit::IModuleDriver::kDefaultNamespace, u8"Audio_WASAPI"));

		RKIT_CHECK(m_mixer.Initialize(m_audioDriver));
		RKIT_CHECK(m_gc.Initialize());

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

		m_mixer.SetOutputLayout(audioFormat, audioOutputStream->GetBufferCapacity());

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

		m_mixer.Shutdown();

		// GC shutdown must be last
		m_gc.Shutdown();
	}

	AudioSubsystem::AudioSubsystem(rkit::IJobQueue &jobQueue)
		: Opaque<AudioSubsystemImpl>(jobQueue)
	{
	}

	rkit::Result AudioSubsystem::CreateEmitter(AudioEmitter *&outEmitter, rkit::RCPtr<IAudioSource> inSource)
	{
		rkit::RCPtr<AudioMixerEmitter> mixerEmitterRC;
		RKIT_CHECK(rkit::New<AudioMixerEmitter>(mixerEmitterRC, Impl().m_gc, std::move(inSource)));

		AudioMixerEmitter *mixerEmitter = mixerEmitterRC.Get();
		Impl().m_mixer.AddEmitter(std::move(mixerEmitterRC));

		outEmitter = mixerEmitter;
	}

	void AudioSubsystem::DestroyEmitter(AudioEmitter *emitter)
	{
		Impl().m_mixer.RemoveEmitter(static_cast<AudioMixerEmitter *>(emitter));
	}

	rkit::Result AudioSubsystem::PlayEmitter(AudioEmitter *emitter)
	{
		return Impl().m_mixer.Cmd_PlayEmitter(static_cast<AudioMixerEmitter *>(emitter));
	}

	rkit::Result AudioSubsystem::Create(rkit::UniquePtr<AudioSubsystem> &outSubsystem, rkit::IJobQueue& jobQueue)
	{
		rkit::UniquePtr<AudioSubsystem> subsystem;
		RKIT_CHECK(rkit::New<AudioSubsystem>(subsystem, jobQueue));

		RKIT_CHECK(subsystem->Impl().Initialize());

		outSubsystem = std::move(subsystem);
		RKIT_RETURN_OK;
	}
}

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(::anox::AudioSubsystemImpl)
