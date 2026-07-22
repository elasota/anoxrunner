#pragma once

#include "rkit/Core/Opaque.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/PathProto.h"

namespace rkit
{
	template<class T>
	class Optional;

	template<class T>
	class UniquePtr;

	template<class T>
	class RCPtr;

	template<class T>
	class Span;
}

namespace rkit::data
{
	struct ContentID;
}

namespace anox
{
	class AnoxResourceBase;
	struct ICaptureHarness;
}

namespace anox::game
{
	class GameResourceManagerImpl;

	class GameResourceManager final : public rkit::Opaque<GameResourceManagerImpl>
	{
	public:
		void SetCaptureHarness(ICaptureHarness *captureHarness);

		rkit::Result GetContentIDKeyedResource(uint32_t &outReqID, uint32_t resourceType, const rkit::data::ContentID &contentID);
		rkit::Result GetCIPathKeyedResource(uint32_t &outReqID, uint32_t resourceType, const rkit::CIPathView &ciPath);
		rkit::Result TryFinishLoadingResourceRequest(rkit::Optional<uint32_t> &outResID, uint32_t reqID);

		rkit::Result DiscardRequest(uint32_t reqID);
		rkit::Result DecRefResource(uint32_t resID);

		rkit::Result GetFileResourceContents(rkit::RCPtr<AnoxResourceBase> &outKeepAlive, rkit::Span<const uint8_t> &outBytes, uint32_t resID) const;

		static rkit::Result Create(rkit::UniquePtr<GameResourceManager> &resManager);
	};
}
