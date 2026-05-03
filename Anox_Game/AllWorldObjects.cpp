#include "AllWorldObjects.h"
#include "GameObjects/WorldObject.h"

#include "rkit/Core/RKitAssert.h"

namespace anox::game
{
	AllWorldObjectsIterator &AllWorldObjectsIterator::operator++()
	{
		RKIT_ASSERT(m_obj != nullptr);

		m_obj = m_obj->GetNextUnsafe();
		return *this;
	}
}
