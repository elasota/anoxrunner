#pragma once

namespace rkit
{
	namespace priv
	{
		template<class T, class TSigned, class TUnsigned, bool TIsUnsigned>
		struct ImplicitIntCastHelper
		{
		};

		template<class T, class TSigned, class TUnsigned>
		struct ImplicitIntCastHelper<T, TSigned, TUnsigned, true>
		{
			typedef TUnsigned ResolvedType_t;

			static TUnsigned Cast(T value)
			{
				return value;
			}
		};

		template<class T, class TSigned, class TUnsigned>
		struct ImplicitIntCastHelper<T, TSigned, TUnsigned, false>
		{
			typedef TSigned ResolvedType_t;

			static TSigned Cast(T value)
			{
				return value;
			}
		};
	}
}

#include <type_traits>

namespace rkit
{
	template<class T, class TSigned, class TUnsigned>
	typename ::rkit::priv::ImplicitIntCastHelper<T, TSigned, TUnsigned, true>::ResolvedType_t ImplicitIntCast(T value)
	{
		return ::rkit::priv::ImplicitIntCastHelper<T, TSigned, TUnsigned, ::std::is_signed<T>::value>::Cast(value);
	}
}
