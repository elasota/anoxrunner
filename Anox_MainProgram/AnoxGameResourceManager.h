#pragma once

#include "rkit/Core/Opaque.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/PathProto.h"

namespace rkit
{
	template<class T>
	class Optional;
}

namespace rkit::data
{
	struct ContentID;
}

namespace anox::game
{
	class GameResourceManagerImpl;

	class GameResourceManager final : public rkit::Opaque<GameResourceManagerImpl>
	{
	public:
		rkit::Result GetContentIDKeyedResource(uint32_t &outReqID, uint32_t resourceType, const rkit::data::ContentID &contentID);
		rkit::Result GetCIPathKeyedResource(uint32_t &outReqID, uint32_t resourceType, const rkit::CIPathView &ciPath);
		rkit::Result TryFinishLoadingResourceRequest(rkit::Optional<uint32_t> &outResID, uint32_t reqID);

		rkit::Result DiscardRequest(uint32_t reqID);
		rkit::Result DiscardResource(uint32_t resID);
	};
}
