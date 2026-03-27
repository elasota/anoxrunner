#include "PlacedComponent.h"

#include "rkit/Core/Coroutine.h"

#include "anox/Data/EntityStructs.h"

#include "EntityStringLoader.h"

namespace anox::game
{
	rkit::ResultCoroutine PlacedComponent::Initialize(rkit::ICoroThread &thread, const WorldObjectSpawnParams &spawnParams, const data::EClass_placed &spawnDef)
	{
		m_origin = rkit::math::Vec3::FromSpan(spawnDef.m_origin.ToSpan());
		m_angles = rkit::math::Vec3::FromSpan(spawnDef.m_angles.ToSpan());
		m_scale = rkit::math::Vec3::FromSpan(spawnDef.m_scale.ToSpan());
		m_newScaling = spawnDef.m_newscaling;
		m_volume = spawnDef.m_volume;
		CORO_CHECK(EntityStringLoader::LoadString(m_noise, spawnParams, spawnDef.m_noise));
		m_attenuation = spawnDef.m_attenuation;
		CORO_CHECK(EntityStringLoader::LoadString(m_target, spawnParams, spawnDef.m_target));
		CORO_CHECK(EntityStringLoader::LoadString(m_targetname, spawnParams, spawnDef.m_targetname));
		m_particleFlags = spawnDef.m_particleflags;
		m_spawnFlags = spawnDef.m_particleflags;
		CORO_CHECK(EntityStringLoader::LoadString(m_spawnCondition, spawnParams, spawnDef.m_spawncondition));
		CORO_RETURN_OK;
	}
}
