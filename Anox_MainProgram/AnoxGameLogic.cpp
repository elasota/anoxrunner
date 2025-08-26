#include "AnoxGameLogic.h"

#include "anox/AnoxGame.h"

#include "rkit/Data/ContentID.h"

#include "rkit/Core/Coroutine.h"
#include "rkit/Core/CoroutineCompiler.h"
#include "rkit/Core/Future.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Path.h"
#include "rkit/Core/String.h"
#include "rkit/Core/UtilitiesDriver.h"

#include "AnoxCaptureHarness.h"
#include "AnoxCommandRegistry.h"
#include "AnoxCommandStack.h"
#include "AnoxConfigurationSaver.h"
#include "AnoxFileResource.h"
#include "AnoxResourceManager.h"
#include "AnoxGlobalVars.h"

namespace anox
{
	class AnoxCommandStackBase;

	class AnoxGameLogic final : public IGameLogic
	{
	public:
		AnoxGameLogic(IAnoxGame *game);

		rkit::Result Start() override;
		rkit::Result RunFrame() override;

		rkit::Result CreateNewGame(rkit::UniquePtr<IConfigurationState> &outConfig, const rkit::StringSliceView &mapName) override;
		rkit::Result SaveGame(rkit::UniquePtr<IConfigurationState> &outConfig) override;

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
		CORO_DECL_METHOD(LoadMap, const rkit::StringSliceView &mapName);
		CORO_DECL_METHOD(LoadContentIDKeyedResource, AnoxResourceRetrieveResult &loadResult, uint32_t resourceType, const rkit::data::ContentID &cid);
		CORO_DECL_METHOD(LoadCIPathKeyedResource, AnoxResourceRetrieveResult &loadResult, uint32_t resourceType, const rkit::CIPathView &path);
		CORO_DECL_METHOD(LoadStringKeyedResource, AnoxResourceRetrieveResult &loadResult, uint32_t resourceType, const rkit::StringView &str);
		CORO_DECL_METHOD(ExecCommandFile, AnoxCommandStackBase &commandStack, const rkit::CIPathView &path);
		CORO_DECL_METHOD(RunCommands, AnoxCommandStackBase &commandStack);
		CORO_DECL_METHOD(RunCommand, AnoxCommandStackBase &commandStack, const rkit::Span<char> &line);
		CORO_DECL_METHOD_OVERRIDE(StartSession);

		CORO_DECL_METHOD(Cmd_Exec, AnoxCommandStackBase &commandStack, const rkit::ISpan<rkit::StringView> &args);
		CORO_DECL_METHOD(Cmd_Map, AnoxCommandStackBase &commandStack, const rkit::ISpan<rkit::StringView> &args);

		IAnoxGame *m_game;
		rkit::UniquePtr<rkit::coro::Thread> m_mainCoroThread;
		rkit::UniquePtr<AnoxCommandStackBase> m_commandStack;

		rkit::UniquePtr<game::GlobalVars> m_globalVars;
	};

	AnoxGameLogic::AnoxGameLogic(IAnoxGame *game)
		: m_game(game)
	{
	}

	rkit::Result AnoxGameLogic::Start()
	{
		RKIT_CHECK(m_game->GetCommandRegistry()->RegisterCommand("exec", this->AsyncCmd_Exec()));
		RKIT_CHECK(m_game->GetCommandRegistry()->RegisterCommand("map", this->AsyncCmd_Map()));

		RKIT_CHECK(AnoxCommandStackBase::Create(m_commandStack, 64 * 1024, 1024));

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

	rkit::Result AnoxGameLogic::CreateNewGame(rkit::UniquePtr<IConfigurationState> &outConfig, const rkit::StringSliceView &mapName)
	{
		rkit::UniquePtr<game::GlobalVars> globalVars;
		RKIT_CHECK(rkit::New<game::GlobalVars>(globalVars));

		RKIT_CHECK(globalVars->m_mapName.Set(mapName));

		rkit::UniquePtr<IConfigurationState> globalVarsConfig;
		RKIT_CHECK(globalVars->Save(globalVarsConfig));

		outConfig = std::move(globalVarsConfig);

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxGameLogic::SaveGame(rkit::UniquePtr<IConfigurationState> &outConfig)
	{
		return rkit::ResultCode::kNotYetImplemented;
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
				}

				startIndex = endIndex + 1;
			}
		}

		RKIT_ASSERT(false);
		return rkit::StringView();
	}

	rkit::Result AnoxGameLogic::InsertAlias(AnoxCommandStackBase &commandStack, const AnoxRegisteredAlias &alias)
	{
		size_t endPos = alias.m_text.Length();

		size_t scanPos = endPos;

		while (scanPos > 0)
		{
			scanPos--;

			size_t startPos = scanPos;

			bool isSplit = false;
			if (scanPos == 0)
			{
				isSplit = true;
			}
			else if (alias.m_text[scanPos] == ';')
			{
				isSplit = true;
				startPos++;
			}

			if (isSplit)
			{
				rkit::StringSliceView slice = alias.m_text.SubString(startPos, endPos - startPos);

				RKIT_CHECK(commandStack.Push(slice));
				endPos = startPos;
			}
		}

		return rkit::ResultCode::kOK;
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
			CORO_CALL(self->AsyncExecCommandFile, *self->m_commandStack, rkit::CIPathView("configs/default.cfg"));
			CORO_CHECK(self->m_commandStack->Push("d1"));
			CORO_CALL(self->AsyncRunCommands, *self->m_commandStack);
		CORO_END
	};

	CORO_DEF_METHOD(AnoxGameLogic, ExecCommandFile)
	{
		struct Locals
		{
			AnoxResourceRetrieveResult resLoadResult;
		};

		struct Params
		{
			AnoxCommandStackBase &commandStack;
			const rkit::CIPathView path;
		};

		CORO_BEGIN
			CORO_CALL(self->AsyncLoadCIPathKeyedResource, locals.resLoadResult, anox::resloaders::kRawFileResourceTypeCode, params.path);

			CORO_CHECK(params.commandStack.Parse(locals.resLoadResult.m_resourceHandle.StaticCast<AnoxFileResourceBase>()->GetContents()));
			CORO_CALL(self->AsyncRunCommands, params.commandStack);
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

			rkit::log::ErrorFmt("Unknown console command {}", locals.parser.GetCommand().GetChars());
		CORO_END
	};

	CORO_DEF_METHOD(AnoxGameLogic, Cmd_Exec)
	{
		struct Locals
		{
			rkit::CIPath path;
			rkit::CIPath relPath;
		};

		struct Params
		{
			AnoxCommandStackBase &cmdStack;
			const rkit::ISpan<rkit::StringView> &args;
		};

		CORO_BEGIN
			if (params.args.Count() < 1)
			{
				rkit::log::Error("Usage: exec <file>");
				CORO_RETURN;
			}

			CORO_CHECK(locals.path.Set(rkit::CIPathView("configs")));

			rkit::StringView configPathStr = params.args[0];
			rkit::PathValidationResult validationResult = rkit::CIPath::Validate(configPathStr);
			rkit::CIPathView configRelPath;

			if (validationResult == rkit::PathValidationResult::kValid)
				configRelPath = rkit::CIPathView(configPathStr);
			else if (validationResult == rkit::PathValidationResult::kConvertible)
			{
				CORO_CHECK(locals.relPath.Set(configPathStr));
				configRelPath = locals.relPath;
			}
			else
			{
				rkit::log::Error("Malformed exec file path");
				CORO_RETURN;
			}

			CORO_CHECK(locals.path.Append(configRelPath));

			CORO_CALL(self->AsyncExecCommandFile, params.cmdStack, locals.path);
		CORO_END
	};

	CORO_DEF_METHOD(AnoxGameLogic, Cmd_Map)
	{
		struct Locals
		{
			rkit::CIPath path;
			rkit::CIPath relPath;
		};

		struct Params
		{
			AnoxCommandStackBase &cmdStack;
			const rkit::ISpan<rkit::StringView> &args;
		};

		CORO_BEGIN
			if (params.args.Count() < 1)
			{
				rkit::log::Error("Usage: map <map name>");
				CORO_RETURN;
			}

			rkit::StringView mapNameStr = params.args[0];

			for (char c : mapNameStr)
			{
				if (c >= 'a' && c <= 'z')
					continue;
				if (c >= 'A' && c <= 'Z')
					continue;
				if (c >= '0' && c <= '9')
					continue;
				if (c == '_')
					continue;

				rkit::log::Error("Malformed map file path");
				CORO_RETURN;
			}

			CORO_CALL(self->m_game->AsyncRestartGame, mapNameStr);
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

	CORO_DEF_METHOD(AnoxGameLogic, LoadMap)
	{
		struct Locals
		{
			AnoxResourceRetrieveResult loadResult;
			rkit::CIPath path;
		};

		struct Params
		{
			rkit::StringSliceView mapName;
		};

		CORO_BEGIN
			rkit::String fullPathStr;
			CORO_CHECK(fullPathStr.Format("ax_bsp/maps/{}.bsp.bspmodel", params.mapName));

			CORO_CHECK(locals.path.Set(fullPathStr));

			CORO_CALL(self->AsyncLoadCIPathKeyedResource, locals.loadResult, anox::resloaders::kBSPModelResourceTypeCode, locals.path);

			int n = 0;
		CORO_END
	};

	CORO_DEF_METHOD(AnoxGameLogic, StartSession)
	{
		struct Locals
		{
			const IConfigurationState *configState = nullptr;
		};

		struct Params {};

		CORO_BEGIN
			CORO_CHECK(self->m_game->GetCaptureHarness()->GetConfigurationState(locals.configState));

			IConfigurationValueView root = locals.configState->GetRoot();

			IConfigurationKeyValueTableView kvt;
			CORO_CHECK(root.Get(kvt));

			IConfigurationValueView mapNameValue;
			CORO_CHECK(kvt.GetValueFromKey("mapName", mapNameValue));

			rkit::StringSliceView mapName;
			CORO_CHECK(mapNameValue.Get(mapName));

			for (char c : mapName)
			{
				if (c >= 'a' && c <= 'z')
					continue;
				if (c >= '0' && c <= '9')
					continue;

				rkit::log::Error("Invalid map name");
				CORO_CHECK(rkit::ResultCode::kDataError);
			}

			CORO_CALL(self->AsyncLoadMap, mapName);
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
