#include "SceneManager.h"

#include "rkit/Core/Coroutine.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/RefCounted.h"
#include "rkit/Core/String.h"
#include "rkit/Core/Vector.h"

#include "rkit/Data/ContentID.h"

#include "anox/Data/ResourceTypeCodes.h"

#include "SandboxResourceLoader.h"
#include "World.h"

namespace anox::game
{
	class SceneManagerImpl final : public rkit::OpaqueImplementation<SceneManager>
	{
		friend class SceneManager;

	public:
		explicit SceneManagerImpl(World &world);

		rkit::ResultCoroutine RunScene(rkit::ICoroThread &thread, rkit::ByteStringSliceView name, rkit::data::ContentID cid, bool loop);

	private:
		struct ActiveScene : public rkit::RefCounted
		{
			rkit::ByteString m_name;
			rkit::data::ContentID m_contentID;
		};

		// SAVEGAME TODO
		rkit::Vector<ActiveScene> m_scenes;


		World &m_world;
	};

	SceneManagerImpl::SceneManagerImpl(World &world)
		: m_world(world)
	{
	}

	rkit::ResultCoroutine SceneManagerImpl::RunScene(rkit::ICoroThread &thread, rkit::ByteStringSliceView name, rkit::data::ContentID cid, bool loop)
	{
		SandboxResourceRequestHandle sceneReqHandle;
		CORO_CHECK(SandboxResourceLoader::LoadContentKeyedResource(sceneReqHandle, resloaders::kContentIDRawFileResourceTypeCode, cid));

		SandboxResourceHandle sceneResHandle;
		CORO_CHECK(co_await sceneReqHandle.WaitForLoaded(thread, sceneResHandle));

		SandboxResourceDataBlob blob;
		CORO_CHECK(SandboxResourceLoader::GetFileResourceContents(blob, sceneResHandle));

		CORO_RETURN_OK;
	}


	SceneManager::SceneManager(World &world)
		: rkit::Opaque<SceneManagerImpl>(world)
	{
	}

	rkit::Result SceneManager::Create(rkit::UniquePtr<SceneManager> &outManager, World &world)
	{
		return rkit::New<SceneManager>(outManager, world);
	}

	rkit::ResultCoroutine SceneManager::RunScene(rkit::ICoroThread &thread, const rkit::ByteStringSliceView &name, const rkit::data::ContentID &cid, bool loop)
	{
		return Impl().RunScene(thread, name, cid, loop);
	}
}

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(anox::game::SceneManagerImpl)
