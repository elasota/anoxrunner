#include "AnoxAudioSubsystem.h"

#include "rkit/Audio/AudioDriver.h"

#include "rkit/Core/Event.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/RefCounted.h"
#include "rkit/Core/SystemDriver.h"

#include "rkit/Core/JobQueue.h"

// FIXME: Delete this
#include "rkit/Win32/IncludeWindows.h"

namespace anox
{
	class AudioGarbageCollector;

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
		explicit AudioSubsystemImpl(rkit::IJobQueue& jobQueue);
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
		AudioGarbageCollector m_gc;
	};

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

	AudioSubsystemImpl::AudioSubsystemImpl(rkit::IJobQueue& jobQueue)
		: m_gc(jobQueue)
	{
	}

	AudioSubsystemImpl::~AudioSubsystemImpl()
	{
		Shutdown();
	}

	rkit::Result AudioSubsystemImpl::Init()
	{
		RKIT_CHECK(m_gc.Initialize());

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

		m_gc.Shutdown();
	}

	AudioSubsystem::AudioSubsystem(rkit::IJobQueue &jobQueue)
		: Opaque<AudioSubsystemImpl>(jobQueue)
	{
	}

	rkit::Result AudioSubsystem::Init()
	{
		return Impl().Init();
	}

	rkit::Result AudioSubsystem::Create(rkit::UniquePtr<AudioSubsystem> &outSubsystem, rkit::IJobQueue& jobQueue)
	{
		rkit::UniquePtr<AudioSubsystem> subsystem;
		RKIT_CHECK(rkit::New<AudioSubsystem>(subsystem, jobQueue));

		RKIT_CHECK(subsystem->Init());

		outSubsystem = std::move(subsystem);
		RKIT_RETURN_OK;
	}
}

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(::anox::AudioSubsystemImpl)
