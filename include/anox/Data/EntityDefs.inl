CLASS_BASE_INVISIBLE(placed)
	FIELD_VEC3(origin)
	FIELD_VEC3(angles)
	FIELD_VEC3(offset)
	FIELD_VEC3(scale)
	FIELD_BOOL(newscaling)
	FIELD_FLOAT(volume)
	FIELD_STRING(noise)
	FIELD_FLOAT(attenuation)
	FIELD_STRING(target)
	FIELD_STRING(targetname)
	FIELD_UINT(particleflags)
	FIELD_UINT(spawnflags)
	FIELD_STRING(spawncondition)
END_CLASS

CLASS_BASE_INVISIBLE(colorable)
	FIELD_VEC3(rgb)
	FIELD_FLOAT(alpha)
	FIELD_BOOL(alphaflags)
END_CLASS

CLASS_BASE_INVISIBLE(clickable)
	FIELD_LABEL(sequence)
	FIELD_LABEL(wsequence)
END_CLASS

CLASS_BASE(info_null)
	FIELD_COMPONENT(placed)
END_CLASS

CLASS_BASE(info_player_start)
	FIELD_COMPONENT(placed)
	FIELD_LABEL(sequence)
	FIELD_STRING(message)
	FIELD_FLOAT(lighting)
END_CLASS

CLASS_BASE(info_party_start)
	FIELD_COMPONENT(placed)
	FIELD_LABEL(sequence)
END_CLASS

CLASS_BASE(func_areaportal)
	FIELD_FLOAT(minsx)
	FIELD_FLOAT(minsy)
	FIELD_FLOAT(minsz)
	FIELD_FLOAT(maxsx)
	FIELD_FLOAT(maxsy)
	FIELD_FLOAT(maxsz)
	FIELD_UINT(style)
	FIELD_STRING(target)
	FIELD_STRING(targetname)
	FIELD_FLOAT(angle)
END_CLASS

CLASS_BASE_INVISIBLE(geometric)
	FIELD_BSPMODEL(model)
END_CLASS

CLASS_BASE(func_button)
	FIELD_COMPONENT(geometric)
	FIELD_FLOAT(angle)
	FIELD_FLOAT(lip)
	FIELD_FLOAT(wait)
	FIELD_STRING(targetname)
	FIELD_STRING(message)
END_CLASS

CLASS_BASE(func_fog)
	FIELD_COMPONENT(geometric)
	FIELD_FLOAT(r)
	FIELD_FLOAT(g)
	FIELD_FLOAT(b)
	FIELD_FLOAT(lighting)
	FIELD_STRING(message)
	FIELD_UINT(lightstyle)
END_CLASS

CLASS_BASE(func_group)
	FIELD_FLOAT(light)
END_CLASS

CLASS_BASE(door)
	FIELD_COMPONENT(geometric)
	FIELD_COMPONENT(clickable)
	FIELD_VEC3(origin)
	FIELD_VEC3(angles)
	FIELD_STRING(noise)
	FIELD_STRING(noise2)
	FIELD_BOOL(force_2d)
	FIELD_FLOAT(volume)
	FIELD_FLOAT(attenuation)
	FIELD_FLOAT(speed)
	FIELD_FLOAT(min)
	FIELD_FLOAT(max)
	FIELD_FLOAT(wait)
	FIELD_STRING(target)
	FIELD_STRING(targetname)
	FIELD_UINT(spawnflags)
	FIELD_FLOAT(distance)
	FIELD_STRING(team)
END_CLASS

CLASS_BASE(func_door)
	FIELD_COMPONENT(door)
	FIELD_FLOAT(lip)
	FIELD_FLOAT(delay)
	FIELD_BOOL(sound)
	FIELD_UINT(dmg)
END_CLASS

CLASS_BASE(func_door_rotating)
	FIELD_COMPONENT(door)
END_CLASS

CLASS_BASE(func_particle)
	FIELD_COMPONENT(geometric)
	FIELD_STRING(message)
END_CLASS

CLASS_BASE(func_plat)
	FIELD_COMPONENT(geometric)
	FIELD_FLOAT(height)
END_CLASS

CLASS_BASE(func_ridebox)
	FIELD_COMPONENT(geometric)
	FIELD_VEC3(origin)
	FIELD_STRING(target)
END_CLASS

CLASS_BASE(func_rotating)
	FIELD_COMPONENT(geometric)
	FIELD_VEC3(origin)
	FIELD_UINT(spawnflags)
	FIELD_FLOAT(speed)
	FIELD_STRING(target)
	FIELD_STRING(targetname)
	FIELD_UINT(dmg)
END_CLASS

CLASS_BASE(func_train)
	FIELD_COMPONENT(geometric)
	FIELD_STRING(noise)
	FIELD_FLOAT(volume)
	FIELD_FLOAT(attenuation)
	FIELD_FLOAT(speed)
	FIELD_STRING(target)
	FIELD_STRING(targetname)
	FIELD_LABEL(sequence)
	FIELD_UINT(spawnflags)
	FIELD_STRING(message)
END_CLASS

CLASS_BASE(func_wall)
	FIELD_COMPONENT(geometric)
	FIELD_COMPONENT(clickable)
	FIELD_VEC3(origin)
	FIELD_UINT(spawnflags)
	FIELD_STRING(targetname)
	FIELD_STRING(spawncondition)
	FIELD_FLOAT(light)
END_CLASS

CLASS_BASE(light)
	FIELD_COMPONENT(placed)
	FIELD_COMPONENT(colorable)
	FIELD_FLOAT(cap)	// ???
	FIELD_FLOAT(_cap)	// ???
	FIELD_FLOAT(light)
	FIELD_FLOAT(falloff)
	FIELD_FLOAT(value)
	FIELD_VEC3(_color)
	FIELD_FLOAT(_cone)
	FIELD_FLOAT(_angfade)
	FIELD_FLOAT(_angwait)
	FIELD_UINT(style)
	FIELD_STRING(np_1)
	FIELD_STRING(np_2)
	FIELD_STRING(np_3)
	FIELD_UINT(particleflags)
	FIELD_VEC3(sun_ambient)
	FIELD_STRING(model)
END_CLASS

CLASS_BASE(path_corner)
	FIELD_COMPONENT(placed)
	FIELD_COMPONENT(clickable)
	FIELD_FLOAT(wait)
	FIELD_STRING(pathtarget)
	FIELD_STRING(path_anim)
	FIELD_STRING(touchconsole)
	FIELD_STRING(pathanim)
	FIELD_VEC3(rgb)
	FIELD_FLOAT(random)
	FIELD_STRING(default_anim)
	FIELD_STRING(message)
	FIELD_FLOAT(speed)
END_CLASS

CLASS_BASE(path_grid_center)
	FIELD_COMPONENT(placed)
	FIELD_VEC3(mins)
	FIELD_VEC3(maxs)
	FIELD_FLOAT(gridsize)
	FIELD_BOOL(lowest)
END_CLASS

CLASS_BASE(spew)
	FIELD_COMPONENT(placed)
	FIELD_COMPONENT(colorable)
	FIELD_STRING(message)
	FIELD_BOOL(particle_disable)
END_CLASS

CLASS_BASE(target_speaker)
	FIELD_COMPONENT(placed)
	FIELD_VEC3(rgb)
	FIELD_FLOAT(wait)
	FIELD_FLOAT(random)
	FIELD_BOOL(force_2d)
	FIELD_FLOAT(min)
	FIELD_FLOAT(max)
	FIELD_BOOL_ON_OFF(sound)
	FIELD_LABEL(sequence)
	FIELD_STRING(default_anim)
	FIELD_STRING(name)
END_CLASS

CLASS_BASE(trigger_console)
	FIELD_COMPONENT(geometric)
	FIELD_STRING(command)
	FIELD_FLOAT(angle)
	FIELD_STRING(spawncondition)
	FIELD_FLOAT(wait)
END_CLASS

CLASS_BASE(trigger_changelevel)
	FIELD_COMPONENT(geometric)
	FIELD_STRING(target)
	FIELD_STRING(targetname)
	FIELD_STRING(map)
	FIELD_FLOAT(angle)
END_CLASS

CLASS_BASE(trigger_elevator)
	FIELD_VEC3(origin)
	FIELD_STRING(targetname)
	FIELD_STRING(target)
	FIELD_STRING(message)
	FIELD_UINT(particleflags)
END_CLASS

CLASS_BASE(trigger_once)
	FIELD_COMPONENT(geometric)
	FIELD_LABEL(sequence)
	FIELD_STRING(target)
END_CLASS

CLASS_BASE(trigger_multiple)
	FIELD_COMPONENT(geometric)
	FIELD_STRING(target)
	FIELD_STRING(targetname)
	FIELD_LABEL(sequence)
	FIELD_FLOAT(wait)
	FIELD_FLOAT(delay)
	FIELD_STRING(pathtarget)
	FIELD_UINT(spawnflags)
	FIELD_STRING(message)
	FIELD_STRING(spawncondition)
	FIELD_STRING(touchconsole)
	FIELD_LABEL(seqence)	// typo
	FIELD_STRING(gamevar)
	FIELD_LABEL(removed_sequence)	// ???
END_CLASS

CLASS_BASE(trigger_push)
	FIELD_COMPONENT(geometric)
	FIELD_FLOAT(speed)
	FIELD_FLOAT(angle)
END_CLASS

CLASS_BASE(trigger_relay)
	FIELD_VEC3(origin)
	FIELD_STRING(target)
	FIELD_STRING(targetname)
	FIELD_FLOAT(delay)
END_CLASS

CLASS_BASE(worldspawn)
	FIELD_LABEL(sequence)
	FIELD_VEC4(fog)
	FIELD_VEC4(gl_fog) // typo
	FIELD_STRING(sky)
	FIELD_STRING(command)
	FIELD_STRING(wad) // typo (???)
	FIELD_UINT(spawnflags)
	FIELD_FLOAT(distance)
	FIELD_FLOAT(attenuation)
	FIELD_FLOAT(volume)
	FIELD_FLOAT(_sun_light)
	FIELD_VEC3(_sun_color)
	FIELD_VEC2(_sun_angle)
	FIELD_FLOAT(_sun2_light)
	FIELD_VEC2(_sun2_angle)
	FIELD_VEC3(_sun2_color)
	FIELD_FLOAT(_sun_ambient)
	FIELD_FLOAT(speed)
	FIELD_VEC3(angles)
	FIELD_VEC3(rgb)
	FIELD_FLOAT(r)
	FIELD_FLOAT(g)
	FIELD_FLOAT(b)
	FIELD_FLOAT(light)
	FIELD_STRING(target)
	FIELD_STRING(team)
	FIELD_FLOAT(wait)
	FIELD_STRING(dlltexture01)
	FIELD_STRING(dlltexture02)
	FIELD_STRING(dlltexture03)
	FIELD_FLOAT(skyrotate)
	FIELD_VEC3(skyaxis)
	FIELD_STRING(message)
	FIELD_VEC3(sun_ambient)
	FIELD_VEC3(sun_diffuse)
	FIELD_FLOAT(falloff)
END_CLASS

CLASS_BASE_INVISIBLE(userentity)
	FIELD_CONTENTID(edef)	// This must be first!
	FIELD_COMPONENT(placed)
END_CLASS

CLASS_BASE(userentity_playerchar)
	FIELD_COMPONENT(userentity)
	FIELD_COMPONENT(colorable)
	FIELD_LABEL(sequence)
	FIELD_STRING(default_anim)
	FIELD_BOOL(norandom)
END_CLASS

CLASS_BASE(userentity_char)
	FIELD_COMPONENT(userentity)
	FIELD_COMPONENT(colorable)
	FIELD_COMPONENT(clickable)
	FIELD_STRING(np)
	FIELD_STRING(np_1)
	FIELD_STRING(np_2)
	FIELD_STRING(default_anim)
	FIELD_STRING(default_ambient)
	FIELD_STRING(default_walk)
	FIELD_STRING(message)
	FIELD_STRING(battlegroupname)
	FIELD_BOOL(norandom)
	FIELD_FLOAT(wait)
	FIELD_FLOAT(lighting)
END_CLASS

CLASS_BASE(userentity_charfly)
	FIELD_COMPONENT(userentity)
	FIELD_COMPONENT(colorable)
	FIELD_COMPONENT(clickable)
	FIELD_STRING(envmap)
	FIELD_STRING(battlegroupname)
	FIELD_STRING(message)
	FIELD_STRING(default_anim)
	FIELD_FLOAT(lighting)
	FIELD_FLOAT(scrollx)
	FIELD_FLOAT(scrolly)
END_CLASS

CLASS_BASE(userentity_noclip)
	FIELD_COMPONENT(userentity)
	FIELD_COMPONENT(colorable)
	FIELD_STRING(default_anim)
END_CLASS

CLASS_BASE(userentity_general)
	FIELD_COMPONENT(userentity)
	FIELD_COMPONENT(colorable)
	FIELD_COMPONENT(clickable)
	FIELD_UINT(count)
	FIELD_UINT(random)
	FIELD_FLOAT(wait)
	FIELD_STRING(np)
	FIELD_STRING(np_1)
	FIELD_STRING(np_2)
	FIELD_STRING(np_3)
	FIELD_STRING(npsimple)
	FIELD_STRING(default_anim)
	FIELD_FLOAT(lighting)
	FIELD_FLOAT(scrollx)
	FIELD_FLOAT(scrolly)
	FIELD_FLOAT(scrollz)
	FIELD_STRING(message)
	FIELD_STRING(envmap)
	FIELD_FLOAT(delay)
END_CLASS

CLASS_BASE(userentity_trashspawn)
	FIELD_COMPONENT(userentity)
END_CLASS

CLASS_BASE(userentity_bugspawn)
	FIELD_COMPONENT(userentity)
	FIELD_COMPONENT(colorable)
END_CLASS

CLASS_BASE(userentity_lottobot)
	FIELD_COMPONENT(userentity)
END_CLASS

CLASS_BASE(userentity_pickup)
	FIELD_COMPONENT(userentity)
	FIELD_COMPONENT(colorable)
	FIELD_COMPONENT(clickable)
	FIELD_STRING(message)
	FIELD_UINT(count)
	FIELD_STRING(default_anim)
	FIELD_FLOAT(wait)
	FIELD_STRING(npsimple)
	FIELD_FLOAT(random)
	FIELD_VEC3(ndscale)
	FIELD_FLOAT(_cone)
	FIELD_FLOAT(light)
END_CLASS

CLASS_BASE(userentity_scavenger)
	FIELD_COMPONENT(userentity)
END_CLASS

CLASS_BASE(userentity_effect)
	FIELD_COMPONENT(userentity)
END_CLASS

CLASS_BASE(userentity_container)
	FIELD_COMPONENT(userentity)
	FIELD_COMPONENT(clickable)
	FIELD_STRING(message)
END_CLASS

CLASS_BASE(userentity_lightsource)
	FIELD_CONTENTID(edef)
END_CLASS

CLASS_BASE(userentity_sunpoint)
	FIELD_CONTENTID(edef)
END_CLASS
