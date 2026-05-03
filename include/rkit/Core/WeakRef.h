#pragma once

#include "RefCounted.h"
#include "UniquePtr.h"

#include <memory>

namespace rkit
{
	template<class T>
	class WeakPtr;

	class WeakRefTracker;
}

namespace rkit::priv
{
	class WeakRefWeakTracker : public RefCountedTracker
	{
	protected:
		explicit WeakRefWeakTracker(RefCountedTracker::RefCount_t initialCount);

		void RCTrackerZero() override final;
		virtual void WeakTrackerZero() = 0;
	};

	class WeakRefStrongTracker : public RefCountedTracker
	{
	protected:
		explicit WeakRefStrongTracker(RefCountedTracker::RefCount_t initialCount);

		void RCTrackerZero() override final;
		WeakRefTracker *CastToWeakRefTracker() override final;

		virtual void StrongTrackerZero() = 0;
	};
}

namespace rkit
{
	class WeakRefTracker final : protected priv::WeakRefWeakTracker, protected priv::WeakRefStrongTracker
	{
	public:
		friend class priv::WeakRefStrongTracker;

		typedef void (*DeleterFunc_t)(void *context) noexcept;

		WeakRefTracker(DeleterFunc_t strongDeleter, DeleterFunc_t finalDeleter, void *context);

		RefCountedTracker &GetWeakTracker();
		RefCountedTracker &GetStrongTracker();

		// WARNING: These only work for refs that were created using WeakRefTracker in the first place!
		static WeakRefTracker *FromStrongTracker(RefCountedTracker *tracker);
		static const WeakRefTracker *FromStrongTracker(const RefCountedTracker *tracker);
		static WeakRefTracker *FromWeakTracker(RefCountedTracker *tracker);
		static const WeakRefTracker *FromWeakTracker(const RefCountedTracker *tracker);

	private:
		void StrongTrackerZero() override;
		void WeakTrackerZero() override;

		WeakRefTracker() = delete;

		DeleterFunc_t m_strongDeleter = nullptr;
		DeleterFunc_t m_finalDeleter = nullptr;
		void *m_deleterContext = nullptr;
	};

	template<class T>
	class WeakPtr
	{
	public:
		template<class TOther>
		friend class WeakPtr;

		WeakPtr();
		explicit WeakPtr(std::nullptr_t);
		explicit WeakPtr(T *ptr, WeakRefTracker *tracker);
		explicit WeakPtr(RCPtrMoveTag, T *ptr, WeakRefTracker *tracker);
		WeakPtr(const WeakPtr<T> &other) noexcept;
		WeakPtr(WeakPtr<T> &&other) noexcept;

		template<class TOther>
		WeakPtr(const WeakPtr<TOther> &other);

		template<class TOther>
		WeakPtr(WeakPtr<TOther> &&other);

		~WeakPtr();

		void Reset();

		WeakPtr &operator=(const WeakPtr &other) noexcept;
		WeakPtr &operator=(WeakPtr &&other) noexcept;

		template<class TOther>
		WeakPtr &operator=(const WeakPtr<TOther> &other) noexcept;

		template<class TOther>
		WeakPtr &operator=(WeakPtr<TOther> &&other) noexcept;

		WeakPtr &operator=(std::nullptr_t);

		RCPtr<T> Lock() const;
		RefCountedTracker *GetWeakTracker() const;
		RefCountedTracker *GetStrongTracker() const;

		template<class TOther>
		WeakPtr<TOther> StaticCast() const;

		template<class TOther>
		WeakPtr<TOther> ReinterpretCast() const;

		template<class TOther>
		WeakPtr<TOther> ConstCast() const;

		template<class TOther>
		WeakPtr<TOther> StaticCastMove();

		template<class TOther>
		WeakPtr<TOther> ReinterpretCastMove();

		template<class TOther>
		WeakPtr<TOther> ConstCastMove();

		template<class TField, class TObject>
		WeakPtr<TField> FieldRef(TField TObject:: *fieldRef) const;

	private:
		T *m_object;
		WeakRefTracker *m_tracker;
	};
}

#include "RKitAssert.h"


namespace rkit::priv
{
	inline WeakRefWeakTracker::WeakRefWeakTracker(RefCountedTracker::RefCount_t initialCount)
		: RefCountedTracker(initialCount)
	{
	}

	inline void WeakRefWeakTracker::RCTrackerZero()
	{
		this->WeakTrackerZero();
	}

	inline WeakRefStrongTracker::WeakRefStrongTracker(RefCountedTracker::RefCount_t initialCount)
		: RefCountedTracker(initialCount)
	{
	}

	inline void WeakRefStrongTracker::RCTrackerZero()
	{
		this->StrongTrackerZero();
	}

	inline WeakRefTracker *WeakRefStrongTracker::CastToWeakRefTracker()
	{
		return static_cast<WeakRefTracker *>(this);
	}
}

namespace rkit
{
	inline WeakRefTracker::WeakRefTracker(DeleterFunc_t strongDeleter, DeleterFunc_t finalDeleter, void *context)
		: priv::WeakRefWeakTracker(1)
		, priv::WeakRefStrongTracker(0)
		, m_strongDeleter(strongDeleter)
		, m_finalDeleter(finalDeleter)
		, m_deleterContext(context)
	{
	}

	inline RefCountedTracker &WeakRefTracker::GetWeakTracker()
	{
		priv::WeakRefWeakTracker *self = this;
		return *self;
	}

	inline RefCountedTracker &WeakRefTracker::GetStrongTracker()
	{
		priv::WeakRefStrongTracker *self = this;
		return *self;
	}

	inline WeakRefTracker *WeakRefTracker::FromStrongTracker(RefCountedTracker *tracker)
	{
		return static_cast<WeakRefTracker *>(static_cast<priv::WeakRefStrongTracker *>(tracker));
	}

	inline const WeakRefTracker *WeakRefTracker::FromStrongTracker(const RefCountedTracker *tracker)
	{
		return static_cast<const WeakRefTracker *>(static_cast<const priv::WeakRefStrongTracker *>(tracker));
	}

	inline WeakRefTracker *WeakRefTracker::FromWeakTracker(RefCountedTracker *tracker)
	{
		return static_cast<WeakRefTracker *>(static_cast<priv::WeakRefWeakTracker *>(tracker));
	}

	inline const WeakRefTracker *WeakRefTracker::FromWeakTracker(const RefCountedTracker *tracker)
	{
		return static_cast<const WeakRefTracker *>(static_cast<const priv::WeakRefWeakTracker *>(tracker));
	}

	inline void WeakRefTracker::StrongTrackerZero()
	{
		m_strongDeleter(m_deleterContext);

		priv::WeakRefWeakTracker *weakSelf = this;
		weakSelf->RCTrackerDecRef();
	}

	inline void WeakRefTracker::WeakTrackerZero()
	{
		void *context = m_deleterContext;
		m_finalDeleter(m_deleterContext);
	}

	template<class T>
	WeakPtr<T>::WeakPtr()
		: m_object(nullptr)
		, m_tracker(nullptr)
	{
	}

	template<class T>
	WeakPtr<T>::WeakPtr(std::nullptr_t)
		: m_object(nullptr)
		, m_tracker(nullptr)
	{
	}

	template<class T>
	WeakPtr<T>::WeakPtr(T *ptr, WeakRefTracker *tracker)
		: m_object(ptr)
		, m_tracker(tracker)
	{
		if (tracker)
			tracker->GetWeakTracker().RCTrackerAddRef();
	}

	template<class T>
	WeakPtr<T>::WeakPtr(RCPtrMoveTag, T *ptr, WeakRefTracker *tracker)
		: m_object(ptr)
		, m_tracker(tracker)
	{
	}

	template<class T>
	WeakPtr<T>::WeakPtr(const WeakPtr<T> &other) noexcept
		: m_object(other.m_object)
		, m_tracker(other.m_tracker)
	{
		if (m_tracker)
			m_tracker->GetWeakTracker().RCTrackerAddRef();
	}

	template<class T>
	WeakPtr<T>::WeakPtr(WeakPtr<T> &&other) noexcept
		: m_object(other.m_object)
		, m_tracker(other.m_tracker)
	{
	}

	template<class T>
	template<class TOther>
	WeakPtr<T>::WeakPtr(const WeakPtr<TOther> &other)
		: m_object(nullptr)
		, m_tracker(nullptr)
	{
		(*this) = other;
	}

	template<class T>
	template<class TOther>
	WeakPtr<T>::WeakPtr(WeakPtr<TOther> &&other)
	{
		(*this) = std::move(other);
	}

	template<class T>
	WeakPtr<T>::~WeakPtr()
	{
		if (m_tracker)
			m_tracker->GetWeakTracker().RCTrackerDecRef();
	}

	template<class T>
	void WeakPtr<T>::Reset()
	{
		WeakRefTracker *tracker = m_tracker;
		m_tracker = nullptr;
		m_object = nullptr;

		if (tracker)
			tracker->GetWeakTracker().RCTrackerDecRef();
	}

	template<class T>
	WeakPtr<T> &WeakPtr<T>::operator=(const WeakPtr<T> &other) noexcept
	{
		WeakRefTracker *oldTracker = m_tracker;
		WeakRefTracker *newTracker = other.m_tracker;

		m_object = other.m_object;
		m_tracker = newTracker;

		if (newTracker)
			newTracker->GetWeakTracker().RCTrackerAddRef();
		if (oldTracker)
			oldTracker->GetWeakTracker().RCTrackerDecRef();

		return *this;
	}

	template<class T>
	WeakPtr<T> &WeakPtr<T>::operator=(WeakPtr &&other) noexcept
	{
		WeakRefTracker *oldTracker = m_tracker;
		m_object = other.m_object;
		m_tracker = other.m_tracker;

		other.m_object = nullptr;
		other.m_tracker = nullptr;

		if (oldTracker)
			oldTracker->GetWeakTracker().RCTrackerDecRef();

		return *this;
	}

	template<class T>
	template<class TOther>
	WeakPtr<T> &WeakPtr<T>::operator=(const WeakPtr<TOther> &other) noexcept
	{
		WeakRefTracker *oldTracker = m_tracker;
		WeakRefTracker *newTracker = other.m_tracker;

		// Should use is_virtual_base_of in C++26 here
		T *recastObject = other.m_object;

		m_object = recastObject;
		m_tracker = newTracker;

		if (newTracker)
			newTracker->GetWeakTracker().RCTrackerAddRef();
		if (oldTracker)
			oldTracker->GetWeakTracker().RCTrackerDecRef();

		return *this;
	}

	template<class T>
	template<class TOther>
	WeakPtr<T> &WeakPtr<T>::operator=(WeakPtr<TOther> &&other) noexcept
	{
		WeakRefTracker *oldTracker = m_tracker;

		// Should use is_virtual_base_of in C++26 here
		// This isn't safe if the base is virtual!
		T *recastObject = other.m_object;

		m_object = recastObject;
		m_tracker = other.m_tracker;

		other.m_object = nullptr;
		other.m_tracker = nullptr;

		if (oldTracker)
			oldTracker->GetWeakTracker().RCTrackerDecRef();

		return *this;
	}

	template<class T>
	WeakPtr<T> &WeakPtr<T>::operator=(std::nullptr_t)
	{
		this->Reset();
		return *this;
	}

	template<class T>
	RCPtr<T> WeakPtr<T>::Lock() const
	{
		if (m_tracker != nullptr)
		{
			rkit::RefCountedTracker *strongTracker = &m_tracker->GetStrongTracker();
			if (strongTracker->RCTrackerAddRefIfNotZero())
				return RCPtr<T>(RCPtrMoveTag(), m_object, strongTracker);
		}

		return nullptr;
	}

	template<class T>
	RefCountedTracker *WeakPtr<T>::GetWeakTracker() const
	{
		if (m_tracker)
			return &m_tracker->GetWeakTracker();

		return nullptr;
	}

	template<class T>
	RefCountedTracker *WeakPtr<T>::GetStrongTracker() const
	{
		if (m_tracker)
			return &m_tracker->GetStrongTracker();

		return nullptr;
	}

	template<class T>
	template<class TOther>
	WeakPtr<TOther> WeakPtr<T>::StaticCast() const
	{
		return this->template StaticCastMove<TOther>(WeakPtr<T>(*this));
	}

	template<class T>
	template<class TOther>
	WeakPtr<TOther> WeakPtr<T>::ReinterpretCast() const
	{
		return WeakPtr<TOther>(reinterpret_cast<TOther *>(m_object), m_tracker);
	}

	template<class T>
	template<class TOther>
	WeakPtr<TOther> WeakPtr<T>::ConstCast() const
	{
		return WeakPtr<TOther>(const_cast<TOther *>(m_object), m_tracker);
	}

	template<class T>
	template<class TOther>
	WeakPtr<TOther> WeakPtr<T>::StaticCastMove()
	{
		// Should use is_virtual_base_of in C++26 here
		TOther *recastPtr = static_cast<TOther *>(m_object);
		WeakRefTracker *tracker = m_tracker;

		m_tracker = nullptr;
		m_object = nullptr;

		return WeakPtr<TOther>(RCPtrMoveTag(), recastPtr, tracker);
	}

	template<class T>
	template<class TOther>
	WeakPtr<TOther> WeakPtr<T>::ReinterpretCastMove()
	{
		TOther *recastPtr = reinterpret_cast<TOther *>(m_object);
		WeakRefTracker *tracker = m_tracker;

		m_tracker = nullptr;
		m_object = nullptr;

		return WeakPtr<TOther>(RCPtrMoveTag(), recastPtr, tracker);
	}

	template<class T>
	template<class TOther>
	WeakPtr<TOther> WeakPtr<T>::ConstCastMove()
	{
		TOther *recastPtr = const_cast<TOther>(m_object);
		WeakRefTracker *tracker = m_tracker;

		m_tracker = nullptr;
		m_object = nullptr;

		return WeakPtr<TOther>(RCPtrMoveTag(), recastPtr, tracker);
	}

	template<class T>
	template<class TField, class TObject>
	WeakPtr<TField> WeakPtr<T>::FieldRef(TField TObject:: *fieldRef) const
	{
		if (m_object)
		{
			TObject *obj = m_object;
			TField *field = &(obj->*fieldRef);

			return WeakPtr<TField>(field, m_tracker);
		}
		else
			return WeakPtr<TField>();
	}
}
