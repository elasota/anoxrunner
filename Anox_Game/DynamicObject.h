#pragma once

#include "rkit/Core/TypeList.h"
#include "rkit/Core/NoCopy.h"



#define ANOX_RTTI_CLASS_MULTI_BASE(cls, ...)	\
public:\
	friend class ::anox::game::priv::PrivateAccessor;\
	typedef ::rkit::TypeList<__VA_ARGS__> BaseClasses_t;\
	typedef cls ThisClass_t;\
	typedef ::anox::game::priv::AutoRTTI<ThisClass_t, BaseClasses_t> RTTIType_t;\
	const RuntimeTypeInfo *GetMostDerivedType() override { return &RTTIType_t::ms_instance; }\
	void *GetMostDerivedObject() override { return this; }

#define ANOX_RTTI_CLASS_NO_BASE(cls)	ANOX_RTTI_CLASS_MULTI_BASE(cls)
#define ANOX_RTTI_CLASS(cls, base)	\
	public:\
	typedef base BaseClass_t;\
	ANOX_RTTI_CLASS_MULTI_BASE(cls, BaseClass_t)

namespace anox::game
{
	struct RuntimeTypeInfo;
	struct RuntimeTypeBaseClassInfo;
}

namespace anox::game::priv
{
	struct PrivateAccessor
	{
		template<class TBase, class TInitial>
		static TBase *ImplicitCast(TInitial *ptr);

		template<class TCastTo, class TInitial>
		static TCastTo *StaticCast(TInitial *ptr);
	};

	template<class TDerivedClass, class TBaseClassList>
	struct AutoRTTIBaseClassList
	{
	};

	template<class TDerivedClass, class... TBaseClasses>
	struct AutoRTTIBaseClassList<TDerivedClass, rkit::TypeList<TBaseClasses...>>
	{
		static const RuntimeTypeBaseClassInfo ms_types[rkit::TypeListSize<rkit::TypeList<TBaseClasses...>>::kValue];
	};

	template<class TClass, class TBaseClassList>
	struct AutoRTTI
	{
		static const RuntimeTypeInfo ms_instance;
	};

	template<class TClass>
	struct AutoRTTI<TClass, rkit::TypeList<>>
	{
		static const RuntimeTypeInfo ms_instance;
	};

	class DynamicObjectVirtualFunctions
	{
	public:
		virtual ~DynamicObjectVirtualFunctions() {}

		virtual const RuntimeTypeInfo *GetMostDerivedType() = 0;
		virtual void *GetMostDerivedObject() = 0;
	};

	template<class TDerived, class TBase>
	struct RTTIPtrAdjust
	{
		static void *DerivedToBase(void *ptr);
		static void *BaseToDerived(void *ptr);
	};
}

namespace anox::game
{
	struct RuntimeTypeBaseClassInfo
	{
		typedef void *(*PtrAdjustFunc_t)(void *ptr);

		PtrAdjustFunc_t m_derivedToBase;
		PtrAdjustFunc_t m_baseToDerived;
		const RuntimeTypeInfo *m_baseClass;
	};

	struct RuntimeTypeInfo
	{
		const RuntimeTypeBaseClassInfo *m_baseClasses;
		size_t m_numBaseClasses;
	};

	class DynamicObject : public priv::DynamicObjectVirtualFunctions, public rkit::NoCopy
	{
		ANOX_RTTI_CLASS_NO_BASE(DynamicObject)

	public:

	private:
		static void *InternalDynamicCast(void *ptr, const RuntimeTypeInfo *ptrType);
	};
}

namespace anox::game::priv
{
	void *DynamicCastToAny(DynamicObject *obj, const RuntimeTypeInfo *destRTTI);
	bool DynamicIsDerivedFrom(DynamicObject *obj, const RuntimeTypeInfo *destRTTI);

	template<class TBase, class TInitial>
	TBase *PrivateAccessor::ImplicitCast(TInitial *ptr)
	{
		TBase *basePtr = ptr;
		return basePtr;
	}

	template<class TCastTo, class TInitial>
	TCastTo *PrivateAccessor::StaticCast(TInitial *ptr)
	{
		TCastTo *recastPtr = static_cast<TCastTo *>(ptr);
		return recastPtr;
	}

	template<class TDerived, class TBase>
	void *RTTIPtrAdjust<TDerived, TBase>::DerivedToBase(void *ptr)
	{
		TDerived *derived = static_cast<TDerived *>(ptr);
		TBase *base = derived;
		return base;
	}

	template<class TDerived, class TBase>
	void *RTTIPtrAdjust<TDerived, TBase>::BaseToDerived(void *ptr)
	{
		TBase *base = static_cast<TBase *>(ptr);
		TDerived *derived = static_cast<TDerived *>(base);
		return derived;
	}

	template<class TClass, class TBaseClassList>
	const RuntimeTypeInfo AutoRTTI<TClass, TBaseClassList>::ms_instance = 
	{
		AutoRTTIBaseClassList<TClass, TBaseClassList>::ms_types,
		rkit::TypeListSize<TBaseClassList>::kValue,
	};

	template<class TClass>
	const RuntimeTypeInfo AutoRTTI<TClass, rkit::TypeList<>>::ms_instance =
	{
		nullptr,
		0
	};

	template<class TDerivedClass, class... TBaseClasses>
	const RuntimeTypeBaseClassInfo AutoRTTIBaseClassList<TDerivedClass, rkit::TypeList<TBaseClasses...>>::ms_types[rkit::TypeListSize<rkit::TypeList<TBaseClasses...>>::kValue] =
	{
		{ RTTIPtrAdjust<TDerivedClass, TBaseClasses>::DerivedToBase, RTTIPtrAdjust<TDerivedClass, TBaseClasses>::BaseToDerived, &TBaseClasses::RTTIType_t::ms_instance }...
	};
}

#include <type_traits>

namespace anox::game
{
	template<class TDestType, class TSourceType>
	inline TDestType *DynamicCast(TSourceType *srcPtr)
	{
		typedef typename std::remove_volatile<typename std::remove_const<TSourceType>::type>::type CVRemovedSource_t;
		typedef typename std::remove_volatile<typename std::remove_const<TDestType>::type>::type CVRemovedDest_t;

		if (srcPtr == nullptr)
			return nullptr;

		static_assert(std::is_const<TSourceType>::value || !std::is_const<TDestType>::value, "const qualifiers of dest are incompatible with source");
		static_assert(std::is_volatile<TSourceType>::value || !std::is_volatile<TDestType>::value, "volatile qualifiers of dest are incompatible with source");

		if constexpr (std::is_base_of<CVRemovedDest_t, CVRemovedSource_t>::value)
			return srcPtr;
		else
		{
			CVRemovedSource_t *cvRemovedSrc = const_cast<CVRemovedSource_t *>(srcPtr);

			if constexpr (std::is_same<typename CVRemovedDest_t::ThisClass_t, CVRemovedDest_t>::value)
			{
				if constexpr (std::is_base_of<CVRemovedSource_t, CVRemovedDest_t>())
				{
					const RuntimeTypeInfo *destRTTI = &CVRemovedDest_t::RTTIType_t::ms_instance;
					if (priv::DynamicIsDerivedFrom(cvRemovedSrc, destRTTI))
						return static_cast<CVRemovedDest_t *>(srcPtr);
					else
						return nullptr;
				}
				else
				{
					const RuntimeTypeInfo *destRTTI = &CVRemovedDest_t::RTTIType_t::ms_instance;
					return static_cast<CVRemovedDest_t *>(priv::DynamicCastToAny(cvRemovedSrc, destRTTI));
				}
			}
			else
				static_assert(false, "Can't dynamic_cast to derived type that isn't the same as the RTTIType_t of the target");
		}
	}
}
