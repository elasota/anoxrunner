#pragma once

#include "anox/Label.h"
#include "rkit/Core/CoroutineProtos.h"

#include "Serializable.h"
#include "Component.h"

namespace anox::data
{
	struct EClass_clickable;
}

namespace anox::game
{
	struct WorldObjectSpawnParams;

	class ClickableComponent : public Component, public Serializable<ClickableComponent>
	{
		ANOX_RTTI_CLASS_MULTI_BASE(ClickableComponent, Component, Serializable<ClickableComponent>)
		ANOX_SERIALIZABLE_PROTOTYPE

	public:
		rkit::ResultCoroutine Initialize(rkit::ICoroThread &thread, const WorldObjectSpawnParams &spawnParams, const data::EClass_clickable &spawnDef);

	private:
		/* reflector:properties(anox::game::ClickableComponent) */
		Label m_sequence;
		Label m_wsequence;
		/* reflector:endproperties */
	};
}
