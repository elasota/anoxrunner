#pragma once

#include "FutureProtos.h"
#include "Optional.h"
#include "RefCounted.h"

#include <atomic>

namespace rkit
{
	enum class FutureState
	{
		kEmpty,

		kWaiting,
		kFailed,
		kAborted,
		kCompleted,

		kInvalid,
	};

	struct FutureContainerBase : public RefCounted
	{
		FutureContainerBase();

		void Fail();
		void Abort();

		FutureState GetState() const;

	protected:
		typedef uint8_t StatePrimitive_t;

		std::atomic<StatePrimitive_t> m_state;
	};

	template<class T>
	struct FutureContainer : public FutureContainerBase
	{
		void Complete(const T &value);
		void Complete(T &&value);

		T &GetResult();

	private:
		Optional<T> m_result;
	};

	template<class T>
	class Future
	{
	public:
		Future();
		explicit Future(const RCPtr<FutureContainer<T>> &futureContainer);
		explicit Future(RCPtr<FutureContainer<T>> &&futureContainer);

		FutureState GetState() const;

		T *TryGetResult() const;
		T &GetResult() const;

		void Reset();

		const RCPtr<FutureContainer<T>> &GetFutureContainer() const;

	private:
		RCPtr<FutureContainer<T>> m_container;
	};
}

#include "RKitAssert.h"

namespace rkit
{
	inline FutureContainerBase::FutureContainerBase()
		: m_state(static_cast<StatePrimitive_t>(FutureState::kWaiting))
	{
	}

	inline void FutureContainerBase::Fail()
	{
		RKIT_ASSERT(this->GetState() == FutureState::kWaiting);

		m_state.store(static_cast<StatePrimitive_t>(FutureState::kFailed), std::memory_order_release);
	}

	inline void FutureContainerBase::Abort()
	{
		RKIT_ASSERT(this->GetState() == FutureState::kWaiting);

		m_state.store(static_cast<StatePrimitive_t>(FutureState::kAborted), std::memory_order_release);
	}

	inline FutureState FutureContainerBase::GetState() const
	{
		return static_cast<FutureState>(m_state.load(std::memory_order_acquire));
	}

	template<class T>
	void FutureContainer<T>::Complete(const T &value)
	{
		RKIT_ASSERT(this->GetState() == FutureState::kWaiting);

		m_result.Emplace(value);
		this->m_state.store(static_cast<StatePrimitive_t>(FutureState::kCompleted), std::memory_order_release);
	}

	template<class T>
	void FutureContainer<T>::Complete(T &&value)
	{
		RKIT_ASSERT(this->GetState() == FutureState::kWaiting);

		m_result.Emplace(std::move(value));
		this->m_state.store(static_cast<StatePrimitive_t>(FutureState::kCompleted), std::memory_order_release);
	}

	template<class T>
	T &FutureContainer<T>::GetResult()
	{
		RKIT_ASSERT(GetState() == FutureState::kCompleted);
		return m_result.Get();
	}

	template<class T>
	Future<T>::Future()
	{
	}

	template<class T>
	Future<T>::Future(const RCPtr<FutureContainer<T>> &futureContainer)
		: m_container(futureContainer)
	{
	}

	template<class T>
	Future<T>::Future(RCPtr<FutureContainer<T>> &&futureContainer)
		: m_container(std::move(futureContainer))
	{
	}

	template<class T>
	FutureState Future<T>::GetState() const
	{
		if (!m_container.IsValid())
			return FutureState::kEmpty;

		return m_container->GetState();
	}

	template<class T>
	T *Future<T>::TryGetResult() const
	{
		if (!m_container.IsValid())
			return nullptr;

		if (m_container->GetState() == FutureState::kCompleted)
			return &m_container->GetResult();

		return nullptr;
	}

	template<class T>
	T &Future<T>::GetResult() const
	{
		T *resultPtr = this->TryGetResult();
		RKIT_ASSERT(resultPtr);
		return *resultPtr;
	}

	template<class T>
	void Future<T>::Reset()
	{
		m_container.Reset();
	}

	template<class T>
	const RCPtr<FutureContainer<T>> &Future<T>::GetFutureContainer() const
	{
		return m_container;
	}
}
