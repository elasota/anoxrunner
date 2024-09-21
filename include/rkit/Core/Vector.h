#pragma once

#include "RKitAssert.h"

#include <cstddef>
#include <limits>

namespace rkit
{
	struct IMallocDriver;
	struct Result;

	template<class T>
	class Span;

	template<class T>
	class SpanIterator;

	template<class T>
	class Vector
	{
	public:
		typedef SpanIterator<T> Iterator_t;
		typedef SpanIterator<const T> ConstIterator_t;

		Vector();
		explicit Vector(IMallocDriver *alloc);
		Vector(Vector &&) noexcept;
		~Vector();

		void RemoveRange(size_t firstArg, size_t numArgs);
		Result Resize(size_t size);

		Result Append(const T &item);
		Result Append(T &&item);

		void Reset();

		T *GetBuffer();
		const T *GetBuffer() const;

		size_t Count() const;

		Span<T> ToSpan();
		Span<const T> ToSpan() const;

		T &operator[](size_t index);
		const T &operator[](size_t index) const;

		Vector &operator=(Vector &&other);

		Iterator_t begin();
		ConstIterator_t begin() const;

		Iterator_t end();
		ConstIterator_t end() const;

	private:
		Vector(const Vector &) = delete;
		Vector &operator=(const Vector &) = delete;

		Result Reallocate(size_t newCapacity);
		Result EnsureCapacityForOneMore();

		T *m_arr;
		size_t m_count;
		size_t m_capacity;
		IMallocDriver *m_alloc;
	};
}

#include "Drivers.h"
#include "MallocDriver.h"
#include "Result.h"
#include "Span.h"

#include <new>
#include <utility>

template<class T>
inline rkit::Vector<T>::Vector()
	: m_alloc(GetDrivers().m_mallocDriver)
	, m_count(0)
	, m_capacity(0)
	, m_arr(nullptr)
{
}

template<class T>
inline rkit::Vector<T>::Vector(IMallocDriver *alloc)
	: m_alloc(alloc)
	, m_count(0)
	, m_capacity(0)
	, m_arr(nullptr)
{
}

template<class T>
inline rkit::Vector<T>::Vector(Vector<T> &&other) noexcept
	: m_alloc(other.m_alloc)
	, m_count(other.m_count)
	, m_capacity(other.m_capacity)
	, m_arr(other.m_arr)
{
	other.m_count = 0;
	other.m_capacity = 0;
	other.m_arr = nullptr;
}

template<class T>
rkit::Vector<T>::~Vector()
{
	Reset();
}

template<class T>
void rkit::Vector<T>::RemoveRange(size_t firstIndex, size_t numElements)
{
	if (numElements == 0)
		return;

	RKIT_ASSERT(firstIndex <= m_count);
	RKIT_ASSERT(numElements <= (m_count - firstIndex));

	size_t indexAfterLast = firstIndex + numElements;
	size_t numElementsToMove = m_count - indexAfterLast;

	size_t count = m_count;
	T *arr = m_arr;
	for (size_t i = 0; i < numElementsToMove; i++)
		arr[i + firstIndex] = std::move(arr[i + indexAfterLast]);

	for (size_t i = 0; i < numElements; i++)
		arr[count - 1 - i].~T();

	m_count -= numElements;
}

template<class T>
rkit::Result rkit::Vector<T>::Resize(size_t size)
{
	constexpr size_t kMaxSize = std::numeric_limits<size_t>::max() / sizeof(T);

	if (size > kMaxSize)
		return ResultCode::kOutOfMemory;

	if (m_capacity < size)
	{
		RKIT_CHECK(Reallocate(size));
	}

	while (size < m_count)
	{
		m_count--;
		m_arr[m_count].~T();
	}

	while (size > m_count)
	{
		new (m_arr + m_count) T();
		m_count++;
	}

	return ResultCode::kOK;
}


template<class T>
rkit::Result rkit::Vector<T>::Append(const T &item)
{
	RKIT_CHECK(EnsureCapacityForOneMore());

	new (m_arr + m_count) T(item);
	m_count++;

	return ResultCode::kOK;
}

template<class T>
rkit::Result rkit::Vector<T>::Append(T &&item)
{
	RKIT_CHECK(EnsureCapacityForOneMore());

	new (m_arr + m_count) T(std::move(item));
	m_count++;

	return ResultCode::kOK;
}

template<class T>
void rkit::Vector<T>::Reset()
{
	if (m_arr)
	{
		T *arr = m_arr;
		size_t count = m_count;

		for (size_t i = 0; i < count; i++)
		{
			T *deletePtr = arr + (count - 1 - i);
			deletePtr->~T();
		}

		m_alloc->Free(m_arr);
	}

	m_arr = nullptr;
	m_count = 0;
	m_capacity = 0;
}

template<class T>
T *rkit::Vector<T>::GetBuffer()
{
	return m_arr;
}

template<class T>
const T *rkit::Vector<T>::GetBuffer() const
{
	return m_arr;
}

template<class T>
size_t rkit::Vector<T>::Count() const
{
	return m_count;
}

template<class T>
rkit::Result rkit::Vector<T>::Reallocate(size_t newCapacity)
{
	constexpr size_t kMaxCount = std::numeric_limits<size_t>::max() / sizeof(T);

	if (newCapacity > kMaxCount)
		return ResultCode::kOutOfMemory;

	void *newMem = m_alloc->Alloc(newCapacity * sizeof(T));
	if (!newMem)
		return ResultCode::kOutOfMemory;

	T *newArr = static_cast<T *>(newMem);
	size_t count = m_count;

	for (size_t i = 0; i < count; i++)
		new (newArr + i) T(std::move(m_arr[i]));

	for (size_t i = 0; i < count; i++)
		m_arr[count - 1 - i].~T();

	m_alloc->Free(m_arr);

	m_arr = newArr;

	m_capacity = newCapacity;

	return ResultCode::kOK;
}

template<class T>
rkit::Result rkit::Vector<T>::EnsureCapacityForOneMore()
{
	if (m_count < m_capacity)
		return ResultCode::kOK;

	constexpr size_t kMaxCount = std::numeric_limits<size_t>::max() / sizeof(T);
	const size_t kInitialAllocSize = 8;

	if (m_capacity == kMaxCount)
		return ResultCode::kOutOfMemory;

	const size_t maxCountToAdd = kMaxCount - m_capacity;

	size_t countToAdd = m_capacity / 2u;
	if (countToAdd < kInitialAllocSize)
		countToAdd = kInitialAllocSize;

	if (countToAdd > maxCountToAdd)
		countToAdd = maxCountToAdd;

	if (countToAdd < 8)
		return ResultCode::kOutOfMemory;

	return Reallocate(m_capacity + countToAdd);
}

template<class T>
rkit::Span<T> rkit::Vector<T>::ToSpan()
{
	return Span<T>(m_arr, m_count);
}

template<class T>
rkit::Span<const T> rkit::Vector<T>::ToSpan() const
{
	return Span<const T>(m_arr, m_count);
}

template<class T>
T &rkit::Vector<T>::operator[](size_t index)
{
	RKIT_ASSERT(index < m_count);
	return m_arr[index];
}

template<class T>
const T &rkit::Vector<T>::operator[](size_t index) const
{
	RKIT_ASSERT(index < m_count);
	return m_arr[index];
}

template<class T>
rkit::Vector<T> &rkit::Vector<T>::operator=(Vector &&other)
{
	Reset();

	m_alloc = other.m_alloc;
	m_arr = other.m_arr;
	m_count = other.m_count;
	m_capacity = other.m_capacity;

	other.m_arr = nullptr;
	other.m_count = 0;
	other.m_capacity = 0;

	return *this;
}


template<class T>
typename rkit::Vector<T>::Iterator_t rkit::Vector<T>::begin()
{
	return ToSpan().begin();
}

template<class T>
typename rkit::Vector<T>::ConstIterator_t rkit::Vector<T>::begin() const
{
	return ToSpan().begin();
}

template<class T>
typename rkit::Vector<T>::Iterator_t rkit::Vector<T>::end()
{
	return ToSpan().end();
}

template<class T>
typename rkit::Vector<T>::ConstIterator_t rkit::Vector<T>::end() const
{
	return ToSpan().end();
}
