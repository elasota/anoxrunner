component PlacedComponent
{
	[level] vec3 origin
	[level] vec3 angles
	[level] vec3 offset
	[level] vec3 scale
	[level] bool newscaling
	[level] float volume
	[level] bytestring noise
	[level] float attenuation
	[level] bytestring target
	[level] bytestring targetname
	[level] uint particleflags
	[level] uint spawnflags
	[level] bytestring spawncondition
}

component ColorableComponent
{
	[level] vec3 rgb
	[level] float alpha
	[level] bool alphaflags
}

component ClickableComponent
{
	[level] label sequence
	[level] label wsequence
}

[level(info_null)]
class InfoNullObject : PlacedComponent
{
}

[level(info_player_start)]
class InfoPlayerStartObject : PlacedComponent
{
	[level] label sequence
	[level] bytestring message
	[level] float lighting
}

[level(info_party_start)]
class InfoPartyStartObject : PlacedComponent
{
	[level] label sequence
}

[level(func_areaportal)]
class AreaPortalObject
{
	[level scalarized(minsx, minsy, minsz)] vec3 mins
	[level scalarized(maxsx, maxsy, maxsz)] vec3 maxs
	[level] uint style
	[level] bytestring target
	[level] bytestring targetname
	[level] float angle
}

component GeometricComponent
{
	[level] bspmodel model
}

[level(func_button)]
class ButtonObject : GeometricComponent
{
	[level] float angle
	[level] float lip
	[level] float wait
	[level] bytestring targetname
	[level] bytestring message
}

[level(func_fog)]
class FogVolumeObject : GeometricComponent
{
	[level scalarized(r, g, b)] vec3 rgb
	[level] float lighting
	[level] bytestring message
	[level] uint lightstyle
}

[level(func_group)]
class GroupObject
{
	[level] float light
}

component DoorComponent : GeometricComponent, ClickableComponent
{
	[level] vec3 origin
	[level] vec3 angles
	[level] bytestring noise
	[level] bytestring noise2
	[level name(force_2d)] bool force2D
	[level] float volume
	[level] float attenuation
	[level] float speed
	[level] float min
	[level] float max
	[level] float wait
	[level] bytestring target
	[level] bytestring targetname
	[level] uint spawnflags
	[level] float distance
	[level] bytestring team
}

[level(func_door)]
class DoorObject : DoorComponent
{
	[level] float lip
	[level] float delay
	[level] bool sound
	[level] uint dmg
}

[level(func_door_rotating)]
class RotatingDoorObject : DoorComponent
{
}

[level(func_particle)]
class ParticleVolumeObject : GeometricComponent
{
	[level] bytestring message
}

[level(func_plat)]
class PlatformObject : GeometricComponent
{
	[level] bytestring height
}

[level(func_ridebox)]
class RideBoxObject : GeometricComponent
{
	[level] vec3 origin
	[level] bytestring target
}

[level(func_rotating)]
class RotatingGeometricObject : GeometricComponent
{
	[level] vec3 origin
	[level] uint spawnflags
	[level] float speed
	[level] bytestring target
	[level] bytestring targetname
	[level] uint dmg
}

[level(func_train)]
class TrainObject : GeometricComponent
{
	[level] bytestring noise
	[level] float volume
	[level] float attenuation
	[level] float speed
	[level] bytestring target
	[level] bytestring targetname
	[level] label sequence
	[level] uint spawnflags
	[level] bytestring message
}

[level(func_wall)]
class WallObject : GeometricComponent, ClickableComponent
{
	[level] vec3 origin
	[level] uint spawnflags
	[level] bytestring targetname
	[level] bytestring spawncondition
	[level] float light
}

[level(light)]
class LightObject : PlacedComponent, ColorableComponent
{
	[level] float cap
	[level] float _cap
	[level] float light
	[level] float falloff
	[level] float value
	[level] vec3 _color
	[level] float _cone
	[level] float _angfade
	[level] float _angwait
	[level] uint style
	[level] bytestring np_1
	[level] bytestring np_2
	[level] bytestring np_3
	[level name(sun_ambient)] vec3 sunAmbient
	[level] bytestring model
}

[level(path_corner)]
class PathCornerObject : PlacedComponent, ClickableComponent
{
	[level] float wait
	[level] bytestring pathtarget
	[level] bytestring touchconsole
	[level] bytestring pathanim
	[level] vec3 rgb
	[level] float random
	[level] bytestring default_anim
	[level] bytestring message
	[level] float speed

	[level] broken path_anim
}

[level(path_grid_center)]
class PathGridCenterObject : PlacedComponent
{
	[level] vec3 mins
	[level] vec3 maxs
	[level] float gridsize
	[level] bool lowest
}

[level(spew)]
class SpewObject : PlacedComponent, ColorableComponent
{
	[level] bytestring message
	[level name(particle_disable)] bool particleDisable
}

[level(target_speaker)]
class TargetSpeakerObject : PlacedComponent
{
	[level] vec3 rgb
	[level] float wait
	[level] float random
	[level name(force_2d)] bool force2D
	[level] float min
	[level] float max
	[level onoff] bool sound
	[level] label sequence
	[level] bytestring default_anim
	[level] bytestring name
}

[level(trigger_console)]
class ConsoleTriggerObject : GeometricComponent
{
	[level] bytestring command
	[level] float angle
	[level] bytestring spawncondition
	[level] float wait
}

[level(trigger_changelevel)]
class ChangeLevelTriggerObject : GeometricComponent
{
	[level] bytestring target
	[level] bytestring targetname
	[level] bytestring map
	[level] float angle
}

[level(trigger_elevator)]
class ElevatorTriggerObject : PlacedComponent
{
	[level] bytestring message
}

[level(trigger_once)]
class OnceTriggerObject : GeometricComponent
{
	[level] label sequence
	[level] bytestring target
}

[level(trigger_multiple)]
class TriggerMultipleObject : GeometricComponent
{
	[level] bytestring target
	[level] bytestring targetname
	[level] label sequence
	[level] float wait
	[level] float delay
	[level] bytestring pathtarget
	[level] uint spawnflags
	[level] bytestring message
	[level] bytestring spawncondition
	[level] bytestring touchconsole
	[level] bytestring gamevar
	[level] label removed_sequence

	[level] broken seqence
}

[level(trigger_push)]
class PushTriggerObject : GeometricComponent
{
	[level] float speed
	[level] float angle
}

[level(trigger_relay)]
class RelayTriggerObject : GeometricComponent
{
	[level] vec3 origin
	[level] bytestring target
	[level] bytestring targetname
	[level] float delay
}

[level(worldspawn)]
class GlobalSingleton
{
	[level] label sequence
	[level] vec4 fog
	[level] bytestring sky
	[level] bytestring command
	[level] bytestring wad
	[level] uint spawnflags
	[level] float distance
	[level] float attenuation
	[level] float volume
	[level] float _sun_light
	[level] vec3 _sun_color
	[level] vec2 _sun_angle
	[level] float _sun2_light
	[level] vec2 _sun2_angle
	[level] vec3 _sun2_color
	[level] float _sun_ambient
	[level] float speed
	[level] vec3 angles
	[level] vec3 rgb
	[level scalarized(r, g, b)] [alias(rgb)] vec3 rgb_scalar
	[level] float light
	[level] bytestring target
	[level] bytestring team
	[level] float wait
	[level] bytestring dlltexture01
	[level] bytestring dlltexture02
	[level] bytestring dlltexture03
	[level] float skyrotate
	[level] vec3 skyaxis
	[level] bytestring message
	[level] vec3 sun_ambient
	[level] vec3 sun_diffuse
	[level] float falloff

	[level] broken gl_fog
}

component AbstractUserEntityComponent
{
	[level] edef edef
}

component UserEntityComponent : AbstractUserEntityComponent, PlacedComponent
{
}

[userentity(playerchar)]
class PlayerCharUserEntityObject : UserEntityComponent, ColorableComponent
{
	[level] label sequence
	[level] bytestring default_anim
	[level] bool norandom
}

[userentity(char)]
class CharUserEntityObject : UserEntityComponent, ColorableComponent, ClickableComponent
{
	[level] bytestring np
	[level] bytestring np_1
	[level] bytestring np_2
	[level] bytestring default_anim
	[level] bytestring default_ambient
	[level] bytestring default_walk
	[level] bytestring message
	[level] bytestring battlegroupname
	[level] bool norandom
	[level] float wait
	[level] float lighting
}

[userentity(charfly)]
class CharFlyUserEntityObject : UserEntityComponent, ColorableComponent, ClickableComponent
{
	[level] bytestring envmap
	[level] bytestring battlegroupname
	[level] bytestring message
	[level] bytestring default_anim
	[level] float lighting
	[level scalarized(scrollx, scrolly, scrollz)] vec3 scroll
}

[userentity(noclip)]
class NoClipUserEntityObject : UserEntityComponent, ColorableComponent
{
	[level] bytestring default_anim
}

[userentity(general)]
class GeneralUserEntityObject : UserEntityComponent, ColorableComponent, ClickableComponent
{
	[level] uint count
	[level] uint random
	[level] float wait
	[level] bytestring np
	[level] bytestring np_1
	[level] bytestring np_2
	[level] bytestring np_3
	[level] bytestring npsimple
	[level] bytestring default_anim
	[level] float lighting)
	[level scalarized(scrollx, scrolly, scrollz)] vec3 scroll
	[level] bytestring message
	[level] bytestring envmap
	[level] float delay
}

[userentity(trashspawn)]
class TrashSpawnUserEntityObject : UserEntityComponent
{
}

[userentity(bugspawn)]
class BugSpawnUserEntityObject : UserEntityComponent, ColorableComponent
{
}

[userentity(lottobot)]
class LottoBotUserEntityObject : UserEntityComponent
{
}

[userentity(pickup)]
class PickupUserEntityObject : UserEntityComponent, ColorableComponent, ClickableComponent
{
	[level] bytestring message
	[level] uint count
	[level] bytestring default_anim
	[level] float wait
	[level] bytestring npsimple
	[level] float random
	[level] vec3 ndscale
	[level] float _cone
	[level] float light
}

[userentity(scavenger)]
class ScavengerUserEntityObject : UserEntityComponent
{
}

[userentity(effect)]
class EffectUserEntityObject : UserEntityComponent
{
}

[userentity(container)]
class ContainerUserEntityObject : UserEntityComponent, ClickableComponent
{
	[level] bytestring message
}

[userentity(lightsource)]
class LightSourceUserEntity : AbstractUserEntityComponent
{
}

[userentity(sunpoint)]
class SunPointUserEntity : AbstractUserEntityComponent
{
}

class ScriptWindowInstance
{
	label windowID
	label startSwitch
	label thinkSwitch
	label finishSwitch
	bool persist
	bool noBackground
	bool noScroll
	bool noGrab
	bool noRelease
	bool subtitle
	bool passive2D
	bool passive
	vec2 pos
	vec2 size
}
