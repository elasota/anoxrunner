#include "APEExternDispatch.generated.h"

#include "rkit/Core/Coroutine.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Data/ContentID.h"

#include "anox/Data/ResourceTypeCodes.h"
#include "anox/AnoxModule.h"

#include "MusicManager.h"
#include "ScriptEnvironment.h"
#include "ScriptManager.h"
#include "ScriptNamedResourceRef.h"
#include "World.h"

namespace anox::game::ape::externs
{
	rkit::ResultCoroutine PlayLevelMusic::Execute(rkit::ICoroThread &thread, const ScriptExternContext &ctx, ExternDispatch::FileResourceArg_t music)
	{
		if (music.m_contentID == nullptr)
		{
			rkit::log::Error(u8"Music resource was missing");
			CORO_RETURN_OK;
		}

		CORO_CHECK(ctx.m_world->GetMusicManager().SetLevelMusic(*music.m_contentID));
		CORO_RETURN_OK;
	}
}
