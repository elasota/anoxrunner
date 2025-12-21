#pragma once

#include <stddef.h>

namespace rkit
{
	template<typename T, T... TValues>
	struct IntList;

	template<typename T, typename TIntList, T TValue>
	struct IntListAppend;
}

namespace rkit { namespace priv {
	template<size_t TIndex, bool TIndexIsZero, typename T, T... TValues>
	struct IntListElementHelper
	{
	};

	template<typename T, T TIndex, T... TValues>
	struct IntListElementHelper<0, true, T, TIndex, TValues...>
	{
		static constexpr T kValue = TIndex;
	};

	template<typename T, size_t TIndex, T TFirst, T... TMore>
	struct IntListElementHelper<TIndex, false, T, TFirst, TMore...>
		: public IntListElementHelper<(TIndex - 1), (TIndex == 1), T, TMore...>
	{
	};

	template<size_t TIndex, bool TIndexIsZero, typename T, T... TValues>
	struct IntListSplitAtHelper
	{
	};

	template<typename T, T TIndex, T... TValues>
	struct IntListSplitAtHelper<0, true, T, TIndex, TValues...>
	{
		typedef IntList<T, TValues...> Type_t;
	};

	template<typename T, size_t TIndex, T TFirst, T... TMore>
	struct IntListSplitAtHelper<TIndex, false, T, TFirst, TMore...>
		: public IntListSplitAtHelper<(TIndex - 1), (TIndex == 1), T, TMore...>
	{
	};

	template<bool TIsCountZero, typename T, T TCount, T TInitial, T TStep>
	struct IntListCreateSequenceHelper
	{
	};

	template<typename T, T TCount, T TInitial, T TStep>
	struct IntListCreateSequenceHelper<true, T, TCount, TInitial, TStep>
	{
		typedef IntList<T> Type_t;
	};

	template<typename T, T TCount, T TInitial, T TStep>
	struct IntListCreateSequenceHelper<false, T, TCount, TInitial, TStep>
	{
		typedef typename IntListCreateSequenceHelper<(TCount == 1), T, (TCount - 1), TInitial, TStep>::Type_t Prefix_t;
		typedef typename IntListAppend<T, Prefix_t, static_cast<T>(TInitial + (TCount - 1) * TStep)>::Type_t Type_t;
	};
} }

namespace rkit
{
	template<typename T, T... TValues>
	struct IntList
	{
	};

	template<typename T>
	struct IntListSize
	{
	};

	template<typename T>
	struct IntListSize<IntList<T>>
	{
		static constexpr size_t kValue = 0;
	};

	template<typename T, T TFirst, T... TMore>
	struct IntListSize<IntList<T, TFirst, TMore...>>
	{
		static constexpr size_t kValue = static_cast<size_t>(1) + IntListSize<IntList<T, TMore...>>::kValue;
	};

	template<typename T, typename TIntList, T TValue>
	struct IntListAppend
	{
	};

	template<typename T, T TValue, T... TPrefix>
	struct IntListAppend<T, IntList<T, TPrefix...>, TValue>
	{
		typedef IntList<T, TPrefix..., TValue> Type_t;
	};

	template<typename TPrefixList, typename TAppendList>
	struct IntListConcat
	{
	};

	template<typename T, T... TValues>
	struct IntListConcat<IntList<T, TValues...>, IntList<T>>
	{
		typedef IntList<T, TValues...> Type_t;
	};

	template<typename TPrefixList, typename T, T TFirst, T... TValues>
	struct IntListConcat<TPrefixList, IntList<T, TFirst, TValues...>>
	{
	private:
		typedef typename IntListAppend<T, TPrefixList, TFirst>::Type_t NewPrefix_t;

	public:
		typedef typename IntListConcat<NewPrefix_t, IntList<T, TValues...>>::Type_t Type_t;
	};

	template<size_t TIndex, typename T>
	struct IntListElement
	{
	};

	template<size_t TIndex, typename T, T TFirst, T... TMore>
	struct IntListElement<TIndex, IntList<T, TFirst, TMore...>>
		: public priv::IntListElementHelper<TIndex, (TIndex == 0), T, TFirst, TMore...>
	{
	};

	template<typename T>
	struct IntListIsSequential
	{
	};

	template<typename T>
	struct IntListIsSequential<IntList<T>>
	{
		static constexpr bool kValue = true;
	};

	template<typename T, T TFirst>
	struct IntListIsSequential<IntList<T, TFirst>>
	{
		static constexpr bool kValue = true;
	};

	template<typename T, T TFirst, T TSecond, T... TMore>
	struct IntListIsSequential<IntList<T, TFirst, TSecond, TMore...>>
	{
		static constexpr bool kValue =
			(TSecond > TFirst) && ((TFirst + 1) == TSecond) && IntListIsSequential<IntList<T, TSecond, TMore...>>::kValue;
	};

	template<typename T, T TCount, T TInitial = 0, T TStep = 1>
	struct IntListCreateSequence
	{
		typedef typename priv::IntListCreateSequenceHelper<(TCount == 0), T, TCount, TInitial, TStep>::Type_t Type_t;
	};
}
