#include "AllWorldObjects.h"
#include "GameObjects/WorldObject.h"

#include "rkit/Core/RKitAssert.h"

namespace anox::game
{
	AllWorldObjectsIterator &AllWorldObjectsIterator::operator++()
	{
		RKIT_ASSERT(m_proxy != nullptr);

		WorldObjectProxy *nextProxy = m_proxy->m_next.Get();
		while (nextProxy != nullptr && nextProxy->m_object.Get() == nullptr)
			nextProxy = nextProxy->m_next.Get();

		m_proxy = nextProxy;
		return *this;
	}
}
