#pragma once


#include <limits>
#include <utility>
#include <new>
#include <type_traits>

namespace rkit
{
	struct IMallocDriver;
	struct Result;

	template<class T>
	class UniquePtr;

	template<class T>
	struct SimpleObjectAllocation;

	template<class TType, class TPtrType, class... TArgs>
	Result New(UniquePtr<TPtrType> &objPtr, TArgs&& ...args);

	template<class TType, class TPtrType, class... TArgs>
	Result NewWithAlloc(UniquePtr<TPtrType> &objPtr, IMallocDriver *alloc, TArgs&& ...args);

	template<class TType, class TPtrType>
	Result New(UniquePtr<TPtrType> &objPtr);

	template<class TType, class TPtrType>
	Result NewUPtr(UniquePtr<TPtrType> &objPtr);

	template<class TType, class TPtrType>
	Result NewWithAlloc(UniquePtr<TPtrType> &objPtr, IMallocDriver *alloc);

	template<class T>
	void Delete(const SimpleObjectAllocation<T> &objAllocation);

	template<class T>
	void SafeDelete(SimpleObjectAllocation<T> &objAllocation);
}

#include "Drivers.h"
#include "MallocDriver.h"
#include "Result.h"
#include "UniquePtr.h"

template<class TType, class TPtrType, class... TArgs>
inline rkit::Result rkit::NewWithAlloc(UniquePtr<TPtrType> &objPtr, IMallocDriver *alloc, TArgs&& ...args)
{
	void *mem = alloc->Alloc(sizeof(TType));
	if (!mem)
		return ResultCode::kOutOfMemory;

	TType *obj = new (mem) TType(std::forward<TArgs>(args)...);
	objPtr = UniquePtr<TPtrType>(obj, mem, alloc);

	return ResultCode::kOK;
}

template<class TType, class TPtrType, class... TArgs>
inline rkit::Result rkit::New(UniquePtr<TPtrType> &objPtr, TArgs&& ...args)
{
	return NewWithAlloc<TType, TPtrType, TArgs...>(objPtr, GetDrivers().m_mallocDriver, std::forward<TArgs>(args)...);
}

template<class TType, class TPtrType>
rkit::Result rkit::NewWithAlloc(UniquePtr<TPtrType> &objPtr, IMallocDriver *alloc)
{
	void *mem = alloc->Alloc(sizeof(TType));
	if (!mem)
		return ResultCode::kOutOfMemory;

	TType *obj = new (mem) TType();
	objPtr = UniquePtr<TPtrType>(obj, mem, alloc);

	return ResultCode::kOK;
}

template<class TType, class TPtrType>
rkit::Result rkit::New(UniquePtr<TPtrType> &objPtr)
{
	return NewWithAlloc<TType, TPtrType>(objPtr, GetDrivers().m_mallocDriver);
}

template<class TType, class TPtrType>
rkit::Result rkit::NewUPtr(UniquePtr<TPtrType> &objPtr)
{
	return NewWithAlloc<TType, TPtrType>(objPtr, GetDrivers().m_mallocDriver);
}


template<class T>
void rkit::Delete(const SimpleObjectAllocation<T> &objAllocation)
{
	if (T *obj = objAllocation.m_obj)
	{
		void *mem = objAllocation.m_mem;
		IMallocDriver *alloc = objAllocation.m_alloc;

		obj->~T();
		alloc->Free(mem);
	}
}

template<class T>
void rkit::SafeDelete(SimpleObjectAllocation<T> &objAllocation)
{
	SimpleObjectAllocation<T> allocCopy = objAllocation;

	objAllocation.Clear();

	if (allocCopy.m_obj)
	{
		allocCopy.m_obj->~T();
		allocCopy.m_alloc->Free(allocCopy.m_mem);
	}
}
