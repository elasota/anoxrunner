#include "AnoxGameLogic.h"

#include "anox/AnoxGame.h"

#include "rkit/Data/ContentID.h"

#include "rkit/Core/Coroutine.h"
#include "rkit/Core/CoroutineCompiler.h"
#include "rkit/Core/Future.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Path.h"
#include "rkit/Core/UtilitiesDriver.h"

#include "AnoxCaptureHarness.h"
#include "AnoxResourceManager.h"

namespace anox
{
	class AnoxGameLogic final : public IGameLogic
	{
	public:
		AnoxGameLogic(IAnoxGame *game);

		rkit::Result Start() override;
		rkit::Result RunFrame() override;

	private:
		typedef void NoParamsSignature();

		typedef void LoadContentIDKeyedResourceSignature(AnoxResourceRetrieveResult &loadResult, uint32_t resourceType, const rkit::data::ContentID &cid);
		typedef void LoadCIPathKeyedResourceSignature(AnoxResourceRetrieveResult &loadResult, uint32_t resourceType, const rkit::CIPathView &path);
		typedef void LoadStringKeyedResourceSignature(AnoxResourceRetrieveResult &loadResult, uint32_t resourceType, const rkit::StringView &str);

		rkit::coro::MethodStarter<NoParamsSignature> AsyncStartUp();
		rkit::coro::MethodStarter<NoParamsSignature> AsyncRunFrame();

		rkit::coro::MethodStarter<LoadContentIDKeyedResourceSignature> AsyncLoadContentIDKeyedResource();
		rkit::coro::MethodStarter<LoadCIPathKeyedResourceSignature> AsyncLoadCIPathKeyedResource();
		rkit::coro::MethodStarter<LoadStringKeyedResourceSignature> AsyncLoadStringKeyedResource();

		struct AsyncStartUpCoroutine;
		struct AsyncRunFrameCoroutine;
		struct AsyncLoadCIPathKeyedResourceCoroutine;
		struct AsyncLoadStringKeyedResourceCoroutine;
		struct AsyncLoadContentIDKeyedResourceCoroutine;

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

	struct AnoxGameLogic::AsyncStartUpCoroutine final : public rkit::coro::MethodCoroutine<AnoxGameLogic, NoParamsSignature>
	{
		struct Locals
		{
			AnoxResourceRetrieveResult resLoadResult;
		};

		struct Params
		{
		};

		CORO_BEGIN
			CORO_CALL(self->AsyncLoadCIPathKeyedResource, locals.resLoadResult, anox::resloaders::kRawFileResourceTypeCode, rkit::CIPathView("configs/anox.cfg"));

		CORO_END
	};

	struct AnoxGameLogic::AsyncLoadCIPathKeyedResourceCoroutine final : public rkit::coro::MethodCoroutine<AnoxGameLogic, LoadCIPathKeyedResourceSignature>
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

	struct AnoxGameLogic::AsyncLoadStringKeyedResourceCoroutine final : public rkit::coro::MethodCoroutine<AnoxGameLogic, LoadStringKeyedResourceSignature>
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

	struct AnoxGameLogic::AsyncLoadContentIDKeyedResourceCoroutine final : public rkit::coro::MethodCoroutine<AnoxGameLogic, LoadContentIDKeyedResourceSignature>
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

	rkit::coro::MethodStarter<AnoxGameLogic::NoParamsSignature> AnoxGameLogic::AsyncStartUp()
	{
		return rkit::coro::MethodStarter<AnoxGameLogic::NoParamsSignature>(*this, AsyncStartUpCoroutine());
	}

	struct AnoxGameLogic::AsyncRunFrameCoroutine final : public rkit::coro::MethodCoroutine<AnoxGameLogic, NoParamsSignature>
	{
		struct Locals {};

		struct Params {};

		CORO_BEGIN
		CORO_END
	};

	rkit::coro::MethodStarter<AnoxGameLogic::NoParamsSignature> AnoxGameLogic::AsyncRunFrame()
	{
		return rkit::coro::MethodStarter<AnoxGameLogic::NoParamsSignature>(*this, AsyncRunFrameCoroutine());
	}

	rkit::coro::MethodStarter<AnoxGameLogic::LoadContentIDKeyedResourceSignature> AnoxGameLogic::AsyncLoadContentIDKeyedResource()
	{
		return rkit::coro::MethodStarter<AnoxGameLogic::LoadContentIDKeyedResourceSignature>(*this, AsyncLoadContentIDKeyedResourceCoroutine());
	}

	rkit::coro::MethodStarter<AnoxGameLogic::LoadCIPathKeyedResourceSignature> AnoxGameLogic::AsyncLoadCIPathKeyedResource()
	{
		return rkit::coro::MethodStarter<AnoxGameLogic::LoadCIPathKeyedResourceSignature>(*this, AsyncLoadCIPathKeyedResourceCoroutine());
	}

	rkit::coro::MethodStarter<AnoxGameLogic::LoadStringKeyedResourceSignature> AnoxGameLogic::AsyncLoadStringKeyedResource()
	{
		return rkit::coro::MethodStarter<AnoxGameLogic::LoadStringKeyedResourceSignature>(*this, AsyncLoadStringKeyedResourceCoroutine());
	}


	rkit::Result IGameLogic::Create(rkit::UniquePtr<IGameLogic> &outGameLoop, IAnoxGame *game)
	{
		return rkit::New<AnoxGameLogic>(outGameLoop, game);
	}
}
