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
	class UserEntity : public WorldObject, public PlacedComponent, public Serializable<UserEntity>
	{
		ANOX_RTTI_CLASS_MULTI_BASE(UserEntity, WorldObject, PlacedComponent, Serializable<UserEntity>)
		ANOX_SERIALIZABLE_PROTOTYPE

	public:
		rkit::ResultCoroutine Initialize(rkit::ICoroThread &thread, const anox::game::WorldObjectSpawnParams &spawnParams, const anox::data::EClass_userentity &spawnDef);

	private:
		/* reflector:properties(anox::game::UserEntity) */
		uint32_t m_modelCodeFourCC = 0;
		rkit::math::Vec3 m_scale;

		data::UserEntityShadowType m_shadowType = data::UserEntityShadowType::kCount;
		rkit::math::BBox3 m_bbox;
		rkit::EnumMask<data::UserEntityFlags> m_userEntityFlags;
		float m_walkSpeed = 0.f;
		float m_runSpeed = 0.f;
		float m_speed = 0.f;
		Label m_targetSequence;
		Label m_startSequence;
		uint32_t m_miscValue = 0;
		uint32_t m_descLength = 0;
		/* reflector:endproperties */
	};

	class GeneralEntity : public UserEntity, public ClickableComponent, public ColorableComponent, public Serializable<GeneralEntity>
	{
		ANOX_RTTI_CLASS_MULTI_BASE(GeneralEntity, UserEntity, ClickableComponent, ColorableComponent, Serializable<GeneralEntity>)
		ANOX_SERIALIZABLE_PROTOTYPE

	public:
		GeneralEntity();

		rkit::ResultCoroutine Initialize(rkit::ICoroThread &thread, const anox::game::WorldObjectSpawnParams &spawnParams, const anox::data::EClass_userentity_general &spawnDef);

	private:
		/* reflector:properties(anox::game::GeneralEntity) */
		uint32_t m_count = 0;
		uint32_t m_random = 0;
		float m_wait = 0;
		rkit::ByteString m_np;
		rkit::ByteString m_np_1;
		rkit::ByteString m_np_2;
		rkit::ByteString m_np_3;
		rkit::ByteString m_npsimple;
		rkit::ByteString m_defaultAnim;
		float m_lighting = 0;
		rkit::math::Vec3 m_scroll;
		rkit::ByteString m_message;
		rkit::ByteString m_envMap;
		float m_delay = 0;
		/* reflector:endproperties */
	};

	rkit::ResultCoroutine UserEntity::Initialize(rkit::ICoroThread &thread, const anox::game::WorldObjectSpawnParams &spawnParams, const anox::data::EClass_userentity &spawnDef)
	{
		const UserEntityDefValues &udef = spawnParams.m_udefs[spawnDef.m_edef];

		CORO_CHECK(co_await PlacedComponent::Initialize(thread, spawnParams, spawnDef.m_placed));

		CORO_RETURN_OK;
	}

	GeneralEntity::GeneralEntity()
	{
	}

	rkit::ResultCoroutine GeneralEntity::Initialize(rkit::ICoroThread &thread, const WorldObjectSpawnParams &spawnParams, const data::EClass_userentity_general &spawnDef)
	{
		CORO_CHECK(co_await UserEntity::Initialize(thread, spawnParams, spawnDef.m_userentity));
		CORO_CHECK(co_await ClickableComponent::Initialize(thread, spawnParams, spawnDef.m_clickable));
		CORO_CHECK(co_await ColorableComponent::Initialize(thread, spawnParams, spawnDef.m_colorable));

		m_count = spawnDef.m_count;
		m_random = spawnDef.m_random;
		m_wait = spawnDef.m_wait;
		CORO_CHECK(EntityStringLoader::LoadString(m_np, spawnParams, spawnDef.m_np));
		CORO_CHECK(EntityStringLoader::LoadString(m_np_1, spawnParams, spawnDef.m_np_1));
		CORO_CHECK(EntityStringLoader::LoadString(m_np_2, spawnParams, spawnDef.m_np_2));
		CORO_CHECK(EntityStringLoader::LoadString(m_np_3, spawnParams, spawnDef.m_np_3));
		CORO_CHECK(EntityStringLoader::LoadString(m_npsimple, spawnParams, spawnDef.m_npsimple));
		CORO_CHECK(EntityStringLoader::LoadString(m_defaultAnim, spawnParams, spawnDef.m_default_anim));
		m_lighting = spawnDef.m_lighting;
		m_scroll = rkit::math::Vec3(spawnDef.m_scrollx, spawnDef.m_scrolly, spawnDef.m_scrollz);
		CORO_CHECK(EntityStringLoader::LoadString(m_message, spawnParams, spawnDef.m_message));
		CORO_CHECK(EntityStringLoader::LoadString(m_envMap, spawnParams, spawnDef.m_envmap));
		m_delay = spawnDef.m_delay;

		CORO_RETURN_OK;
	}
}

ANOX_IMPLEMENT_WORLD_OBJECT_INSTANTIATOR(anox::game::GeneralEntity, userentity_general)

