#pragma once

#include "TypeTraits.h"

#include <cstddef>

namespace rkit
{
	template<class... T>
	struct TypeTuple
	{
	};

	template<class TTupleA, class TTupleB>
	struct TypeTupleConcatenate
	{
	};

	template<class... TTupleATypes, class... TTupleBTypes>
	struct TypeTupleConcatenate<TypeTuple<TTupleATypes...>, TypeTuple<TTupleBTypes...>>
	{
		typedef TypeTuple<TTupleATypes..., TTupleBTypes...> Type_t;
	};

	template<class... TTupleATypes>
	struct TypeTupleConcatenate<TypeTuple<TTupleATypes...>, TypeTuple<>>
	{
		typedef TypeTuple<TTupleATypes...> Type_t;
	};

	template<class... TTupleBTypes>
	struct TypeTupleConcatenate<TypeTuple<>, TypeTuple<TTupleBTypes...>>
	{
		typedef TypeTuple<TTupleBTypes...> Type_t;
	};

	template<class TOnly>
	struct TypeTuple<TOnly>
	{
		static const size_t kNumTypes = 1;

		typedef TypeTuple<> RemoveLast_t;
		typedef TypeTuple<> RemoveFirst_t;
		typedef TOnly First_t;
		typedef TOnly Last_t;
	};

	template<>
	struct TypeTuple<>
	{
		static const size_t kNumTypes = 0;
	};

	template<class TFirst, class TSecond>
	struct TypeTuple<TFirst, TSecond>
	{
		typedef TypeTuple<TFirst> RemoveLast_t;
		typedef TypeTuple<TSecond> RemoveFirst_t;
		typedef TFirst First_t;
		typedef TSecond Last_t;

		static const size_t kNumTypes = 2;
	};

	template<class TFirst, class... TMore>
	struct TypeTuple<TFirst, TMore...>
	{
		typedef typename TypeTupleConcatenate<TypeTuple<TFirst>, typename TypeTuple<TMore...>::RemoveLast_t>::Type_t RemoveLast_t;
		typedef TypeTuple<TMore...> RemoveFirst_t;
		typedef TFirst First_t;
		typedef typename TypeTuple<TMore...>::Last_t Last_t;

		static const size_t kNumTypes = 1 + TypeTuple<TMore...>::kNumTypes;
	};

	template<class TTypeTuple, class TType>
	struct TypeTupleQuery
	{
	};

	template<class TType>
	struct TypeTupleQuery<TypeTuple<>, TType>
	{
		static const bool kContainsType = false;
		static const size_t kTypeIndex = static_cast<size_t>(-1);
	};

	template<class TType, class TOnlyType>
	struct TypeTupleQuery<TypeTuple<TOnlyType>, TType>
	{
		static const bool kContainsType = IsSameType<TOnlyType, TType>::kValue;
		static const size_t kTypeIndex = (kContainsType ? static_cast<size_t>(0) : static_cast<size_t>(-1));
	};

	template<class TType, class TFirstType, class... TMoreTypes>
	struct TypeTupleQuery<TypeTuple<TFirstType, TMoreTypes...>, TType>
	{
		static const bool kContainsType = (IsSameType<TFirstType, TType>::kValue) || TypeTupleQuery<TypeTuple<TMoreTypes...>, TType>::kContainsType;
		static const size_t kTypeIndex = (IsSameType<TFirstType, TType>::kValue) ? static_cast<size_t>(0) : static_cast<size_t>(1u + TypeTupleQuery<TypeTuple<TMoreTypes...>, TType>::kTypeIndex);
	};
}
