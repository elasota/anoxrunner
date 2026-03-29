#include "EntityLevelLoader.h"

#include "AnoxWorldObjectFactory.h"
#include "anox/Game/UserEntityDefValues.h"

#include "GameObjects/UserEntityDef.h"

#include "rkit/Core/Optional.h"
#include "rkit/Core/String.h"

#include "rkit/Core/LogDriver.h"

namespace anox::game
{
	rkit::Result EntityLevelLoader::LoadByteString(rkit::ByteString &str, const void *source, const WorldObjectSpawnParams &spawnParams)
	{
		const uint32_t strID = static_cast<const rkit::endian::LittleUInt32_t *>(source)->Get();
		if (strID == 0)
		{
			RKIT_RETURN_OK;
		}

		if (strID > spawnParams.m_spawnDefStrings.Count())
		{
			rkit::log::Error(u8"Spawn def string out of range");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		return str.Set(spawnParams.m_spawnDefStrings[strID - 1]);
	}

	rkit::Result EntityLevelLoader::LoadEDef(anox::game::UserEntityDef &edef, const void *source, const WorldObjectSpawnParams &spawnParams)
	{
		const uint32_t udefID = static_cast<const rkit::endian::LittleUInt32_t *>(source)->Get();

		if (udefID >= spawnParams.m_udefs.Count())
		{
			rkit::log::Error(u8"Spawn def user entity def was out of range");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		const game::UserEntityDefValues &udef = spawnParams.m_udefs[udefID];

		edef.m_modelCodeFourCC = udef.m_modelCodeFourCC;
		edef.m_scale = rkit::math::Vec3::FromSpan(udef.m_scale.ToSpan());
		edef.m_shadowType = udef.m_shadowType;
		edef.m_bbox = rkit::math::BBox3(
			rkit::math::Vec3::FromSpan(udef.m_bboxMin.ToSpan()),
			rkit::math::Vec3::FromSpan(udef.m_bboxMax.ToSpan())
		);
		edef.m_userEntityFlags = udef.m_userEntityFlags;
		edef.m_walkSpeed = udef.m_walkSpeed;
		edef.m_runSpeed = udef.m_runSpeed;
		edef.m_speed = udef.m_speed;
		edef.m_targetSequence = udef.m_targetSequence;
		edef.m_startSequence = udef.m_startSequence;
		edef.m_miscValue = udef.m_miscValue;
		edef.m_description = spawnParams.m_udefDescriptions[udefID];
		RKIT_RETURN_OK;
	}
}
