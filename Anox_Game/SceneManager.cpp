#include "SceneManager.h"

#include "rkit/Core/NewDelete.h"

namespace anox::game
{
	class SceneManagerImpl final : public rkit::OpaqueImplementation<SceneManager>
	{
	};

	rkit::Result SceneManager::Create(rkit::UniquePtr<SceneManager> &outManager)
	{
		return rkit::New<SceneManager>(outManager);
	}
}

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(anox::game::SceneManagerImpl)
