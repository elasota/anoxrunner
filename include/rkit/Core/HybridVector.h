#pragma once

#include "Vector.h"

#include "RKitAssert.h"

#include <cstddef>
#include <limits>

namespace rkit
{
	struct IMallocDriver;

	template<class T>
	class Span;

	template<class T>
	class SpanIterator;

	template<class T, size_t TStaticSize>
	class HybridVector
	{
	public:
		typedef SpanIterator<T> Iterator_t;
		typedef SpanIterator<const T> ConstIterator_t;

		HybridVector();
		explicit HybridVector(IMallocDriver *alloc);
		HybridVector(HybridVector<T, TStaticSize> &&) noexcept;
		HybridVector(Vector<T> &&) noexcept;

		~HybridVector();

		void RemoveRange(size_t firstArg, size_t numArgs);
		Result Resize(size_t size);
		Result Reserve(size_t size);
		void ShrinkToSize(size_t size);

		Result Append(const T &item);
		Result Append(T &&item);
		Result Append(const Span<const T> &items);

		void Reset();

		T *GetBuffer();
		const T *GetBuffer() const;

		size_t Count() const;

		Span<T> ToSpan();
		Span<const T> ToSpan() const;

		T &operator[](size_t index);
		const T &operator[](size_t index) const;

		HybridVector &operator=(HybridVector &&other) noexcept;

		Iterator_t begin();
		ConstIterator_t begin() const;

		Iterator_t end();
		ConstIterator_t end() const;

	private:
		struct StaticStorageBuffer
		{
			StaticStorageBuffer() = delete;
			StaticStorageBuffer(const StaticStorageBuffer &) = delete;

			explicit StaticStorageBuffer(IMallocDriver *alloc);
			StaticStorageBuffer(StaticStorageBuffer &&other);
			StaticStorageBuffer(IMallocDriver *alloc, T *elementsToMove, size_t countToMove);

			StaticStorageBuffer &operator=(StaticStorageBuffer &&other);
			StaticStorageBuffer &operator=(const StaticStorageBuffer &other) = delete;

			T *GetStorage();
			const T *GetStorage() const;

			alignas(alignof(T)) uint8_t m_staticStorage[sizeof(T) * TStaticSize];
			size_t m_size;
			IMallocDriver *m_alloc;
		};

		union StorageUnion
		{
			StorageUnion(IMallocDriver *alloc);
			StorageUnion(StaticStorageBuffer &&other);
			StorageUnion(IMallocDriver *alloc, T *elementsToMove, size_t countToMove);
			StorageUnion(Vector<T> &&vector);
			~StorageUnion();

			StaticStorageBuffer m_staticStorage;
			Vector<T> m_vector;
		};

		HybridVector(const HybridVector &) = delete;
		HybridVector &operator=(const HybridVector &) = delete;

		bool IsUsingStaticStorage() const;

		StorageUnion m_storage;
		bool m_isUsingStaticStorage;
	};
}

namespace rkit
{
	template<class T, size_t TStaticSize>
	HybridVector<T, TStaticSize>::StaticStorageBuffer::StaticStorageBuffer(IMallocDriver *alloc)
		: m_alloc(alloc)
		, m_size(0)
	{
	}

	template<class T, size_t TStaticSize>
	HybridVector<T, TStaticSize>::StaticStorageBuffer::StaticStorageBuffer(StaticStorageBuffer &&other)
		: m_alloc(other.m_alloc)
		, m_size(other.m_size)
	{
		const size_t count = m_size;
		T *thisArr = GetStorage();
		T *otherArr = other.GetStorage();

		for (size_t i = 0; i < count; i++)
			new (thisArr + i) T(std::move(otherArr[i]));
	}

	template<class T, size_t TStaticSize>
	HybridVector<T, TStaticSize>::StaticStorageBuffer::StaticStorageBuffer(IMallocDriver *alloc, T *elementsToMove, size_t countToMove)
		: m_alloc(alloc)
		, m_size(countToMove)
	{
		T *thisArr = GetStorage();

		for (size_t i = 0; i < countToMove; i++)
			new (thisArr + i) T(std::move(elementsToMove[i]));
	}

	template<class T, size_t TStaticSize>
	typename HybridVector<T, TStaticSize>::StaticStorageBuffer &HybridVector<T, TStaticSize>::StaticStorageBuffer::operator=(HybridVector<T, TStaticSize>::StaticStorageBuffer &&other)
	{
		const size_t thisSize = m_size;
		const size_t otherSize = other.m_size;

		if (otherSize < thisSize)
		{
			size_t assignStop = otherSize;
			size_t destroyStop = thisSize;

			T *thisArr = GetStorage();
			T *otherArr = other.GetStorage();

			size_t i = 0;
			while (i < assignStop)
			{
				thisArr[i] = std::move(otherArr[i]);
				i++;
			}

			while (i < destroyStop)
			{
				thisArr[i].~T();
				i++;
			}
		}
		else
		{
			size_t assignStop = thisSize;
			size_t createStop = otherSize;

			T *thisArr = GetStorage();
			T *otherArr = other.GetStorage();

			size_t i = 0;
			while (i < assignStop)
			{
				thisArr[i] = std::move(otherArr[i]);
				i++;
			}

			while (i < createStop)
			{
				new (thisArr + i) T(std::move(otherArr[i]));
				i++;
			}
		}

		m_size = otherSize;

		return *this;
	}

	template<class T, size_t TStaticSize>
	inline T *HybridVector<T, TStaticSize>::StaticStorageBuffer::GetStorage()
	{
		return reinterpret_cast<T *>(m_staticStorage);
	}

	template<class T, size_t TStaticSize>
	const T *HybridVector<T, TStaticSize>::StaticStorageBuffer::GetStorage() const
	{
		return reinterpret_cast<const T *>(m_staticStorage);
	}


	template<class T, size_t TStaticSize>
	HybridVector<T, TStaticSize>::StorageUnion::StorageUnion(IMallocDriver *alloc)
		: m_staticStorage(alloc)
	{
	}

	template<class T, size_t TStaticSize>
	HybridVector<T, TStaticSize>::StorageUnion::StorageUnion(StaticStorageBuffer &&other)
		: m_staticStorage(std::move(other))
	{
	}

	template<class T, size_t TStaticSize>
	HybridVector<T, TStaticSize>::StorageUnion::StorageUnion(IMallocDriver *alloc, T *elementsToMove, size_t countToMove)
		: m_staticStorage(alloc, elementsToMove, countToMove)
	{
	}

	template<class T, size_t TStaticSize>
	HybridVector<T, TStaticSize>::StorageUnion::StorageUnion(Vector<T> &&vector)
		: m_vector(std::move(vector))
	{
	}

	template<class T, size_t TStaticSize>
	HybridVector<T, TStaticSize>::StorageUnion::~StorageUnion()
	{
	}

	template<class T, size_t TStaticSize>
	inline HybridVector<T, TStaticSize>::HybridVector()
		: m_storage(GetDrivers().m_mallocDriver)
		, m_isUsingStaticStorage(true)
	{
	}

	template<class T, size_t TStaticSize>
	inline HybridVector<T, TStaticSize>::HybridVector(IMallocDriver *alloc)
		: m_storage(GetDrivers().m_mallocDriver)
		, m_isUsingStaticStorage(true)
	{
	}

	template<class T, size_t TStaticSize>
	inline HybridVector<T, TStaticSize>::HybridVector(HybridVector<T, TStaticSize> &&other) noexcept
		: m_storage(nullptr)
		, m_isUsingStaticStorage(other.m_isUsingStaticStorage)
	{
		if (other.m_isUsingStaticStorage)
			m_storage.m_staticStorage = std::move(other.m_storage.m_staticStorage);
		else
		{
			m_storage.m_staticStorage.~StaticStorageBuffer();
			new (&m_storage.m_vector) Vector<T>(std::move(other.m_storage.m_vector));
		}
	}

	template<class T, size_t TStaticSize>
	HybridVector<T, TStaticSize>::~HybridVector()
	{
		Reset();

		m_storage.m_staticStorage.~StaticStorageBuffer();
	}

	template<class T, size_t TStaticSize>
	void HybridVector<T, TStaticSize>::RemoveRange(size_t firstIndex, size_t numElements)
	{
		if (numElements == 0)
			return;

		size_t count = Count();

		RKIT_ASSERT(firstIndex <= count);
		RKIT_ASSERT(numElements <= (count - firstIndex));

		size_t indexAfterLast = firstIndex + numElements;
		size_t numElementsToMove = count - indexAfterLast;

		T *arr = GetBuffer();
		for (size_t i = 0; i < numElementsToMove; i++)
			arr[i + firstIndex] = std::move(arr[i + indexAfterLast]);

		for (size_t i = 0; i < numElements; i++)
			arr[count - 1 - i].~T();

		ShrinkToSize(count - numElements);
	}

	template<class T, size_t TStaticSize>
	Result HybridVector<T, TStaticSize>::Resize(size_t size)
	{
		RKIT_CHECK(Reserve(size));

		if (IsUsingStaticStorage())
		{
			T *arr = m_storage.m_staticStorage.GetStorage();
			size_t count = m_storage.m_staticStorage.m_size;

			while (size < count)
			{
				count--;
				arr[count].~T();
			}

			while (size > count)
			{
				new (arr + count) T();
				count++;
			}

			m_storage.m_staticStorage.m_size = count;

			return ResultCode::kOK;
		}
		else
			return m_storage.m_vector.Resize(size);
	}

	template<class T, size_t TStaticSize>
	Result HybridVector<T, TStaticSize>::Reserve(size_t size)
	{
		constexpr size_t kMaxSize = std::numeric_limits<size_t>::max() / sizeof(T);

		if (size > kMaxSize)
			return ResultCode::kOutOfMemory;

		if (!IsUsingStaticStorage())
			return m_storage.m_vector.Reserve(size);

		if (size > TStaticSize)
		{
			IMallocDriver *alloc = m_storage.m_staticStorage.m_alloc;

			Vector<T> newVector(alloc);

			RKIT_CHECK(newVector.Reserve(size));
			RKIT_CHECK(newVector.AppendMove(ToSpan()));

			m_storage.m_staticStorage.~StaticStorageBuffer();
			new (&m_storage.m_vector) Vector<T>(std::move(newVector));

			m_isUsingStaticStorage = false;
		}

		return ResultCode::kOK;
	}

	template<class T, size_t TStaticSize>
	void HybridVector<T, TStaticSize>::ShrinkToSize(size_t size)
	{
		if (IsUsingStaticStorage())
		{
			T *arr = m_storage.m_staticStorage.GetBuffer();
			size_t count = m_storage.m_staticStorage.m_size;

			RKIT_ASSERT(size < count);

			if (size > count)
				size = count;

			while (size < count)
			{
				count--;
				arr[count].~T();
			}

			m_storage.m_staticStorage.m_size = size;
		}
		else
			m_storage.m_vector.ShrinkToSize(size);
	}

	template<class T, size_t TStaticSize>
	Result HybridVector<T, TStaticSize>::Append(const T &item)
	{
		if (IsUsingStaticStorage())
		{
			size_t count = m_storage.m_staticStorage.m_size;
			if (count < TStaticSize)
			{
				new (m_storage.m_staticStorage.GetStorage() + count) T(item);
				m_storage.m_staticStorage.m_size = count + 1;

				return ResultCode::kOK;
			}
			else
			{
				RKIT_CHECK(Reserve(TStaticSize + 1));
			}
		}

		return m_storage.m_vector.Append(item);
	}

	template<class T, size_t TStaticSize>
	Result HybridVector<T, TStaticSize>::Append(const Span<const T> &items)
	{
		if (IsUsingStaticStorage())
		{
			size_t count = m_storage.m_staticStorage.m_size;
			if (items.Count() <= (TStaticSize - count))
			{
				T *arr = m_storage.m_staticStorage.GetStorage();

				for (const T &item : items)
				{
					new (arr + count) T(item);
					count++;
				}

				m_storage.m_staticStorage.m_size = count;

				return ResultCode::kOK;
			}
			else
			{
				constexpr size_t kMaxSize = std::numeric_limits<size_t>::max() / sizeof(T);

				if ((kMaxSize - count) < items.Count())
					return ResultCode::kOutOfMemory;

				RKIT_CHECK(Reserve(count + items.Count()));
			}
		}

		return m_storage.m_vector.Append(items);
	}

	template<class T, size_t TStaticSize>
	Result HybridVector<T, TStaticSize>::Append(T &&item)
	{
		if (IsUsingStaticStorage())
		{
			size_t count = m_storage.m_staticStorage.m_size;
			if (count < TStaticSize)
			{
				new (m_storage.m_staticStorage.GetStorage() + count) T(std::move(item));
				m_storage.m_staticStorage.m_size = count + 1;

				return ResultCode::kOK;
			}
			else
			{
				RKIT_CHECK(Reserve(TStaticSize + 1));
			}
		}

		return m_storage.m_vector.Append(std::move(item));
	}

	template<class T, size_t TStaticSize>
	void HybridVector<T, TStaticSize>::Reset()
	{
		if (IsUsingStaticStorage())
		{
			size_t count = m_storage.m_staticStorage.m_size;
			T *arr = m_storage.m_staticStorage.GetStorage();

			while (count > 0)
			{
				count--;
				arr[count].~T();
			}

			m_storage.m_staticStorage.m_size = 0;
		}
		else
		{
			IMallocDriver *alloc = m_storage.m_vector.GetAllocator();

			m_storage.m_vector.~Vector<T>();
			new (&m_storage.m_staticStorage) StaticStorageBuffer(alloc);

			m_isUsingStaticStorage = true;
		}
	}

	template<class T, size_t TStaticSize>
	T *HybridVector<T, TStaticSize>::GetBuffer()
	{
		if (IsUsingStaticStorage())
			return m_storage.m_staticStorage.GetStorage();
		else
			return m_storage.m_vector.GetBuffer();
	}

	template<class T, size_t TStaticSize>
	const T *HybridVector<T, TStaticSize>::GetBuffer() const
	{
		if (IsUsingStaticStorage())
			return m_storage.m_staticStorage.GetStorage();
		else
			return m_storage.m_vector.GetBuffer();
	}

	template<class T, size_t TStaticSize>
	size_t HybridVector<T, TStaticSize>::Count() const
	{
		if (IsUsingStaticStorage())
			return m_storage.m_staticStorage.m_size;
		else
			return m_storage.m_vector.Count();
	}

	template<class T, size_t TStaticSize>
	Span<T> HybridVector<T, TStaticSize>::ToSpan()
	{
		if (IsUsingStaticStorage())
			return Span<T>(m_storage.m_staticStorage.GetStorage(), m_storage.m_staticStorage.m_size);
		else
			return m_storage.m_vector.ToSpan();
	}

	template<class T, size_t TStaticSize>
	Span<const T> HybridVector<T, TStaticSize>::ToSpan() const
	{
		if (IsUsingStaticStorage())
			return Span<const T>(m_storage.m_staticStorage.GetStorage(), m_storage.m_staticStorage.m_size);
		else
			return m_storage.m_vector.ToSpan();
	}

	template<class T, size_t TStaticSize>
	T &HybridVector<T, TStaticSize>::operator[](size_t index)
	{
		if (IsUsingStaticStorage())
		{
			RKIT_ASSERT(index < m_storage.m_staticStorage.m_size);
			return m_storage.m_staticStorage.GetStorage()[index];
		}
		else
			return m_storage.m_vector[index];
	}

	template<class T, size_t TStaticSize>
	const T &HybridVector<T, TStaticSize>::operator[](size_t index) const
	{
		if (IsUsingStaticStorage())
		{
			RKIT_ASSERT(index < m_storage.m_staticStorage.m_size);
			return m_storage.m_staticStorage.GetStorage()[index];
		}
		else
			return m_storage.m_vector[index];
	}

	template<class T, size_t TStaticSize>
	HybridVector<T, TStaticSize> &HybridVector<T, TStaticSize>::operator=(HybridVector<T, TStaticSize> &&other) noexcept
	{
		if (other.IsUsingStaticStorage())
		{
			if (IsUsingStaticStorage())
				m_storage.m_staticStorage = std::move(other.m_storage.m_staticStorage);
			else
			{
				m_storage.m_vector.~Vector<T>();
				new (&m_storage.m_staticStorage) StaticStorageBuffer(std::move(other.m_storage.m_staticStorage));

				m_isUsingStaticStorage = true;
			}
		}
		else
		{
			if (IsUsingStaticStorage())
			{
				m_storage.m_staticStorage.~StaticStorageBuffer();
				new (&m_storage.m_vector) Vector<T>(std::move(other.m_storage.m_vector));

				m_isUsingStaticStorage = false;
			}
			else
			{
				m_storage.m_vector.~Vector<T>();
				new (&m_storage.m_staticStorage) StaticStorageBuffer(std::move(other.m_storage.m_staticStorage));
			}
		}

		return *this;
	}

	template<class T, size_t TStaticSize>
	typename HybridVector<T, TStaticSize>::Iterator_t HybridVector<T, TStaticSize>::begin()
	{
		return ToSpan().begin();
	}

	template<class T, size_t TStaticSize>
	typename HybridVector<T, TStaticSize>::ConstIterator_t HybridVector<T, TStaticSize>::begin() const
	{
		return ToSpan().begin();
	}

	template<class T, size_t TStaticSize>
	typename HybridVector<T, TStaticSize>::Iterator_t HybridVector<T, TStaticSize>::end()
	{
		return ToSpan().end();
	}

	template<class T, size_t TStaticSize>
	typename HybridVector<T, TStaticSize>::ConstIterator_t HybridVector<T, TStaticSize>::end() const
	{
		return ToSpan().end();
	}

	template<class T, size_t TStaticSize>
	inline bool HybridVector<T, TStaticSize>::IsUsingStaticStorage() const
	{
		return m_isUsingStaticStorage;
	}
}
