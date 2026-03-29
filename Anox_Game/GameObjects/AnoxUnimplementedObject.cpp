#include "AnoxWorldObjectFactory.h"

#include "anox/Data/EntityStructs.h"

#include "rkit/Core/Coroutine.h"

#define ANOX_UNIMPLEMENTED_ECLASS(objClass) \
class objClass;\
template<>\
rkit::Result WorldObjectInstantiator<objClass>::CreateObject(rkit::UniquePtr<WorldObject> &outObject, ObjectFieldsBase<objClass> *&outFieldsRef) \
{\
	RKIT_THROW(rkit::ResultCode::kNotYetImplemented);\
}\
template<>\
rkit::Result WorldObjectInstantiator<objClass>::LoadObjectFromLevel(ObjectFieldsBase<objClass> &object, const WorldObjectSpawnParams &spawnParams, const uint8_t *bytes) \
{ \
	RKIT_THROW(rkit::ResultCode::kNotYetImplemented); \
}


namespace anox::game
{
	ANOX_UNIMPLEMENTED_ECLASS(AreaPortalObject)
	ANOX_UNIMPLEMENTED_ECLASS(BugSpawnUserEntityObject)
	ANOX_UNIMPLEMENTED_ECLASS(ButtonObject)
	ANOX_UNIMPLEMENTED_ECLASS(ChangeLevelTriggerObject)
	ANOX_UNIMPLEMENTED_ECLASS(CharFlyUserEntityObject)
	ANOX_UNIMPLEMENTED_ECLASS(CharUserEntityObject)
	ANOX_UNIMPLEMENTED_ECLASS(ConsoleTriggerObject)
	ANOX_UNIMPLEMENTED_ECLASS(ContainerUserEntityObject)
	ANOX_UNIMPLEMENTED_ECLASS(DoorObject)
	ANOX_UNIMPLEMENTED_ECLASS(EffectUserEntityObject)
	ANOX_UNIMPLEMENTED_ECLASS(ElevatorTriggerObject)
	ANOX_UNIMPLEMENTED_ECLASS(FogVolumeObject)
	ANOX_UNIMPLEMENTED_ECLASS(GroupObject)
	ANOX_UNIMPLEMENTED_ECLASS(InfoNullObject)
	ANOX_UNIMPLEMENTED_ECLASS(InfoPartyStartObject)
	ANOX_UNIMPLEMENTED_ECLASS(LightObject)
	ANOX_UNIMPLEMENTED_ECLASS(LightSourceUserEntity)
	ANOX_UNIMPLEMENTED_ECLASS(LottoBotUserEntityObject)
	ANOX_UNIMPLEMENTED_ECLASS(NoClipUserEntityObject)
	ANOX_UNIMPLEMENTED_ECLASS(OnceTriggerObject)
	ANOX_UNIMPLEMENTED_ECLASS(ParticleVolumeObject)
	ANOX_UNIMPLEMENTED_ECLASS(PathCornerObject)
	ANOX_UNIMPLEMENTED_ECLASS(PathGridCenterObject)
	ANOX_UNIMPLEMENTED_ECLASS(PickupUserEntityObject)
	ANOX_UNIMPLEMENTED_ECLASS(PlatformObject)
	ANOX_UNIMPLEMENTED_ECLASS(PlayerCharUserEntityObject)
	ANOX_UNIMPLEMENTED_ECLASS(PushTriggerObject)
	ANOX_UNIMPLEMENTED_ECLASS(RelayTriggerObject)
	ANOX_UNIMPLEMENTED_ECLASS(RideBoxObject)
	ANOX_UNIMPLEMENTED_ECLASS(RotatingDoorObject)
	ANOX_UNIMPLEMENTED_ECLASS(RotatingGeometricObject)
	ANOX_UNIMPLEMENTED_ECLASS(ScavengerUserEntityObject)
	ANOX_UNIMPLEMENTED_ECLASS(SpewObject)
	ANOX_UNIMPLEMENTED_ECLASS(SunPointUserEntity)
	ANOX_UNIMPLEMENTED_ECLASS(TargetSpeakerObject)
	ANOX_UNIMPLEMENTED_ECLASS(TrainObject)
	ANOX_UNIMPLEMENTED_ECLASS(TrashSpawnUserEntityObject)
	ANOX_UNIMPLEMENTED_ECLASS(TriggerMultipleObject)
	ANOX_UNIMPLEMENTED_ECLASS(WallObject)
}
