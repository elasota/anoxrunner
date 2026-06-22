#pragma once

#include "CoreDefs.h"
#include "NoCopy.h"
#include "SimpleObjectAllocation.h"

#include <atomic>

namespace rkit
{
	template<class T>
	class UniquePtr;

	template<class T>
	class WeakPtr;

	class RefCounted;
	class WeakRefTracker;
	struct RefCountedTracker;

	namespace priv
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
		bool RCTrackerAddRefIfNotZero();

	protected:
		virtual void RCTrackerZero() = 0;
		virtual WeakRefTracker *CastToWeakRefTracker();

		RefCount_t RCTrackerRefCount() const;

	private:
		RefCountedTracker() = delete;

		std::atomic<RefCount_t> m_refCount;
	};

	class RefCounted : protected RefCountedTracker
	{
	public:
		friend struct rkit::priv::RefCountedInstantiator;

	protected:
		RefCounted();
		virtual ~RefCounted() {}

		void RCTrackerZero() override;
		RefCount_t RCTrackerRefCount() const;

	private:
		void InitRefCounted(const SimpleObjectAllocation<RefCounted> &alloc);

		SimpleObjectAllocation<RefCounted> m_allocation;
	};

	enum class RCPtrMoveTag
	{
	};

	class TypelessRCPtr
	{
	public:
		template<class TOther>
		friend class RCPtr;

		TypelessRCPtr();
		explicit TypelessRCPtr(RefCountedTracker *tracker);
		explicit TypelessRCPtr(RCPtrMoveTag, RefCountedTracker *tracker);

		TypelessRCPtr(const TypelessRCPtr &other);
		TypelessRCPtr(TypelessRCPtr &&other) noexcept;

		template<class T>
		TypelessRCPtr(const RCPtr<T> &other);
		template<class T>
		TypelessRCPtr(RCPtr<T> &&other) noexcept;
		~TypelessRCPtr();

		bool IsValid() const;
		RKIT_NODISCARD RefCountedTracker *Detach();

		void Reset();

		TypelessRCPtr &operator=(const TypelessRCPtr &other) noexcept;
		TypelessRCPtr &operator=(TypelessRCPtr &&other) noexcept;

		template<class TOther>
		TypelessRCPtr &operator=(const RCPtr<TOther> &other) noexcept;
		template<class TOther>
		TypelessRCPtr &operator=(RCPtr<TOther> &&other) noexcept;

		RefCountedTracker *GetTracker() const;

	private:
		RefCountedTracker *m_tracker;
	};

	template<class T>
	class RCPtr
	{
	public:
		template<class TOther>
		friend class RCPtr;

		friend class TypelessRCPtr;

		RCPtr();
		explicit RCPtr(std::nullptr_t);
		explicit RCPtr(T *ptr);
		explicit RCPtr(T *ptr, RefCountedTracker *tracker);
		explicit RCPtr(RCPtrMoveTag, T *ptr, RefCountedTracker *tracker);
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
		RefCountedTracker *GetTracker() const;
		T& operator*() const;
		T *operator->() const;

		template<class TOther>
		RCPtr<TOther> StaticCast() const;

		template<class TOther>
		RCPtr<TOther> ReinterpretCast() const;

		template<class TOther>
		RCPtr<TOther> ConstCast() const;

		template<class TOther, class TFunction>
		RCPtr<TOther> CastWithFunction(const TFunction &function) const;

		template<class TOther>
		RCPtr<TOther> StaticCastMove();

		template<class TOther>
		RCPtr<TOther> ReinterpretCastMove();

		template<class TOther>
		RCPtr<TOther> ConstCastMove();

		template<class TOther, class TFunction>
		RCPtr<TOther> CastWithFunctionMove(const TFunction &function);

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

	template<class RCType, class UPtrType>
	Result MakeRC(RCPtr<RCType> &rcPtr, UniquePtr<UPtrType> &&uniquePtr);

	template<class T>
	UniquePtr<T> RCPtrToUniquePtr(RCPtr<T> rcPtr);
}

#include "Drivers.h"
#include "MallocDriver.h"
#include "Result.h"
#include "UniquePtr.h"

#include <utility>

namespace rkit::priv
{
	struct UniquePtrRCPtrMallocWrapper : public IMallocDriver
	{
		void *InternalAlloc(size_t size) override;
		void *InternalRealloc(void *ptr, size_t size) override;
		void InternalFree(void *ptr) override;

		static UniquePtrRCPtrMallocWrapper ms_instance;
	};

	template<class T>
	class UniquePtrTracker final : public RefCountedTracker
	{
	public:
		UniquePtrTracker() = delete;
		UniquePtrTracker(UniquePtr<T> &&object);

		void SetSelf(const SimpleObjectAllocation<UniquePtrTracker<T>> &self);

		void RCTrackerZero() override;

	private:

		void InitRefCounted(const SimpleObjectAllocation<RefCounted> &alloc);

		UniquePtr<T> m_object;
		SimpleObjectAllocation<UniquePtrTracker<T>> m_self;
	};

	template<class T>
	UniquePtrTracker<T>::UniquePtrTracker(UniquePtr<T> &&object)
		: RefCountedTracker(0)
		, m_object(std::move(object))
	{
	}

	template<class T>
	void UniquePtrTracker<T>::SetSelf(const SimpleObjectAllocation<UniquePtrTracker<T>> &self)
	{
		m_self = self;
	}

	template<class T>
	void UniquePtrTracker<T>::RCTrackerZero()
	{
		rkit::SafeDelete(m_self);
	}

	inline void *UniquePtrRCPtrMallocWrapper::InternalAlloc(size_t size)
	{
		RKIT_ASSERT(false);
		return nullptr;
	}

	inline void *UniquePtrRCPtrMallocWrapper::InternalRealloc(void *ptr, size_t size)
	{
		if (size == 0)
		{
			InternalFree(ptr);
			return nullptr;
		}

		RKIT_ASSERT(false);
		return nullptr;
	}

	inline void UniquePtrRCPtrMallocWrapper::InternalFree(void *ptr)
	{
		static_cast<RefCountedTracker *>(ptr)->RCTrackerDecRef();
	}

	inline UniquePtrRCPtrMallocWrapper UniquePtrRCPtrMallocWrapper::ms_instance;
}

namespace rkit
{
	inline RefCountedTracker::RefCountedTracker(RefCount_t initialCount)
		: m_refCount(initialCount)
	{
	}

	inline void RefCountedTracker::RCTrackerAddRef()
	{
		(void)m_refCount.fetch_add(1, std::memory_order_relaxed);
	}

	inline bool RefCountedTracker::RCTrackerAddRefIfNotZero()
	{
		RefCount_t refCount = m_refCount.load(std::memory_order_relaxed);
		while (refCount != 0)
		{
			const RefCount_t desiredCount = refCount + 1;
			if (m_refCount.compare_exchange_weak(refCount, desiredCount, std::memory_order_relaxed))
				return true;
		}

		return false;
	}

	inline WeakRefTracker *RefCountedTracker::CastToWeakRefTracker()
	{
		return nullptr;
	}

	inline void RefCountedTracker::RCTrackerDecRef()
	{
		if (m_refCount.fetch_sub(1, std::memory_order_release) == 1)
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
		, m_tracker(priv::RefCountedInstantiator::GetTrackerFromObject(ptr))
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
	RCPtr<T>::RCPtr(RCPtrMoveTag, T *ptr, RefCountedTracker *tracker)
		: m_object(ptr)
		, m_tracker(tracker)
	{
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
			RefCountedTracker *newTracker = priv::RefCountedInstantiator::GetTrackerFromObject(newObj);

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
	RefCountedTracker *RCPtr<T>::GetTracker() const
	{
		return m_tracker;
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
	template<class TOther, class TFunction>
	RCPtr<TOther> RCPtr<T>::CastWithFunction(const TFunction &function) const
	{
		TOther *castPtr = function(static_cast<T *>(m_object));

		if (castPtr == nullptr)
			return RCPtr<TOther>();

		return RCPtr<TOther>(castPtr, m_tracker);
	}

	template<class T>
	template<class TOther>
	RCPtr<TOther> RCPtr<T>::StaticCastMove()
	{
		TOther *object = static_cast<TOther *>(m_object);
		RefCountedTracker *tracker = m_tracker;

		m_object = nullptr;
		m_tracker = nullptr;

		return RCPtr<TOther>(RCPtrMoveTag(), object, tracker);
	}

	template<class T>
	template<class TOther>
	RCPtr<TOther> RCPtr<T>::ReinterpretCastMove()
	{
		TOther *object = reinterpret_cast<TOther *>(m_object);
		RefCountedTracker *tracker = m_tracker;

		m_object = nullptr;
		m_tracker = nullptr;

		return RCPtr<TOther>(RCPtrMoveTag(), object, tracker);
	}

	template<class T>
	template<class TOther>
	RCPtr<TOther> RCPtr<T>::ConstCastMove()
	{
		TOther *object = const_cast<TOther *>(m_object);
		RefCountedTracker *tracker = m_tracker;

		m_object = nullptr;
		m_tracker = nullptr;

		return RCPtr<TOther>(RCPtrMoveTag(), object, tracker);
	}

	template<class T>
	template<class TOther, class TFunction>
	RCPtr<TOther> RCPtr<T>::CastWithFunctionMove(const TFunction &function)
	{
		if (m_object == nullptr)
			return RCPtr<TOther>();

		TOther *castPtr = function(static_cast<T *>(m_object));

		if (castPtr == nullptr)
			return RCPtr<TOther>();

		RefCountedTracker *tracker = m_tracker;

		m_object = nullptr;
		m_tracker = nullptr;

		return RCPtr<TOther>(RCPtrMoveTag(), castPtr, m_tracker);
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
	inline T& RCPtr<T>::operator*() const
	{
		RKIT_ASSERT(m_object != nullptr);
		return *m_object;
	}

	template<class T>
	inline T *RCPtr<T>::operator->() const
	{
		return m_object;
	}

	// Typeless
	inline TypelessRCPtr::TypelessRCPtr()
		: m_tracker(nullptr)
	{
	}

	inline TypelessRCPtr::TypelessRCPtr(RefCountedTracker *tracker)
		: m_tracker(tracker)
	{
		if (tracker)
			tracker->RCTrackerAddRef();
	}

	inline TypelessRCPtr::TypelessRCPtr(RCPtrMoveTag, RefCountedTracker *tracker)
		: m_tracker(tracker)
	{
	}

	template<class T>
	inline TypelessRCPtr::TypelessRCPtr(const RCPtr<T> &other)
		: m_tracker(other.m_tracker)
	{
		if (m_tracker)
			m_tracker->RCTrackerAddRef();
	}

	template<class T>
	inline TypelessRCPtr::TypelessRCPtr(RCPtr<T> &&other) noexcept
		: m_tracker(other.m_tracker)
	{
		other.m_object = nullptr;
		other.m_tracker = nullptr;
	}

	inline TypelessRCPtr::TypelessRCPtr(const TypelessRCPtr &other)
		: m_tracker(other.m_tracker)
	{
		if (m_tracker)
			m_tracker->RCTrackerAddRef();
	}

	inline TypelessRCPtr::TypelessRCPtr(TypelessRCPtr &&other) noexcept
		: m_tracker(other.m_tracker)
	{
		other.m_tracker = nullptr;
	}

	inline TypelessRCPtr::~TypelessRCPtr()
	{
		if (m_tracker)
			m_tracker->RCTrackerDecRef();
	}

	inline bool TypelessRCPtr::IsValid() const
	{
		return m_tracker != nullptr;
	}

	inline RefCountedTracker *TypelessRCPtr::Detach()
	{
		RefCountedTracker *tracker = m_tracker;
		m_tracker = nullptr;
		return tracker;
	}

	inline void TypelessRCPtr::Reset()
	{
		RefCountedTracker *tracker = m_tracker;	// In case this gets destroyed
		m_tracker = nullptr;

		if (m_tracker)
			m_tracker->RCTrackerDecRef();
	}

	inline TypelessRCPtr &TypelessRCPtr::operator=(const TypelessRCPtr &other) noexcept
	{
		if (this != &other)
		{
			RefCountedTracker *oldTracker = m_tracker;
			RefCountedTracker *newTracker = other.m_tracker;

			m_tracker = newTracker;
			if (newTracker)
				newTracker->RCTrackerAddRef();
			if (oldTracker)
				oldTracker->RCTrackerDecRef();			
		}

		return *this;
	}

	inline TypelessRCPtr &TypelessRCPtr::operator=(TypelessRCPtr &&other) noexcept
	{
		RKIT_ASSERT(this != &other);

		RefCountedTracker *oldTracker = m_tracker;
		RefCountedTracker *newTracker = other.m_tracker;

		m_tracker = newTracker;
		other.m_tracker = nullptr;

		if (oldTracker)
			oldTracker->RCTrackerDecRef();

		return *this;
	}

	template<class TOther>
	TypelessRCPtr &TypelessRCPtr::operator=(const RCPtr<TOther> &other) noexcept
	{
		(*this) = TypelessRCPtr(other);
		return *this;
	}

	template<class TOther>
	inline TypelessRCPtr &TypelessRCPtr::operator=(RCPtr<TOther> &&other) noexcept
	{
		(*this) = TypelessRCPtr(std::move(other));
		return *this;
	}

	template<class TType, class TPtrType, class... TArgs>
	inline Result NewWithAlloc(RCPtr<TPtrType> &objPtr, IMallocDriver *alloc, TArgs&& ...args)
	{
		void *mem = alloc->Alloc(sizeof(TType));
		if (!mem)
			RKIT_THROW(ResultCode::kOutOfMemory);

		TType *obj = new (mem) TType(std::forward<TArgs>(args)...);

		RefCounted *refCounted = obj;

		SimpleObjectAllocation<RefCounted> allocation;
		allocation.m_alloc = alloc;
		allocation.m_mem = mem;
		allocation.m_obj = refCounted;

		priv::RefCountedInstantiator::InitRefCounted(*refCounted, allocation);
		RefCountedTracker *tracker = priv::RefCountedInstantiator::GetTrackerFromObject(refCounted);

		objPtr = RCPtr<TPtrType>(obj, tracker);

		RKIT_RETURN_OK;
	}

	template<class TType, class TPtrType, class... TArgs>
	inline Result New(RCPtr<TPtrType> &objPtr, TArgs&& ...args)
	{
		return NewWithAlloc<TType, TPtrType, TArgs...>(objPtr, GetDrivers().m_mallocDriver.Get(), std::forward<TArgs>(args)...);
	}

	template<class TType, class TPtrType>
	Result NewWithAlloc(RCPtr<TPtrType> &objPtr, IMallocDriver *alloc)
	{
		void *mem = alloc->Alloc(sizeof(TType));
		if (!mem)
			RKIT_THROW(ResultCode::kOutOfMemory);

		TType *obj = new (mem) TType();

		RefCounted *refCounted = obj;

		SimpleObjectAllocation<RefCounted> allocation;
		allocation.m_alloc = alloc;
		allocation.m_mem = mem;
		allocation.m_obj = refCounted;

		priv::RefCountedInstantiator::InitRefCounted(*refCounted, allocation);
		RefCountedTracker *tracker = priv::RefCountedInstantiator::GetTrackerFromObject(refCounted);

		objPtr = RCPtr<TPtrType>(obj, tracker);

		RKIT_RETURN_OK;
	}

	template<class TType, class TPtrType>
	Result New(RCPtr<TPtrType> &objPtr)
	{
		return NewWithAlloc<TType, TPtrType>(objPtr, GetDrivers().m_mallocDriver.Get());
	}

	template<class RCType, class UPtrType>
	Result MakeRC(RCPtr<RCType> &rcPtr, UniquePtr<UPtrType> &&uniquePtr)
	{
		RCType *object = uniquePtr.Get();

		if (object == nullptr)
		{
			rcPtr.Reset();
			RKIT_RETURN_OK;
		}

		UniquePtr<priv::UniquePtrTracker<UPtrType>> tracker;
		RKIT_CHECK(New<priv::UniquePtrTracker<UPtrType>>(tracker, std::move(uniquePtr)));

		SimpleObjectAllocation<priv::UniquePtrTracker<UPtrType>> trackerAllocation = tracker.Detach();
		trackerAllocation.m_obj->SetSelf(trackerAllocation);

		rcPtr = RCPtr<RCType>(object, trackerAllocation.m_obj);
		RKIT_RETURN_OK;
	}

	template<class T>
	UniquePtr<T> RCPtrToUniquePtr(RCPtr<T> rcPtr)
	{
		rkit::RefCountedTracker *tracker = nullptr;
		T *obj = nullptr;

		rcPtr.Detach(obj, tracker);

		if (!obj)
			return UniquePtr<T>();

		return UniquePtr<T>(obj, tracker, &priv::UniquePtrRCPtrMallocWrapper::ms_instance);
	}

	namespace priv
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

