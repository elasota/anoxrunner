#pragma once

#include "rkit/Core/Opaque.h"
#include "rkit/Core/Result.h"

namespace rkit
{
	template<class T>
	class UniquePtr;
}

namespace rkit::data
{
	struct ContentID;
}

namespace anox::game
{
	class SceneManagerImpl;

	class SceneManager final : public rkit::Opaque<SceneManagerImpl>
	{
	public:
		static rkit::Result Create(rkit::UniquePtr<SceneManager> &outManager);

	private:
		// SAVEGAME TODO
	};
}
