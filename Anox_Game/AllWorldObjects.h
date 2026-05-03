#pragma once

namespace anox::game
{
	class WorldObject;
	class WorldImpl;

	class AllWorldObjectsIterator
	{
	public:
		AllWorldObjectsIterator();
		explicit AllWorldObjectsIterator(WorldObject *obj);

		WorldObject *operator*() const;

		bool operator==(const AllWorldObjectsIterator &other) const;
		bool operator!=(const AllWorldObjectsIterator &other) const;

		AllWorldObjectsIterator operator++(int);
		AllWorldObjectsIterator &operator++();

	private:
		WorldObject *m_obj;
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
		explicit AllWorldObjectsCollection(WorldObject *firstObj);

		WorldObject *m_firstObject;
	};
}

namespace anox::game
{
	inline AllWorldObjectsIterator::AllWorldObjectsIterator()
		: m_obj(nullptr)
	{
	}

	inline AllWorldObjectsIterator::AllWorldObjectsIterator(WorldObject *obj)
		: m_obj(obj)
	{
	}

	inline WorldObject *AllWorldObjectsIterator::operator*() const
	{
		return m_obj;
	}

	inline bool AllWorldObjectsIterator::operator==(const AllWorldObjectsIterator &other) const
	{
		return m_obj == other.m_obj;
	}

	inline bool AllWorldObjectsIterator::operator!=(const AllWorldObjectsIterator &other) const
	{
		return m_obj != other.m_obj;
	}

	inline AllWorldObjectsIterator AllWorldObjectsIterator::operator++(int)
	{
		AllWorldObjectsIterator copy = *this;
		++(*this);
		return copy;
	}

	inline AllWorldObjectsCollection::AllWorldObjectsCollection(WorldObject *firstObj)
		: m_firstObject(firstObj)
	{
	}

	inline AllWorldObjectsIterator AllWorldObjectsCollection::begin() const
	{
		return AllWorldObjectsIterator(m_firstObject);
	}

	inline AllWorldObjectsIterator AllWorldObjectsCollection::end() const
	{
		return AllWorldObjectsIterator();
	}
}
