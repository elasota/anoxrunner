#include "AnoxGameLogic.h"

#include "anox/AnoxGame.h"

#include "rkit/Data/ContentID.h"

#include "rkit/Core/Coroutine.h"
#include "rkit/Core/CoroutineCompiler.h"
#include "rkit/Core/Future.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Path.h"
#include "rkit/Core/UtilitiesDriver.h"

#include "AnoxCaptureHarness.h"
#include "AnoxCommandRegistry.h"
#include "AnoxCommandStack.h"
#include "AnoxFileResource.h"
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
		class DestructiveSpanArgParser final : public rkit::ISpan<rkit::StringView>
		{
		public:
			DestructiveSpanArgParser();

			void Acquire(const rkit::Span<char> &line);

			rkit::StringView GetCommand() const;

			size_t Count() const override;
			rkit::StringView operator[](size_t index) const override;

		private:
			rkit::StringView m_cmd;
			rkit::ConstSpan<char> m_line;
			size_t m_count;
		};

		static rkit::Result InsertAlias(AnoxCommandStackBase &commandStack, const AnoxRegisteredAlias &alias);
		static rkit::Result ApplyConsoleVar(const AnoxRegisteredConsoleVar &consoleVar, const rkit::ISpan<rkit::StringView> &args);

		CORO_DECL_METHOD(StartUp);
		CORO_DECL_METHOD(RunFrame);
		CORO_DECL_METHOD(LoadContentIDKeyedResource, AnoxResourceRetrieveResult &loadResult, uint32_t resourceType, const rkit::data::ContentID &cid);
		CORO_DECL_METHOD(LoadCIPathKeyedResource, AnoxResourceRetrieveResult &loadResult, uint32_t resourceType, const rkit::CIPathView &path);
		CORO_DECL_METHOD(LoadStringKeyedResource, AnoxResourceRetrieveResult &loadResult, uint32_t resourceType, const rkit::StringView &str);
		CORO_DECL_METHOD(ExecCommandFile, const rkit::CIPathView &path);
		CORO_DECL_METHOD(RunCommands, AnoxCommandStackBase &commandStack);
		CORO_DECL_METHOD(RunCommand, AnoxCommandStackBase &commandStack, const rkit::Span<char> &line);

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


	AnoxGameLogic::DestructiveSpanArgParser::DestructiveSpanArgParser()
		: m_count(0)
	{
	}

	void AnoxGameLogic::DestructiveSpanArgParser::Acquire(const rkit::Span<char> &lineRef)
	{
		rkit::Span<char> line = lineRef;

		size_t argCount = 0;

		bool isInQuote = false;

		// Tokenize the string
		size_t startIndex = 0;
		for (size_t i = 0; i < line.Count(); i++)
		{
			if (rkit::IsASCIIWhitespace(line[i]) && !isInQuote)
			{
				startIndex = 0;
				line[i] = 0;
				startIndex = i + 1;
			}
			else
			{
				if (line[i] == '\"')
				{
					if (i == startIndex)
						isInQuote = true;
					else
					{
						// Only cull end quotes so empty strings are preserved
						if (isInQuote)
						{
							isInQuote = false;
							line[i] = 0;
						}
					}
				}
			}
		}

		startIndex = 0;
		for (size_t i = 0; i <= line.Count(); i++)
		{
			if (i == line.Count() || line[i] == 0)
			{
				if (i != startIndex)
				{
					argCount++;
					if (argCount == 1)
					{
						if (line[startIndex] == '\"')
							startIndex++;

						m_cmd = rkit::StringView(line.Ptr() + startIndex, i - startIndex);
					}
					else if (argCount == 2)
						m_line = line.SubSpan(startIndex);
				}

				startIndex = i + 1;
			}
		}

		RKIT_ASSERT(argCount >= 1);
		m_count = argCount - 1;
	}

	rkit::StringView AnoxGameLogic::DestructiveSpanArgParser::GetCommand() const
	{
		return m_cmd;
	}

	size_t AnoxGameLogic::DestructiveSpanArgParser::Count() const
	{
		return m_count;
	}

	rkit::StringView AnoxGameLogic::DestructiveSpanArgParser::operator[](size_t index) const
	{
		size_t numArgsParsed = 0;

		size_t startIndex = 0;
		for (size_t endIndex = 0; endIndex <= m_line.Count(); endIndex++)
		{
			if (endIndex == m_line.Count() || m_line[endIndex] == 0)
			{
				if (startIndex != endIndex)
				{
					numArgsParsed++;
					if (numArgsParsed > index)
					{
						if (m_line[startIndex] == '\"')
							startIndex++;

						return rkit::StringView(m_line.Ptr() + startIndex, endIndex - startIndex);
					}

					startIndex = endIndex + 1;
				}
			}
		}

		RKIT_ASSERT(false);
		return rkit::StringView();
	}

	rkit::Result AnoxGameLogic::InsertAlias(AnoxCommandStackBase &commandStack, const AnoxRegisteredAlias &alias)
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result AnoxGameLogic::ApplyConsoleVar(const AnoxRegisteredConsoleVar &consoleVar, const rkit::ISpan<rkit::StringView> &args)
	{
		return rkit::ResultCode::kNotYetImplemented;
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
			CORO_CHECK(AnoxCommandStackBase::Create(locals.commandStack, 64 * 1024, 1024));

			CORO_CALL(self->AsyncLoadCIPathKeyedResource, locals.resLoadResult, anox::resloaders::kRawFileResourceTypeCode, rkit::CIPathView("configs/default.cfg"));

			CORO_CHECK(locals.commandStack->Parse(locals.resLoadResult.m_resourceHandle.StaticCast<AnoxFileResourceBase>()->GetContents()));
			CORO_CALL(self->AsyncRunCommands, *locals.commandStack);
		CORO_END
	};


	CORO_DEF_METHOD(AnoxGameLogic, RunCommands)
	{
		struct Locals
		{
			rkit::Span<char> line;
		};

		struct Params
		{
			AnoxCommandStackBase &commandStack;
		};

		CORO_BEGIN
			CORO_WHILE(params.commandStack.Pop(locals.line))
				CORO_CALL(self->AsyncRunCommand, params.commandStack, locals.line);
			CORO_END_WHILE
		CORO_END
	};


	CORO_DEF_METHOD(AnoxGameLogic, RunCommand)
	{
		struct Locals
		{
			DestructiveSpanArgParser parser;
			rkit::HashValue_t cmdHash = 0;
			AnoxCommandRegistryBase *cmdRegistry = nullptr;

			const AnoxRegisteredCommand *cmd = nullptr;
			const AnoxRegisteredAlias *alias = nullptr;
			const AnoxRegisteredConsoleVar *consoleVar = nullptr;
		};

		struct Params
		{
			AnoxCommandStackBase &commandStack;
			rkit::Span<char> line;
		};

		CORO_BEGIN
			locals.parser.Acquire(params.line);

			locals.cmdHash = rkit::Hasher<rkit::StringView>::ComputeHash(0, locals.parser.GetCommand());

			locals.cmdRegistry = self->m_game->GetCommandRegistry();

			locals.cmd = locals.cmdRegistry->FindCommand(locals.parser.GetCommand(), locals.cmdHash);

			CORO_IF(locals.cmd != nullptr)
				CORO_CALL(locals.cmd->AsyncCall, params.commandStack, locals.parser);
				CORO_RETURN;
			CORO_END_IF

			locals.alias = locals.cmdRegistry->FindAlias(locals.parser.GetCommand(), locals.cmdHash);

			CORO_IF(locals.alias != nullptr)
				CORO_CHECK(InsertAlias(params.commandStack, *locals.alias));
				CORO_RETURN;
			CORO_END_IF

			locals.consoleVar = locals.cmdRegistry->FindConsoleVar(locals.parser.GetCommand(), locals.cmdHash);

			CORO_IF(locals.consoleVar != nullptr)
				CORO_CHECK(ApplyConsoleVar(*locals.consoleVar, locals.parser));
				CORO_RETURN;
			CORO_END_IF

			rkit::log::ErrorFmt("Unknown console command %s", locals.parser.GetCommand().GetChars());
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
