#pragma once

#include "rkit/Core/CoroutineProtos.h"
#include "rkit/Core/RefCounted.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/PathProto.h"
#include "rkit/Core/StringProto.h"

namespace anox::game
{
	class SandboxResource;
}

namespace anox::game
{
	class SandboxResourceHandle
	{
	public:
		SandboxResourceHandle();
		explicit SandboxResourceHandle(uint32_t resID);
		SandboxResourceHandle(const SandboxResourceHandle &other) = delete;
		SandboxResourceHandle(SandboxResourceHandle &&other) noexcept;;
		~SandboxResourceHandle();

		SandboxResourceHandle &operator=(const SandboxResourceHandle &other) = delete;
		SandboxResourceHandle &operator=(SandboxResourceHandle &&other) noexcept;;

		uint32_t GetResourceID() const;

	private:
		static void Dispose(uint32_t resourceID);

		uint32_t m_resourceID = 0;
	};

	class SandboxResourceRequestHandle
	{
	public:
		SandboxResourceRequestHandle();
		explicit SandboxResourceRequestHandle(uint32_t reqID);
		SandboxResourceRequestHandle(const SandboxResourceRequestHandle &other) = delete;
		SandboxResourceRequestHandle(SandboxResourceRequestHandle &&other) noexcept;
		~SandboxResourceRequestHandle();

		SandboxResourceRequestHandle &operator=(const SandboxResourceRequestHandle &other) = delete;
		SandboxResourceRequestHandle &operator=(SandboxResourceRequestHandle &&other) noexcept;

		rkit::Result TryFinishLoading(bool &isFinished, SandboxResourceHandle& reqHandle);

		rkit::ResultCoroutine WaitForLoaded(rkit::ICoroThread &thread, SandboxResourceHandle &outReqHandle);

		uint32_t GetRequestID() const;
		bool IsValid() const;

	private:
		static void Dispose(uint32_t requestID);

		uint32_t m_requestID = 0;
	};

	class SandboxResourceDataBlob
	{
	public:
		SandboxResourceDataBlob();
		SandboxResourceDataBlob(uint32_t mmid, void *ptr, size_t size);
		SandboxResourceDataBlob(const SandboxResourceDataBlob& other) = delete;
		SandboxResourceDataBlob(SandboxResourceDataBlob &&other) noexcept;
		~SandboxResourceDataBlob();

		SandboxResourceDataBlob& operator=(const SandboxResourceDataBlob &other) = delete;
		SandboxResourceDataBlob& operator=(SandboxResourceDataBlob &&other) noexcept;

		rkit::Span<const uint8_t> GetContents() const;

	private:
		static void Dispose(uint32_t mmid, void *mem);

		void *m_mem;
		size_t m_size;
		uint32_t m_mmid;
	};

	class SandboxResourceLoader
	{
	public:
		static rkit::Result LoadCIPathKeyedResource(SandboxResourceRequestHandle &outRequest, uint32_t resourceType, const rkit::StringSliceView &path);

		static rkit::Result GetFileResourceContents(SandboxResourceDataBlob &outBlob, const SandboxResourceHandle &res);
	};
}

#include "rkit/Core/Span.h"

namespace anox::game
{
	inline SandboxResourceHandle::SandboxResourceHandle()
		: m_resourceID(0)
	{
	}

	inline SandboxResourceHandle::SandboxResourceHandle(uint32_t resID)
		: m_resourceID(resID)
	{
	}

	inline SandboxResourceHandle::SandboxResourceHandle(SandboxResourceHandle &&other) noexcept
		: m_resourceID(other.m_resourceID)
	{
		other.m_resourceID = 0;
	}

	inline SandboxResourceHandle::~SandboxResourceHandle()
	{
		if (m_resourceID != 0)
			Dispose(m_resourceID);
	}

	inline SandboxResourceHandle &SandboxResourceHandle::operator=(SandboxResourceHandle &&other) noexcept
	{
		if (m_resourceID)
			Dispose(m_resourceID);

		m_resourceID = other.m_resourceID;
		other.m_resourceID = 0;

		return *this;
	}

	inline uint32_t SandboxResourceHandle::GetResourceID() const
	{
		return m_resourceID;
	}

	inline SandboxResourceRequestHandle::SandboxResourceRequestHandle()
		: m_requestID(0)
	{
	}

	inline SandboxResourceRequestHandle::SandboxResourceRequestHandle(uint32_t reqID)
		: m_requestID(reqID)
	{
	}

	inline SandboxResourceRequestHandle::SandboxResourceRequestHandle(SandboxResourceRequestHandle &&other) noexcept
		: m_requestID(other.m_requestID)
	{
		other.m_requestID = 0;
	}

	inline SandboxResourceRequestHandle::~SandboxResourceRequestHandle()
	{
		if (m_requestID)
			Dispose(m_requestID);
	}

	inline SandboxResourceRequestHandle &SandboxResourceRequestHandle::operator=(SandboxResourceRequestHandle &&other) noexcept
	{
		if (m_requestID)
			Dispose(m_requestID);

		m_requestID = other.m_requestID;
		other.m_requestID = 0;

		return *this;
	}

	inline uint32_t SandboxResourceRequestHandle::GetRequestID() const
	{
		return m_requestID;
	}

	inline bool SandboxResourceRequestHandle::IsValid() const
	{
		return m_requestID != 0;
	}

	inline SandboxResourceDataBlob::SandboxResourceDataBlob()
		: m_mem(nullptr)
		, m_mmid(0)
		, m_size(0)
	{
	}

	inline SandboxResourceDataBlob::SandboxResourceDataBlob(uint32_t mmid, void *ptr, size_t size)
		: m_mem(ptr)
		, m_mmid(mmid)
		, m_size(size)
	{
	}

	inline SandboxResourceDataBlob::SandboxResourceDataBlob(SandboxResourceDataBlob &&other) noexcept
		: m_mem(other.m_mem)
		, m_mmid(other.m_mmid)
		, m_size(other.m_size)
	{
		other.m_mem = nullptr;
		other.m_mmid = 0;
		other.m_size = 0;
	}

	inline SandboxResourceDataBlob::~SandboxResourceDataBlob()
	{
		if (m_mem)
			Dispose(m_mmid, m_mem);
	}

	inline SandboxResourceDataBlob &SandboxResourceDataBlob::operator=(SandboxResourceDataBlob &&other) noexcept
	{
		if (m_mem)
			Dispose(m_mmid, m_mem);

		m_mem = other.m_mem;
		m_mmid = other.m_mmid;
		m_size = other.m_size;

		other.m_mem = nullptr;
		other.m_mmid = 0;
		other.m_size = 0;

		return *this;
	}

	inline rkit::Span<const uint8_t> SandboxResourceDataBlob::GetContents() const
	{
		return rkit::Span<const uint8_t>(static_cast<const uint8_t *>(m_mem), m_size);
	}
}
