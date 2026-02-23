#include "AnoxGameLogic.h"

#include "anox/AnoxGame.h"

#include "rkit/Data/ContentID.h"

#include "rkit/Core/Coroutine2.h"
#include "rkit/Core/CoroThread.h"
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

		rkit::ResultCoroutine StartUp(rkit::ICoroThread &thread);
		rkit::ResultCoroutine AsyncRunFrame(rkit::ICoroThread &thread);
		rkit::ResultCoroutine LoadMap(rkit::ICoroThread &thread, const rkit::StringSliceView &mapName);
		rkit::ResultCoroutine SpawnMapInitialObjects(rkit::ICoroThread &thread, const rkit::StringSliceView &mapName);
		rkit::ResultCoroutine LoadContentIDKeyedResource(rkit::ICoroThread &thread, AnoxResourceRetrieveResult &loadResult, uint32_t resourceType, const rkit::data::ContentID &cid);
		rkit::ResultCoroutine LoadCIPathKeyedResource(rkit::ICoroThread &thread, AnoxResourceRetrieveResult &loadResult, uint32_t resourceType, const rkit::CIPathView &path);
		rkit::ResultCoroutine LoadStringKeyedResource(rkit::ICoroThread &thread, AnoxResourceRetrieveResult &loadResult, uint32_t resourceType, const rkit::StringView &str);
		rkit::ResultCoroutine ExecCommandFile(rkit::ICoroThread &thread, AnoxCommandStackBase &commandStack, rkit::CIPathView path);
		rkit::ResultCoroutine RunCommands(rkit::ICoroThread &thread, AnoxCommandStackBase &commandStack);
		rkit::ResultCoroutine RunCommand(rkit::ICoroThread &thread, AnoxCommandStackBase &commandStack, const rkit::Span<uint8_t> &line);
		rkit::ResultCoroutine StartSession(rkit::ICoroThread &thread) override;

		rkit::ResultCoroutine Cmd_Exec(rkit::ICoroThread &thread, AnoxCommandStackBase &commandStack, const rkit::ISpan<rkit::ByteStringView> &args);
		rkit::ResultCoroutine Cmd_Map(rkit::ICoroThread &thread, AnoxCommandStackBase &commandStack, const rkit::ISpan<rkit::ByteStringView> &args);

		IAnoxGame *m_game;
		rkit::UniquePtr<rkit::ICoroThread> m_mainCoroThread;
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
		RKIT_CHECK(m_game->GetCommandRegistry()->RegisterMemberFuncCommand<&AnoxGameLogic::Cmd_Exec>(u8"exec", this));
		RKIT_CHECK(m_game->GetCommandRegistry()->RegisterMemberFuncCommand<&AnoxGameLogic::Cmd_Map>(u8"map", this));

		RKIT_CHECK(AnoxCommandStackBase::Create(m_commandStack, 64 * 1024, 1024));

		RKIT_CHECK(rkit::GetDrivers().m_utilitiesDriver->CreateCoro2Thread(m_mainCoroThread, 1 * 1024 * 1024));
		RKIT_CHECK(m_mainCoroThread->EnterFunction(StartUp(*m_mainCoroThread)));

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
			case rkit::CoroThreadState::kInactive:
				if (!haveKickedOffRunFrame)
				{
					RKIT_CHECK(m_mainCoroThread->EnterFunction(AsyncRunFrame(*m_mainCoroThread)));
					haveMainThreadWork = true;

					haveKickedOffRunFrame = true;
				}
				break;
			case rkit::CoroThreadState::kSuspended:
				RKIT_CHECK(m_mainCoroThread->Resume());
				haveMainThreadWork = true;
				break;
			case rkit::CoroThreadState::kBlocked:
				if (m_mainCoroThread->TryUnblock())
					haveMainThreadWork = true;
				break;
			default:
				RKIT_THROW(rkit::ResultCode::kInternalError);
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
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
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
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	rkit::ResultCoroutine AnoxGameLogic::StartUp(rkit::ICoroThread &thread)
	{
		CORO2_CHECK(co_await ExecCommandFile(thread, *m_commandStack, rkit::CIPathView(u8"configs/default.cfg")));
		CORO2_CHECK(m_commandStack->Push(u8"d1"));
		CORO2_CHECK(co_await RunCommands(thread, *m_commandStack));

		CORO2_RETURN_OK;
	}

	rkit::ResultCoroutine AnoxGameLogic::ExecCommandFile(rkit::ICoroThread &thread, AnoxCommandStackBase &commandStack, rkit::CIPathView path)
	{
		AnoxResourceRetrieveResult resLoadResult;

		CORO2_CHECK(co_await LoadCIPathKeyedResource(thread, resLoadResult, anox::resloaders::kRawFileResourceTypeCode, path));

		CORO2_CHECK(commandStack.Parse(resLoadResult.m_resourceHandle.StaticCast<AnoxFileResourceBase>()->GetContents()));
		CORO2_CHECK(co_await RunCommands(thread, commandStack));

		CORO2_RETURN_OK;
	}


	rkit::ResultCoroutine AnoxGameLogic::RunCommands(rkit::ICoroThread &thread, AnoxCommandStackBase &commandStack)
	{
		rkit::Span<uint8_t> line;

		while (commandStack.Pop(line))
		{
			CORO2_CHECK(co_await RunCommand(thread, commandStack, line));
		}

		CORO2_RETURN_OK;
	}


	rkit::ResultCoroutine AnoxGameLogic::RunCommand(rkit::ICoroThread &thread, AnoxCommandStackBase &commandStack, const rkit::Span<uint8_t> &line)
	{
		DestructiveSpanArgParser parser;
		AnoxPrehashedRegistryKeyView cmdView;
		AnoxCommandRegistryBase *cmdRegistry = nullptr;

		parser.Acquire(line);

		cmdView = AnoxPrehashedRegistryKeyView(parser.GetCommand());

		cmdRegistry = m_game->GetCommandRegistry();

		const AnoxRegisteredCommand *cmd = cmdRegistry->FindCommand(cmdView);

		if (cmd != nullptr)
		{
			CORO2_CHECK(co_await cmd->m_methodStarter(cmd->m_obj, thread, commandStack, parser));
			CORO2_RETURN_OK;
		}

		const AnoxRegisteredAlias *alias = cmdRegistry->FindAlias(cmdView);

		if (alias != nullptr)
		{
			CORO2_CHECK(InsertAlias(commandStack, *alias));
			CORO2_RETURN_OK;
		}

		const AnoxRegisteredConsoleVar *consoleVar = cmdRegistry->FindConsoleVar(cmdView);

		if (consoleVar != nullptr)
		{
			CORO2_CHECK(ApplyConsoleVar(*consoleVar, parser));
			CORO2_RETURN_OK;
		}

		rkit::log::ErrorFmt(u8"Unknown console command {}", parser.GetCommand());

		CORO2_RETURN_OK;
	};

	rkit::ResultCoroutine AnoxGameLogic::Cmd_Exec(rkit::ICoroThread &thread, AnoxCommandStackBase &cmdStack, const rkit::ISpan<rkit::ByteStringView> &args)
	{
		if (args.Count() < 1)
		{
			rkit::log::Error(u8"Usage: exec <file>");
			CORO2_RETURN_OK;
		}

		rkit::CIPath path;
		CORO2_CHECK(path.Set(rkit::CIPathView(u8"configs")));

		rkit::ByteStringView configPathBStr = args[0];

		if (!rkit::CharacterEncodingValidator<rkit::CharacterEncoding::kASCII>::ValidateSpan(configPathBStr.ToSpan()))
		{
			rkit::log::Error(u8"exec file was invalid");
			CORO2_RETURN_OK;
		}

		rkit::StringView configPathStr = configPathBStr.ToUTF8Unsafe();

		rkit::PathValidationResult validationResult = rkit::CIPath::Validate(configPathStr);
		rkit::CIPathView configRelPath;

		rkit::CIPath relPath;
		if (validationResult == rkit::PathValidationResult::kValid)
			configRelPath = rkit::CIPathView(configPathStr);
		else if (validationResult == rkit::PathValidationResult::kConvertible)
		{
			CORO2_CHECK(relPath.Set(configPathStr));
			configRelPath = relPath;
		}
		else
		{
			rkit::log::Error(u8"Malformed exec file path");
			CORO2_RETURN_OK;
		}

		CORO2_CHECK(path.Append(configRelPath));

		CORO2_CHECK(co_await ExecCommandFile(thread, cmdStack, path));

		CORO2_RETURN_OK;
	}

	rkit::ResultCoroutine AnoxGameLogic::Cmd_Map(rkit::ICoroThread &thread, AnoxCommandStackBase &cmdStack, const rkit::ISpan<rkit::ByteStringView> &args)
	{
		if (args.Count() < 1)
		{
			rkit::log::Error(u8"Usage: map <map name>");
			CORO2_RETURN_OK;
		}

		rkit::ByteStringView mapNameStr = args[0];

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
			CORO2_RETURN_OK;
		}

		rkit::StringView unicodeStrView(reinterpret_cast<const rkit::Utf8Char_t *>(mapNameStr.GetChars()), mapNameStr.Length());

		CORO2_CHECK(co_await m_game->RestartGame(thread,  unicodeStrView));

		CORO2_RETURN_OK;
	}

	rkit::ResultCoroutine AnoxGameLogic::LoadCIPathKeyedResource(rkit::ICoroThread &thread, AnoxResourceRetrieveResult &loadResult,
		uint32_t resourceType, const rkit::CIPathView &path)
	{
		rkit::Future<AnoxResourceRetrieveResult> resLoadResult;

		CORO2_CHECK(m_game->GetCaptureHarness()->GetCIPathKeyedResource(resLoadResult, resourceType, path));
		CORO2_CHECK(co_await thread.AwaitFuture(resLoadResult));

		loadResult = resLoadResult.GetResult();

		CORO2_RETURN_OK;
	}

	rkit::ResultCoroutine AnoxGameLogic::LoadStringKeyedResource(rkit::ICoroThread &thread, AnoxResourceRetrieveResult &loadResult,
		uint32_t resourceType, const rkit::StringView &str)
	{
		rkit::Future<AnoxResourceRetrieveResult> resLoadResult;
		CORO2_CHECK(m_game->GetCaptureHarness()->GetStringKeyedResource(resLoadResult, resourceType, str));
		CORO2_CHECK(co_await thread.AwaitFuture(resLoadResult));

		loadResult = resLoadResult.GetResult();
		CORO2_RETURN_OK;
	}

	rkit::ResultCoroutine AnoxGameLogic::LoadContentIDKeyedResource(rkit::ICoroThread &thread, AnoxResourceRetrieveResult &loadResult,
		uint32_t resourceType, const rkit::data::ContentID &cid)
	{
		rkit::Future<AnoxResourceRetrieveResult> resLoadResult;

		CORO2_CHECK(m_game->GetCaptureHarness()->GetContentIDKeyedResource(resLoadResult, resourceType, cid));
		CORO2_CHECK(co_await thread.AwaitFuture(resLoadResult));

		loadResult = resLoadResult.GetResult();
		CORO2_RETURN_OK;
	}

	rkit::ResultCoroutine AnoxGameLogic::LoadMap(rkit::ICoroThread &thread, const rkit::StringSliceView &mapName)
	{
		AnoxResourceRetrieveResult modelLoadResult;
		rkit::CIPath path;

		m_bspModel.Reset();

		{
			rkit::String fullPathStr;
			CORO2_CHECK(fullPathStr.Format(u8"ax_bsp/maps/{}.bsp.bspmodel", mapName));

			CORO2_CHECK(path.Set(fullPathStr));
		}

		rkit::log::LogInfo(u8"GameLogic: Loading map");

		CORO2_CHECK(co_await LoadCIPathKeyedResource(thread, modelLoadResult, anox::resloaders::kBSPModelResourceTypeCode, path));

		m_bspModel = modelLoadResult.m_resourceHandle.StaticCast<AnoxBSPModelResourceBase>();

		rkit::log::LogInfo(u8"GameLogic: Map loaded successfully");

		CORO2_RETURN_OK;
	}

	rkit::ResultCoroutine AnoxGameLogic::SpawnMapInitialObjects(rkit::ICoroThread &thread, const rkit::StringSliceView &mapName)
	{
		AnoxResourceRetrieveResult objectsLoadResult;
		rkit::CIPath path;

		rkit::String fullPathStr;
		CORO2_CHECK(fullPathStr.Format(u8"ax_bsp/maps/{}.bsp.objects", mapName));

		CORO2_CHECK(path.Set(fullPathStr));

		rkit::log::LogInfo(u8"GameLogic: Loading spawn objects");

		CORO2_CHECK(co_await LoadCIPathKeyedResource(thread, objectsLoadResult, anox::resloaders::kSpawnDefsResourceTypeCode, path));

		rkit::log::LogInfo(u8"GameLogic: Spawning objects");

		int n = 0;
		CORO2_RETURN_OK;
	}

	rkit::ResultCoroutine AnoxGameLogic::StartSession(rkit::ICoroThread &thread)
	{
		const IConfigurationState *configState = nullptr;

		rkit::log::LogInfo(u8"GameLogic: Starting session");

		CORO2_CHECK(m_game->GetCaptureHarness()->GetConfigurationState(configState));

		IConfigurationValueView root = configState->GetRoot();

		IConfigurationKeyValueTableView kvt;
		CORO2_CHECK(root.Get(kvt));

		IConfigurationValueView mapNameValue;
		CORO2_CHECK(kvt.GetValueFromKey(u8"mapName", mapNameValue));

		rkit::StringSliceView mapName;
		CORO2_CHECK(mapNameValue.Get(mapName));

		for (char c : mapName)
		{
			if (c >= 'a' && c <= 'z')
				continue;
			if (c >= '0' && c <= '9')
				continue;

			rkit::log::Error(u8"Invalid map name");
			CORO2_THROW(rkit::ResultCode::kDataError);
		}

		CORO2_CHECK(co_await LoadMap(thread, mapName));
		CORO2_CHECK(co_await SpawnMapInitialObjects(thread, mapName));

		CORO2_RETURN_OK;
	}

	rkit::ResultCoroutine AnoxGameLogic::AsyncRunFrame(rkit::ICoroThread &thread)
	{
		CORO2_RETURN_OK;
	}

	rkit::Result IGameLogic::Create(rkit::UniquePtr<IGameLogic> &outGameLoop, IAnoxGame *game)
	{
		return rkit::New<AnoxGameLogic>(outGameLoop, game);
	}
}
