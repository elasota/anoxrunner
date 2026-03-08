#include "AnoxWorldObjectFactory.h"

#include "anox/Data/EntityStructs.h"

#include "rkit/Core/Coroutine.h"

#define ANOX_UNIMPLEMENTED_ECLASS(eclass)\
template<>\
rkit::ResultCoroutine WorldObjectInstantiator<data::eclass>::SpawnObject(rkit::ICoroThread &thread, rkit::RCPtr<WorldObject> &outObject, const WorldObjectSpawnParams &spawnParams, const data::eclass &spawnDef)\
{\
	if (true)\
		CORO_THROW(rkit::ResultCode::kNotYetImplemented);\
	CORO_RETURN_OK;\
}

namespace anox::game
{
	ANOX_UNIMPLEMENTED_ECLASS(EClass_info_player_start)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_info_party_start)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_func_areaportal)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_func_button)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_func_fog)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_func_group)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_door)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_func_door)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_func_door_rotating)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_func_particle)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_func_plat)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_func_ridebox)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_func_rotating)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_func_train)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_func_wall)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_light)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_path_corner)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_path_grid_center)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_spew)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_target_speaker)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_trigger_console)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_trigger_changelevel)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_trigger_elevator)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_trigger_once)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_trigger_multiple)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_trigger_push)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_trigger_relay)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_worldspawn)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_userentity_playerchar)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_userentity_char)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_userentity_charfly)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_userentity_noclip)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_userentity_general)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_userentity_trashspawn)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_userentity_bugspawn)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_userentity_lottobot)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_userentity_pickup)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_userentity_scavenger)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_userentity_effect)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_userentity_container)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_userentity_lightsource)
	ANOX_UNIMPLEMENTED_ECLASS(EClass_userentity_sunpoint)
}
