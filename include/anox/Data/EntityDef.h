#pragma once

#include "rkit/Core/Endian.h"
#include "rkit/Core/FourCC.h"

namespace anox { namespace data {
	enum class EntityFieldType
	{
		kID,
		kVec3,
	};

	struct EntityDataVec3
	{
		float m_data[3];
	};

	struct EntityFieldDef
	{
		size_t m_offset;
		EntityFieldType m_fieldType;

		const char *m_name;
		size_t m_nameLength;
	};

	struct EntityClassDef
	{
		const EntityClassDef *m_parent;

		uint32_t m_entityClassIndex;

		const char *m_name;
		size_t m_nameLength;

		size_t m_size;

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

	enum class UserEntityType
	{
		kPlayerChar,
		kChar,
		kCharFly,
		kNoClip,
		kGeneral,
		kTrashSpawn,
		kBugSpawn,
		kLottoBot,
		kPickup,
		kScavenger,
		kEffect,
		kContainer,
		kLightSource,
		kSunPoint,

		kCount,
	};

	enum class UserEntityFlags
	{
		kSolid = (1 << 0),
		kLighting = (1 << 1),
		kBlending = (1 << 2),
		kNoMip = (1 << 3),
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
		rkit::endian::LittleUInt16_t m_classNameStringID;
		rkit::endian::LittleUInt16_t m_modelPathStringID;
		rkit::endian::BigUInt32_t m_modelCode;
		rkit::endian::LittleFloat32_t m_scale[3];
		uint8_t m_userEntityType = 0;
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
		rkit::endian::LittleUInt16_t m_descriptionStringID;
	};

	struct UserEntityDefHeader
	{
		static const uint32_t kExpectedMagic = RKIT_FOURCC('U', 'E', 'D', 'F');
		static const uint32_t kExpectedVersion = 1;

		rkit::endian::BigUInt32_t m_magic;
		rkit::endian::LittleUInt32_t m_version;

		rkit::endian::LittleUInt16_t m_numStrings;
		rkit::endian::LittleUInt32_t m_numEDefs;

		// uint8_t m_stringLengthsMinusOne[m_numStrings]
		// UserEntityDef m_eDefs
	};
} }
