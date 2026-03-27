#pragma once

#include "rkit/Core/CoroutineProtos.h"
#include "rkit/Core/String.h"

#include "rkit/Math/Vec.h"

#include "Component.h"
#include "Serializable.h"

namespace rkit
{
	struct ICoroThread;
}

namespace anox::data
{
	struct EClass_placed;
}

namespace anox::game
{
	struct WorldObjectSpawnParams;

	class PlacedComponent : public Component, public Serializable<PlacedComponent>
	{
		ANOX_RTTI_CLASS_MULTI_BASE(PlacedComponent, Component, Serializable<PlacedComponent>)
		ANOX_SERIALIZABLE_PROTOTYPE

	public:
		rkit::ResultCoroutine Initialize(rkit::ICoroThread &thread, const WorldObjectSpawnParams &spawnParams, const data::EClass_placed &spawnDef);

	private:
		/* reflector:properties(anox::game::PlacedComponent) */
		rkit::math::Vec3 m_origin;
		rkit::math::Vec3 m_angles;
		rkit::math::Vec3 m_offset;
		rkit::math::Vec3 m_scale;
		bool m_newScaling = false;
		float m_volume = 0;
		rkit::ByteString m_noise;
		float m_attenuation = 0;
		rkit::ByteString m_target;
		rkit::ByteString m_targetname;
		uint32_t m_particleFlags = 0;
		uint32_t m_spawnFlags = 0;
		rkit::ByteString m_spawnCondition;
		/* reflector:endproperties */
	};
}
