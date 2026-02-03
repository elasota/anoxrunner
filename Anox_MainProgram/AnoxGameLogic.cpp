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

#include "AnoxBSPModelResource.h"
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
	class AnoxBSPModelResourceBase;

	class AnoxGameLogic final : public IGameLogic
	{
	public:
		AnoxGameLogic(IAnoxGame *game);

		rkit::Result Start() override;
		rkit::Result RunFrame() override;

		rkit::Result CreateNewGame(rkit::UniquePtr<IConfigurationState> &outConfig, const rkit::StringSliceView &mapName) override;
		rkit::Result SaveGame(rkit::UniquePtr<IConfigurationState> &outConfig) override;

	private:
		class DestructiveSpanArgParser final : public rkit::ISpan<rkit::ByteStringView>
		{
		public:
			DestructiveSpanArgParser();

			void Acquire(const rkit::Span<uint8_t> &line);

			rkit::ByteStringView GetCommand() const;

			size_t Count() const override;
			rkit::ByteStringView operator[](size_t index) const override;

		private:
			rkit::ByteStringView m_cmd;
			rkit::ConstSpan<uint8_t> m_line;
			size_t m_count;
		};

		static rkit::Result InsertAlias(AnoxCommandStackBase &commandStack, const AnoxRegisteredAlias &alias);
		static rkit::Result ApplyConsoleVar(const AnoxRegisteredConsoleVar &consoleVar, const rkit::ISpan<rkit::ByteStringView> &args);

		CORO_DECL_METHOD(StartUp);
		CORO_DECL_METHOD(RunFrame);
		CORO_DECL_METHOD(LoadMap, const rkit::StringSliceView &mapName);
		CORO_DECL_METHOD(SpawnMapInitialObjects, const rkit::StringSliceView &mapName);
		CORO_DECL_METHOD(LoadContentIDKeyedResource, AnoxResourceRetrieveResult &loadResult, uint32_t resourceType, const rkit::data::ContentID &cid);
		CORO_DECL_METHOD(LoadCIPathKeyedResource, AnoxResourceRetrieveResult &loadResult, uint32_t resourceType, const rkit::CIPathView &path);
		CORO_DECL_METHOD(LoadStringKeyedResource, AnoxResourceRetrieveResult &loadResult, uint32_t resourceType, const rkit::StringView &str);
		CORO_DECL_METHOD(ExecCommandFile, AnoxCommandStackBase &commandStack, const rkit::CIPathView &path);
		CORO_DECL_METHOD(RunCommands, AnoxCommandStackBase &commandStack);
		CORO_DECL_METHOD(RunCommand, AnoxCommandStackBase &commandStack, const rkit::Span<uint8_t> &line);
		CORO_DECL_METHOD_OVERRIDE(StartSession);

		CORO_DECL_METHOD(Cmd_Exec, AnoxCommandStackBase &commandStack, const rkit::ISpan<rkit::ByteStringView> &args);
		CORO_DECL_METHOD(Cmd_Map, AnoxCommandStackBase &commandStack, const rkit::ISpan<rkit::ByteStringView> &args);

		IAnoxGame *m_game;
		rkit::UniquePtr<rkit::coro::Thread> m_mainCoroThread;
		rkit::UniquePtr<AnoxCommandStackBase> m_commandStack;

		rkit::UniquePtr<game::GlobalVars> m_globalVars;

		rkit::RCPtr<AnoxBSPModelResourceBase> m_bspModel;
	};

	AnoxGameLogic::AnoxGameLogic(IAnoxGame *game)
		: m_game(game)
	{
	}

	rkit::Result AnoxGameLogic::Start()
	{
		RKIT_CHECK(m_game->GetCommandRegistry()->RegisterCommand(u8"exec", this->AsyncCmd_Exec()));
		RKIT_CHECK(m_game->GetCommandRegistry()->RegisterCommand(u8"map", this->AsyncCmd_Map()));

		RKIT_CHECK(AnoxCommandStackBase::Create(m_commandStack, 64 * 1024, 1024));

		RKIT_CHECK(rkit::GetDrivers().m_utilitiesDriver->CreateCoroThread(m_mainCoroThread, 1 * 1024 * 1024));
		RKIT_CHECK(m_mainCoroThread->EnterFunction(AsyncStartUp()));

		RKIT_RETURN_OK;
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

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxGameLogic::CreateNewGame(rkit::UniquePtr<IConfigurationState> &outConfig, const rkit::StringSliceView &mapName)
	{
		rkit::UniquePtr<game::GlobalVars> globalVars;
		RKIT_CHECK(rkit::New<game::GlobalVars>(globalVars));

		RKIT_CHECK(globalVars->m_mapName.Set(mapName));

		rkit::UniquePtr<IConfigurationState> globalVarsConfig;
		RKIT_CHECK(globalVars->Save(globalVarsConfig));

		outConfig = std::move(globalVarsConfig);

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxGameLogic::SaveGame(rkit::UniquePtr<IConfigurationState> &outConfig)
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	AnoxGameLogic::DestructiveSpanArgParser::DestructiveSpanArgParser()
		: m_count(0)
	{
	}

	void AnoxGameLogic::DestructiveSpanArgParser::Acquire(const rkit::Span<uint8_t> &lineRef)
	{
		rkit::Span<uint8_t> line = lineRef;

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

						m_cmd = rkit::ByteStringView(line.Ptr() + startIndex, i - startIndex);
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

	rkit::ByteStringView AnoxGameLogic::DestructiveSpanArgParser::GetCommand() const
	{
		return m_cmd;
	}

	size_t AnoxGameLogic::DestructiveSpanArgParser::Count() const
	{
		return m_count;
	}

	rkit::ByteStringView AnoxGameLogic::DestructiveSpanArgParser::operator[](size_t index) const
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

						return rkit::ByteStringView(m_line.Ptr() + startIndex, endIndex - startIndex);
					}
				}

				startIndex = endIndex + 1;
			}
		}

		RKIT_ASSERT(false);
		return rkit::ByteStringView();
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
				rkit::ByteStringSliceView slice = alias.m_text.SubString(startPos, endPos - startPos);

				RKIT_CHECK(commandStack.PushBStr(slice));
				endPos = startPos;
			}
		}

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxGameLogic::ApplyConsoleVar(const AnoxRegisteredConsoleVar &consoleVar, const rkit::ISpan<rkit::ByteStringView> &args)
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
			CORO_CALL(self->AsyncExecCommandFile, *self->m_commandStack, rkit::CIPathView(u8"configs/default.cfg"));
			CORO_CHECK(self->m_commandStack->Push(u8"d1"));
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
			rkit::Span<uint8_t> line;
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
			AnoxPrehashedRegistryKeyView cmdView;
			AnoxCommandRegistryBase *cmdRegistry = nullptr;

			const AnoxRegisteredCommand *cmd = nullptr;
			const AnoxRegisteredAlias *alias = nullptr;
			const AnoxRegisteredConsoleVar *consoleVar = nullptr;
		};

		struct Params
		{
			AnoxCommandStackBase &commandStack;
			rkit::Span<uint8_t> line;
		};

		CORO_BEGIN
			locals.parser.Acquire(params.line);

			locals.cmdView = AnoxPrehashedRegistryKeyView(locals.parser.GetCommand());

			locals.cmdRegistry = self->m_game->GetCommandRegistry();

			locals.cmd = locals.cmdRegistry->FindCommand(locals.cmdView);

			CORO_IF(locals.cmd != nullptr)
				CORO_CALL(locals.cmd->AsyncCall, params.commandStack, locals.parser);
				CORO_RETURN;
			CORO_END_IF

			locals.alias = locals.cmdRegistry->FindAlias(locals.cmdView);

			CORO_IF(locals.alias != nullptr)
				CORO_CHECK(InsertAlias(params.commandStack, *locals.alias));
				CORO_RETURN;
			CORO_END_IF

			locals.consoleVar = locals.cmdRegistry->FindConsoleVar(locals.cmdView);

			CORO_IF(locals.consoleVar != nullptr)
				CORO_CHECK(ApplyConsoleVar(*locals.consoleVar, locals.parser));
				CORO_RETURN;
			CORO_END_IF

			rkit::log::ErrorFmt(u8"Unknown console command {}", locals.parser.GetCommand());
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
			const rkit::ISpan<rkit::ByteStringView> &args;
		};

		CORO_BEGIN
			if (params.args.Count() < 1)
			{
				rkit::log::Error(u8"Usage: exec <file>");
				CORO_RETURN;
			}

			CORO_CHECK(locals.path.Set(rkit::CIPathView(u8"configs")));

			rkit::ByteStringView configPathBStr = params.args[0];

			if (!rkit::CharacterEncodingValidator<rkit::CharacterEncoding::kASCII>::ValidateSpan(configPathBStr.ToSpan()))
			{
				rkit::log::Error(u8"exec file was invalid");
				CORO_RETURN;
			}

			rkit::StringView configPathStr = configPathBStr.ToUTF8Unsafe();

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
				rkit::log::Error(u8"Malformed exec file path");
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
			const rkit::ISpan<rkit::ByteStringView> &args;
		};

		CORO_BEGIN
			if (params.args.Count() < 1)
			{
				rkit::log::Error(u8"Usage: map <map name>");
				CORO_RETURN;
			}

			rkit::ByteStringView mapNameStr = params.args[0];

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

				rkit::log::Error(u8"Malformed map file path");
				CORO_RETURN;
			}

			rkit::StringView unicodeStrView(reinterpret_cast<const rkit::Utf8Char_t *>(mapNameStr.GetChars()), mapNameStr.Length());

			CORO_CALL(self->m_game->AsyncRestartGame, unicodeStrView);
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
			AnoxResourceRetrieveResult modelLoadResult;
			rkit::CIPath path;
		};

		struct Params
		{
			rkit::StringSliceView mapName;
		};

		CORO_BEGIN
			self->m_bspModel.Reset();

			{
				rkit::String fullPathStr;
				CORO_CHECK(fullPathStr.Format(u8"ax_bsp/maps/{}.bsp.bspmodel", params.mapName));

				CORO_CHECK(locals.path.Set(fullPathStr));
			}

			rkit::log::LogInfo(u8"GameLogic: Loading map");

			CORO_CALL(self->AsyncLoadCIPathKeyedResource, locals.modelLoadResult, anox::resloaders::kBSPModelResourceTypeCode, locals.path);

			self->m_bspModel = locals.modelLoadResult.m_resourceHandle.StaticCast<AnoxBSPModelResourceBase>();

			rkit::log::LogInfo(u8"GameLogic: Map loaded successfully");
		CORO_END
	};

	CORO_DEF_METHOD(AnoxGameLogic, SpawnMapInitialObjects)
	{
		struct Locals
		{
			AnoxResourceRetrieveResult objectsLoadResult;
			rkit::CIPath path;
		};

		struct Params
		{
			rkit::StringSliceView mapName;
		};

		CORO_BEGIN
			rkit::String fullPathStr;
			CORO_CHECK(fullPathStr.Format(u8"ax_bsp/maps/{}.bsp.objects", params.mapName));

			CORO_CHECK(locals.path.Set(fullPathStr));

			rkit::log::LogInfo(u8"GameLogic: Loading spawn objects");

			CORO_CALL(self->AsyncLoadCIPathKeyedResource, locals.objectsLoadResult, anox::resloaders::kSpawnDefsResourceTypeCode, locals.path);

			rkit::log::LogInfo(u8"GameLogic: Spawning objects");

			int n = 0;
		CORO_END
	};

	CORO_DEF_METHOD(AnoxGameLogic, StartSession)
	{
		struct Locals
		{
			const IConfigurationState *configState = nullptr;
			rkit::StringSliceView mapName;
		};

		struct Params {};

		CORO_BEGIN
			rkit::log::LogInfo(u8"GameLogic: Starting session");

			CORO_CHECK(self->m_game->GetCaptureHarness()->GetConfigurationState(locals.configState));

			IConfigurationValueView root = locals.configState->GetRoot();

			IConfigurationKeyValueTableView kvt;
			CORO_CHECK(root.Get(kvt));

			IConfigurationValueView mapNameValue;
			CORO_CHECK(kvt.GetValueFromKey(u8"mapName", mapNameValue));

			CORO_CHECK(mapNameValue.Get(locals.mapName));

			for (char c : locals.mapName)
			{
				if (c >= 'a' && c <= 'z')
					continue;
				if (c >= '0' && c <= '9')
					continue;

				rkit::log::Error(u8"Invalid map name");
				CORO_CHECK(rkit::ResultCode::kDataError);
			}

			CORO_CALL(self->AsyncLoadMap, locals.mapName);
			CORO_CALL(self->AsyncSpawnMapInitialObjects, locals.mapName);
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
