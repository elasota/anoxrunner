#include "ColorableComponent.h"

#include "anox/Data/EntityStructs.h"

#include "rkit/Core/Coroutine.h"

namespace anox::game
{
	rkit::ResultCoroutine ColorableComponent::Initialize(rkit::ICoroThread &thread, const WorldObjectSpawnParams &spawnParams, const data::EClass_colorable &spawnDef)
	{
		m_rgb = rkit::math::Vec3(spawnDef.m_rgb[0], spawnDef.m_rgb[1], spawnDef.m_rgb[2]);
		m_alpha = spawnDef.m_alpha;
		m_alphaFlags = spawnDef.m_alphaflags;
		CORO_RETURN_OK;
	}
}

