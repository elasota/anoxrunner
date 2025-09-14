#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/Endian.h"
#include "rkit/Data/ContentID.h"

#include "AnoxEntityDefs.h"

#include "anox/Data/EntityDef.h"

namespace anox { namespace utils { namespace priv {

// Packed field calculations and class prototypes
#define FIELD_METADATA(name, eft, size, classDef)	\
	template<> \
	struct PackedStorageMeta<__LINE__> \
	{ \
		enum Metadata\
		{\
			kOffset = PackedStorageMeta<__LINE__ - 1>::kEnd, \
			kEnd = kOffset + size \
		};\
	}; \
	static constexpr ::anox::data::EntityFieldType fld_type_ ## name = ::anox::data::EntityFieldType::eft;\
	static constexpr size_t fld_offset_ ## name = PackedStorageMeta<__LINE__>::kOffset;
	

#define CLASS_BASE(className)	\
	namespace cls_ ## className \
	{ \
		extern const ::anox::data::EntityClassDef g_classDef; \
		template<size_t> \
		struct PackedStorageMeta \
		{ \
		}; \
		template<> \
		struct PackedStorageMeta<__LINE__> \
		{ \
			enum Metadata \
			{ \
				kOffset = 0, \
				kEnd = 0 \
			}; \
		};

#define CLASS_BASE_INVISIBLE(className) CLASS_BASE(className)

#define END_CLASS	\
		static constexpr size_t kSize = PackedStorageMeta<__LINE__ - 1>::kEnd; \
	}

#define FIELD_BOOL(name) FIELD_METADATA(name, kBool, 1, nullptr)
#define FIELD_BOOL_ON_OFF(name) FIELD_METADATA(name, kBoolOnOff, 1, nullptr)
#define FIELD_UINT(name) FIELD_METADATA(name, kUInt, 4, nullptr)
#define FIELD_LABEL(name) FIELD_METADATA(name, kLabel, 4, nullptr)
#define FIELD_VEC2(name) FIELD_METADATA(name, kVec2, 8, nullptr)
#define FIELD_VEC3(name) FIELD_METADATA(name, kVec3, 12, nullptr)
#define FIELD_VEC4(name) FIELD_METADATA(name, kVec4, 16, nullptr)
#define FIELD_STRING(name) FIELD_METADATA(name, kString, 4, nullptr)
#define FIELD_FLOAT(name) FIELD_METADATA(name, kFloat, 4, nullptr)
#define FIELD_BSPMODEL(name) FIELD_METADATA(name, kBSPModel, 4, nullptr)
#define FIELD_CONTENTID(name) FIELD_METADATA(name, kContentID, sizeof(::rkit::data::ContentID), nullptr)
#define FIELD_COMPONENT(name) FIELD_METADATA(name, kComponent, cls_ ## name::kSize, &cls_ ## name::g_classDef)

#include "anox/Data/EntityDefs.inl"

#undef FIELD_METADATA
#undef CLASS_INHERIT
#undef CLASS_INHERIT_INVISIBLE
#undef CLASS_BASE
#undef CLASS_BASE_INVISIBLE
#undef END_CLASS

// Class IDs
#define FIELD_METADATA(name, eft, size, classDef)

#define CLASS_BASE(name) \
	kClassID_ ## name,

#define CLASS_INHERIT(name, parent) \
	kClassID_ ## name,

#define CLASS_INHERIT_INVISIBLE(name, parent)
#define CLASS_BASE_INVISIBLE(name)

#define END_CLASS

	enum ClassIDs
	{
#include "anox/Data/EntityDefs.inl"
		kClassID_count,
	};

#undef FIELD_METADATA
#undef CLASS_INHERIT
#undef CLASS_INHERIT_INVISIBLE
#undef CLASS_BASE
#undef CLASS_BASE_INVISIBLE
#undef END_CLASS

// Field def array
#define FIELD_METADATA(name, eft, size, classDef) \
	{ \
		fld_offset_ ## name, \
		fld_type_ ## name, \
		#name, \
		sizeof(#name) - 1, \
		classDef, \
	},

#define CLASS_BASE(name) \
	namespace cls_ ## name \
	{ \
		static const ::anox::data::EntityFieldDef g_fieldDefs[] = \
		{

#define CLASS_INHERIT(name, parent) CLASS_BASE(name)

#define CLASS_INHERIT_INVISIBLE(name, parent) CLASS_INHERIT(name, parent)
#define CLASS_BASE_INVISIBLE(name) CLASS_BASE(name)

// Add a blank one so the list can be empty
#define END_CLASS \
			{} \
		}; \
	}

#include "anox/Data/EntityDefs.inl"


#undef FIELD_METADATA
#undef CLASS_INHERIT
#undef CLASS_INHERIT_INVISIBLE
#undef CLASS_BASE
#undef CLASS_BASE_INVISIBLE
#undef END_CLASS

#define CLASS_FILL_IN(name, cid) \
	kClassID_ ## cid, \
	#name, \
	sizeof(#name) - 1, \
	kSize, \
	g_fieldDefs, \
	sizeof(g_fieldDefs) / sizeof(g_fieldDefs[0]) - 1

// Individual class defs
#define CLASS_BASE(name) \
	namespace cls_ ## name \
	{ \
		const ::anox::data::EntityClassDef g_classDef = \
		{\
			nullptr, \
			CLASS_FILL_IN(name, name)

#define CLASS_BASE_INVISIBLE(name) \
	namespace cls_ ## name \
	{ \
		static const ::anox::data::EntityClassDef g_classDef = \
		{\
			nullptr, \
			CLASS_FILL_IN(name, count)

#define CLASS_INHERIT(name, parent) \
	namespace cls_ ## name \
	{ \
		static const ::anox::data::EntityClassDef g_classDef = \
		{\
			&cls_ ## parent::g_classDef, \
			CLASS_FILL_IN(name, name)

#define CLASS_INHERIT_INVISIBLE(name, parent) \
	namespace cls_ ## name \
	{ \
		static const ::anox::data::EntityClassDef g_classDef = \
		{\
			&cls_ ## parent::g_classDef, \
			CLASS_FILL_IN(name, count)

#define END_CLASS \
		}; \
	}

#define FIELD_METADATA(name, eft, size, classDef)

#include "anox/Data/EntityDefs.inl"

#undef CLASS_FILL_IN

#undef FIELD_METADATA
#undef CLASS_INHERIT
#undef CLASS_INHERIT_INVISIBLE
#undef CLASS_BASE
#undef CLASS_BASE_INVISIBLE
#undef END_CLASS

// Class list
#define CLASS_BASE(name) \
	&cls_ ## name::g_classDef,

#define CLASS_BASE_INVISIBLE(name)

#define CLASS_INHERIT(name, parent) \
	&cls_ ## name::g_classDef,

#define CLASS_INHERIT_INVISIBLE(className, parent)

#define END_CLASS

#define FIELD_METADATA(name, eft, size, classDef)

	static const ::anox::data::EntityClassDef *const g_classDefs[] =
	{
#include "anox/Data/EntityDefs.inl"
	};

#undef FIELD_METADATA
#undef CLASS_INHERIT
#undef CLASS_INHERIT_INVISIBLE
#undef CLASS_BASE
#undef CLASS_BASE_INVISIBLE
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
