#include "APEExternDispatch.generated.h"

#include "rkit/Core/Coroutine.h"
#include "rkit/Data/ContentID.h"

#include "MusicManager.h"
#include "ScriptEnvironment.h"
#include "ScriptManager.h"
#include "World.h"

namespace anox::game::ape::externs
{
	rkit::ResultCoroutine PlayLevelMusic::Execute(rkit::ICoroThread &thread, const ScriptExternContext &ctx, ExternDispatch::FileResourceArg_t music)
	{
		CORO_CHECK(ctx.m_world->GetMusicManager().SetLevelMusic(music));
		CORO_RETURN_OK;
	}
}
