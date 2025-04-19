#include "AnoxGameLogic.h"

#include "rkit/Core/NewDelete.h"

#include "rkit/Core/Coroutine.h"
#include "rkit/Core/CoroutineCompiler.h"

#include "rkit/Core/UtilitiesDriver.h"

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
		rkit::coro::MethodStarter<NoParamsSignature> AsyncStartUp();
		rkit::coro::MethodStarter<NoParamsSignature> AsyncRunFrame();

		struct AsyncStartUpCoroutine;
		struct AsyncRunFrameCoroutine;

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
		if (m_mainCoroThread->GetState() == rkit::coro::ThreadState::kInactive)
		{
			RKIT_CHECK(m_mainCoroThread->EnterFunction(AsyncRunFrame()));
		}

		if (m_mainCoroThread->GetState() == rkit::coro::ThreadState::kSuspended)
		{
			RKIT_CHECK(m_mainCoroThread->Resume());
		}

		if (m_mainCoroThread->GetState() == rkit::coro::ThreadState::kFaulted)
		{
			return rkit::ResultCode::kNotYetImplemented;
		}

		return rkit::ResultCode::kOK;
	}


	struct AnoxGameLogic::AsyncStartUpCoroutine final : public rkit::coro::MethodCoroutine<AnoxGameLogic, NoParamsSignature>
	{
		struct Locals {};

		struct Params {};

		CORO_BEGIN
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

	rkit::Result IGameLogic::Create(rkit::UniquePtr<IGameLogic> &outGameLoop, IAnoxGame *game)
	{
		return rkit::New<AnoxGameLogic>(outGameLoop, game);
	}
}
