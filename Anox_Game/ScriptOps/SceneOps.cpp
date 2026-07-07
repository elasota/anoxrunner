#include "APEExternDispatch.generated.h"

#include "rkit/Core/Coroutine.h"
#include "rkit/Data/ContentID.h"

#include "SceneManager.h"
#include "ScriptEnvironment.h"
#include "ScriptManager.h"
#include "ScriptNamedResourceRef.h"
#include "World.h"

namespace anox::game::ape::externs
{
	rkit::ResultCoroutine loopscene::Execute(rkit::ICoroThread &thread, const ScriptExternContext &ctx, ExternDispatch::SceneResourceArg_t scene)
	{
		//CORO_CHECK(ctx.m_world->GetSceneManager().LoopScene(scene));
		CORO_RETURN_OK;
	}
}
