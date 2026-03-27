#pragma once

#include "rkit/Core/CoroutineProtos.h"
#include "rkit/Math/Vec.h"

#include "Component.h"
#include "Serializable.h"

namespace anox::data
{
	struct EClass_colorable;
}

namespace anox::game
{
	struct WorldObjectSpawnParams;

	class ColorableComponent : public Component, public Serializable<ColorableComponent>
	{
		ANOX_RTTI_CLASS_MULTI_BASE(ColorableComponent, Component, Serializable<ColorableComponent>)
		ANOX_SERIALIZABLE_PROTOTYPE

	public:
		rkit::ResultCoroutine Initialize(rkit::ICoroThread &thread, const WorldObjectSpawnParams &spawnParams, const data::EClass_colorable &spawnDef);

	private:
		/* reflector:properties(anox::game::ColorableComponent) */
		rkit::math::Vec3 m_rgb;
		float m_alpha = 0;
		bool m_alphaFlags = false;
		/* reflector:endproperties */
	};
}
