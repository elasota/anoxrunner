#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/Endian.h"

#include "AnoxEntityDefs.h"

#include "anox/Data/EntityDef.h"

namespace anox { namespace utils { namespace priv {

// Packed field calculations
#define FIELD_METADATA(name, eft, type)	\
	template<> \
	struct PackedStorageMeta<__LINE__> \
	{ \
		enum Metadata\
		{\
			kOffset = PackedStorageMeta<__LINE__ - 1>::kEnd, \
			kEnd = kOffset + sizeof(type) \
		};\
	}; \
	static constexpr ::anox::data::EntityFieldType fld_type_ ## name = ::anox::data::EntityFieldType::eft;\
	static constexpr size_t fld_offset_ ## name = PackedStorageMeta<__LINE__>::kOffset;
	

#define CLASS_BASE(className)	\
	namespace cls_ ## className \
	{ \
		template<size_t> \
		struct PackedStorageMeta \
		{ \
			enum Metadata \
			{ \
				kOffset = 0, \
				kEnd = 0 \
			}; \
		};

#define END_CLASS	\
		static constexpr size_t kSize = PackedStorageMeta<__LINE__ - 1>::kEnd; \
	}

#define FIELD_SWITCH(name) FIELD_METADATA(name, kID, ::rkit::endian::LittleUInt32_t)
#define FIELD_VEC3(name) FIELD_METADATA(name, kVec3, ::rkit::endian::LittleFloat32_t[3])

#include "anox/Data/EntityDefs.inl"

#undef FIELD_METADATA
#undef CLASS_BASE
#undef END_CLASS

// Class IDs
#define FIELD_METADATA(name, eft, type)

#define CLASS_BASE(name) \
	kClassID_ ## name,

#define END_CLASS

	enum ClassIDs
	{
#include "anox/Data/EntityDefs.inl"
	};

#undef FIELD_METADATA
#undef CLASS_BASE
#undef END_CLASS

// Field def array
#define FIELD_METADATA(name, eft, type) \
	{ \
		fld_offset_ ## name, \
		fld_type_ ## name, \
		#name, \
		sizeof(#name) - 1 \
	},

#define CLASS_BASE(name) \
	namespace cls_ ## name \
	{ \
		static const ::anox::data::EntityFieldDef g_fieldDefs[] = \
		{

// Add a blank one so the list can be empty
#define END_CLASS \
			{} \
		}; \
	}

#include "anox/Data/EntityDefs.inl"


#undef FIELD_METADATA
#undef CLASS_BASE
#undef END_CLASS

#define CLASS_FILL_IN(name) \
	kClassID_ ## name, \
	#name, \
	sizeof(#name) - 1, \
	kSize, \
	g_fieldDefs, \
	sizeof(g_fieldDefs) / sizeof(g_fieldDefs[0])

// Individual class defs
#define CLASS_BASE(name) \
	namespace cls_ ## name \
	{ \
		static const ::anox::data::EntityClassDef g_classDef = \
		{\
			nullptr, \
			CLASS_FILL_IN(name)

#define END_CLASS \
		}; \
	}

#define FIELD_METADATA(name, eft, type)

#include "anox/Data/EntityDefs.inl"

#undef CLASS_FILL_IN

#undef FIELD_METADATA
#undef CLASS_BASE
#undef END_CLASS

// Class list
#define CLASS_BASE(name) \
	&cls_ ## name::g_classDef,

#define END_CLASS

#define FIELD_METADATA(name, eft, type)

	static const ::anox::data::EntityClassDef *const g_classDefs[] =
	{
#include "anox/Data/EntityDefs.inl"
	};

#undef FIELD_METADATA
#undef CLASS_BASE
#undef END_CLASS

	static const char *const g_badClasses[] =
	{
		"ob_building-1",
		"ob_building-4",
		"ob_building-5",
		"ob_building-6",
		"ob_flame-stop",
		"boots_cine",
		"pathcorner",
		"ob_Scrate_healths",
		"npc_tme",
	};

	static const ::anox::data::EntityDefsSchema g_schema =
	{
		g_classDefs,
		sizeof(g_classDefs) / sizeof(g_classDefs[0]),

		g_badClasses,
		sizeof(g_badClasses) / sizeof(g_badClasses[0])
	};

} } }


namespace anox { namespace utils {
	const data::EntityDefsSchema &GetEntityDefs()
	{
		return priv::g_schema;
	}
} }
