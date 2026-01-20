#pragma once

#include "rkit/Core/Endian.h"
#include "rkit/Core/Optional.h"
#include "rkit/Data/ContentID.h"

#include "rkit/Math/Vec.h"

#include "anox/Label.h"

namespace anox { namespace data {

#define CLASS_BASE(name) \
	struct EClass_ ## name\
	{\

#define CLASS_BASE_INVISIBLE(name) CLASS_BASE(name)

#define FIELD_VEC2(name) rkit::math::Vec2 m_ ## name;
#define FIELD_VEC3(name) rkit::math::Vec3 m_ ## name;
#define FIELD_VEC4(name) rkit::math::Vec4 m_ ## name;
#define FIELD_BOOL(name) bool m_ ## name;
#define FIELD_BOOL_ON_OFF(name) bool m_ ## name;
#define FIELD_STRING(name) ::rkit::Optional<uint32_t> m_ ## name;
#define FIELD_FLOAT(name) float m_ ## name;
#define FIELD_UINT(name) uint32_t m_ ## name;
#define FIELD_LABEL(name) Label m_ ## name;
#define FIELD_COMPONENT(name) EClass_ ## name m_ ## name;
#define FIELD_EDEF(name) uint32_t m_ ## name;
#define FIELD_BSPMODEL(name) uint32_t m_ ## name;

#define END_CLASS \
	};

#include "EntityDefs.inl"

#undef CLASS_BASE
#undef CLASS_BASE_INVISIBLE
#undef FIELD_VEC2
#undef FIELD_VEC3
#undef FIELD_VEC4
#undef FIELD_BOOL
#undef FIELD_BOOL_ON_OFF
#undef FIELD_STRING
#undef FIELD_FLOAT
#undef FIELD_UINT
#undef FIELD_LABEL
#undef FIELD_COMPONENT
#undef FIELD_EDEF
#undef FIELD_BSPMODEL
#undef END_CLASS

#define CLASS_BASE(name) k_ ##name,
#define CLASS_BASE_INVISIBLE(name)

#define FIELD_VEC2(name)
#define FIELD_VEC3(name)
#define FIELD_VEC4(name)
#define FIELD_BOOL(name)
#define FIELD_BOOL_ON_OFF(name)
#define FIELD_STRING(name)
#define FIELD_FLOAT(name)
#define FIELD_UINT(name)
#define FIELD_LABEL(name)
#define FIELD_COMPONENT(name)
#define FIELD_EDEF(name)
#define FIELD_BSPMODEL(name)
#define END_CLASS

	enum class EClass_ID
	{
#include "EntityDefs.inl"
		k_count,
	};

#undef CLASS_BASE
#undef CLASS_BASE_INVISIBLE
#undef FIELD_VEC2
#undef FIELD_VEC3
#undef FIELD_VEC4
#undef FIELD_BOOL
#undef FIELD_BOOL_ON_OFF
#undef FIELD_STRING
#undef FIELD_FLOAT
#undef FIELD_UINT
#undef FIELD_LABEL
#undef FIELD_COMPONENT
#undef FIELD_EDEF
#undef FIELD_BSPMODEL
#undef END_CLASS

} }
