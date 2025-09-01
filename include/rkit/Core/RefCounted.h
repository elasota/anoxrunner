#pragma once

#include "CoreDefs.h"
#include "NoCopy.h"
#include "SimpleObjectAllocation.h"

#include <atomic>

namespace rkit
{
	class RefCounted;
	struct RefCountedTracker;

	namespace Private
	{
		struct RefCountedInstantiator
		{
			static void InitRefCounted(RefCounted &refCounted, const SimpleObjectAllocation<RefCounted> &alloc);
			static RefCountedTracker *GetTrackerFromObject(RefCounted *obj);
		};
	}

	struct RefCountedTracker : public NoCopy
	{
	public:
		typedef size_t RefCount_t;

		explicit RefCountedTracker(RefCount_t initialCount);

		void RCTrackerAddRef();
		void RCTrackerDecRef();

	protected:
		virtual void RCTrackerZero() = 0;

		RefCount_t RCTrackerRefCount() const;

	private:
		RefCountedTracker() = delete;

		std::atomic<RefCount_t> m_refCount;
	};

	class RefCounted : private RefCountedTracker
	{
	public:
		friend struct rkit::Private::RefCountedInstantiator;

	protected:
		RefCounted();
		virtual ~RefCounted() {}

		void RCTrackerZero() override;
		RefCount_t RCTrackerRefCount() const;

	private:
		void InitRefCounted(const SimpleObjectAllocation<RefCounted> &alloc);

		SimpleObjectAllocation<RefCounted> m_allocation;
	};

	template<class T>
	class RCPtr
	{
	public:
		template<class TOther>
		friend class RCPtr;

		RCPtr();
		explicit RCPtr(std::nullptr_t);
		explicit RCPtr(T *ptr);
		explicit RCPtr(T *ptr, RefCountedTracker *tracker);
		RCPtr(const RCPtr<T> &other) noexcept;
		RCPtr(RCPtr<T> &&other) noexcept;

		template<class TOther>
		RCPtr(const RCPtr<TOther> &other);

		template<class TOther>
		RCPtr(RCPtr<TOther> &&other);

		~RCPtr();

		bool IsValid() const;
		void Detach(T *& outPtr, RefCountedTracker *&outTracker);

		void Reset();

		RCPtr &operator=(const RCPtr &other) noexcept;
		RCPtr &operator=(RCPtr &&other) noexcept;

		template<class TOther>
		RCPtr &operator=(const RCPtr<TOther> &other) noexcept;

		template<class TOther>
		RCPtr &operator=(RCPtr<TOther> &&other) noexcept;

		RCPtr &operator=(T *other);
		RCPtr &operator=(std::nullptr_t);

		T *Get() const;
		operator T *() const;
		T *operator->() const;

		template<class TOther>
		RCPtr<TOther> StaticCast() const;

		template<class TOther>
		RCPtr<TOther> ReinterpretCast() const;

		template<class TOther>
		RCPtr<TOther> ConstCast() const;

		template<class TField, class TObject>
		RCPtr<TField> FieldRef(TField TObject::* fieldRef) const;

	private:
		T *m_object;
		RefCountedTracker *m_tracker;
	};


	template<class TType, class TPtrType, class... TArgs>
	Result New(RCPtr<TPtrType> &objPtr, TArgs&& ...args);

	template<class TType, class TPtrType, class... TArgs>
	Result NewWithAlloc(RCPtr<TPtrType> &objPtr, IMallocDriver *alloc, TArgs&& ...args);

	template<class TType, class TPtrType>
	Result New(RCPtr<TPtrType> &objPtr);
}

#include "Drivers.h"
#include "MallocDriver.h"
#include "Result.h"

#include <utility>

namespace rkit
{
	inline RefCountedTracker::RefCountedTracker(RefCount_t initialCount)
		: m_refCount(initialCount)
	{
	}

	inline void RefCountedTracker::RCTrackerAddRef()
	{
		(void)m_refCount.fetch_add(1, std::memory_order_seq_cst);
	}

	inline void RefCountedTracker::RCTrackerDecRef()
	{
		if (m_refCount.fetch_sub(1, std::memory_order_seq_cst) == 1)
			RCTrackerZero();
	}

	inline RefCountedTracker::RefCount_t RefCountedTracker::RCTrackerRefCount() const
	{
		return m_refCount.load(std::memory_order_relaxed);
	}

	inline void RefCounted::RCTrackerZero()
	{
		SimpleObjectAllocation<RefCounted> allocation = m_allocation;

		rkit::IMallocDriver *alloc = allocation.m_alloc;
		void *mem = allocation.m_mem;
		RefCounted *obj = allocation.m_obj;

		obj->~RefCounted();
		alloc->Free(mem);
	}

	inline RefCounted::RefCount_t RefCounted::RCTrackerRefCount() const
	{
		return RefCountedTracker::RCTrackerRefCount();
	}

	inline RefCounted::RefCounted()
		: RefCountedTracker(0)
	{
		m_allocation.Clear();
	}

	inline void RefCounted::InitRefCounted(const SimpleObjectAllocation<RefCounted> &alloc)
	{
		m_allocation = alloc;
	}

	template<class T>
	inline RCPtr<T>::RCPtr()
		: m_object(nullptr)
		, m_tracker(nullptr)
	{
	}

	template<class T>
	inline RCPtr<T>::RCPtr(std::nullptr_t)
		: m_object(nullptr)
		, m_tracker(nullptr)
	{
	}

	template<class T>
	inline RCPtr<T>::RCPtr(T *ptr)
		: m_object(ptr)
		, m_tracker(Private::RefCountedInstantiator::GetTrackerFromObject(ptr))
	{
		if (m_tracker != nullptr)
			m_tracker->RCTrackerAddRef();
	}

	template<class T>
	RCPtr<T>::RCPtr(T *ptr, RefCountedTracker *tracker)
		: m_object(ptr)
		, m_tracker(tracker)
	{
		if (tracker != nullptr)
			tracker->RCTrackerAddRef();
	}

	template<class T>
	inline RCPtr<T>::RCPtr(const RCPtr<T> &other) noexcept
		: m_object(other.m_object)
		, m_tracker(other.m_tracker)
	{
		if (m_tracker != nullptr)
			m_tracker->RCTrackerAddRef();
	}

	template<class T>
	inline RCPtr<T>::RCPtr(RCPtr &&other) noexcept
		: m_object(other.m_object)
		, m_tracker(other.m_tracker)
	{
		other.m_object = nullptr;
		other.m_tracker = nullptr;
	}


	template<class T>
	template<class TOther>
	inline RCPtr<T>::RCPtr(const RCPtr<TOther> &other)
		: m_object(nullptr)
		, m_tracker(nullptr)
	{
		(*this) = other;
	}

	template<class T>
	template<class TOther>
	inline RCPtr<T>::RCPtr(RCPtr<TOther> &&other)
		: m_object(nullptr)
		, m_tracker(nullptr)
	{
		(*this) = std::move(other);
	}

	template<class T>
	inline RCPtr<T>::~RCPtr()
	{
		if (m_tracker)
			m_tracker->RCTrackerDecRef();
	}

	template<class T>
	inline bool RCPtr<T>::IsValid() const
	{
		return m_object != nullptr;
	}

	template<class T>
	inline void RCPtr<T>::Detach(T *&outPtr, RefCountedTracker *&outTracker)
	{
		outPtr = m_object;
		outTracker = m_tracker;

		m_object = nullptr;
		m_tracker = nullptr;
	}

	template<class T>
	inline void RCPtr<T>::Reset()
	{
		m_object = nullptr;

		if (m_tracker)
		{
			RefCountedTracker *tracker = m_tracker;	// In case this gets destroyed
			m_tracker = nullptr;

			tracker->RCTrackerDecRef();
		}
	}

	template<class T>
	inline RCPtr<T> &RCPtr<T>::operator=(const RCPtr &other) noexcept
	{
		m_object = other.m_object;

		if (m_tracker != other.m_tracker)
		{
			RefCountedTracker *oldTracker = m_tracker;

			m_tracker = other.m_tracker;

			if (m_tracker)
				m_tracker->RCTrackerAddRef();

			if (oldTracker)
				oldTracker->RCTrackerDecRef();
		}

		return *this;
	}

	template<class T>
	template<class TOther>
	inline RCPtr<T> &RCPtr<T>::operator=(const RCPtr<TOther> &other) noexcept
	{
		m_object = other.m_object;

		if (m_tracker != other.m_tracker)
		{
			RefCountedTracker *oldTracker = m_tracker;

			m_tracker = other.m_tracker;

			if (m_tracker)
				m_tracker->RCTrackerAddRef();

			if (oldTracker)
				oldTracker->RCTrackerDecRef();
		}

		return *this;
	}

	template<class T>
	inline RCPtr<T> &RCPtr<T>::operator=(RCPtr<T> &&other) noexcept
	{
		RefCountedTracker *oldTracker = m_tracker;

		RefCountedTracker *newTracker = nullptr;
		T *newObj = nullptr;
		other.Detach(newObj, newTracker);

		m_object = newObj;
		m_tracker = newTracker;

		if (oldTracker)
			oldTracker->RCTrackerDecRef();

		return *this;
	}

	template<class T>
	template<class TOther>
	inline RCPtr<T> &RCPtr<T>::operator=(RCPtr<TOther> &&other) noexcept
	{
		RefCountedTracker *oldTracker = m_tracker;

		RefCountedTracker *newTracker = nullptr;
		TOther *newObj = nullptr;
		other.Detach(newObj, newTracker);

		m_object = newObj;
		m_tracker = newTracker;

		if (oldTracker)
			oldTracker->RCTrackerDecRef();

		return *this;
	}

	template<class T>
	RCPtr<T> &RCPtr<T>::operator=(T *other)
	{
		if (other != m_object)
		{
			RefCountedTracker *oldTracker = m_tracker;

			T *newObj = other;
			RefCountedTracker *newTracker = Private::RefCountedInstantiator::GetTrackerFromObject(newObj);

			m_object = newObj;
			m_tracker = newTracker;

			if (newTracker)
				newTracker->RCTrackerAddRef();

			if (oldTracker)
				oldTracker->RCTrackerDecRef();
		}

		return *this;
	}

	template<class T>
	RCPtr<T> &RCPtr<T>::operator=(std::nullptr_t)
	{
		if (m_object != nullptr)
		{
			if (m_tracker)
				m_tracker->RCTrackerDecRef();

			m_object = nullptr;
			m_tracker = nullptr;
		}

		return *this;
	}

	template<class T>
	inline T *RCPtr<T>::Get() const
	{
		return m_object;
	}

	template<class T>
	template<class TOther>
	RCPtr<TOther> RCPtr<T>::StaticCast() const
	{
		return RCPtr<TOther>(static_cast<TOther *>(m_object), m_tracker);
	}

	template<class T>
	template<class TOther>
	RCPtr<TOther> RCPtr<T>::ReinterpretCast() const
	{
		return RCPtr<TOther>(reinterpret_cast<TOther *>(m_object), m_tracker);
	}

	template<class T>
	template<class TOther>
	RCPtr<TOther> RCPtr<T>::ConstCast() const
	{
		return RCPtr<TOther>(const_cast<TOther *>(m_object), m_tracker);
	}

	template<class T>
	template<class TField, class TObject>
	RCPtr<TField> RCPtr<T>::FieldRef(TField TObject:: *fieldRef) const
	{
		RKIT_ASSERT(m_object != nullptr);
		TObject *obj = m_object;

		TField *field = &(obj->*fieldRef);

		return RCPtr<TField>(field, m_tracker);
	}

	template<class T>
	inline RCPtr<T>::operator T *() const
	{
		return m_object;
	}

	template<class T>
	inline T *RCPtr<T>::operator->() const
	{
		return m_object;
	}

	template<class TType, class TPtrType, class... TArgs>
	inline Result NewWithAlloc(RCPtr<TPtrType> &objPtr, IMallocDriver *alloc, TArgs&& ...args)
	{
		void *mem = alloc->Alloc(sizeof(TType));
		if (!mem)
			return ResultCode::kOutOfMemory;

		TType *obj = new (mem) TType(std::forward<TArgs>(args)...);

		RefCounted *refCounted = obj;

		SimpleObjectAllocation<RefCounted> allocation;
		allocation.m_alloc = alloc;
		allocation.m_mem = mem;
		allocation.m_obj = refCounted;

		Private::RefCountedInstantiator::InitRefCounted(*refCounted, allocation);
		RefCountedTracker *tracker = Private::RefCountedInstantiator::GetTrackerFromObject(refCounted);

		objPtr = RCPtr<TPtrType>(obj, tracker);

		return ResultCode::kOK;
	}

	template<class TType, class TPtrType, class... TArgs>
	inline Result New(RCPtr<TPtrType> &objPtr, TArgs&& ...args)
	{
		return NewWithAlloc<TType, TPtrType, TArgs...>(objPtr, GetDrivers().m_mallocDriver, std::forward<TArgs>(args)...);
	}

	template<class TType, class TPtrType>
	Result NewWithAlloc(RCPtr<TPtrType> &objPtr, IMallocDriver *alloc)
	{
		void *mem = alloc->Alloc(sizeof(TType));
		if (!mem)
			return ResultCode::kOutOfMemory;

		TType *obj = new (mem) TType();

		RefCounted *refCounted = obj;

		SimpleObjectAllocation<RefCounted> allocation;
		allocation.m_alloc = alloc;
		allocation.m_mem = mem;
		allocation.m_obj = refCounted;

		Private::RefCountedInstantiator::InitRefCounted(*refCounted, allocation);
		RefCountedTracker *tracker = Private::RefCountedInstantiator::GetTrackerFromObject(refCounted);

		objPtr = RCPtr<TPtrType>(obj, tracker);

		return ResultCode::kOK;
	}

	template<class TType, class TPtrType>
	Result New(RCPtr<TPtrType> &objPtr)
	{
		return NewWithAlloc<TType, TPtrType>(objPtr, GetDrivers().m_mallocDriver);
	}

	namespace Private
	{
		inline void RefCountedInstantiator::InitRefCounted(RefCounted &refCounted, const SimpleObjectAllocation<RefCounted> &alloc)
		{
			refCounted.InitRefCounted(alloc);
		}

		inline RefCountedTracker *RefCountedInstantiator::GetTrackerFromObject(RefCounted *obj)
		{
			return obj;
		}
	}
}

