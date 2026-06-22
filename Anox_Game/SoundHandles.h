#pragma once

#include "rkit/Core/Result.h"
#include "rkit/Audio/AudioFileFormat.h"

namespace anox::game
{
	class SoundSourceHandle;
	class SandboxResourceHandle;
	struct SoundEmitterProperties;

	class SoundEmitterHandle
	{
	public:
		SoundEmitterHandle() = default;
		SoundEmitterHandle(const SoundEmitterHandle &other) = delete;
		SoundEmitterHandle(SoundEmitterHandle &&other) noexcept;
		~SoundEmitterHandle();

		SoundEmitterHandle& operator=(const SoundEmitterHandle &other) = delete;
		SoundEmitterHandle &operator=(SoundEmitterHandle &&other) noexcept;

		void Play() const;

		static rkit::Result Create(SoundEmitterHandle &outHandle, SoundSourceHandle &&src, const SoundEmitterProperties &emitterProperties);

	private:
		uint32_t m_emitterID = 0;
	};

	class SoundSourceHandle
	{
	public:
		SoundSourceHandle() = default;
		SoundSourceHandle(const SoundSourceHandle &other) = delete;
		SoundSourceHandle(SoundSourceHandle &&other) noexcept;
		~SoundSourceHandle();

		SoundSourceHandle &operator=(const SoundSourceHandle &other) = delete;
		SoundSourceHandle &operator=(SoundSourceHandle &&other) noexcept;

		// Releases the source ID without freeing it
		uint32_t GetSourceID() const;
		void UnsafeRelease();

		static rkit::Result CreateFromFileResource(SoundSourceHandle &outHandle, const SandboxResourceHandle &resHandle, rkit::audio::AudioContainerFormat containerFormat);

	private:
		uint32_t m_srcID = 0;
	};
}

namespace anox::game
{
	inline SoundEmitterHandle::SoundEmitterHandle(SoundEmitterHandle &&other) noexcept
		: m_emitterID(other.m_emitterID)
	{
		other.m_emitterID = 0;
	}

	inline SoundEmitterHandle &SoundEmitterHandle::operator=(SoundEmitterHandle &&other) noexcept
	{
		m_emitterID = other.m_emitterID;
		other.m_emitterID = 0;
		return *this;
	}

	inline SoundSourceHandle::SoundSourceHandle(SoundSourceHandle &&other) noexcept
		: m_srcID(other.m_srcID)
	{
		other.m_srcID = 0;
	}

	inline SoundSourceHandle& SoundSourceHandle::operator=(SoundSourceHandle &&other) noexcept
	{
		m_srcID = other.m_srcID;
		other.m_srcID = 0;
		return *this;
	}

	inline uint32_t SoundSourceHandle::GetSourceID() const
	{
		return m_srcID;
	}

	inline void SoundSourceHandle::UnsafeRelease()
	{
		m_srcID = 0;
	}
}
