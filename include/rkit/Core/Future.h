#pragma once

#include "Optional.h"
#include "RefCounted.h"

#include <atomic>

namespace rkit
{
	enum class FutureState
	{
		kWaiting,
		kFailed,
		kAborted,
		kCompleted,

		kInvalid,
	};

	template<class T>
	struct FutureContainer : public RefCounted
	{
		void Complete(const T &value);
		void Complete(T &&value);

		void Fail();
		void Abort();

		FutureState GetState() const;
		T &GetResult();

	protected:
		typedef uint8_t StatePrimitive_t;

		Optional<T> m_result;
		std::atomic<StatePrimitive_t> m_state;
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

		void Reset();

	private:
		RCPtr<FutureContainer<T>> m_container;
	};
}

#include "RKitAssert.h"

namespace rkit
{
	template<class T>
	void FutureContainer<T>::Complete(const T &value)
	{
		RKIT_ASSERT(this->GetState() == FutureState::kWaiting);

		m_result.Emplace(value);
		m_state.store(static_cast<StatePrimitive_t>(FutureState::kCompleted), std::memory_order_release);
	}

	template<class T>
	void FutureContainer<T>::Complete(T &&value)
	{
		RKIT_ASSERT(this->GetState() == FutureState::kWaiting);

		m_result.Emplace(std::move(value));
		m_state.store(static_cast<StatePrimitive_t>(FutureState::kCompleted), std::memory_order_release);
	}

	template<class T>
	void FutureContainer<T>::Fail()
	{
		RKIT_ASSERT(this->GetState() == FutureState::kWaiting);

		m_state.store(static_cast<StatePrimitive_t>(FutureState::kFailed), std::memory_order_release);
	}

	template<class T>
	void FutureContainer<T>::Abort()
	{
		RKIT_ASSERT(this->GetState() == FutureState::kWaiting);

		m_state.store(static_cast<StatePrimitive_t>(FutureState::kAborted), std::memory_order_release);
	}

	template<class T>
	FutureState FutureContainer<T>::GetState() const
	{
		return static_cast<FutureState>(m_state.load(std::memory_order_acquire));
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
			return m_container->TryGetResult();

		return nullptr;
	}

	template<class T>
	void Future<T>::Reset()
	{
		m_container.Reset();
	}
}
