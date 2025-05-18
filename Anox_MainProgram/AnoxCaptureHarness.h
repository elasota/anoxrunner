#pragma once

#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/PathProto.h"
#include "rkit/Core/StringProto.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	template<class T>
	class Future;

	namespace data
	{
		struct ContentID;
	}
}

namespace anox
{
	struct IConfigurationState;
	struct ISessionState;
	struct AnoxResourceRetrieveResult;
	struct IAnoxGame;

	class AnoxResourceManagerBase;

	struct ICaptureHarness
	{
		virtual ~ICaptureHarness() {}

		virtual rkit::Result GetConfigurationState(const IConfigurationState **outConfigStatePtr) = 0;
		virtual rkit::Result GetSessionState(const IConfigurationState **outConfigStatePtr) = 0;

		virtual rkit::Result GetContentIDKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::data::ContentID &cid) = 0;
		virtual rkit::Result GetCIPathKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::CIPathView &path) = 0;
		virtual rkit::Result GetStringKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::StringView &str) = 0;

		static rkit::Result CreateRealTime(rkit::UniquePtr<ICaptureHarness> &outHarness, IAnoxGame &game, AnoxResourceManagerBase &resManager);
	};
}
