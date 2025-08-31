#pragma once

#include "CoreDefs.h"
#include "RKitAssert.h"

#include <cstddef>
#include <limits>

namespace rkit { namespace priv {
	template<class T>
	struct VectorItemCopier
	{
		static void Construct(T *dest, const T &src);
		static void Assign(T *dest, const T &src);
	};

	template<class T>
	struct VectorItemMover
	{
		static void Construct(T *dest, T &src);
		static void Assign(T *dest, T &src);
	};
} }

namespace rkit
{
	struct IMallocDriver;

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
		Result ResetAndResize(size_t size);
		Result Resize(size_t size);
		Result Reserve(size_t size);
		void ShrinkToSize(size_t size);

		Result InsertAt(size_t index, const T &item);
		Result InsertAt(size_t index, T &&item);
		Result InsertAt(size_t index, const Span<const T> &items);
		Result InsertAtMove(size_t index, const Span<T> &items);

		Result Append(const T &item);
		Result Append(T &&item);
		Result Append(const Span<const T> &items);
		Result AppendMove(const Span<T> &items);

		void Reset();

		T *GetBuffer();
		const T *GetBuffer() const;

		size_t Count() const;

		IMallocDriver *GetAllocator() const;

		Span<T> ToSpan();
		Span<const T> ToSpan() const;

		T &operator[](size_t index);
		const T &operator[](size_t index) const;

		Vector &operator=(Vector &&other) noexcept;

		Iterator_t begin();
		ConstIterator_t begin() const;

		Iterator_t end();
		ConstIterator_t end() const;

	private:
		Vector(const Vector &) = delete;
		Vector &operator=(const Vector &) = delete;

		template<class TBehavior, class TSrcItem>
		Result InsertWithBehavior(size_t index, const Span<TSrcItem> &items);

		Result Reallocate(size_t newCapacity);
		Result EnsureCapacityForOneMore();
		Result EnsureCapacityForMore(size_t extra);

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

namespace rkit { namespace priv {

	template<class T>
	void VectorItemCopier<T>::Construct(T *dest, const T &src)
	{
		new (dest) T(src);
	}

	template<class T>
	void VectorItemCopier<T>::Assign(T *dest, const T &src)
	{
		(*dest) = src;
	}

	template<class T>
	void VectorItemMover<T>::Construct(T *dest, T &src)
	{
		new (dest) T(std::move(src));
	}

	template<class T>
	void VectorItemMover<T>::Assign(T *dest, T &src)
	{
		(*dest) = std::move(src);
	}
} }

namespace rkit
{
	template<class T>
	inline Vector<T>::Vector()
		: m_alloc(GetDrivers().m_mallocDriver)
		, m_count(0)
		, m_capacity(0)
		, m_arr(nullptr)
	{
	}

	template<class T>
	inline Vector<T>::Vector(IMallocDriver *alloc)
		: m_alloc(alloc)
		, m_count(0)
		, m_capacity(0)
		, m_arr(nullptr)
	{
	}

	template<class T>
	inline Vector<T>::Vector(Vector<T> &&other) noexcept
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
	Vector<T>::~Vector()
	{
		Reset();
	}

	template<class T>
	void Vector<T>::RemoveRange(size_t firstIndex, size_t numElements)
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
	Result Vector<T>::Resize(size_t size)
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
	Result Vector<T>::ResetAndResize(size_t size)
	{
		Reset();

		constexpr size_t kMaxCount = std::numeric_limits<size_t>::max() / sizeof(T);

		if (size > kMaxCount)
			return ResultCode::kOutOfMemory;

		void *newMem = m_alloc->Alloc(size * sizeof(T));
		if (!newMem)
			return ResultCode::kOutOfMemory;

		T *newArr = static_cast<T *>(newMem);
		size_t count = m_count;

		m_arr = newArr;

		m_capacity = size;
		m_count = size;

		for (size_t i = 0; i < size; i++)
			new (newArr + i) T();

		return ResultCode::kOK;
	}

	template<class T>
	Result Vector<T>::Reserve(size_t size)
	{
		if (m_capacity < size)
		{
			RKIT_CHECK(Reallocate(size));
		}

		return ResultCode::kOK;
	}

	template<class T>
	void Vector<T>::ShrinkToSize(size_t size)
	{
		RKIT_ASSERT(size <= m_count);

		if (size > m_count)
			size = m_count;

		while (size < m_count)
		{
			m_count--;
			m_arr[m_count].~T();
		}
	}

	template<class T>
	Result Vector<T>::InsertAt(size_t index, const T &item)
	{
		return InsertAt(index, Span<const T>(&item, 1));
	}

	template<class T>
	Result Vector<T>::InsertAt(size_t index, T &&item)
	{
		return InsertAtMove(index, Span<T>(&item, 1));
	}

	template<class T>
	Result Vector<T>::InsertAt(size_t index, const Span<const T> &items)
	{
		return InsertWithBehavior<priv::VectorItemCopier<T>>(index, items);
	}

	template<class T>
	Result Vector<T>::InsertAtMove(size_t index, const Span<T> &items)
	{
		return InsertWithBehavior<priv::VectorItemMover<T>>(index, items);
	}

	template<class T>
	Result Vector<T>::Append(const T &item)
	{
		return InsertAt(m_count, item);
	}

	template<class T>
	Result Vector<T>::Append(T &&item)
	{
		return InsertAt(m_count, std::move(item));
	}

	template<class T>
	Result Vector<T>::Append(const Span<const T> &items)
	{
		return InsertAt(m_count, items);
	}

	template<class T>
	Result Vector<T>::AppendMove(const Span<T> &items)
	{
		return InsertAtMove(m_count, items);
	}

	template<class T>
	template<class TBehavior, class TSrcItem>
	Result Vector<T>::InsertWithBehavior(size_t index, const Span<TSrcItem> &items)
	{
		RKIT_ASSERT(index <= m_count);

		T *oldArr = m_arr;
		const size_t oldCount = m_count;

		TSrcItem *itemsPtr = items.Ptr();
		const size_t itemsCount = items.Count();

		if (oldCount > 0 && itemsPtr >= oldArr && itemsPtr < (oldArr + oldCount))
		{
			// Inserting existing items
			size_t srcIndex = itemsPtr - oldArr;
			RKIT_CHECK(EnsureCapacityForMore(itemsCount));

			itemsPtr = m_arr + srcIndex;

			if (srcIndex >= index)
				itemsPtr += itemsCount;
		}
		else
		{
			RKIT_CHECK(EnsureCapacityForMore(itemsCount));
		}

		T *newArr = m_arr;
		const size_t newCount = m_count + itemsCount;

		const size_t relocateCount = oldCount - index;
		const size_t relocateConstructCount = (itemsCount < relocateCount) ? itemsCount : relocateCount;
		const size_t relocateConstructStop = newCount - relocateConstructCount;
		const size_t relocateAssignStop = newCount - relocateCount;

		size_t relocatePos = newCount;
		while (relocatePos > relocateConstructStop)
		{
			relocatePos--;
			new (newArr + relocatePos) T(std::move(newArr[relocatePos - itemsCount]));
		}
		while (relocatePos > relocateAssignStop)
		{
			relocatePos--;
			newArr[relocatePos] = std::move(newArr[relocatePos - itemsCount]);
		}

		size_t assignCount = oldCount - index;
		size_t constructCount = 0;

		if (assignCount > itemsCount)
			assignCount = itemsCount;

		size_t i = 0;
		while (i < assignCount)
		{
			TBehavior::Assign(&newArr[index + i], itemsPtr[i]);
			i++;
		}

		while (i < itemsCount)
		{
			TBehavior::Construct(&newArr[index + i], itemsPtr[i]);
			i++;
		}

		m_count = newCount;
		return ResultCode::kOK;
	}

	template<class T>
	void Vector<T>::Reset()
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
	T *Vector<T>::GetBuffer()
	{
		return m_arr;
	}

	template<class T>
	const T *Vector<T>::GetBuffer() const
	{
		return m_arr;
	}

	template<class T>
	size_t Vector<T>::Count() const
	{
		return m_count;
	}

	template<class T>
	IMallocDriver *Vector<T>::GetAllocator() const
	{
		return m_alloc;
	}

	template<class T>
	Result Vector<T>::Reallocate(size_t newCapacity)
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
	Result Vector<T>::EnsureCapacityForOneMore()
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

		if (countToAdd < kInitialAllocSize)
			return ResultCode::kOutOfMemory;

		return Reallocate(m_capacity + countToAdd);
	}

	template<class T>
	Result Vector<T>::EnsureCapacityForMore(size_t extra)
	{
		if (m_capacity - m_count >= extra)
			return ResultCode::kOK;

		if (extra == 1)
			return EnsureCapacityForOneMore();

		constexpr size_t kMaxCount = std::numeric_limits<size_t>::max() / sizeof(T);
		const size_t kInitialAllocSize = 8;

		const size_t maxCountToAdd = kMaxCount - m_capacity;

		if (maxCountToAdd < extra)
			return ResultCode::kOutOfMemory;

		size_t countToAdd = m_capacity / 2u;

		if (countToAdd < extra)
			countToAdd = extra;

		if (countToAdd < kInitialAllocSize)
			countToAdd = kInitialAllocSize;

		if (countToAdd > maxCountToAdd)
			countToAdd = maxCountToAdd;

		return Reallocate(m_capacity + countToAdd);
	}

	template<class T>
	Span<T> Vector<T>::ToSpan()
	{
		return Span<T>(m_arr, m_count);
	}

	template<class T>
	Span<const T> Vector<T>::ToSpan() const
	{
		return Span<const T>(m_arr, m_count);
	}

	template<class T>
	T &Vector<T>::operator[](size_t index)
	{
		RKIT_ASSERT(index < m_count);
		return m_arr[index];
	}

	template<class T>
	const T &Vector<T>::operator[](size_t index) const
	{
		RKIT_ASSERT(index < m_count);
		return m_arr[index];
	}

	template<class T>
	Vector<T> &Vector<T>::operator=(Vector &&other) noexcept
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
	typename Vector<T>::Iterator_t Vector<T>::begin()
	{
		return ToSpan().begin();
	}

	template<class T>
	typename Vector<T>::ConstIterator_t Vector<T>::begin() const
	{
		return ToSpan().begin();
	}

	template<class T>
	typename Vector<T>::Iterator_t Vector<T>::end()
	{
		return ToSpan().end();
	}

	template<class T>
	typename Vector<T>::ConstIterator_t Vector<T>::end() const
	{
		return ToSpan().end();
	}
}
