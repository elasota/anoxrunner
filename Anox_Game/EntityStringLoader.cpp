#include "EntityStringLoader.h"

#include "AnoxWorldObjectFactory.h"

#include "rkit/Core/Optional.h"
#include "rkit/Core/String.h"

namespace anox::game
{
	rkit::Result EntityStringLoader::LoadString(rkit::ByteString &str, const WorldObjectSpawnParams &spawnParams, const rkit::Optional<uint32_t> &spawnDefValue)
	{
		if (spawnDefValue.IsSet())
			return str.Set(spawnParams.m_spawnDefStrings[spawnDefValue.Get()]);
		else
		{
			str.Clear();
			RKIT_RETURN_OK;
		}
	}
}
