#pragma once

#include "TypeTuple.h"
#include "TypeTraits.h"

namespace rkit
{
	namespace priv
	{
		typedef void *(*RecastRefFunc_t)(void *ptr);

		template<class TSourceType, class TTargetType>
		void *DynamicCastRecastFunc(void *ptr);

		template<class TSourceType, class TTargetType, bool TContainsType>
		struct DynamicCastFunctionHelper
		{
		};

		template<class TSourceType, class TTargetType>
		struct DynamicCastFunctionHelper<TSourceType, TTargetType, true>
		{
			static void *Recast(void *ptr);
		};

		template<class TSourceType, class TTargetType>
		struct DynamicCastFunctionHelper<TSourceType, TTargetType, false>
		{
			static void *Recast(void *ptr);
		};

		template<class TSourceType, class TTargetType, class TValidTargetTypesTuple>
		struct DynamicCastFunctionHelperResolver : public DynamicCastFunctionHelper<TSourceType, TTargetType, TypeTupleQuery<TValidTargetTypesTuple, TTargetType>::kContainsType>
		{
		};

		template<class TSourceType, class TValidTargetTypesTuple, class... TTargetTypes>
		struct DynamicCastHelper
		{
			static const RecastRefFunc_t ms_recastRefFuncs[TypeTuple<TTargetTypes...>::kNumTypes];
		};
	}
}

namespace rkit
{
	template<class... TTargetTypes>
	class DynamicallyCastableRef
	{
	public:
		DynamicallyCastableRef();
		DynamicallyCastableRef(const DynamicallyCastableRef &other);
		explicit DynamicallyCastableRef(void *obj, const priv::RecastRefFunc_t *recastVTablePtr);

		DynamicallyCastableRef &operator=(const DynamicallyCastableRef &other);

		template<class T>
		T *CastTo();

		template<class TSourceClass, class... TValidTargetTypes>
		static DynamicallyCastableRef<TTargetTypes...> CreateFrom(TSourceClass *srcPointer);

	private:
		void *m_object = nullptr;
		const priv::RecastRefFunc_t *m_recastVTablePtr = nullptr;
	};

	template<class... TTargetTypes>
	struct IDynamicCastable
	{
		typedef DynamicallyCastableRef<TTargetTypes...> DynamicCastRef_t;

		template<class T>
		T *DynamicCast();

		template<class T>
		const T *DynamicCast() const;

		template<class T>
		volatile T *DynamicCast() volatile;

	protected:
		virtual DynamicCastRef_t InternalDynamicCast() = 0;
	};
}

namespace rkit
{
	template<class... TTargetTypes>
	DynamicallyCastableRef<TTargetTypes...>::DynamicallyCastableRef()
		: m_object(nullptr)
		, m_recastVTablePtr(nullptr)
	{
	}

	template<class... TTargetTypes>
	DynamicallyCastableRef<TTargetTypes...>::DynamicallyCastableRef(const DynamicallyCastableRef<TTargetTypes...> &other)
		: m_object(other.m_object)
		, m_recastVTablePtr(other.m_recastVTablePtr)
	{
	}

	template<class... TTargetTypes>
	DynamicallyCastableRef<TTargetTypes...>::DynamicallyCastableRef(void *obj, const priv::RecastRefFunc_t *recastVTablePtr)
		: m_object(obj)
		, m_recastVTablePtr(recastVTablePtr)
	{
	}

	template<class... TTargetTypes>
	DynamicallyCastableRef<TTargetTypes...> &DynamicallyCastableRef<TTargetTypes...>::operator=(const DynamicallyCastableRef<TTargetTypes...> &other)
	{
		m_object = other.m_object;
		m_recastVTablePtr = other.m_recastVTablePtr;
		return *this;
	}

	template<class... TTargetTypes>
	template<class T>
	T *DynamicallyCastableRef<TTargetTypes...>::CastTo()
	{
		if (m_object == nullptr)
			return nullptr;

		typedef typename RemoveConstVolatile<T>::Type_t SimplifiedType_t;

		return static_cast<SimplifiedType_t *>(m_recastVTablePtr[TypeTupleQuery<TypeTuple<TTargetTypes...>, SimplifiedType_t>::kTypeIndex](m_object));
	}

	template<class... TTargetTypes>
	template<class TSourceClass, class... TValidTargetTypes>
	DynamicallyCastableRef<TTargetTypes...> DynamicallyCastableRef<TTargetTypes...>::CreateFrom(TSourceClass *srcPointer)
	{
		return DynamicallyCastableRef<TTargetTypes...>(static_cast<void *>(srcPointer), priv::DynamicCastHelper<TSourceClass, TypeTuple<TValidTargetTypes...>, TTargetTypes...>::ms_recastRefFuncs);
	}


	template<class... TTargetTypes>
	template<class T>
	T *IDynamicCastable<TTargetTypes...>::DynamicCast()
	{
		return this->InternalDynamicCast().CastTo<T>();
	}

	template<class... TTargetTypes>
	template<class T>
	const T *IDynamicCastable<TTargetTypes...>::DynamicCast() const
	{
		return const_cast<IDynamicCastable<TTargetTypes...> *>(this)->InternalDynamicCast().CastTo<T>();
	}

	template<class... TTargetTypes>
	template<class T>
	volatile T *IDynamicCastable<TTargetTypes...>::DynamicCast() volatile
	{
		return const_cast<IDynamicCastable<TTargetTypes...> *>(this)->InternalDynamicCast().CastTo<T>();
	}
}

namespace rkit::priv
{
	template<class TSourceType, class TValidTargetTypesTuple, class... TTargetTypes>
	const RecastRefFunc_t DynamicCastHelper<TSourceType, TValidTargetTypesTuple, TTargetTypes...>::ms_recastRefFuncs[TypeTuple<TTargetTypes...>::kNumTypes] =
	{
		DynamicCastFunctionHelperResolver<TSourceType, TTargetTypes, TValidTargetTypesTuple>::Recast...
	};

	template<class TSourceType, class TTargetType>
	void *DynamicCastFunctionHelper<TSourceType, TTargetType, true>::Recast(void *ptr)
	{
		TSourceType *src = static_cast<TSourceType *>(ptr);
		TTargetType *target = src;
		return static_cast<void *>(target);
	}

	template<class TSourceType, class TTargetType>
	void *DynamicCastFunctionHelper<TSourceType, TTargetType, false>::Recast(void *)
	{
		return nullptr;
	}
}
