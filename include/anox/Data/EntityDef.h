#pragma once

#include "rkit/Core/Endian.h"
#include "rkit/Core/FourCC.h"

#include "rkit/Data/ContentID.h"

namespace anox { namespace data {
	struct EntityClassDef;

	enum class EntityFieldType
	{
		kLabel,
		kBool,
		kBoolOnOff,
		kUInt,
		kFloat,
		kVec2,
		kVec3,
		kVec4,
		kString,
		kEntityDef,
		kBSPModel,
		kComponent,
	};

	struct EntityFieldDef
	{
		uint32_t m_dataOffset;
		uint32_t m_structOffset;
		EntityFieldType m_fieldType;

		const char *m_name;
		size_t m_nameLength;

		const EntityClassDef *m_classDef;
	};

	struct EntityClassDef
	{
		const EntityClassDef *m_parent;

		uint32_t m_entityClassIndex;

		const char *m_name;
		size_t m_nameLength;

		size_t m_dataSize;
		size_t m_structSize;

		const EntityFieldDef *m_fields;
		size_t m_numFields;
	};

	struct EntityDefsSchema
	{
		const EntityClassDef *const *m_classDefs;
		size_t m_numClassDefs;

		const char *const *m_badClassDefs;
		size_t m_numBadClassDefs;
	};

	enum class UserEntityFlags
	{
		kSolid,
		kLighting,
		kBlending,
		kNoMip,

		kCount,
	};

	enum class UserEntityShadowType
	{
		kNoShadow,
		kShadow,
		kLightning,

		kCount,
	};

	struct UserEntityDef
	{
		rkit::data::ContentID m_modelContentID;
		rkit::endian::BigUInt32_t m_modelCode;
		rkit::endian::LittleFloat32_t m_scale[3];
		rkit::endian::LittleUInt32_t m_entityType;
		uint8_t m_shadowType = 0;
		rkit::endian::LittleFloat32_t m_bboxMin[3];
		rkit::endian::LittleFloat32_t m_bboxMax[3];
		uint8_t m_flags = 0;
		rkit::endian::LittleFloat32_t m_walkSpeed;
		rkit::endian::LittleFloat32_t m_runSpeed;
		rkit::endian::LittleFloat32_t m_speed;
		rkit::endian::LittleUInt32_t m_targetSequenceID;
		rkit::endian::LittleUInt32_t m_miscValue;
		rkit::endian::LittleUInt32_t m_startSequenceID;
		uint8_t m_descriptionStringLength = 0;

		// uint8_t[m_descriptionStringLength]
	};
} }
