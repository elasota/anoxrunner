#pragma once

#include "rkit/Core/EnumMask.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/RefCounted.h"

#include <compare>

namespace rkit::audio
{
	struct U64Fraction
	{
		uint64_t m_numerator = 0;
		uint64_t m_denominator = 0;
	};

	enum class SampleType : uint8_t
	{
		kUnknown,

		kSInt32,
		kSInt32_24bit,
		kSInt16,
		kFloat32,

		kLargestSampleType = kFloat32,
	};

	enum class SpeakerPosition : uint8_t
	{
		kFrontLeft,
		kFrontRight,
		kFrontCenter,
		kLowFrequency,
		kBackLeft,
		kBackRight,
		kFrontLeftOfCenter,
		kFrontRightOfCenter,
		kBackCenter,
		kSideLeft,
		kSideRight,
		kTopCenter,
		kTopFrontLeft,
		kTopFrontCenter,
		kTopFrontRight,
		kTopBackLeft,
		kTopBackCenter,
		kTopBackRight,

		kCount,
	};

	struct AudioFormat
	{
		uint32_t m_sampleRate = 0;
		SampleType m_sampleType = SampleType::kUnknown;
		EnumMask<SpeakerPosition> m_speakers;

		bool operator==(const AudioFormat &other) const = default;
	};

	struct IAbstractDeviceID : public RefCounted
	{
		virtual bool CompareEqual(const IAbstractDeviceID &other) const = 0;
		virtual std::strong_ordering CompareOrdered(const IAbstractDeviceID &other) const = 0;
	};

	class AudioDeviceID
	{
	public:
		AudioDeviceID() = default;
		AudioDeviceID(const AudioDeviceID &other) = default;
		AudioDeviceID(AudioDeviceID &&other) = default;

		bool operator==(const AudioDeviceID &other) const;
		std::strong_ordering operator<=>(const AudioDeviceID &other) const;

		static AudioDeviceID FromAbstractID(RCPtr<IAbstractDeviceID> &&deviceID);
		static AudioDeviceID FromAbstractID(const RCPtr<IAbstractDeviceID> &deviceID);

	private:
		explicit AudioDeviceID(RCPtr<IAbstractDeviceID> &&deviceID);
		explicit AudioDeviceID(const RCPtr<IAbstractDeviceID> &deviceID);

		RCPtr<IAbstractDeviceID> m_deviceID;
	};

	struct IAudioDeviceInfo
	{
		virtual bool IsInputDevice() const = 0;
		virtual bool IsOutputDevice() const = 0;
		virtual AudioDeviceID GetDeviceID() const = 0;
	};

	struct IAudioOutputStateQuery
	{
		RKIT_NODISCARD virtual U64Fraction GetTimestamp() const = 0;
	};

	struct IAudioOutputStream
	{
		virtual ~IAudioOutputStream() {}

		virtual void Start() = 0;
		virtual void Stop() = 0;
		virtual bool IsFaulted() const = 0;
		virtual size_t GetBufferCapacity() const = 0;
	};

	struct IAudioEndpoint : public RefCounted
	{
		virtual const IAudioDeviceInfo &GetDeviceInfo() const = 0;
	};

	struct IAudioInputEndpoint : public IAudioEndpoint
	{
	};

	struct IAudioOutputRenderer
	{
		// Return true if there is audio data, false if the entire frame is silent
		virtual bool Render(void *buffer, size_t numSamples, const IAudioOutputStateQuery &outputQuery) = 0;
		virtual void RunTrailingActions() = 0;
	};

	struct IAudioOutputEndpoint : public IAudioEndpoint
	{
		virtual Result GetAudioFormat(AudioFormat &outFormat) const = 0;
		virtual Result TryOpenOutputStream(UniquePtr<IAudioOutputStream> &outOutputStream, const AudioFormat &preferredAudioFormat, uint32_t bufferCapacityInSamples, IAudioOutputRenderer *renderer) = 0;
	};

	struct IAudioDevice : public RefCounted
	{
		virtual ~IAudioDevice() {}
	};

	struct IAudioDriver : public ICustomDriver
	{
		// These may output no endpoint
		virtual Result GetDefaultInputEndpoint(RCPtr<IAudioInputEndpoint> &outEndpoint) const = 0;
		virtual Result GetDefaultOutputEndpoint(RCPtr<IAudioOutputEndpoint> &outEndpoint) const = 0;
		virtual U64Fraction GetTimestamp() const = 0;
	};
}

#include <utility>

namespace rkit::audio
{
	inline bool AudioDeviceID::operator==(const AudioDeviceID &other) const
	{
		if (!m_deviceID.IsValid())
			return !other.m_deviceID.IsValid();

		return m_deviceID->CompareEqual(*other.m_deviceID);
	}

	inline std::strong_ordering AudioDeviceID::operator<=>(const AudioDeviceID &other) const
	{
		if (!m_deviceID.IsValid())
		{
			if (other.m_deviceID.IsValid())
				return std::strong_ordering::less;
			else
				return std::strong_ordering::equal;
		}

		if (!other.m_deviceID.IsValid())
			return std::strong_ordering::greater;

		return m_deviceID->CompareOrdered(*other.m_deviceID);
	}

	inline AudioDeviceID AudioDeviceID::FromAbstractID(RCPtr<IAbstractDeviceID> &&deviceID)
	{
		return AudioDeviceID(std::move(deviceID));
	}

	inline AudioDeviceID AudioDeviceID::FromAbstractID(const RCPtr<IAbstractDeviceID> &deviceID)
	{
		return AudioDeviceID(deviceID);
	}

	inline AudioDeviceID::AudioDeviceID(RCPtr<IAbstractDeviceID> &&deviceID)
		: m_deviceID(std::move(deviceID))
	{
	}

	inline AudioDeviceID::AudioDeviceID(const RCPtr<IAbstractDeviceID> &deviceID)
		: m_deviceID(deviceID)
	{
	}
}
