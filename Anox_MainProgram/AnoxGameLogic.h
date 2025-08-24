#pragma once

#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/Coroutine.h"
#include "rkit/Core/StringProto.h"

namespace rkit
{
	template<class T>
	class UniquePtr;
}

namespace anox
{
	struct IAnoxGame;
	struct IConfigurationState;

	class IGameLogic
	{
	public:
		virtual ~IGameLogic() {}

		virtual rkit::Result Start() = 0;
		virtual rkit::Result RunFrame() = 0;

		virtual rkit::Result CreateNewGame(rkit::UniquePtr<IConfigurationState> &outConfig, const rkit::StringSliceView &mapName) = 0;
		virtual rkit::Result SaveGame(rkit::UniquePtr<IConfigurationState> &outConfig) = 0;

		CORO_DECL_METHOD_ABSTRACT(StartSession);

		static rkit::Result Create(rkit::UniquePtr<IGameLogic> &outGameLoop, IAnoxGame *game);
	};
}
