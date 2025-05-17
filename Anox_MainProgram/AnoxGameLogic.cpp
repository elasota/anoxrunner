#include "AnoxGameLogic.h"

#include "anox/AnoxGame.h"

#include "rkit/Data/ContentID.h"

#include "rkit/Core/Coroutine.h"
#include "rkit/Core/CoroutineCompiler.h"
#include "rkit/Core/Future.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Path.h"
#include "rkit/Core/UtilitiesDriver.h"

#include "AnoxFileResource.h"
#include "AnoxCaptureHarness.h"
#include "AnoxCommandStack.h"
#include "AnoxResourceManager.h"

namespace anox
{
	class AnoxCommandStackBase;

	class AnoxGameLogic final : public IGameLogic
	{
	public:
		AnoxGameLogic(IAnoxGame *game);

		rkit::Result Start() override;
		rkit::Result RunFrame() override;

	private:

		CORO_DECL_METHOD(StartUp);
		CORO_DECL_METHOD(RunFrame);
		CORO_DECL_METHOD(LoadContentIDKeyedResource, AnoxResourceRetrieveResult &loadResult, uint32_t resourceType, const rkit::data::ContentID &cid);
		CORO_DECL_METHOD(LoadCIPathKeyedResource, AnoxResourceRetrieveResult &loadResult, uint32_t resourceType, const rkit::CIPathView &path);
		CORO_DECL_METHOD(LoadStringKeyedResource, AnoxResourceRetrieveResult &loadResult, uint32_t resourceType, const rkit::StringView &str);
		CORO_DECL_METHOD(ExecCommandFile, const rkit::CIPathView &path);
		CORO_DECL_METHOD(RunCommands, AnoxCommandStackBase &commandStack);

		IAnoxGame *m_game;
		rkit::UniquePtr<rkit::coro::Thread> m_mainCoroThread;
	};

	AnoxGameLogic::AnoxGameLogic(IAnoxGame *game)
		: m_game(game)
	{
	}

	rkit::Result AnoxGameLogic::Start()
	{
		RKIT_CHECK(rkit::GetDrivers().m_utilitiesDriver->CreateCoroThread(m_mainCoroThread, 1 * 1024 * 1024));
		RKIT_CHECK(m_mainCoroThread->EnterFunction(AsyncStartUp()));

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxGameLogic::RunFrame()
	{
		bool haveMainThreadWork = true;
		bool haveKickedOffRunFrame = false;
		while (haveMainThreadWork)
		{
			haveMainThreadWork = false;

			switch (m_mainCoroThread->GetState())
			{
			case rkit::coro::ThreadState::kInactive:
				if (!haveKickedOffRunFrame)
				{
					RKIT_CHECK(m_mainCoroThread->EnterFunction(AsyncRunFrame()));
					haveMainThreadWork = true;

					haveKickedOffRunFrame = true;
				}
				break;
			case rkit::coro::ThreadState::kSuspended:
				RKIT_CHECK(m_mainCoroThread->Resume());
				haveMainThreadWork = true;
				break;
			case rkit::coro::ThreadState::kBlocked:
				if (m_mainCoroThread->TryUnblock())
					haveMainThreadWork = true;
				break;
			default:
				return rkit::ResultCode::kInternalError;
			};
		}

		return rkit::ResultCode::kOK;
	}

	CORO_DEF_METHOD(AnoxGameLogic, StartUp)
	{
		struct Locals
		{
		};

		struct Params
		{
		};

		CORO_BEGIN
			CORO_CALL(self->AsyncExecCommandFile, rkit::CIPathView("configs/default.cfg"));

		CORO_END
	};

	CORO_DEF_METHOD(AnoxGameLogic, ExecCommandFile)
	{
		struct Locals
		{
			AnoxResourceRetrieveResult resLoadResult;
			rkit::UniquePtr<AnoxCommandStackBase> commandStack;
		};

		struct Params
		{
			const rkit::CIPathView path;
		};

		CORO_BEGIN
			CORO_CHECK(AnoxCommandStackBase::Create(locals.commandStack, 64 * 1024));

			CORO_CALL(self->AsyncLoadCIPathKeyedResource, locals.resLoadResult, anox::resloaders::kRawFileResourceTypeCode, rkit::CIPathView("configs/default.cfg"));

			CORO_CHECK(locals.commandStack->Parse(locals.resLoadResult.m_resourceHandle.StaticCast<AnoxFileResourceBase>()->GetContents()));
			CORO_CALL(self->AsyncRunCommands, *locals.commandStack);
		CORO_END
	};


	CORO_DEF_METHOD(AnoxGameLogic, RunCommands)
	{
		struct Locals
		{
			rkit::StringView line;
		};

		struct Params
		{
			AnoxCommandStackBase &commandStack;
		};

		CORO_BEGIN
			CORO_WHILE(params.commandStack.Pop(locals.line))
				CORO_CHECK(rkit::ResultCode::kNotYetImplemented);
			CORO_END_WHILE
		CORO_END
	};

	CORO_DEF_METHOD(AnoxGameLogic, LoadCIPathKeyedResource)
	{
		struct Locals
		{
			rkit::Future<AnoxResourceRetrieveResult> resLoadResult;
		};

		struct Params
		{
			AnoxResourceRetrieveResult &loadResult;
			uint32_t resourceType;
			rkit::CIPathView path;
		};

		CORO_BEGIN
			CORO_CHECK(self->m_game->GetCaptureHarness()->GetCIPathKeyedResource(locals.resLoadResult, params.resourceType, params.path));
			CORO_AWAIT(locals.resLoadResult);

			params.loadResult = locals.resLoadResult.GetResult();
		CORO_END
	};

	CORO_DEF_METHOD(AnoxGameLogic, LoadStringKeyedResource)
	{
		struct Locals
		{
			rkit::Future<AnoxResourceRetrieveResult> resLoadResult;
		};

		struct Params
		{
			AnoxResourceRetrieveResult &loadResult;
			uint32_t resourceType;
			rkit::StringView str;
		};

		CORO_BEGIN
			CORO_CHECK(self->m_game->GetCaptureHarness()->GetStringKeyedResource(locals.resLoadResult, params.resourceType, params.str));
			CORO_AWAIT(locals.resLoadResult);

			params.loadResult = locals.resLoadResult.GetResult();
		CORO_END
	};

	CORO_DEF_METHOD(AnoxGameLogic, LoadContentIDKeyedResource)
	{
		struct Locals
		{
			rkit::Future<AnoxResourceRetrieveResult> resLoadResult;
		};

		struct Params
		{
			AnoxResourceRetrieveResult &loadResult;
			uint32_t resourceType;
			rkit::data::ContentID cid;
		};

		CORO_BEGIN
			CORO_CHECK(self->m_game->GetCaptureHarness()->GetContentIDKeyedResource(locals.resLoadResult, params.resourceType, params.cid));
			CORO_AWAIT(locals.resLoadResult);

			params.loadResult = locals.resLoadResult.GetResult();
		CORO_END
	};

	CORO_DEF_METHOD(AnoxGameLogic, RunFrame)
	{
		struct Locals {};

		struct Params {};

		CORO_BEGIN
		CORO_END
	};

	rkit::Result IGameLogic::Create(rkit::UniquePtr<IGameLogic> &outGameLoop, IAnoxGame *game)
	{
		return rkit::New<AnoxGameLogic>(outGameLoop, game);
	}
}
