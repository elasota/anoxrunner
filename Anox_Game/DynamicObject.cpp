#include "DynamicObject.h"

namespace anox::game::priv
{
	static void *DynamicCastToBase(void *ptr, const RuntimeTypeInfo *ptrType, const RuntimeTypeInfo *targetType)
	{
		const RuntimeTypeBaseClassInfo *baseClasses = ptrType->m_baseClasses;
		const size_t numBaseClasses = ptrType->m_numBaseClasses;

		for (size_t i = 0; i < numBaseClasses; i++)
		{
			const RuntimeTypeBaseClassInfo &baseInfo = baseClasses[i];
			if (baseInfo.m_baseClass == targetType)
				return baseInfo.m_derivedToBase(ptr);

			if (baseInfo.m_baseClass->m_numBaseClasses)
			{
				void *baseDynCast = DynamicCastToBase(baseInfo.m_derivedToBase(ptr), baseInfo.m_baseClass, targetType);
				if (baseDynCast != nullptr)
					return baseDynCast;
			}
		}

		return nullptr;
	}

	static bool DynamicIsBaseOf(const RuntimeTypeInfo *baseRTTI, const RuntimeTypeInfo *derivedRTTI)
	{
		if (baseRTTI == derivedRTTI)
			return true;

		const RuntimeTypeBaseClassInfo *baseClasses = derivedRTTI->m_baseClasses;
		const size_t numBaseClasses = derivedRTTI->m_numBaseClasses;

		for (size_t i = 0; i < numBaseClasses; i++)
		{
			const RuntimeTypeBaseClassInfo &baseInfo = baseClasses[i];
			if (DynamicIsBaseOf(baseRTTI, baseInfo.m_baseClass))
				return true;
		}

		return false;
	}

	void *DynamicCastToAny(DynamicObject *obj, const RuntimeTypeInfo *destRTTI)
	{
		void *mostDerivedPtr = obj->GetMostDerivedObject();
		const RuntimeTypeInfo *mostDerivedType = obj->GetMostDerivedType();

		if (mostDerivedType == destRTTI)
			return mostDerivedPtr;

		return DynamicCastToBase(mostDerivedPtr, mostDerivedType, destRTTI);
	}

	bool DynamicIsDerivedFrom(DynamicObject *obj, const RuntimeTypeInfo *destRTTI)
	{
		return DynamicIsBaseOf(destRTTI, obj->GetMostDerivedType());
	}
}
