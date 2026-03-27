#pragma once

#include "rkit/Core/TypeList.h"
#include "rkit/Core/NoCopy.h"



#define ANOX_RTTI_CLASS_MULTI_BASE(cls, ...)	\
public:\
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
	};
}

namespace anox::game::priv
{

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
