#pragma once

#include "rkit/Core/Opaque.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/CoroutineProtos.h"
#include "rkit/Core/StringProto.h"

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
	class World;

	class SceneManager final : public rkit::Opaque<SceneManagerImpl>
	{
	public:
		explicit SceneManager(World &world);

		static rkit::Result Create(rkit::UniquePtr<SceneManager> &outManager, World &world);

		rkit::ResultCoroutine RunScene(rkit::ICoroThread &thread, const rkit::ByteStringSliceView &name, const rkit::data::ContentID &cid, bool loop);
		rkit::ResultCoroutine OnFrame(rkit::ICoroThread &thread);

	private:
		// SAVEGAME TODO
	};
}
