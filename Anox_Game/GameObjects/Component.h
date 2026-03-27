#pragma once

#include "DynamicObject.h"

namespace anox::game
{
	class Component : public DynamicObject
	{
		ANOX_RTTI_CLASS(Component, DynamicObject)
	};
}
