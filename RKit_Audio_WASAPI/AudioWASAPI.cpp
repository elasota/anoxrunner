#include "rkit/Audio/AudioDriver.h"

#include "rkit/Core/DriverModuleStub.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/ModuleGlue.h"
#include "rkit/Core/NoCopy.h"
#include "rkit/Core/Pair.h"
#include "rkit/Core/ScopeExit.h"
#include "rkit/Core/Thread.h"
#include "rkit/Core/Vector.h"

#include "rkit/Win32/IncludeWindows.h"

#include "rkit/Win32/ComPtr.h"

#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <mmreg.h>

namespace rkit::audio::wasapi
{
	struct WASAPIAudioOutputStream;

	class WASAPIAudioOutputThreadContext final : public IThreadContext, public NoCopy
	{
	public:
		WASAPIAudioOutputThreadContext();
		~WASAPIAudioOutputThreadContext();

		Result Initialize();
		Result Run() override;

		void SetOutputStream(WASAPIAudioOutputStream &stream);
		HANDLE GetWakeUpEventHandle() const;

		void SignalShutdown();

		template<class TFunc>
		void RunActionOnAudioThread(const TFunc &func);

	private:
		template<class TFunc>
		static void StaticSyncActionThunk(void *userdata);

		HANDLE m_wakeUpEvent;
		HANDLE m_syncActionReadyEvent;
		HANDLE m_syncActionCompletedEvent;

		void *m_syncActionUserdata = nullptr;
		void (*m_syncActionFunction)(void *userdata) = nullptr;
		std::atomic<bool> m_syncActionReady;

		WASAPIAudioOutputStream *m_stream = nullptr;

		bool m_hasBufferingFault = false;
		bool m_isShuttingDown = false;
	};

	class WASAPIAudioDeviceID final : public IAbstractDeviceID
	{
	public:
		WASAPIAudioDeviceID() = delete;
		WASAPIAudioDeviceID(Vector<wchar_t> &&deviceID);

		bool CompareEqual(const IAbstractDeviceID &other) const override;
		std::strong_ordering CompareOrdered(const IAbstractDeviceID &other) const override;

		ConstSpan<wchar_t> GetChars() const;

	private:
		Vector<wchar_t> m_deviceID;
	};

	class WASAPIAudioOutputEndpoint final : public IAudioOutputEndpoint
	{
	public:
		WASAPIAudioOutputEndpoint() = delete;
		explicit WASAPIAudioOutputEndpoint(win32::ComPtr<IMMDevice> &&device, win32::ComPtr<IMMEndpoint> &&endpoint, RCPtr<WASAPIAudioDeviceID> &&deviceID);

		const IAudioDeviceInfo &GetDeviceInfo() const override;
		Result GetAudioFormat(AudioFormat &outFormat) const override;

		Result TryOpenOutputStream(UniquePtr<IAudioOutputStream> &outOutputStream, const AudioFormat &preferredAudioFormat, uint32_t bufferCapacityInSamples, IAudioOutputRenderer *renderer) override;

	private:
		struct AudioDeviceInfo final : public IAudioDeviceInfo
		{
			bool IsInputDevice() const override { return false; }
			bool IsOutputDevice() const override { return true; }

			AudioDeviceID GetDeviceID() const override;

			const WASAPIAudioOutputEndpoint &GetOwner() const;
			WASAPIAudioOutputEndpoint &GetOwner();
		};

		RCPtr<WASAPIAudioDeviceID> m_deviceID;
		win32::ComPtr<IMMDevice> m_device;
		win32::ComPtr<IMMEndpoint> m_endpoint;
		AudioDeviceInfo m_deviceInfo;
	};

	struct WASAPIAudioOutputStreamProperties
	{
		uint32_t m_bufferSize = 0;
		IAudioOutputRenderer *m_renderer = nullptr;
		AudioFormat m_audioFormat;

		win32::ComPtr<IAudioClient> m_audioClient;
		win32::ComPtr<IAudioRenderClient> m_renderClient;
		UniqueThreadRef m_audioThread;
		WASAPIAudioOutputThreadContext *m_threadContext = nullptr;
	};

	class WASAPIAudioOutputStream final : public IAudioOutputStream
	{
	public:
		WASAPIAudioOutputStream() = delete;
		explicit WASAPIAudioOutputStream(WASAPIAudioOutputStreamProperties &&properties);

		~WASAPIAudioOutputStream();

		void Start() override;
		void Stop() override;
		bool IsFaulted() const override;
		size_t GetBufferCapacity() const override;

		void RunAudioThreadTick();

	private:
		class StateQuery final : public IAudioOutputStateQuery
		{
		public:
			StateQuery() = delete;
			explicit StateQuery(const WASAPIAudioOutputStream &owner);

		private:
			const WASAPIAudioOutputStream &m_owner;
		};

		void SetFault();

		WASAPIAudioOutputStreamProperties m_props;

		bool m_isRunning;
		std::atomic<bool> m_hasFaulted;
	};

	class WASAPIAudioDriver : public IAudioDriver
	{
	public:
		struct ChannelBitAssociation
		{
			DWORD m_channelMask;
			SpeakerPosition m_speakerPosition;
		};

		Result InitDriver(const DriverInitParameters *initParams) override;
		void ShutdownDriver() override;

		uint32_t GetDriverNamespaceID() const override { return IModuleDriver::kDefaultNamespace; }
		StringView GetDriverName() const override { return u8"Audio_WASAPI"; }

		Result GetDefaultInputEndpoint(RCPtr<IAudioInputEndpoint> &outEndpoint) const override;
		Result GetDefaultOutputEndpoint(RCPtr<IAudioOutputEndpoint> &outEndpoint) const override;

		static AudioFormat WaveFormatToAudioFormat(const WAVEFORMATEX &waveFormat);
		static AudioFormat WaveFormatToAudioFormat(const WAVEFORMATEXTENSIBLE &waveFormat);

		static bool AudioFormatToWaveFormat(WAVEFORMATEXTENSIBLE &outWaveFormat, const AudioFormat &audioFormat);

		static ConstSpan<ChannelBitAssociation> GetChannelBitAssociations();

	private:
		static Result GetMMDeviceID(IMMDevice &device, RCPtr<WASAPIAudioDeviceID> &outDeviceID);
		static SampleType AudioSampleTypeFromSubFormat(const GUID &guid, WORD bitSize);
		static EnumMask<SpeakerPosition> SpeakerPositionsFromChannelMask(DWORD channelMask);
		static DWORD ChannelMaskFromSpeakerPositions(const EnumMask<SpeakerPosition> &speakerPositions);

		struct ComResources
		{
			win32::ComPtr<IMMDeviceEnumerator> m_devEnum;
		};

		bool m_comInitialized = false;
		ComResources m_comRes;

		static const ChannelBitAssociation ms_channelBitAssociations[];
	};

	typedef CustomDriverModuleStub<WASAPIAudioDriver> AudioWASAPIModule;


	WASAPIAudioOutputThreadContext::WASAPIAudioOutputThreadContext()
		: m_wakeUpEvent(nullptr)
		, m_syncActionReadyEvent(nullptr)
		, m_syncActionCompletedEvent(nullptr)
		, m_syncActionReady(false)
		, m_isShuttingDown(false)
	{
	}

	WASAPIAudioOutputThreadContext::~WASAPIAudioOutputThreadContext()
	{
		if (m_wakeUpEvent != nullptr)
			::CloseHandle(m_wakeUpEvent);
		if (m_syncActionReadyEvent != nullptr)
			::CloseHandle(m_syncActionReadyEvent);
		if (m_syncActionCompletedEvent != nullptr)
			::CloseHandle(m_syncActionCompletedEvent);
	}

	Result WASAPIAudioOutputThreadContext::Initialize()
	{
		m_wakeUpEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		m_syncActionReadyEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		m_syncActionCompletedEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (!m_wakeUpEvent || !m_syncActionReadyEvent || !m_syncActionCompletedEvent)
			RKIT_THROW(ResultCode::kOperationFailed);

		RKIT_RETURN_OK;
	}

	Result WASAPIAudioOutputThreadContext::Run()
	{
		const HANDLE handles[2] = { m_wakeUpEvent, m_syncActionReadyEvent };

		for (;;)
		{
			DWORD waitResult = ::WaitForMultipleObjects(2, handles, FALSE, INFINITE);

			if (waitResult == WAIT_OBJECT_0 + 1 || m_syncActionReady.load(std::memory_order_acquire))
			{
				if (m_isShuttingDown)
					break;

				m_syncActionReady.store(false, std::memory_order_relaxed);
				m_syncActionFunction(m_syncActionUserdata);
				::ResetEvent(m_syncActionReadyEvent);
				::SetEvent(m_syncActionCompletedEvent);
			}
			else if (waitResult == WAIT_OBJECT_0)
				m_stream->RunAudioThreadTick();
		}

		RKIT_RETURN_OK;
	}

	void WASAPIAudioOutputThreadContext::SetOutputStream(WASAPIAudioOutputStream &stream)
	{
		m_stream = &stream;
	}

	HANDLE WASAPIAudioOutputThreadContext::GetWakeUpEventHandle() const
	{
		return m_wakeUpEvent;
	}

	void WASAPIAudioOutputThreadContext::SignalShutdown()
	{
		m_isShuttingDown = true;
		m_syncActionReady.store(true, std::memory_order_release);
		::SetEvent(m_syncActionReadyEvent);
	}

	template<class TFunc>
	void WASAPIAudioOutputThreadContext::RunActionOnAudioThread(const TFunc &func)
	{
		m_syncActionUserdata = const_cast<TFunc *>(&func);
		m_syncActionFunction = StaticSyncActionThunk<TFunc>;
		m_syncActionReady.store(true, std::memory_order_release);
		::SetEvent(m_syncActionReadyEvent);
		::WaitForSingleObject(m_syncActionCompletedEvent, INFINITE);
	}

	template<class TFunc>
	void WASAPIAudioOutputThreadContext::StaticSyncActionThunk(void *userdata)
	{
		const TFunc &func = *static_cast<const TFunc *>(userdata);
		func();
	}

	WASAPIAudioDeviceID::WASAPIAudioDeviceID(Vector<wchar_t> &&deviceID)
		: m_deviceID(std::move(deviceID))
	{
	}

	bool WASAPIAudioDeviceID::CompareEqual(const IAbstractDeviceID &other) const
	{
		return CompareSpansEqual(GetChars(), static_cast<const WASAPIAudioDeviceID &>(other).GetChars());
	}

	std::strong_ordering WASAPIAudioDeviceID::CompareOrdered(const IAbstractDeviceID &other) const
	{
		return CompareSpans<wchar_t, DefaultComparer<wchar_t, wchar_t, std::strong_ordering>, std::strong_ordering>(GetChars(), static_cast<const WASAPIAudioDeviceID &>(other).GetChars());
	}

	ConstSpan<wchar_t> WASAPIAudioDeviceID::GetChars() const
	{
		const ConstSpan<wchar_t> nullTerminatedSpan = m_deviceID.ToSpan();
		return nullTerminatedSpan.SubSpan(0, nullTerminatedSpan.Count() - 1);
	}

	WASAPIAudioOutputEndpoint::WASAPIAudioOutputEndpoint(win32::ComPtr<IMMDevice> &&device, win32::ComPtr<IMMEndpoint> &&endpoint, RCPtr<WASAPIAudioDeviceID> &&deviceID)
		: m_device(std::move(device))
		, m_endpoint(std::move(endpoint))
		, m_deviceID(std::move(deviceID))
	{
	}

	const IAudioDeviceInfo &WASAPIAudioOutputEndpoint::GetDeviceInfo() const
	{
		return m_deviceInfo;
	}

	Result WASAPIAudioOutputEndpoint::GetAudioFormat(AudioFormat &outFormat) const
	{
		win32::ComPtr<IPropertyStore> propertyStore;
		RKIT_COM_CHECK(m_device->OpenPropertyStore(STGM_READ, propertyStore.WriteTo()));

		PROPVARIANT prop;
		PropVariantInit(&prop);
		RKIT_COM_CHECK(propertyStore->GetValue(PKEY_AudioEngine_DeviceFormat, &prop));

		if (prop.blob.cbSize < sizeof(WAVEFORMATEX))
		{
			PropVariantClear(&prop);
			RKIT_THROW(ResultCode::kInternalError);
		}

		outFormat = WASAPIAudioDriver::WaveFormatToAudioFormat(*reinterpret_cast<const WAVEFORMATEX *>(prop.blob.pBlobData));
		PropVariantClear(&prop);

		RKIT_RETURN_OK;
	}

	Result WASAPIAudioOutputEndpoint::TryOpenOutputStream(UniquePtr<IAudioOutputStream> &outOutputStream, const AudioFormat &preferredAudioFormat, uint32_t bufferCapacityInSamples, IAudioOutputRenderer *renderer)
	{
		UniquePtr<WASAPIAudioOutputThreadContext> threadContextUniquePtr;
		RKIT_CHECK(New<WASAPIAudioOutputThreadContext>(threadContextUniquePtr));

		WASAPIAudioOutputThreadContext &threadContext = *threadContextUniquePtr;
		RKIT_CHECK(threadContext.Initialize());

		ISystemDriver &sysDriver = *GetDrivers().m_systemDriver;

		UniqueThreadRef audioThread;
		RKIT_CHECK(sysDriver.CreateThreadWithPriority(audioThread, std::move(threadContextUniquePtr), ThreadPriority::kCritical, u8"Audio Thread"));

		bool initializedOK = false;
		ScopeExit initializeFailureCleanup([&initializedOK, &threadContext]
			{
				if (!initializedOK)
					threadContext.SignalShutdown();
			});

		const AUDCLNT_SHAREMODE shareMode = AUDCLNT_SHAREMODE_SHARED;

		outOutputStream.Reset();

		PROPVARIANT activationParams = {};
		PropVariantInit(&activationParams);

		win32::ComPtr<IAudioClient> audioClient;
		HRESULT hr = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, &activationParams, RKIT_COM_PPV_ARG(audioClient));
		if (hr != S_OK)
			RKIT_RETURN_OK;

		WAVEFORMATEXTENSIBLE waveFormat;
		if (!WASAPIAudioDriver::AudioFormatToWaveFormat(waveFormat, preferredAudioFormat))
			RKIT_RETURN_OK;

		const uint64_t refTimeUnitsPerSecond = 10000000u;
		const REFERENCE_TIME bufferDuration = (bufferCapacityInSamples * refTimeUnitsPerSecond + preferredAudioFormat.m_sampleRate - 1u) / preferredAudioFormat.m_sampleRate;

		const DWORD streamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_RATEADJUST | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
		REFERENCE_TIME refTime = {};
		hr = audioClient->Initialize(shareMode, streamFlags, bufferDuration, 0, &waveFormat.Format, nullptr);

		if (hr != S_OK)
			RKIT_RETURN_OK;

		UINT32 bufferSize = 0;
		hr = audioClient->GetBufferSize(&bufferSize);
		if (hr != S_OK)
			RKIT_RETURN_OK;

		hr = audioClient->SetEventHandle(threadContext.GetWakeUpEventHandle());
		if (hr != S_OK)
			RKIT_RETURN_OK;

		win32::ComPtr<IAudioRenderClient> renderClient;
		hr = audioClient->GetService(RKIT_COM_IID_PPV_ARGS(renderClient));
		if (hr != S_OK)
			RKIT_RETURN_OK;

		WASAPIAudioOutputStreamProperties streamProps;
		streamProps.m_audioClient = std::move(audioClient);
		streamProps.m_audioThread = std::move(audioThread);
		streamProps.m_renderClient = std::move(renderClient);
		streamProps.m_threadContext = &threadContext;
		streamProps.m_audioFormat = preferredAudioFormat;
		streamProps.m_renderer = renderer;
		streamProps.m_bufferSize = bufferSize;

		RKIT_CHECK(New<WASAPIAudioOutputStream>(outOutputStream, std::move(streamProps)));

		// Once everything has been handed off to the output stream, we no longer need to auto-cleanup the audio thread
		initializedOK = true;

		RKIT_RETURN_OK;
	}

	AudioDeviceID WASAPIAudioOutputEndpoint::AudioDeviceInfo::GetDeviceID() const
	{
		return AudioDeviceID::FromAbstractID(GetOwner().m_deviceID);
	}

	WASAPIAudioOutputEndpoint &WASAPIAudioOutputEndpoint::AudioDeviceInfo::GetOwner()
	{
		return *reinterpret_cast<WASAPIAudioOutputEndpoint *>(reinterpret_cast<uint8_t *>(this) - offsetof(WASAPIAudioOutputEndpoint, m_deviceInfo));
	}

	const WASAPIAudioOutputEndpoint &WASAPIAudioOutputEndpoint::AudioDeviceInfo::GetOwner() const
	{
		return const_cast<AudioDeviceInfo *>(this)->GetOwner();
	}


	WASAPIAudioOutputStream::StateQuery::StateQuery(const WASAPIAudioOutputStream &owner)
		: m_owner(owner)
	{
	}

	WASAPIAudioOutputStream::WASAPIAudioOutputStream(WASAPIAudioOutputStreamProperties &&properties)
		: m_props(std::move(properties))
		, m_isRunning(false)
		, m_hasFaulted(false)
	{
		m_props.m_threadContext->SetOutputStream(*this);
	}

	WASAPIAudioOutputStream::~WASAPIAudioOutputStream()
	{
		// Stop the audio client so the audio thread stops working
		Stop();

		// Shut down the audio thread
		m_props.m_threadContext->SignalShutdown();

		// Wait for the audio thread to finish
		(void) m_props.m_audioThread.Finalize();

		// Delete the audio client
		m_props.m_audioClient.Reset();
	}

	void WASAPIAudioOutputStream::Start()
	{
		if (m_isRunning)
			return;

		bool faulted = false;
		m_props.m_threadContext->RunActionOnAudioThread([this, &faulted]()
			{
				HRESULT hr = m_props.m_audioClient->Reset();

				// S_FALSE is returned if the stream is already reset
				if (hr != S_FALSE && hr != S_OK)
				{
					faulted = true;
					return;
				}

				if (m_props.m_audioClient->Start() != S_OK)
				{
					faulted = true;
					return;
				}
			});

		if (faulted)
			SetFault();
		else
			m_isRunning = true;
	}

	void WASAPIAudioOutputStream::Stop()
	{
		if (!m_isRunning)
			return;

		bool faulted = false;
		m_props.m_threadContext->RunActionOnAudioThread([this, &faulted]()
			{
				HRESULT hr = m_props.m_audioClient->Stop();

				// The wakeup event handle may have been set by the audio client, so reset it.
				::ResetEvent(m_props.m_threadContext->GetWakeUpEventHandle());

				if (hr != S_OK && hr != S_FALSE)
					faulted = true;
			});

		m_isRunning = false;

		if (faulted)
			SetFault();
	}

	void WASAPIAudioOutputStream::SetFault()
	{
		return m_hasFaulted.store(true, std::memory_order_relaxed);
	}

	bool WASAPIAudioOutputStream::IsFaulted() const
	{
		return m_hasFaulted.load(std::memory_order_relaxed);
	}

	size_t WASAPIAudioOutputStream::GetBufferCapacity() const
	{
		return m_props.m_bufferSize;
	}

	void WASAPIAudioOutputStream::RunAudioThreadTick()
	{
		bool succeeded = false;

		ScopeExit faultCheck([this, &succeeded]()
			{
				if (!succeeded)
					this->SetFault();
			});

		// Padding frames are the number of frames currently queued for playback
		UINT32 paddingFrames = 0;
		HRESULT hr = m_props.m_audioClient->GetCurrentPadding(&paddingFrames);
		if (hr != S_OK)
			return;

		const UINT32 availableFrames = m_props.m_bufferSize - paddingFrames;

		const UINT32 framesToRender = availableFrames;

		if (framesToRender != 0)
		{
			BYTE *frameData = nullptr;
			hr = m_props.m_renderClient->GetBuffer(framesToRender, &frameData);
			if (hr != S_OK)
				return;

			const bool hasData = m_props.m_renderer->Render(frameData, framesToRender, StateQuery(*this));

			DWORD releaseFlags = 0;
			if (!hasData)
				releaseFlags |= AUDCLNT_BUFFERFLAGS_SILENT;

			hr = m_props.m_renderClient->ReleaseBuffer(framesToRender, releaseFlags);
			if (hr != S_OK)
				return;
		}

		succeeded = true;
	}

	Result WASAPIAudioDriver::InitDriver(const DriverInitParameters *initParams)
	{
		m_comInitialized = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
		if (!m_comInitialized)
			RKIT_THROW(ResultCode::kComError);
		
		win32::ComPtr<IMMDeviceEnumerator> deviceEnumerator;
		RKIT_COM_CHECK(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, RKIT_COM_IID_PPV_ARGS(m_comRes.m_devEnum)));

		RKIT_RETURN_OK;
	}

	void WASAPIAudioDriver::ShutdownDriver()
	{
		m_comRes = {};

		if (m_comInitialized)
			CoUninitialize();

		m_comInitialized = false;
	}

	Result WASAPIAudioDriver::GetDefaultInputEndpoint(RCPtr<IAudioInputEndpoint> &outEndpoint) const
	{
		RKIT_THROW(ResultCode::kNotYetImplemented);
	}

	Result WASAPIAudioDriver::GetDefaultOutputEndpoint(RCPtr<IAudioOutputEndpoint> &outEndpoint) const
	{
		outEndpoint.Reset();

		win32::ComPtr<IMMDevice> mmDevice;
		RKIT_COM_CHECK(m_comRes.m_devEnum->GetDefaultAudioEndpoint(eRender, eConsole, mmDevice.WriteTo()));

		RCPtr<WASAPIAudioDeviceID> deviceID;
		RKIT_CHECK(GetMMDeviceID(*mmDevice, deviceID));

		if (!mmDevice.IsValid())
			RKIT_RETURN_OK;

		win32::ComPtr<IMMEndpoint> endpoint = mmDevice.TryGetInterface<IMMEndpoint>();
		if (!endpoint.IsValid())
			RKIT_RETURN_OK;

		return rkit::New<WASAPIAudioOutputEndpoint>(outEndpoint, std::move(mmDevice), std::move(endpoint), std::move(deviceID));
	}

	AudioFormat WASAPIAudioDriver::WaveFormatToAudioFormat(const WAVEFORMATEXTENSIBLE &formatExtensible)
	{
		AudioFormat result = {};
		const WAVEFORMATEX &formatEx = formatExtensible.Format;

		result.m_sampleType = AudioSampleTypeFromSubFormat(formatExtensible.SubFormat, formatExtensible.Samples.wValidBitsPerSample);
		result.m_sampleRate = formatEx.nSamplesPerSec;
		result.m_speakers = SpeakerPositionsFromChannelMask(formatExtensible.dwChannelMask);

		return result;
	}

	bool WASAPIAudioDriver::AudioFormatToWaveFormat(WAVEFORMATEXTENSIBLE &outWaveFormat, const AudioFormat &audioFormat)
	{
		const size_t numChannels = audioFormat.m_speakers.CountSetBits();

		WAVEFORMATEX &formatEx = outWaveFormat.Format;
		GUID subFormatGUID = {};

		switch (audioFormat.m_sampleType)
		{
		case SampleType::kSInt16:
			formatEx.wBitsPerSample = 16;
			subFormatGUID = KSDATAFORMAT_SUBTYPE_PCM;
			break;
		case SampleType::kSInt32:
			formatEx.wBitsPerSample = 32;
			subFormatGUID = KSDATAFORMAT_SUBTYPE_PCM;
			break;

		default:
			return false;
		};

		formatEx.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
		formatEx.nChannels = static_cast<WORD>(numChannels);
		formatEx.nSamplesPerSec = audioFormat.m_sampleRate;
		formatEx.nAvgBytesPerSec = static_cast<uint32_t>(formatEx.wBitsPerSample) * numChannels * audioFormat.m_sampleRate / 8u;
		formatEx.nBlockAlign = formatEx.nChannels * formatEx.wBitsPerSample / 8;
		formatEx.cbSize = 22;

		outWaveFormat.Samples.wValidBitsPerSample = formatEx.wBitsPerSample;
		outWaveFormat.dwChannelMask = ChannelMaskFromSpeakerPositions(audioFormat.m_speakers);
		outWaveFormat.SubFormat = subFormatGUID;

		return true;
	}

	const WASAPIAudioDriver::ChannelBitAssociation WASAPIAudioDriver::ms_channelBitAssociations[] =
	{
		{ SPEAKER_FRONT_LEFT, SpeakerPosition::kFrontLeft },
		{ SPEAKER_FRONT_RIGHT, SpeakerPosition::kFrontRight },
		{ SPEAKER_FRONT_CENTER, SpeakerPosition::kFrontCenter },
		{ SPEAKER_LOW_FREQUENCY, SpeakerPosition::kLowFrequency },
		{ SPEAKER_BACK_LEFT, SpeakerPosition::kBackLeft },
		{ SPEAKER_BACK_RIGHT, SpeakerPosition::kBackRight },
		{ SPEAKER_FRONT_LEFT_OF_CENTER, SpeakerPosition::kFrontLeftOfCenter },
		{ SPEAKER_FRONT_RIGHT_OF_CENTER, SpeakerPosition::kFrontRightOfCenter },
		{ SPEAKER_BACK_CENTER, SpeakerPosition::kBackCenter },
		{ SPEAKER_SIDE_LEFT, SpeakerPosition::kSideLeft },
		{ SPEAKER_SIDE_RIGHT, SpeakerPosition::kSideRight },
		{ SPEAKER_TOP_CENTER, SpeakerPosition::kTopCenter },
		{ SPEAKER_TOP_FRONT_LEFT, SpeakerPosition::kFrontLeft },
		{ SPEAKER_TOP_FRONT_CENTER, SpeakerPosition::kFrontCenter },
		{ SPEAKER_TOP_FRONT_RIGHT, SpeakerPosition::kFrontRight },
		{ SPEAKER_TOP_BACK_LEFT, SpeakerPosition::kBackLeft },
		{ SPEAKER_TOP_BACK_CENTER, SpeakerPosition::kBackCenter },
		{ SPEAKER_TOP_BACK_RIGHT, SpeakerPosition::kBackRight }
	};

	ConstSpan<WASAPIAudioDriver::ChannelBitAssociation> WASAPIAudioDriver::GetChannelBitAssociations()
	{
		return ConstSpan<WASAPIAudioDriver::ChannelBitAssociation>(ms_channelBitAssociations);
	}

	AudioFormat WASAPIAudioDriver::WaveFormatToAudioFormat(const WAVEFORMATEX &waveFormat)
	{
		if (waveFormat.wFormatTag == WAVE_FORMAT_EXTENSIBLE && waveFormat.cbSize >= 22)
		{
			const WAVEFORMATEXTENSIBLE &waveFormatExtensible = *reinterpret_cast<const WAVEFORMATEXTENSIBLE *>(&waveFormat);
			if (waveFormatExtensible.SubFormat != KSDATAFORMAT_SUBTYPE_WAVEFORMATEX)
				return WaveFormatToAudioFormat(waveFormatExtensible);
		}

		if (waveFormat.wFormatTag == WAVE_FORMAT_PCM)
		{
			AudioFormat result;

			if (waveFormat.nChannels == 2)
				result.m_speakers = EnumMask<SpeakerPosition>({ SpeakerPosition::kFrontLeft, SpeakerPosition::kFrontRight });
			else if (waveFormat.nChannels == 1)
				result.m_speakers = EnumMask<SpeakerPosition>({ SpeakerPosition::kFrontCenter });
			else
				result.m_speakers = EnumMask<SpeakerPosition>();

			result.m_sampleRate = waveFormat.nSamplesPerSec;
			if (waveFormat.cbSize == 16)
				result.m_sampleType = SampleType::kSInt16;

			return result;
		}
		else
			return AudioFormat();
	}

	Result WASAPIAudioDriver::GetMMDeviceID(IMMDevice &device, RCPtr<WASAPIAudioDeviceID> &outDeviceID)
	{
		LPWSTR deviceID = nullptr;
		RKIT_COM_CHECK(device.GetId(&deviceID));

		const size_t deviceIDLength = wcslen(deviceID);

		Vector<wchar_t> stringStorage;
		RKIT_TRY_CATCH_RETHROW(stringStorage.Resize(deviceIDLength + 1),
			rkit::CatchContext(
				[deviceID]
				{
					CoTaskMemFree(deviceID);
				}
			)
		);

		CopySpanNonOverlapping(stringStorage.ToSpan(), ConstSpan<wchar_t>(deviceID, deviceIDLength + 1));
		CoTaskMemFree(deviceID);

		return rkit::New<WASAPIAudioDeviceID>(outDeviceID, std::move(stringStorage));
	}

	SampleType WASAPIAudioDriver::AudioSampleTypeFromSubFormat(const GUID &guid, WORD bitSize)
	{
		if (guid == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
		{
			if (bitSize == 32)
				return SampleType::kFloat32;
		}
		else if (guid == KSDATAFORMAT_SUBTYPE_PCM)
		{
			if (bitSize == 32)
				return SampleType::kSInt32;
			if (bitSize == 24)
				return SampleType::kSInt32_24bit;
			if (bitSize == 16)
				return SampleType::kSInt16;
			if (bitSize == 8)
				return SampleType::kSInt8;
		}

		return SampleType::kUnknown;
	}

	EnumMask<SpeakerPosition> WASAPIAudioDriver::SpeakerPositionsFromChannelMask(DWORD channelMask)
	{
		EnumMask<SpeakerPosition> mask;
		for (const ChannelBitAssociation &association : GetChannelBitAssociations())
		{
			if (channelMask & association.m_channelMask)
				mask.Set(association.m_speakerPosition, true);
		}

		return mask;
	}

	DWORD WASAPIAudioDriver::ChannelMaskFromSpeakerPositions(const EnumMask<SpeakerPosition> &speakerPositions)
	{
		DWORD mask = 0;
		for (const ChannelBitAssociation &association : GetChannelBitAssociations())
		{
			if (speakerPositions.Get(association.m_speakerPosition))
				mask |= association.m_channelMask;
		}

		return mask;
	}
}


RKIT_IMPLEMENT_MODULE(RKit, Audio_WASAPI, ::rkit::audio::wasapi::AudioWASAPIModule)
