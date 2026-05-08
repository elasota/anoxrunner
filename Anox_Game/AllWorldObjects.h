#pragma once

namespace anox::game
{
	class WorldObject;
	struct WorldObjectProxy;
	class WorldImpl;

	class AllWorldObjectsIterator
	{
	public:
		friend class AllWorldObjectsCollection;

		AllWorldObjectsIterator();

		WorldObject &operator*() const;

		bool operator==(const AllWorldObjectsIterator &other) const;
		bool operator!=(const AllWorldObjectsIterator &other) const;

		AllWorldObjectsIterator operator++(int);
		AllWorldObjectsIterator &operator++();

	private:
		explicit AllWorldObjectsIterator(WorldObjectProxy *obj);

		WorldObjectProxy *m_proxy;
	};

	// NOTE: Adding objects while iterating a collection is safe.
	// Removing objects while iterating a collection is NOT safe.
	class AllWorldObjectsCollection
	{
	public:
		friend class WorldImpl;

		AllWorldObjectsCollection() = delete;
		AllWorldObjectsCollection(const AllWorldObjectsCollection&) = delete;

		AllWorldObjectsIterator begin() const;
		AllWorldObjectsIterator end() const;

	private:
		explicit AllWorldObjectsCollection(WorldObjectProxy *firstProxy);

		WorldObjectProxy *m_firstProxy;
	};
}

#include "GameObjects/WorldObject.h"

#include "rkit/Core/RKitAssert.h"

namespace anox::game
{
	inline AllWorldObjectsIterator::AllWorldObjectsIterator()
		: m_proxy(nullptr)
	{
	}

	inline AllWorldObjectsIterator::AllWorldObjectsIterator(WorldObjectProxy *proxy)
		: m_proxy(proxy)
	{
	}

	inline bool AllWorldObjectsIterator::operator==(const AllWorldObjectsIterator &other) const
	{
		return m_proxy == other.m_proxy;
	}

	inline bool AllWorldObjectsIterator::operator!=(const AllWorldObjectsIterator &other) const
	{
		return m_proxy != other.m_proxy;
	}

	inline AllWorldObjectsIterator AllWorldObjectsIterator::operator++(int)
	{
		AllWorldObjectsIterator copy = *this;
		++(*this);
		return copy;
	}

	inline AllWorldObjectsCollection::AllWorldObjectsCollection(WorldObjectProxy *firstProxy)
		: m_firstProxy(firstProxy)
	{
	}

	inline AllWorldObjectsIterator AllWorldObjectsCollection::begin() const
	{
		return AllWorldObjectsIterator(m_firstProxy);
	}

	inline AllWorldObjectsIterator AllWorldObjectsCollection::end() const
	{
		return AllWorldObjectsIterator();
	}

	inline WorldObject &AllWorldObjectsIterator::operator*() const
	{
		WorldObject *obj = this->m_proxy->m_object.Get();
		RKIT_ASSERT(obj != nullptr);
		return *obj;
	}
}
