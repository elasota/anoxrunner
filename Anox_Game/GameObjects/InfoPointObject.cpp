#include "AnoxWorldObjectFactory.h"

#include "anox/Data/EntityStructs.h"
#include "anox/Game/UserEntityDefValues.h"

#include "rkit/Core/Coroutine.h"
#include "rkit/Core/String.h"
#include "rkit/Core/UniquePtr.h"
#include "rkit/Math/BBox.h"
#include "rkit/Math/Vec.h"

#include "ClickableComponent.h"
#include "ColorableComponent.h"
#include "PlacedComponent.h"
#include "Serializable.h"
#include "AnoxWorldObject.h"
#include "EntityStringLoader.h"

namespace anox::game
{
	class PlayerStart : public WorldObject, public PlacedComponent, public Serializable<PlayerStart>
	{
		ANOX_RTTI_CLASS_MULTI_BASE(PlayerStart, WorldObject, PlacedComponent, Serializable<PlayerStart>)
		ANOX_SERIALIZABLE_PROTOTYPE

	public:
		rkit::ResultCoroutine Initialize(rkit::ICoroThread &thread, const anox::game::WorldObjectSpawnParams &spawnParams, const anox::data::EClass_info_player_start &spawnDef);

	private:
		/* reflector:properties(anox::game::PlayerStart) */
		Label m_sequence;
		rkit::ByteString m_message;
		float m_lighting = 0.f;
		/* reflector:endproperties */
	};

	rkit::ResultCoroutine PlayerStart::Initialize(rkit::ICoroThread &thread, const WorldObjectSpawnParams &spawnParams, const data::EClass_info_player_start &spawnDef)
	{
		CORO_CHECK(co_await PlacedComponent::Initialize(thread, spawnParams, spawnDef.m_placed));

		m_sequence = spawnDef.m_sequence;
		CORO_CHECK(EntityStringLoader::LoadString(m_message, spawnParams, spawnDef.m_message));
		m_lighting = spawnDef.m_lighting;

		CORO_RETURN_OK;
	}
}


ANOX_IMPLEMENT_WORLD_OBJECT_INSTANTIATOR(anox::game::PlayerStart, info_player_start)

