#pragma once

#include "NoCopy.h"

namespace rkit
{
	struct IMutex;

	template<class T>
	class BaseMutexLock final : public NoCopy
	{
	public:
		explicit BaseMutexLock(T &mutex);
		~BaseMutexLock();

	private:
		T &m_mutex;
	};

	template<class T>
	BaseMutexLock<T>::BaseMutexLock(T &mutex)
		: m_mutex(mutex)
	{
		mutex.Lock();
	}

	template<class T>
	BaseMutexLock<T>::~BaseMutexLock()
	{
		m_mutex.Unlock();
	}

	typedef BaseMutexLock<IMutex> MutexLock;
}
