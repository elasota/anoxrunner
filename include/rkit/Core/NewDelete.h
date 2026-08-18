#pragma once

#include "OpaquePairing.h"

#include <limits>
#include <utility>
#include <new>
#include <type_traits>

namespace rkit
{
	template<class TImpl>
	class Opaque;

	template<class TImpl>
	class OpaqueImplementation;

	namespace priv
	{
		template<class T>
		struct NonOpaqueSizeAlignResolver
		{
			static constexpr size_t GetSize()
			{
				return sizeof(T);
			}

			static constexpr size_t GetAlign()
			{
				return alignof(T);
			}
		};

		template<class TBase, class TImpl>
		struct OpaqueSizeAlignResolver
		{
		public:
			inline static void CheckOpaque()
			{
				OpaqueImplementation<TBase> *opaqueImpl = static_cast<TImpl *>(nullptr);
				Opaque<TImpl> *opaque = static_cast<TBase *>(nullptr);

				(void)opaqueImpl;
				(void)opaque;
			}

		public:
			static constexpr size_t GetSize()
			{
				CheckOpaque();
				return sizeof(OpaquePairing<TBase, TImpl>);
			}

			static constexpr size_t GetAlign()
			{
				CheckOpaque();
				return alignof(OpaquePairing<TBase, TImpl>);
			}
		};

		template<class TType>
		constexpr NonOpaqueSizeAlignResolver<TType> IsOpaqueChecker(TType *, void *)
		{
			return NonOpaqueSizeAlignResolver<TType>();
		}

		template<class TType, class TImpl>
		constexpr OpaqueSizeAlignResolver<TType, TImpl> IsOpaqueChecker(TType *type, Opaque<TImpl> *)
		{
			return OpaqueSizeAlignResolver<TType, TImpl>();
		}

		template<class TType>
		struct NewSizeAlignResolver
		{
			static constexpr size_t GetSize()
			{
				return IsOpaqueChecker(static_cast<TType *>(nullptr), static_cast<TType *>(nullptr)).GetSize();
			}

			static constexpr size_t GetAlign()
			{
				return IsOpaqueChecker(static_cast<TType *>(nullptr), static_cast<TType *>(nullptr)).GetAlign();
			}
		};
	}
}

#include "Result.h"

namespace rkit
{
	struct IMallocDriver;

	template<class T>
	class UniquePtr;

	template<class T>
	struct SimpleObjectAllocation;

	template<class TType, class... TArgs>
	RKIT_NODISCARD UniquePtr<TType> New(TArgs&& ...args);

	template<class TType, class... TArgs>
	RKIT_NODISCARD UniquePtr<TType> NewWithAlloc(IMallocDriver *alloc, TArgs&& ...args);

	template<class TType>
	RKIT_NODISCARD UniquePtr<TType> New();

	template<class TType>
	RKIT_NODISCARD UniquePtr<TType> NewWithAlloc(IMallocDriver *alloc);

	template<class TType, class... TArgs>
	RKIT_NODISCARD UniquePtr<TType> NewInitialize(TArgs&& ...args);

	template<class TType, class... TArgs>
	RKIT_NODISCARD UniquePtr<TType> NewInitializeWithAlloc(IMallocDriver *alloc, TArgs&& ...args);

	template<class TType>
	RKIT_NODISCARD UniquePtr<TType> NewInitialize();

	template<class TType>
	RKIT_NODISCARD UniquePtr<TType> NewInitializeWithAlloc(IMallocDriver *alloc);


	template<class T>
	void Delete(const SimpleObjectAllocation<T> &objAllocation);

	template<class T>
	void SafeDelete(SimpleObjectAllocation<T> &objAllocation);
}

#include "Drivers.h"
#include "MallocDriver.h"
#include "UniquePtr.h"

template<class TType, class... TArgs>
inline rkit::UniquePtr<TType> rkit::NewWithAlloc(IMallocDriver *alloc, TArgs&& ...args)
{
	void *mem = alloc->Alloc(priv::NewSizeAlignResolver<TType>::GetSize());
	if (!mem)
		RKIT_THROW(ResultCode::kOutOfMemory);

	TType *obj = new (mem) TType(std::forward<TArgs>(args)...);
	return UniquePtr<TType>(obj, mem, alloc);
}

template<class TType, class... TArgs>
inline rkit::UniquePtr<TType> rkit::NewInitializeWithAlloc(IMallocDriver *alloc, TArgs&& ...args)
{
	UniquePtr<TType> obj = NewWithAlloc<TType>(obj, alloc);

	TType *objRawPtr = obj.Get();
	objRawPtr->Initialize(std::forward<TArgs>(args)...);

	return obj;
}

template<class TType, class... TArgs>
inline rkit::UniquePtr<TType> rkit::New(TArgs&& ...args)
{
	return NewWithAlloc<TType, TArgs...>(GetDrivers().m_mallocDriver.Get(), std::forward<TArgs>(args)...);
}

template<class TType, class... TArgs>
inline rkit::UniquePtr<TType> rkit::NewInitialize(TArgs&& ...args)
{
	return NewInitializeWithAlloc<TType, TArgs...>(GetDrivers().m_mallocDriver.Get(), std::forward<TArgs>(args)...);
}

template<class TType>
inline rkit::UniquePtr<TType> rkit::NewWithAlloc(IMallocDriver *alloc)
{
	void *mem = alloc->Alloc(priv::NewSizeAlignResolver<TType>::GetSize());
	if (!mem)
		RKIT_THROW(ResultCode::kOutOfMemory);

	TType *obj = new (mem) TType();
	return UniquePtr<TType>(obj, mem, alloc);
}

template<class TType>
rkit::UniquePtr<TType> rkit::NewInitializeWithAlloc(IMallocDriver *alloc)
{
	UniquePtr<TType> obj = NewWithAlloc<TType>(alloc);

	TType *objRawPtr = obj.Get();
	objRawPtr->Initialize();
	return obj;
}

template<class TType>
rkit::UniquePtr<TType> rkit::New()
{
	return NewWithAlloc<TType>(GetDrivers().m_mallocDriver.Get());
}

template<class TType>
rkit::UniquePtr<TType> rkit::NewInitialize()
{
	return NewInitializeWithAlloc<TType>(GetDrivers().m_mallocDriver.Get());
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
