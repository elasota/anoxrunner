#pragma once

#include "rkit/Core/StringProto.h"

namespace rkit
{
	template<class T>
	class Optional;
}

namespace anox::game
{
	struct WorldObjectSpawnParams;

	class EntityStringLoader
	{
	public:
		static rkit::Result LoadString(rkit::ByteString &str, const WorldObjectSpawnParams &spawnParams, const rkit::Optional<uint32_t> &spawnDefValue);
	};
}
