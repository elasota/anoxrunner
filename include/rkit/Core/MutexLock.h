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

		void Unlock();

	private:
		T *m_mutex;
	};

	template<class T>
	BaseMutexLock<T>::BaseMutexLock(T &mutex)
		: m_mutex(&mutex)
	{
		mutex.Lock();
	}

	template<class T>
	BaseMutexLock<T>::~BaseMutexLock()
	{
		this->Unlock();
	}

	template<class T>
	void BaseMutexLock<T>::Unlock()
	{
		if (m_mutex)
		{
			m_mutex->Unlock();
			m_mutex = nullptr;
		}
	}

	typedef BaseMutexLock<IMutex> MutexLock;
}
