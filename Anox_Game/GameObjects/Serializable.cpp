#include "Serializable.h"

namespace anox::game
{
	SerializableBase::SerializableBase(const VTable &vtable)
		: m_vtable(vtable)
	{
	}
}
