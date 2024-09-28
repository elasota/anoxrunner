#pragma once

#include "UniquePtr.h"

#include <atomic>

namespace rkit
{
	struct Result;

	struct BaseRefCountTracker
	{
		typedef void (*SelfDestructFunc_t)(BaseRefCountTracker &tracker);

		std::atomic<size_t> m_count;
		SelfDestructFunc_t m_selfDestructFunc;

		void ReleaseCheckDelete();
	};

	template<class T>
	class SharedPtr
	{
	public:
		SharedPtr();
		SharedPtr(const SharedPtr<T> &other);
		template<class TOther>
		SharedPtr(const SharedPtr<TOther> &other);
		SharedPtr(T *obj, BaseRefCountTracker *tracker);
		~SharedPtr();

		BaseRefCountTracker *GetTracker() const;
		T *Get() const;

		T &operator*() const;
		T *operator->() const;

		SharedPtr<T> &operator=(const SharedPtr<T> &other);
		SharedPtr<T> &operator=(SharedPtr<T> &&other);

		static Result Create(SharedPtr<T> &outRCPtr, IMallocDriver *alloc, UniquePtr<T> &&original);

	private:
		struct RCContainer : public BaseRefCountTracker
		{
			UniquePtr<T> m_obj;
			UniquePtr<RCContainer> m_self;

			static void SelfDestruct(BaseRefCountTracker &tracker);
		};

		explicit SharedPtr(RCContainer *rcContainer);

		T *m_obj;
		BaseRefCountTracker *m_tracker;
	};

	template<class T>
	Result MakeSharedWithAlloc(SharedPtr<T> &outRCPtr, IMallocDriver *alloc, UniquePtr<T> &&original);

	template<class T>
	Result MakeShared(SharedPtr<T> &outRCPtr, UniquePtr<T> &&original);
}

#include "Drivers.h"
#include "MallocDriver.h"
#include "NewDelete.h"

template<class T>
rkit::SharedPtr<T>::SharedPtr()
	: m_tracker(nullptr)
	, m_obj(nullptr)
{
}

template<class T>
rkit::SharedPtr<T>::SharedPtr(RCContainer *rcContainer)
	: m_obj(rcContainer->m_obj.Get())
	, m_tracker(rcContainer)
{
}

template<class T>
rkit::SharedPtr<T>::SharedPtr(const SharedPtr<T> &other)
	: m_tracker(other.m_tracker)
	, m_obj(other.m_obj)
{
	if (m_tracker)
		m_tracker->m_count.fetch_add(1);
}

template<class T>
rkit::SharedPtr<T>::SharedPtr(T *obj, BaseRefCountTracker *tracker)
	: m_tracker(tracker)
	, m_obj(obj)
{
	if (m_tracker)
		m_tracker->m_count.fetch_add(1);
}

template<class T>
template<class TOther>
rkit::SharedPtr<T>::SharedPtr(const SharedPtr<TOther> &other)
	: m_tracker(other.GetTracker())
	, m_obj(other.Get())
{
	if (m_tracker)
		m_tracker->m_count.fetch_add(1);
}

template<class T>
rkit::SharedPtr<T>::~SharedPtr()
{
	if (m_tracker)
		m_tracker->ReleaseCheckDelete();
}


template<class T>
rkit::BaseRefCountTracker *rkit::SharedPtr<T>::GetTracker() const
{
	return m_tracker;
}

template<class T>
T *rkit::SharedPtr<T>::Get() const
{
	return m_obj;
}

template<class T>
T &rkit::SharedPtr<T>::operator*() const
{
	return *m_obj;
}

template<class T>
T *rkit::SharedPtr<T>::operator->() const
{
	return m_obj;
}

template<class T>
rkit::SharedPtr<T> &rkit::SharedPtr<T>::operator=(const SharedPtr<T> &other)
{
	if (this != &other)
	{
		if (other.m_tracker)
			other.m_tracker->m_count.fetch_add(1);

		if (m_tracker)
			m_tracker->ReleaseCheckDelete();

		m_obj = other.m_obj;
		m_tracker = other.m_tracker;
	}

	return *this;
}

template<class T>
rkit::SharedPtr<T> &rkit::SharedPtr<T>::operator=(SharedPtr<T> &&other)
{
	if (this != &other)
	{
		T *obj = other.m_obj;
		BaseRefCountTracker *tracker = other.m_tracker;

		other.m_obj = nullptr;
		other.m_tracker = nullptr;

		if (m_tracker)
			m_tracker->ReleaseCheckDelete();

		m_obj = obj;
		m_tracker = tracker;
	}

	return *this;
}

template<class T>
rkit::Result rkit::SharedPtr<T>::Create(SharedPtr<T> &outRCPtr, IMallocDriver *alloc, UniquePtr<T> &&original)
{
	UniquePtr<RCContainer> rcContainerPtr;

	RKIT_CHECK(NewWithAlloc<RCContainer>(rcContainerPtr, alloc));

	RCContainer *rcContainer = rcContainerPtr.Get();

	rcContainer->m_count = 1;
	rcContainer->m_self = std::move(rcContainerPtr);
	rcContainer->m_obj = std::move(original);
	rcContainer->m_selfDestructFunc = RCContainer::SelfDestruct;

	outRCPtr = SharedPtr<T>(rcContainer);

	return ResultCode::kOK;
}

template<class T>
void rkit::SharedPtr<T>::RCContainer::SelfDestruct(BaseRefCountTracker &tracker)
{
	static_cast<SharedPtr<T>::RCContainer &>(tracker).m_self.Reset();
}

template<class T>
rkit::Result rkit::MakeSharedWithAlloc(SharedPtr<T> &outRCPtr, IMallocDriver *alloc, UniquePtr<T> &&original)
{
	return SharedPtr<T>::Create(outRCPtr, alloc, std::move(original));
}

template<class T>
rkit::Result rkit::MakeShared(SharedPtr<T> &outRCPtr, UniquePtr<T> &&original)
{
	return MakeSharedWithAlloc<T>(outRCPtr, GetDrivers().m_mallocDriver, std::move(original));
}

inline void rkit::BaseRefCountTracker::ReleaseCheckDelete()
{
	if (m_count.fetch_sub(1) == 1)
		m_selfDestructFunc(*this);
}
