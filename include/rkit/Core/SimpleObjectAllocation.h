#pragma once

namespace rkit
{
	struct IMallocDriver;

	template<class T>
	struct SimpleObjectAllocation
	{
		T *m_obj;
		void *m_mem;
		IMallocDriver *m_alloc;

		operator T *() const;
		T *operator->() const;

		template<class TOther>
		operator SimpleObjectAllocation<TOther>() const;

		void Clear();
	};
}

template<class T>
rkit::SimpleObjectAllocation<T>::operator T *() const
{
	return m_obj;
}

template<class T>
T *rkit::SimpleObjectAllocation<T>::operator->() const
{
	return m_obj;
}


template<class T>
template<class TOther>
rkit::SimpleObjectAllocation<T>::operator SimpleObjectAllocation<TOther>() const
{
	SimpleObjectAllocation<TOther> result;
	result.m_alloc = m_alloc;
	result.m_mem = m_mem;
	result.m_obj = m_obj;
	return result;
}


template<class T>
void rkit::SimpleObjectAllocation<T>::Clear()
{
	m_alloc = nullptr;
	m_mem = nullptr;
	m_obj = nullptr;
}
