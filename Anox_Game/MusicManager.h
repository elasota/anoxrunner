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
	class MusicManagerImpl;

	class MusicManager final : public rkit::Opaque<MusicManagerImpl>
	{
	public:
		static rkit::Result Create(rkit::UniquePtr<MusicManager> &outManager);

		rkit::Result Initialize();
		rkit::Result OnFrame();

		rkit::Result SetLevelMusic(const rkit::data::ContentID &contentID);

	private:
		// SAVEGAME TODO
	};
}
