#pragma once

#include "CoreDefs.h"
#include "RKitAssert.h"
#include "Tuple.h"

#include <cstddef>
#include <limits>

namespace rkit
{
	template<class... T>
	class StripedRef;
}

namespace rkit::priv
{
	template<class... T>
	struct StripedVectorLimits
	{
	};

	template<class T>
	struct StripedVectorLimits<T>
	{
		static constexpr size_t kMaxAlign = alignof(T);
		static constexpr size_t kMaxSize = sizeof(T);
	};

	template<class TFirst, class... TMore>
	struct StripedVectorLimits<TFirst, TMore...>
	{
		static constexpr size_t kMaxAlign = (alignof(TFirst) > StripedVectorLimits<TMore...>::kMaxAlign)
			? alignof(TFirst)
			: StripedVectorLimits<TMore...>::kMaxAlign;
		static constexpr size_t kMaxSize = (sizeof(TFirst) > StripedVectorLimits<TMore...>::kMaxSize)
			? sizeof(TFirst)
			: StripedVectorLimits<TMore...>::kMaxSize;
	};

	template<size_t TIndex, class... T>
	struct StripedVectorItemCopierByIndex
	{
		static void Construct(StripedRef<T...> dest, StripedRef<const T...> src);
		static void Assign(StripedRef<T...> dest, StripedRef<const T...> src);
	};

	template<size_t TIndex, class... T>
	struct StripedVectorItemMoverByIndex
	{
		static void Construct(StripedRef<T...> dest, StripedRef<T...> src);
		static void Assign(StripedRef<T...> dest, StripedRef<T...> src);
	};

	template<size_t TIndex, class... T>
	struct StripedVectorItemHandlerByIndex
	{
		static void Destruct(StripedRef<T...> ref) noexcept;
		static void DefaultConstruct(StripedRef<T...> ref);
	};

	template<class... T>
	struct StripedVectorItemCopier
	{
		static void Construct(StripedRef<T...> dest, StripedRef<const T...> src);
		static void Assign(StripedRef<T...> dest, StripedRef<const T...> src);
	};

	template<class... T>
	struct StripedVectorItemMover
	{
		static void Construct(StripedRef<T...> dest, StripedRef<T...> src);
		static void Assign(StripedRef<T...> dest, StripedRef<T...> src);
	};

	template<class TTuple, size_t TStartIndex, class... T>
	struct StripedVectorTupleInitializer
	{
	};

	template<class TTuple, size_t TStartIndex, class T>
	struct StripedVectorTupleInitializer<TTuple, TStartIndex, T>
	{
		static void InitTuple(TTuple &tuple, void *mem, const size_t *offsets);
	};

	template<class TTuple, size_t TStartIndex, class TFirst, class... TMore>
	struct StripedVectorTupleInitializer<TTuple, TStartIndex, TFirst, TMore...>
	{
		static void InitTuple(TTuple &tuple, void *mem, const size_t *offsets);
	};

	template<class... T>
	struct StripedVectorItemHandler
	{
		static void Destruct(StripedRef<T...> ref) noexcept;
		static void DefaultConstruct(StripedRef<T...> ref);
	};
}

namespace rkit
{
	struct IMallocDriver;

	template<class... T>
	class StripedSpan;

	template<class... T>
	class StripedRef;

	template<class... T>
	class StripedVector
	{
	public:
		typedef StripedRef<T...> Iterator_t;
		typedef StripedRef<const T...> ConstIterator_t;

		StripedVector();
		explicit StripedVector(IMallocDriver *alloc);
		StripedVector(StripedVector &&) noexcept;
		~StripedVector();

		void RemoveRange(size_t firstArg, size_t numArgs);
		void RemoveAtIndex(size_t index);
		void RemoveAt(Iterator_t it);
		Result ResetAndResize(size_t size);
		Result Resize(size_t size);
		Result Reserve(size_t size);
		void ShrinkToSize(size_t size) noexcept;

		Result InsertAt(size_t index, T... item);

		Result Append(T... item);

		StripedRef<T...> First();
		StripedRef<const T...> First() const;

		StripedRef<T...> Last();
		StripedRef<const T...> Last() const;

		StripedRef<T...> operator[](size_t index);
		StripedRef<const T...> operator[](size_t index) const;

		void Reset();

		size_t Count() const;

		IMallocDriver *GetAllocator() const;

		StripedSpan<T...> ToSpan();
		StripedSpan<const T...> ToSpan() const;

		StripedVector &operator=(StripedVector &&other) noexcept;

		Iterator_t begin();
		ConstIterator_t begin() const;

		Iterator_t end();
		ConstIterator_t end() const;

		template<size_t TIndex>
		typename priv::TupleTypeAtIndex<TIndex, T...>::Type_t *GetBufferAt();

		template<size_t TIndex>
		const typename priv::TupleTypeAtIndex<TIndex, T...>::Type_t *GetBufferAt() const;

		template<class TDesiredType>
		TDesiredType *GetBufferOf();

		template<class TDesiredType>
		const TDesiredType *GetBufferOf() const;

	private:
		StripedVector(const StripedVector &) = delete;
		StripedVector &operator=(const StripedVector &) = delete;

		template<class TBehavior>
		Result InsertWithBehavior(size_t index, T &&... items);

		Result Reallocate(size_t newCapacity);
		Result EnsureCapacityForOneMore();
		Result EnsureCapacityForMore(size_t extra);

		Tuple<T*...> m_arr;
		size_t m_count;
		size_t m_capacity;
		IMallocDriver *m_alloc;
	};
}

#include "Drivers.h"
#include "MallocDriver.h"
#include "Result.h"
#include "Span.h"
#include "TypeList.h"

#include <new>
#include <utility>

namespace rkit::priv
{
	template<size_t TIndex, class... T>
	void StripedVectorItemCopierByIndex<TIndex, T...>::Construct(StripedRef<T...> dest, StripedRef<const T...> src)
	{
		constexpr size_t kTypeListSize = TypeListSize<TypeList<T...>>::kValue;

		if constexpr (TIndex < kTypeListSize)
		{
			using ElementType_t = TypeListElement<TIndex, TypeList<T...>>::Type_t;

			ElementType_t &destRef = dest.GetAt<TIndex>();
			const ElementType_t srcRef = src.GetAt<TIndex>();

			new (&destRef) ElementType_t(srcRef);

			StripedVectorItemCopierByIndex<TIndex + 1, T...>::Construct(dest, src);
		}
	}

	template<size_t TIndex, class... T>
	void StripedVectorItemCopierByIndex<TIndex, T...>::Assign(StripedRef<T...> dest, const StripedRef<const T...> src)
	{
		constexpr size_t kTypeListSize = TypeListSize<TypeList<T...>>::kValue;

		if constexpr (TIndex < kTypeListSize)
		{
			using ElementType_t = TypeListElement<TIndex, TypeList<T...>>::Type_t;

			ElementType_t& destRef = dest.GetAt<TIndex>();
			const ElementType_t& srcRef = src.GetAt<TIndex>();

			destRef = srcRef;

			StripedVectorItemCopierByIndex<TIndex + 1, T...>::Construct(dest, src);
		}
	}

	template<size_t TIndex, class... T>
	void StripedVectorItemMoverByIndex<TIndex, T...>::Construct(StripedRef<T...> dest, StripedRef<T...> src)
	{
		constexpr size_t kTypeListSize = TypeListSize<TypeList<T...>>::kValue;

		if constexpr (TIndex < kTypeListSize)
		{
			using ElementType_t = TypeListElement<TIndex, TypeList<T...>>::Type_t;

			ElementType_t destRef = dest.GetAt<TIndex>();
			ElementType_t srcRef = src.GetAt<TIndex>();

			new (&destRef) ElementType_t(std::move(srcRef));

			StripedVectorItemCopierByIndex<TIndex + 1, T...>::Construct(dest, src);
		}
	}

	template<size_t TIndex, class... T>
	void StripedVectorItemMoverByIndex<TIndex, T...>::Assign(StripedRef<T...> dest, StripedRef<T...> src)
	{
		constexpr size_t kTypeListSize = TypeListSize<TypeList<T...>>::kValue;

		if constexpr (TIndex < kTypeListSize)
		{
			using ElementType_t = TypeListElement<TIndex, TypeList<T...>>::Type_t;

			ElementType_t& destRef = dest.GetAt<TIndex>();
			ElementType_t& srcRef = src.GetAt<TIndex>();

			destRef = std::move(srcRef);

			StripedVectorItemCopierByIndex<TIndex + 1, T...>::Construct(dest, src);
		}
	}

	template<size_t TIndex, class... T>
	void StripedVectorItemHandlerByIndex<TIndex, T...>::Destruct(StripedRef<T...> ref) noexcept
	{
		constexpr size_t kTypeListSize = TypeListSize<TypeList<T...>>::kValue;

		if constexpr (TIndex < kTypeListSize)
		{
			using ElementType_t = TypeListElement<TIndex, TypeList<T...>>::Type_t;

			ElementType_t& itemRef = ref.GetAt<TIndex>();

			itemRef.~ElementType_t();

			StripedVectorItemHandlerByIndex<TIndex + 1, T...>::Destruct(ref);
		}
	}

	template<size_t TIndex, class... T>
	void StripedVectorItemHandlerByIndex<TIndex, T...>::DefaultConstruct(StripedRef<T...> ref)
	{
		constexpr size_t kTypeListSize = TypeListSize<TypeList<T...>>::kValue;

		if constexpr (TIndex < kTypeListSize)
		{
			using ElementType_t = TypeListElement<TIndex, TypeList<T...>>::Type_t;

			ElementType_t *itemPtr = &ref.GetAt<TIndex>();

			new (itemPtr) ElementType_t();

			if constexpr (TIndex + 1 < kTypeListSize)
			{
				try
				{
					StripedVectorItemHandlerByIndex<TIndex + 1, T...>::DefaultConstruct(ref);
				}
				catch(...)
				{
					itemPtr->~ElementType_t();
					throw;
				}
			}
		}
	}

	template<class... T>
	void StripedVectorItemCopier<T...>::Construct(StripedRef<T...> dest, StripedRef<const T...> src)
	{
		StripedVectorItemCopierByIndex<0, T...>::Construct(dest, src);
	}

	template<class... T>
	void StripedVectorItemCopier<T...>::Assign(StripedRef<T...> dest, const StripedRef<const T...> src)
	{
		StripedVectorItemCopierByIndex<0, T...>::Assign(dest, src);
	}

	template<class... T>
	void StripedVectorItemMover<T...>::Construct(StripedRef<T...> dest, StripedRef<T...> src)
	{
		StripedVectorItemMoverByIndex<0, T...>::Construct(dest, src);
	}

	template<class... T>
	void StripedVectorItemMover<T...>::Assign(StripedRef<T...> dest, StripedRef<T...> src)
	{
		StripedVectorItemMoverByIndex<0, T...>::Assign(dest, src);
	}


	template<class TTuple, size_t TStartIndex, class T>
	void StripedVectorTupleInitializer<TTuple, TStartIndex, T>::InitTuple(TTuple &tuple, void *mem, const size_t *offsets)
	{
		T *&ptr = tuple.template GetAt<TStartIndex>();
		ptr = reinterpret_cast<T *>(static_cast<uint8_t *>(mem) + offsets[TStartIndex]);
	}

	template<class TTuple, size_t TStartIndex, class TFirst, class... TMore>
	void StripedVectorTupleInitializer<TTuple, TStartIndex, TFirst, TMore...>::InitTuple(TTuple &tuple, void *mem, const size_t *offsets)
	{
		StripedVectorTupleInitializer<TTuple, TStartIndex, TFirst>::InitTuple(tuple, mem, offsets);
		StripedVectorTupleInitializer<TTuple, TStartIndex + 1, TMore...>::InitTuple(tuple, mem, offsets);
	}

	template<class... T>
	void StripedVectorItemHandler<T...>::Destruct(StripedRef<T...> ref) noexcept
	{
		StripedVectorItemHandlerByIndex<0, T...>::Destruct(ref);
	}

	template<class... T>
	void StripedVectorItemHandler<T...>::DefaultConstruct(StripedRef<T...> ref)
	{
		StripedVectorItemHandlerByIndex<0, T...>::DefaultConstruct(ref);
	}
}

namespace rkit
{
	template<class... T>
	inline StripedVector<T...>::StripedVector()
		: m_alloc(GetDrivers().m_mallocDriver.Get())
		, m_count(0)
		, m_capacity(0)
		, m_arr(static_cast<T *>(nullptr)...)
	{
	}

	template<class... T>
	inline StripedVector<T...>::StripedVector(IMallocDriver *alloc)
		: m_alloc(alloc)
		, m_count(0)
		, m_capacity(0)
		, m_arr(static_cast<T *>(nullptr)...)
	{
	}

	template<class... T>
	inline StripedVector<T...>::StripedVector(StripedVector<T...> &&other) noexcept
		: m_alloc(other.m_alloc)
		, m_count(other.m_count)
		, m_capacity(other.m_capacity)
		, m_arr(other.m_arr)
	{
		other.m_count = 0;
		other.m_capacity = 0;
		other.m_arr = Tuple<T *...>(static_cast<T *>(nullptr)...);
	}

	template<class... T>
	StripedVector<T...>::~StripedVector()
	{
		Reset();
	}

	template<class... T>
	void StripedVector<T...>::RemoveRange(size_t firstIndex, size_t numElements)
	{
		if (numElements == 0)
			return;

		using Mover_t = priv::StripedVectorItemMover<T...>;

		RKIT_ASSERT(firstIndex <= m_count);
		RKIT_ASSERT(numElements <= (m_count - firstIndex));

		size_t indexAfterLast = firstIndex + numElements;
		size_t numElementsToMove = m_count - indexAfterLast;

		StripedRef<T...> arr = StripedRef<T...>(m_arr);
		size_t count = m_count;

		{
			StripedRef<T...> moveDest = arr + firstIndex;
			StripedRef<T...> moveSrc = arr + indexAfterLast;

			while (numElementsToMove > 0)
			{
				priv::StripedVectorItemMover<T...>::Assign(moveDest, moveSrc);
				++moveDest;
				++moveSrc;
				--numElementsToMove;
			}
		}

		if (count > 0)
		{
			StripedRef<T...> destructRef = arr + (count - 1);

			for (size_t i = 0; i < numElements; i++)
			{
				priv::StripedVectorItemHandler<T...>::Destruct(destructRef);
				--destructRef;
			}
		}

		m_count -= numElements;
	}

	template<class... T>
	void StripedVector<T...>::RemoveAtIndex(size_t index)
	{
		RemoveRange(index, 1);
	}

	template<class... T>
	void StripedVector<T...>::RemoveAt(Iterator_t it)
	{
		RemoveAtIndex(it - begin());
	}

	template<class... T>
	Result StripedVector<T...>::Resize(size_t size)
	{
		constexpr size_t kMaxSize = std::numeric_limits<size_t>::max() / (priv::StripedVectorLimits<T...>::kMaxSize);

		if (size > kMaxSize)
			RKIT_THROW(ResultCode::kOutOfMemory);

		if (m_capacity < size)
			Reallocate(size);

		StripedRef<T...> endRef = end();

		size_t initialCount = m_count;

		if (size > initialCount)
		{
			size_t newCount = initialCount;
			try
			{
				while (size > newCount)
				{
					priv::StripedVectorItemHandler<T...>::DefaultConstruct(endRef);
					++endRef;
					++newCount;
				}
			}
			catch (...)
			{
				while (newCount > initialCount)
				{
					priv::StripedVectorItemHandler<T...>::Destruct(endRef);
					--endRef;
					--newCount;
				}
				throw;
			}
		}
		else
		{
			size_t newCount = initialCount;
			while (size < newCount)
			{
				--endRef;
				--newCount;
				priv::StripedVectorItemHandler<T...>::Destruct(endRef);
			}
		}

		m_count = size;
	}

	template<class... T>
	Result StripedVector<T...>::ResetAndResize(size_t size)
	{
		Reset();
		Resize(size);
	}

	template<class... T>
	Result StripedVector<T...>::Reserve(size_t size)
	{
		if (m_capacity < size)
			Reallocate(size);
	}

	template<class... T>
	void StripedVector<T...>::ShrinkToSize(size_t size) noexcept
	{
		RKIT_ASSERT(size <= m_count);

		if (size > m_count)
			size = m_count;

		StripedRef<T...> endRef = end();

		size_t newCount = m_count;
		while (size < newCount)
		{
			priv::StripedVectorItemHandler<T...>::Destruct(endRef);
			--endRef;
			--newCount;
		}

		m_count = newCount;
	}

	template<class... T>
	Result StripedVector<T...>::InsertAt(size_t index, T... items)
	{
		return InsertWithBehavior<priv::StripedVectorItemMover<T...>>(index, static_cast<T &&>(items)...);
	}

	template<class... T>
	Result StripedVector<T...>::Append(T... items)
	{
		return InsertWithBehavior<priv::StripedVectorItemMover<T...>>(m_count, static_cast<T &&>(items)...);
	}

	template<class... T>
	StripedRef<T...> StripedVector<T...>::First()
	{
		RKIT_ASSERT(m_count > 0);
		return StripedRef<T...>(m_arr);
	}

	template<class... T>
	StripedRef<const T...> StripedVector<T...>::First() const
	{
		return const_cast<StripedVector<T...> *>(this)->First();
	}

	template<class... T>
	StripedRef<T...> StripedVector<T...>::Last()
	{
		RKIT_ASSERT(m_count > 0);
		return end() - 1;
	}

	template<class... T>
	StripedRef<const T...> StripedVector<T...>::Last() const
	{
		return const_cast<StripedVector<T...> *>(this)->Last();
	}

	template<class... T>
	StripedRef<T...> StripedVector<T...>::operator[](size_t index)
	{
		RKIT_ASSERT(index < m_count);
		return begin() + index;
	}

	template<class... T>
	StripedRef<const T...> StripedVector<T...>::operator[](size_t index) const
	{
		RKIT_ASSERT(index < m_count);
		return begin() + index;
	}

	template<class... T>
	template<class TBehavior>
	Result StripedVector<T...>::InsertWithBehavior(size_t index, T &&... items)
	{
		RKIT_ASSERT(index <= m_count);

		StripedRef<T...> itemsPtr(Tuple<T *...>((&items)...));
		const size_t itemsCount = items.Count();

		EnsureCapacityForMore(itemsCount);

		const size_t oldCount = m_count;
		StripedRef<T...> newArr = begin();

		const size_t newCount = m_count + itemsCount;

		const size_t relocateCount = oldCount - index;
		const size_t relocateConstructCount = (itemsCount < relocateCount) ? itemsCount : relocateCount;
		const size_t relocateConstructStop = newCount - relocateConstructCount;
		const size_t relocateAssignStop = newCount - relocateCount;

		{
			size_t relocatePos = newCount;

			StripedRef<T...> relocateRef = newArr + relocatePos;

			while (relocatePos > relocateConstructStop)
			{
				--relocatePos;
				--relocateRef;
				priv::StripedVectorItemMover<T...>::Construct(relocateRef, relocateRef - itemsCount);
			}
			while (relocatePos > relocateAssignStop)
			{
				--relocatePos;
				--relocateRef;
				priv::StripedVectorItemMover<T...>::Assign(relocateRef, relocateRef - itemsCount);
			}
		}

		size_t assignCount = oldCount - index;
		size_t constructCount = 0;

		if (assignCount > itemsCount)
			assignCount = itemsCount;

		StripedRef<T...> insertRef = newArr + index;

		size_t i = 0;
		while (i < assignCount)
		{
			TBehavior::Assign(insertRef, itemsPtr);
			++i;
			++insertRef;
			++itemsPtr;
		}

		while (i < itemsCount)
		{
			TBehavior::Construct(insertRef, itemsPtr);
			++i;
			++insertRef;
			++itemsPtr;
		}

		m_count = newCount;
	}

	template<class... T>
	void StripedVector<T...>::Reset()
	{
		size_t count = m_count;
		if (count > 0)
		{
			StripedRef<T...> endRef = end();

			while (count > 0)
			{
				--count;
				--endRef;
				priv::StripedVectorItemHandler<T...>::Destruct(endRef);
			}

			m_alloc->Free(m_arr.GetAt<0>());
		}

		m_arr = Tuple<T *...>(static_cast<T *>(nullptr)...);
		m_count = 0;
		m_capacity = 0;
	}

	template<class... T>
	size_t StripedVector<T...>::Count() const
	{
		return m_count;
	}

	template<class... T>
	IMallocDriver *StripedVector<T...>::GetAllocator() const
	{
		return m_alloc;
	}

	template<class... T>
	typename StripedVector<T...>::Iterator_t StripedVector<T...>::begin()
	{
		return StripedRef<T...>(m_arr);
	}

	template<class... T>
	typename StripedVector<T...>::ConstIterator_t StripedVector<T...>::begin() const
	{
		Tuple<const T*...> result;
		priv::StripedRefConstHelper<0, T...>::MakeConst(result, m_arr);
		return StripedRef<const T...>(result);
	}

	template<class... T>
	typename StripedVector<T...>::Iterator_t StripedVector<T...>::end()
	{
		return begin() + m_count;
	}

	template<class... T>
	typename StripedVector<T...>::ConstIterator_t StripedVector<T...>::end() const
	{
		return begin() + m_count;
	}

	template<class... T>
	template<size_t TIndex>
	typename priv::TupleTypeAtIndex<TIndex, T...>::Type_t *StripedVector<T...>::GetBufferAt()
	{
		return m_arr.GetAt<TIndex>();
	}

	template<class... T>
	template<size_t TIndex>
	const typename priv::TupleTypeAtIndex<TIndex, T...>::Type_t *StripedVector<T...>::GetBufferAt() const
	{
		return m_arr.GetAt<TIndex>();
	}

	template<class... T>
	template<class TDesiredType>
	TDesiredType *StripedVector<T...>::GetBufferOf()
	{
		return m_arr.GetUnique<TDesiredType *>();
	}

	template<class... T>
	template<class TDesiredType>
	const TDesiredType *StripedVector<T...>::GetBufferOf() const
	{
		return m_arr.GetUnique<TDesiredType *>();
	}

	template<class... T>
	Result StripedVector<T...>::Reallocate(size_t newCapacity)
	{
		constexpr size_t kNumTypes = TypeListSize<TypeList<T...>>::kValue;
		constexpr size_t kMaxCount = std::numeric_limits<size_t>::max() / priv::StripedVectorLimits<T...>::kMaxSize;

		if (newCapacity > kMaxCount)
			RKIT_THROW(ResultCode::kOutOfMemory);

		constexpr size_t kSizes[] =
		{
			sizeof(T)...
		};
		constexpr size_t kAligns[] =
		{
			alignof(T)...
		};

		size_t offsets[kNumTypes];

		size_t totalSize = 0;
		for (size_t i = 0; i < kNumTypes; i++)
		{
			const size_t alignment = kAligns[i];
			const size_t alignMask = alignment - 1u;
			const size_t padding = alignment - (totalSize & alignMask);

			if (padding != alignment)
			{
				if (std::numeric_limits<size_t>::max() - totalSize < padding)
					RKIT_THROW(ResultCode::kOutOfMemory);

				totalSize += padding;
			}

			offsets[i] = totalSize;

			const size_t itemSize = kSizes[i] * newCapacity;

			if (std::numeric_limits<size_t>::max() - totalSize < itemSize)
				RKIT_THROW(ResultCode::kOutOfMemory);

			totalSize += itemSize;
		}

		void *newMem = m_alloc->Alloc(totalSize);
		if (!newMem)
			RKIT_THROW(ResultCode::kOutOfMemory);

		Tuple<T *...> newArrTuple;
		priv::StripedVectorTupleInitializer<Tuple<T*...>, 0, T...>::InitTuple(newArrTuple, newMem, offsets);

		size_t count = m_count;

		{
			StripedRef<T...> newArr = StripedRef<T...>(newArrTuple);
			StripedRef<T...> oldArr = StripedRef<T...>(m_arr);

			for (size_t i = 0; i < count; i++)
			{
				priv::StripedVectorItemMover<T...>::Construct(newArr, oldArr);
				priv::StripedVectorItemHandler<T...>::Destruct(oldArr);
				++oldArr;
				++newArr;
			}

			void *oldPtr = &oldArr.GetAt<0>();

			if (oldPtr)
				m_alloc->Free(oldPtr);
		}

		m_arr = newArrTuple;
		m_capacity = newCapacity;
	}

	template<class... T>
	Result StripedVector<T...>::EnsureCapacityForOneMore()
	{
		if (m_count < m_capacity)
			RKIT_RETURN_OK;

		constexpr size_t kMaxCount = std::numeric_limits<size_t>::max() / priv::StripedVectorLimits<T...>::kMaxSize;
		const size_t kInitialAllocSize = 8;

		if (m_capacity == kMaxCount)
			RKIT_THROW(ResultCode::kOutOfMemory);

		const size_t maxCountToAdd = kMaxCount - m_capacity;

		size_t countToAdd = m_capacity / 2u;
		if (countToAdd < kInitialAllocSize)
			countToAdd = kInitialAllocSize;

		if (countToAdd > maxCountToAdd)
			countToAdd = maxCountToAdd;

		if (countToAdd < kInitialAllocSize)
			RKIT_THROW(ResultCode::kOutOfMemory);

		return Reallocate(m_capacity + countToAdd);
	}

	template<class... T>
	Result StripedVector<T...>::EnsureCapacityForMore(size_t extra)
	{
		if (m_capacity - m_count >= extra)
			RKIT_RETURN_OK;

		if (extra == 1)
			return EnsureCapacityForOneMore();

		constexpr size_t kMaxCount = std::numeric_limits<size_t>::max() / priv::StripedVectorLimits<T...>::kMaxSize;
		const size_t kInitialAllocSize = 8;

		const size_t maxCountToAdd = kMaxCount - m_capacity;

		if (maxCountToAdd < extra)
			RKIT_THROW(ResultCode::kOutOfMemory);

		size_t countToAdd = m_capacity / 2u;

		if (countToAdd < extra)
			countToAdd = extra;

		if (countToAdd < kInitialAllocSize)
			countToAdd = kInitialAllocSize;

		if (countToAdd > maxCountToAdd)
			countToAdd = maxCountToAdd;

		return Reallocate(m_capacity + countToAdd);
	}


	template<class... T>
	StripedVector<T...> &StripedVector<T...>::operator=(StripedVector &&other) noexcept
	{
		Reset();

		m_alloc = other.m_alloc;
		m_arr = other.m_arr;
		m_count = other.m_count;
		m_capacity = other.m_capacity;

		other.m_arr = Tuple<T*...>(static_cast<T *>(nullptr)...);
		other.m_count = 0;
		other.m_capacity = 0;

		return *this;
	}
}
