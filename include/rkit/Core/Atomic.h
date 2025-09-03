#pragma once

#include <cstdint>
#include <atomic>

namespace rkit
{
	template<class T>
	struct AtomicInt
	{
	public:
		typedef T ValueType_t;

		AtomicInt();
		explicit AtomicInt(const T &initialValue);

		// All functions return the old value
		T Increment();
		T Decrement();

		T Min(T newValue);
		T Max(T newValue);

		T Get() const;
		T GetWeak() const;

		bool CompareExchange(T &expected, T replacement);
		bool CompareExchangeWeak(T &expected, T replacement);

	private:
		std::atomic<T> m_value;
	};

	typedef AtomicInt<uint64_t> AtomicUInt64_t;
	typedef AtomicInt<uint32_t> AtomicUInt32_t;
	typedef AtomicInt<int64_t> AtomicInt64_t;
	typedef AtomicInt<int32_t> AtomicInt32_t;

	template<class T>
	struct AtomicPtr
	{
	public:
		typedef T *ValueType_t;

		AtomicPtr();
		explicit AtomicPtr(T *initialValue);

		T *Get() const;
		T *GetWeak() const;

		bool CompareExchange(T *&expected, T *replacement);
		bool CompareExchangeWeak(T *&expected, T *replacement);

		void Set(T *value);

	private:
		std::atomic<T *> m_value;
	};
}

template<class T>
rkit::AtomicInt<T>::AtomicInt()
	: m_value(static_cast<T>(0))
{
}

template<class T>
rkit::AtomicInt<T>::AtomicInt(const T &initialValue)
	: m_value(initialValue)
{
}

template<class T>
T rkit::AtomicInt<T>::Increment()
{
	return m_value.fetch_add(1);
}

template<class T>
T rkit::AtomicInt<T>::Decrement()
{
	return m_value.fetch_sub(1);
}

template<class T>
T rkit::AtomicInt<T>::Min(T candidate)
{
	T existing = GetWeak();

	T replacement = (candidate < existing) ? candidate : existing;

	while (!CompareExchange(existing, replacement))
	{
		replacement = (candidate < existing) ? candidate : existing;
	}

	return existing;
}

template<class T>
T rkit::AtomicInt<T>::Max(T candidate)
{
	T existing = GetWeak();

	T replacement = (candidate > existing) ? candidate : existing;

	while (!CompareExchange(existing, replacement))
	{
		replacement = (candidate > existing) ? candidate : existing;
	}

	return existing;
}

template<class T>
T rkit::AtomicInt<T>::Get() const
{
	return m_value.load();
}

template<class T>
T rkit::AtomicInt<T>::GetWeak() const
{
	return m_value.load(std::memory_order_relaxed);
}

template<class T>
bool rkit::AtomicInt<T>::CompareExchange(T &expected, T replacement)
{
	return m_value.compare_exchange_strong(expected, replacement);
}

template<class T>
bool rkit::AtomicInt<T>::CompareExchangeWeak(T &expected, T replacement)
{
	return m_value.compare_exchange_weak(expected, replacement);
}

template<class T>
rkit::AtomicPtr<T>::AtomicPtr()
	: m_value(nullptr)
{
}

template<class T>
rkit::AtomicPtr<T>::AtomicPtr(T *initialValue)
	: m_value(initialValue)
{
}

template<class T>
T *rkit::AtomicPtr<T>::Get() const
{
	return m_value.load();
}

template<class T>
T *rkit::AtomicPtr<T>::GetWeak() const
{
	return m_value.load(std::memory_order_relaxed);
}

template<class T>
bool rkit::AtomicPtr<T>::CompareExchange(T *&expected, T *replacement)
{
	return m_value.compare_exchange_strong(expected, replacement);
}

template<class T>
bool rkit::AtomicPtr<T>::CompareExchangeWeak(T *&expected, T *replacement)
{
	return m_value.compare_exchange_weak(expected, replacement);
}

template<class T>
void rkit::AtomicPtr<T>::Set(T *value)
{
	m_value.store(value);
}
