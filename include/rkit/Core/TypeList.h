#pragma once

#include <cstddef>

namespace rkit
{
	template<class... T>
	struct TypeList
	{
	};

	template<class TTypeList>
	struct TypeListSize
	{
	};

	template<>
	struct TypeListSize<TypeList<>>
	{
		static const size_t kSize = 0;
	};

	template<class TType>
	struct TypeListSize<TypeList<TType>>
	{
		static const size_t kSize = 1;
	};

	template<class TFirstType, class... TMoreTypes>
	struct TypeListSize<TypeList<TFirstType, TMoreTypes...>>
	{
		static const size_t kSize = TypeListSize<TypeList<TMoreTypes...>>::kSize + 1;
	};

	template<size_t TIndex, class TTypeList>
	struct TypeListElement
	{
	};

	template<class TFirstType, class... TMoreTypes>
	struct TypeListElement<0, TypeList<TFirstType, TMoreTypes...>>
	{
		typedef TFirstType Resolution_t;
	};

	template<class TType>
	struct TypeListElement<0, TypeList<TType>>
	{
		typedef TType Resolution_t;
	};

	template<size_t TIndex, class TFirstType, class... TMoreTypes>
	struct TypeListElement<TIndex, TypeList<TFirstType, TMoreTypes...>>
	{
		typedef typename TypeListElement<TIndex - 1, TypeList<TMoreTypes...>>::Resolution_t Resolution_t;
	};

	template<class TTypeList, class TAdditional>
	struct TypeListAppend
	{
	};

	template<class TAdditional>
	struct TypeListAppend<TypeList<>, TAdditional>
	{
		typedef TypeList<TAdditional> Type_t;
	};

	template<class... TTypes, class TAdditional>
	struct TypeListAppend<TypeList<TTypes...>, TAdditional>
	{
		typedef TypeList<TTypes..., TAdditional> Type_t;
	};
}
