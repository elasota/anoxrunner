#include "anox/Data/EntityDef.h"

#include "LevelEntities.generated.inl"

namespace anox::buildsystem
{
	static const rkit::Utf8Char_t *const g_badClasses[] =
	{
		u8"ob_building-1",
		u8"ob_building-4",
		u8"ob_building-5",
		u8"ob_building-6",
		u8"ob_flame-stop",
		u8"boots_cine",
		u8"pathcorner",
		u8"ob_Scrate_healths",
		u8"npc_tme",
	};

	static const ::anox::data::EntityDefsSchema2 g_schema =
	{
		anox::data::priv::gs_levelEntityClasses,
		sizeof(anox::data::priv::gs_levelEntityClasses) / sizeof(anox::data::priv::gs_levelEntityClasses[0]),

		g_badClasses,
		sizeof(g_badClasses) / sizeof(g_badClasses[0])
	};

	const ::anox::data::EntityDefsSchema2 &GetLevelEntitiesSchema()
	{
		return g_schema;
	}
}
