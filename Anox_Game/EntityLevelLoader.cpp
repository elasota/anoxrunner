#include "EntityLevelLoader.h"

#include "AnoxWorldObjectFactory.h"

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
}
