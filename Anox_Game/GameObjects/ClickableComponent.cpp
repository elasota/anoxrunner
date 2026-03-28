#include "ClickableComponent.h"

#include "rkit/Core/Coroutine.h"

#include "anox/Data/EntityStructs.h"

namespace anox::game
{
	rkit::ResultCoroutine ClickableComponent::Initialize(rkit::ICoroThread &thread, const WorldObjectSpawnParams &spawnParams, const data::EClass_clickable &spawnDef)
	{
		m_sequence = spawnDef.m_sequence;
		m_wsequence = spawnDef.m_wsequence;
		CORO_RETURN_OK;
	}
}

