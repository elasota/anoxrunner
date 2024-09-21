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

	private:
		std::atomic<T> m_value;
	};

	typedef AtomicInt<uint64_t> AtomicUInt64_t;
	typedef AtomicInt<uint32_t> AtomicUInt32_t;
	typedef AtomicInt<int64_t> AtomicInt64_t;
	typedef AtomicInt<int32_t> AtomicInt32_t;
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
