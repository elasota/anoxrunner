#pragma once

#include "Vector.h"
#include "Trait.h"

namespace rkit { namespace priv {
	template<class T>
	struct VectorMethodsTmpl
	{
	};
} }

namespace rkit
{
	template<class T>
	struct VectorTraitTag;

	enum class VectorTraitMethods
	{
		Append,
		AppendRValue,
		Reserve,
		Count,
	};

	template<class T> RKIT_TRAIT_FUNC_TEMPLATED(VectorTraitTag<T>, VectorTraitMethods, Append, Result(const T& item));
	template<class T> RKIT_TRAIT_FUNC_BIND_TEMPLATED(VectorTraitTag<T>, VectorTraitMethods, AppendRValue, Append, Result(T &&item));
	template<class T> RKIT_TRAIT_FUNC_TEMPLATED(VectorTraitTag<T>, VectorTraitMethods, Reserve, Result(size_t size));
	template<class T> RKIT_TRAIT_FUNC_TEMPLATED(VectorTraitTag<T>, VectorTraitMethods, Count, size_t());

	template<class T>
	struct VectorTrait : public rkit::traits::Trait<VectorTraitMethods, VectorTraitTag<T>> {};
}
