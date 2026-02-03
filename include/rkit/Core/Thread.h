#pragma once

#include "UniquePtr.h"

namespace rkit
{
	struct IThreadContext
	{
		virtual ~IThreadContext() {}

		virtual Result Run() = 0;
	};

	struct IThread
	{
		virtual ~IThread() {}

		virtual void Finalize(PackedResultAndExtCode &outResult) = 0;
	};

	class UniqueThreadRef
	{
	public:
		UniqueThreadRef();
		UniqueThreadRef(UniqueThreadRef &&other) noexcept;
		explicit UniqueThreadRef(UniquePtr<IThread> &&thread);
		~UniqueThreadRef();

		UniqueThreadRef &operator=(UniqueThreadRef &&other) noexcept;

		bool IsValid() const;

		PackedResultAndExtCode Finalize();

	private:
		UniqueThreadRef(const UniqueThreadRef &) = delete;
		UniqueThreadRef &operator=(const UniqueThreadRef &) = delete;

		UniquePtr<IThread> m_thread;
		PackedResultAndExtCode m_finalResult;
	};
}

#include "Result.h"

namespace rkit
{
	inline UniqueThreadRef::UniqueThreadRef()
		: m_finalResult(utils::PackResult(ResultCode::kInvalidParameter))
	{
	}

	inline UniqueThreadRef::UniqueThreadRef(UniqueThreadRef &&other) noexcept
		: m_thread(std::move(other.m_thread))
		, m_finalResult(other.m_finalResult)
	{
	}

	inline UniqueThreadRef::UniqueThreadRef(UniquePtr<IThread> &&thread)
		: m_thread(std::move(thread))
		, m_finalResult(utils::PackResult(ResultCode::kOK))
	{
	}

	inline UniqueThreadRef::~UniqueThreadRef()
	{
		if (m_thread.IsValid())
			m_thread->Finalize(m_finalResult);
	}

	inline UniqueThreadRef &UniqueThreadRef::operator=(UniqueThreadRef &&other) noexcept
	{
		if (m_thread.IsValid())
		{
			PackedResultAndExtCode oldThreadResult = utils::PackResult(ResultCode::kOK);

			m_thread->Finalize(oldThreadResult);
		}

		m_thread = std::move(other.m_thread);

		return *this;
	}

	inline bool UniqueThreadRef::IsValid() const
	{
		return m_thread.IsValid();
	}

	inline PackedResultAndExtCode UniqueThreadRef::Finalize()
	{
		if (m_thread.IsValid())
		{
			m_thread->Finalize(m_finalResult);
			m_thread.Reset();
		}

		return m_finalResult;
	}
}
