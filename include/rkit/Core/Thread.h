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

		virtual void Finalize(Result &outResult) = 0;
	};

	class UniqueThreadRef
	{
	public:
		UniqueThreadRef();
		UniqueThreadRef(UniqueThreadRef &&other);
		explicit UniqueThreadRef(UniquePtr<IThread> &&thread);
		~UniqueThreadRef();

		UniqueThreadRef &operator=(UniqueThreadRef &&other);

		bool IsValid() const;

		Result Finalize();

	private:
		UniqueThreadRef(const UniqueThreadRef &) = delete;
		UniqueThreadRef &operator=(const UniqueThreadRef &) = delete;

		UniquePtr<IThread> m_thread;
		Result m_finalResult;
	};
}

#include "Result.h"

namespace rkit
{
	inline UniqueThreadRef::UniqueThreadRef()
		: m_finalResult(Result::SoftFault(ResultCode::kInvalidParameter))
	{
	}

	inline UniqueThreadRef::UniqueThreadRef(UniqueThreadRef &&other)
		: m_thread(std::move(other.m_thread))
	{
	}

	inline UniqueThreadRef::UniqueThreadRef(UniquePtr<IThread> &&thread)
		: m_thread(std::move(thread))
	{
	}

	inline UniqueThreadRef::~UniqueThreadRef()
	{
		if (m_thread.IsValid())
			m_thread->Finalize(m_finalResult);
	}

	inline UniqueThreadRef &UniqueThreadRef::operator=(UniqueThreadRef &&other)
	{
		if (m_thread.IsValid())
		{
			Result oldThreadResult;
			m_thread->Finalize(oldThreadResult);
		}

		m_thread = std::move(other.m_thread);

		return *this;
	}

	inline bool UniqueThreadRef::IsValid() const
	{
		return m_thread.IsValid();
	}

	inline Result UniqueThreadRef::Finalize()
	{
		if (m_thread.IsValid())
		{
			m_thread->Finalize(m_finalResult);
			m_thread.Reset();
		}

		return m_finalResult;
	}
}
