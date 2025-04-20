#pragma once

#include "rkit/Core/CoreDefs.h"

namespace rkit
{
	template<class T>
	class UniquePtr;
}

namespace anox
{
	struct IAnoxGame;

	class IGameLogic
	{
	public:
		virtual ~IGameLogic() {}

		virtual rkit::Result Start() = 0;
		virtual rkit::Result RunFrame() = 0;

		static rkit::Result Create(rkit::UniquePtr<IGameLogic> &outGameLoop, IAnoxGame *game);
	};
}
