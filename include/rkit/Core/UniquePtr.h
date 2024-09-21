#pragma once

#include "SimpleObjectAllocation.h"

namespace rkit
{
	struct IMallocDriver;

	template<class T>
	class UniquePtr
	{
	public:
		UniquePtr();
		UniquePtr(T *ptr, void *memAddr);
		UniquePtr(T *ptr, void *memAddr, IMallocDriver *alloc);
		UniquePtr(UniquePtr &&other) noexcept;
		template<class TOther>
		UniquePtr(UniquePtr<TOther> &&other);
		~UniquePtr();

		bool IsValid() const;
		SimpleObjectAllocation<T> Detach();

		void Reset();

		UniquePtr &operator=(UniquePtr &&) noexcept;

		T *Get() const;
		T *operator->() const;

	private:
		UniquePtr(const UniquePtr &) = delete;
		UniquePtr &operator=(const UniquePtr &) = delete;

		SimpleObjectAllocation<T> m_allocation;
	};
}

#include "Drivers.h"
#include "NewDelete.h"
#include "RKitAssert.h"

template<class T>
inline rkit::UniquePtr<T>::UniquePtr()
{
	m_allocation.Clear();
}

template<class T>
inline rkit::UniquePtr<T>::UniquePtr(T *ptr, void *memAddr)
{
	m_allocation.m_obj = ptr;
	m_allocation.m_mem = memAddr;
	m_allocation.m_alloc = GetDrivers().m_mallocDriver;
}

template<class T>
inline rkit::UniquePtr<T>::UniquePtr(T *ptr, void *memAddr, IMallocDriver *alloc)
{
	m_allocation.m_obj = ptr;
	m_allocation.m_mem = memAddr;
	m_allocation.m_alloc = alloc;
}

template<class T>
rkit::UniquePtr<T>::UniquePtr(UniquePtr<T> &&other) noexcept
	: m_allocation(other.m_allocation)
{
	other.m_allocation.Clear();
}

template<class T>
template<class TOther>
rkit::UniquePtr<T>::UniquePtr(UniquePtr<TOther> &&other)
{
	SimpleObjectAllocation<TOther> detached = other.Detach();

	m_allocation.m_alloc = detached.m_alloc;
	m_allocation.m_mem = detached.m_mem;
	m_allocation.m_obj = detached.m_obj;
}

template<class T>
rkit::UniquePtr<T>::~UniquePtr()
{
	Delete<T>(m_allocation);
}

template<class T>
void rkit::UniquePtr<T>::Reset()
{
	SafeDelete<T>(m_allocation);
}

template<class T>
inline bool rkit::UniquePtr<T>::IsValid() const
{
	return m_allocation.m_obj != nullptr;
}

template<class T>
rkit::SimpleObjectAllocation<T> rkit::UniquePtr<T>::Detach()
{
	rkit::SimpleObjectAllocation<T> result = m_allocation;

	m_allocation.Clear();

	return result;
}

template<class T>
rkit::UniquePtr<T> &rkit::UniquePtr<T>::operator=(UniquePtr &&other) noexcept
{
	Delete<T>(m_allocation);

	m_allocation = other.m_allocation;

	other.m_allocation.Clear();

	return *this;
}

template<class T>
T *rkit::UniquePtr<T>::operator->() const
{
	RKIT_ASSERT(m_allocation.m_obj);
	return m_allocation.m_obj;
}

template<class T>
T *rkit::UniquePtr<T>::Get() const
{
	return m_allocation.m_obj;
}
