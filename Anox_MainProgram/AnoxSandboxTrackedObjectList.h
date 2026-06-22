#pragma once

#include "rkit/Core/Optional.h"
#include "rkit/Core/Vector.h"

namespace anox::game::priv
{
	template<class T>
	class TrackedObjectDefaultDisposer
	{
	public:
		static void DisposeObject(T &object) {}
	};

	template<class T>
	class TrackedObjectListBase
	{
	public:
		rkit::Result RegisterObject(uint32_t &outObjectID, const T &object);
		rkit::Result RegisterObject(uint32_t &outObjectID, T &&object);

		void ReplaceObject(uint32_t objectID, const T &object);
		void ReplaceObject(uint32_t objectID, T &&object);

		T GetObject(uint32_t objectID) const;
		T *TryGetObject(uint32_t objectID);
		const T *TryGetObject(uint32_t objectID) const;

	private:
		rkit::Result AcquireObjectID(uint32_t &outObjectID);

	protected:
		// This is used to keep the object and free ID lists the same size, but the
		// free flag is associated with the object and the free ID is not.
		struct ObjectAndFreeID
		{
			rkit::Optional<T> m_object;
			uint32_t m_freeID = 0;
		};

		rkit::Vector<ObjectAndFreeID> m_objects;

		size_t m_numFreeIDs = 0;
	};
}

namespace anox::game
{

	template<class T, class TDisposer = priv::TrackedObjectDefaultDisposer<T>>
	class TrackedObjectList : public priv::TrackedObjectListBase<T>
	{
	public:
		TrackedObjectList();
		explicit TrackedObjectList(const TDisposer &disposer);
		explicit TrackedObjectList(TDisposer &&disposer);
		~TrackedObjectList();

		void DestroyObject(uint32_t objectID);

	private:
		TDisposer m_disposer;
	};
}

namespace anox::game::priv
{
	template<class T>
	rkit::Result TrackedObjectListBase<T>::RegisterObject(uint32_t &outObjectID, const T &object)
	{
		uint32_t oid = 0;
		RKIT_CHECK(AcquireObjectID(oid));

		this->m_objects[oid - 1].m_object = object;

		outObjectID = oid;

		RKIT_RETURN_OK;
	}

	template<class T>
	rkit::Result TrackedObjectListBase<T>::RegisterObject(uint32_t &outObjectID, T &&object)
	{
		uint32_t oid = 0;
		RKIT_CHECK(AcquireObjectID(oid));

		this->m_objects[oid - 1].m_object = std::move(object);

		outObjectID = oid;

		RKIT_RETURN_OK;
	}

	template<class T>
	void TrackedObjectListBase<T>::ReplaceObject(uint32_t objectID, const T &object)
	{
		RKIT_ASSERT(this->m_objects[objectID - 1].m_object.IsSet());
		this->m_objects[objectID - 1].m_object = object;
	}

	template<class T>
	void TrackedObjectListBase<T>::ReplaceObject(uint32_t objectID, T &&object)
	{
		this->m_objects[objectID - 1].m_object = std::move(object);
	}

	template<class T>
	T TrackedObjectListBase<T>::GetObject(uint32_t objectID) const
	{
		const T *objPtr = TryGetObject(objectID);
		if (objPtr == nullptr)
			return T();

		return *objPtr;
	}

	template<class T>
	const T *TrackedObjectListBase<T>::TryGetObject(uint32_t objectID) const
	{
		return const_cast<TrackedObjectList<T> *>(this)->TryGetObject(objectID);
	}

	template<class T>
	T *TrackedObjectListBase<T>::TryGetObject(uint32_t objectID)
	{
		if (objectID == 0 || objectID > m_objects.Count())
			return nullptr;

		return &m_objects[objectID - 1].m_object.Get();
	}

	template<class T>
	rkit::Result TrackedObjectListBase<T>::AcquireObjectID(uint32_t &objectID)
	{
		if (m_numFreeIDs == 0)
		{
			if (m_objects.Count() == std::numeric_limits<uint32_t>::max())
				RKIT_THROW(rkit::ResultCode::kIntegerOverflow);

			RKIT_CHECK(m_objects.Append(ObjectAndFreeID()));
			objectID = static_cast<uint32_t>(m_objects.Count());
		}
		else
		{
			--m_numFreeIDs;
			objectID = m_objects[m_numFreeIDs].m_freeID;
		}

		RKIT_RETURN_OK;
	}
}

namespace anox::game
{
	template<class T, class TDisposer>
	TrackedObjectList<T, TDisposer>::TrackedObjectList()
	{
	}

	template<class T, class TDisposer>
	TrackedObjectList<T, TDisposer>::TrackedObjectList(const TDisposer &disposer)
		: m_disposer(disposer)
	{
	}

	template<class T, class TDisposer>
	TrackedObjectList<T, TDisposer>::TrackedObjectList(TDisposer &&disposer)
		: m_disposer(std::move(disposer))
	{
	}

	template<class T, class TDisposer>
	TrackedObjectList<T, TDisposer>::~TrackedObjectList()
	{
		typedef typename priv::TrackedObjectListBase<T>::ObjectAndFreeID ObjectAndFreeID_t;

		for (ObjectAndFreeID_t &objAndFreeID : this->m_objects)
		{
			rkit::Optional<T> &obj = objAndFreeID.m_object;
			if (obj.IsSet())
				m_disposer.DisposeObject(obj.Get());
		}
	}

	template<class T, class TDisposer>
	void TrackedObjectList<T, TDisposer>::DestroyObject(uint32_t objectID)
	{
		if (objectID > 0 && objectID <= this->m_objects.Count())
		{
			rkit::Optional<T> &obj = this->m_objects[objectID - 1].m_object;
			if (obj.IsSet())
			{
				m_disposer.DisposeObject(obj.Get());
				obj.Reset();
				this->m_objects[this->m_numFreeIDs++].m_freeID = objectID;
			}
		}
	}
}
