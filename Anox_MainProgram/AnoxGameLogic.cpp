#include "AnoxGameLogic.h"

#include "anox/AnoxGame.h"

#include "rkit/Data/ContentID.h"

#include "rkit/Core/Coroutine.h"
#include "rkit/Core/CoroThread.h"
#include "rkit/Core/Future.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Pair.h"
#include "rkit/Core/Path.h"
#include "rkit/Core/String.h"
#include "rkit/Core/UtilitiesDriver.h"

#include "rkit/Sandbox/Sandbox.h"
#include "rkit/Sandbox/ThreadCreationParameters.h"

#include "AnoxBSPModelResource.h"
#include "AnoxCaptureHarness.h"
#include "AnoxCommandRegistry.h"
#include "AnoxCommandStack.h"
#include "AnoxConfigurationSaver.h"
#include "AnoxEntityDefResource.h"
#include "AnoxFileResource.h"
#include "AnoxGameSandboxEnv.h"
#include "AnoxResourceManager.h"
#include "AnoxSpawnDefsResource.h"
#include "AnoxGlobalVars.h"
#include "AnoxGameResourceManager.h"

#include "anox/AnoxModule.h"

#include "anox/Game/UserEntityDefValues.h"
#include "anox/Data/EntitySpawnData.h"
#include "anox/Data/ResourceTypeCodes.h"
#include "anox/Sandbox/AnoxGame.host.generated.h"

namespace anox
{
	class AnoxGameSandboxInterface;
	class AnoxCommandStackBase;
	class AnoxBSPModelResourceBase;
	class AnoxGameLogic;

	class AnoxGameLogic final : public IGameLogic
	{
	public:
		AnoxGameLogic(IAnoxGame *game);

		rkit::Result Start() override;
		rkit::Result RunFrame() override;

		rkit::Result CreateNewGame(rkit::UniquePtr<IConfigurationState> &outConfig, const rkit::StringSliceView &mapName) override;
		rkit::Result SaveGame(rkit::UniquePtr<IConfigurationState> &outConfig) override;

	private:
		struct SandboxMemObject
		{
			rkit::sandbox::Address_t m_addr = 0;
			uint32_t m_mmid = 0;
			size_t m_size = 0;
		};

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

		class SandboxMainThreadBlocker
		{
		public:
			explicit SandboxMainThreadBlocker(AnoxGameLogic *gameLogic);

			SandboxMainThreadBlocker() = delete;
			SandboxMainThreadBlocker(const SandboxMainThreadBlocker &) = delete;
			SandboxMainThreadBlocker &operator=(const SandboxMainThreadBlocker &) = delete;

			rkit::CoroThreadBlocker CreateBlocker();

		private:
			rkit::Optional<rkit::PackedResultAndExtCode> m_result;
			AnoxGameLogic *m_gameLogic = nullptr;

			static bool StaticCheckUnblock(void *selfPtr);
			static rkit::Result StaticConsume(void *selfPtr);
			static void StaticRelease(void *selfPtr);
		};

		static rkit::Result InsertAlias(AnoxCommandStackBase &commandStack, const AnoxRegisteredAlias &alias);
		static rkit::Result ApplyConsoleVar(const AnoxRegisteredConsoleVar &consoleVar, const rkit::ISpan<rkit::ByteStringView> &args);

		rkit::ResultCoroutine StartUp(rkit::ICoroThread &thread);
		rkit::ResultCoroutine AsyncRunFrame(rkit::ICoroThread &thread);
		rkit::ResultCoroutine LoadGlobalScripts(rkit::ICoroThread &thread);
		rkit::ResultCoroutine LoadMap(rkit::ICoroThread &thread, const rkit::StringSliceView &mapName);
		rkit::ResultCoroutine LoadMapScripts(rkit::ICoroThread &thread, const rkit::StringSliceView &mapName);
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

		rkit::UniquePtr<game::GameResourceManager> m_resManager;
		rkit::UniquePtr<game::GlobalVars> m_globalVars;

		rkit::UniquePtr<rkit::ISandbox> m_sandbox;
		rkit::UniquePtr<rkit::sandbox::IThreadContext> m_sandboxMainThreadContext;
		anox::game::sandbox::HostImports m_sandboxImports;
		game::AnoxGameSandboxEnvironment m_sandboxEnv;

		rkit::RCPtr<AnoxBSPModelResourceBase> m_bspModel;

		template<class T>
		rkit::Result CopySpanToSandbox(SandboxMemObject &outMemObject, const rkit::Span<T> &span);

		rkit::Result CopyDataToSandbox(SandboxMemObject &outMemObject, const void *ptr, size_t size);
	};

	AnoxGameLogic::SandboxMainThreadBlocker::SandboxMainThreadBlocker(AnoxGameLogic *gameLogic)
		: m_gameLogic(gameLogic)
	{
	}

	rkit::CoroThreadBlocker AnoxGameLogic::SandboxMainThreadBlocker::CreateBlocker()
	{
		return rkit::CoroThreadBlocker::Create(this, StaticCheckUnblock, StaticConsume, StaticRelease, false);
	}

	bool AnoxGameLogic::SandboxMainThreadBlocker::StaticCheckUnblock(void *selfPtr)
	{
		SandboxMainThreadBlocker *self = static_cast<SandboxMainThreadBlocker *>(selfPtr);
		AnoxGameLogic *gameLogic = self->m_gameLogic;

		bool isFinished = false;
		rkit::PackedResultAndExtCode result = RKIT_TRY_EVAL(gameLogic->m_sandboxImports.WaitForMainThread(gameLogic->m_sandboxMainThreadContext.Get(), isFinished, gameLogic->m_sandboxEnv.m_gameSessionObjAddr));
		if (isFinished || !rkit::utils::ResultIsOK(result))
		{
			self->m_result = result;
			return true;
		}
		else
			return false;
	}

	rkit::Result AnoxGameLogic::SandboxMainThreadBlocker::StaticConsume(void *selfPtr)
	{
		SandboxMainThreadBlocker *self = static_cast<SandboxMainThreadBlocker *>(selfPtr);
		RKIT_CHECK(rkit::ThrowIfError(self->m_result.Get()));
		RKIT_RETURN_OK;
	}

	void AnoxGameLogic::SandboxMainThreadBlocker::StaticRelease(void *selfPtr)
	{
	}

	AnoxGameLogic::AnoxGameLogic(IAnoxGame *game)
		: m_game(game)
	{
	}

	rkit::Result AnoxGameLogic::Start()
	{
		RKIT_CHECK(m_game->GetCommandRegistry()->RegisterMemberFuncCommand<&AnoxGameLogic::Cmd_Exec>(u8"exec", this));
		RKIT_CHECK(m_game->GetCommandRegistry()->RegisterMemberFuncCommand<&AnoxGameLogic::Cmd_Map>(u8"map", this));

		RKIT_CHECK(AnoxCommandStackBase::Create(m_commandStack, 64 * 1024, 1024));

		RKIT_CHECK(rkit::utils::CreateCoroThread(m_mainCoroThread, rkit::GetDrivers().m_mallocDriver, 1 * 1024 * 1024, rkit::GetDrivers().GetAssertDriver()));
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
		CORO_CHECK(co_await ExecCommandFile(thread, *m_commandStack, rkit::CIPathView(u8"configs/default.cfg")));
		CORO_CHECK(m_commandStack->Push(u8"d1"));
		CORO_CHECK(co_await RunCommands(thread, *m_commandStack));

		CORO_RETURN_OK;
	}

	rkit::ResultCoroutine AnoxGameLogic::ExecCommandFile(rkit::ICoroThread &thread, AnoxCommandStackBase &commandStack, rkit::CIPathView path)
	{
		AnoxResourceRetrieveResult resLoadResult;

		rkit::CIPath loosePath;
		RKIT_CHECK(loosePath.AppendComponent(u8"loose"));
		RKIT_CHECK(loosePath.Append(path));

		CORO_CHECK(co_await LoadCIPathKeyedResource(thread, resLoadResult, anox::resloaders::kCIPathRawFileResourceTypeCode, loosePath));

		CORO_CHECK(commandStack.Parse(resLoadResult.m_resourceHandle.StaticCast<AnoxFileResourceBase>()->GetContents()));
		CORO_CHECK(co_await RunCommands(thread, commandStack));

		CORO_RETURN_OK;
	}


	rkit::ResultCoroutine AnoxGameLogic::RunCommands(rkit::ICoroThread &thread, AnoxCommandStackBase &commandStack)
	{
		rkit::Span<uint8_t> line;

		while (commandStack.Pop(line))
		{
			CORO_CHECK(co_await RunCommand(thread, commandStack, line));
		}

		CORO_RETURN_OK;
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
			CORO_CHECK(co_await cmd->m_methodStarter(cmd->m_obj, thread, commandStack, parser));
			CORO_RETURN_OK;
		}

		const AnoxRegisteredAlias *alias = cmdRegistry->FindAlias(cmdView);

		if (alias != nullptr)
		{
			CORO_CHECK(InsertAlias(commandStack, *alias));
			CORO_RETURN_OK;
		}

		const AnoxRegisteredConsoleVar *consoleVar = cmdRegistry->FindConsoleVar(cmdView);

		if (consoleVar != nullptr)
		{
			CORO_CHECK(ApplyConsoleVar(*consoleVar, parser));
			CORO_RETURN_OK;
		}

		rkit::log::ErrorFmt(u8"Unknown console command {}", parser.GetCommand());

		CORO_RETURN_OK;
	};

	rkit::ResultCoroutine AnoxGameLogic::Cmd_Exec(rkit::ICoroThread &thread, AnoxCommandStackBase &cmdStack, const rkit::ISpan<rkit::ByteStringView> &args)
	{
		if (args.Count() < 1)
		{
			rkit::log::Error(u8"Usage: exec <file>");
			CORO_RETURN_OK;
		}

		rkit::CIPath path;
		CORO_CHECK(path.Set(rkit::CIPathView(u8"configs")));

		rkit::ByteStringView configPathBStr = args[0];

		if (!rkit::CharacterEncodingValidator<rkit::CharacterEncoding::kASCII>::ValidateSpan(configPathBStr.ToSpan()))
		{
			rkit::log::Error(u8"exec file was invalid");
			CORO_RETURN_OK;
		}

		rkit::StringView configPathStr = configPathBStr.ToUTF8Unsafe();

		rkit::PathValidationResult validationResult = rkit::CIPath::Validate(configPathStr);
		rkit::CIPathView configRelPath;

		rkit::CIPath relPath;
		if (validationResult == rkit::PathValidationResult::kValid)
			configRelPath = rkit::CIPathView(configPathStr);
		else if (validationResult == rkit::PathValidationResult::kConvertible)
		{
			CORO_CHECK(relPath.Set(configPathStr));
			configRelPath = relPath;
		}
		else
		{
			rkit::log::Error(u8"Malformed exec file path");
			CORO_RETURN_OK;
		}

		CORO_CHECK(path.Append(configRelPath));

		CORO_CHECK(co_await ExecCommandFile(thread, cmdStack, path));

		CORO_RETURN_OK;
	}

	rkit::ResultCoroutine AnoxGameLogic::Cmd_Map(rkit::ICoroThread &thread, AnoxCommandStackBase &cmdStack, const rkit::ISpan<rkit::ByteStringView> &args)
	{
		if (args.Count() < 1)
		{
			rkit::log::Error(u8"Usage: map <map name>");
			CORO_RETURN_OK;
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
			CORO_RETURN_OK;
		}

		rkit::StringView unicodeStrView(reinterpret_cast<const rkit::Utf8Char_t *>(mapNameStr.GetChars()), mapNameStr.Length());

		CORO_CHECK(co_await m_game->RestartGame(thread,  unicodeStrView));

		CORO_RETURN_OK;
	}

	rkit::ResultCoroutine AnoxGameLogic::LoadCIPathKeyedResource(rkit::ICoroThread &thread, AnoxResourceRetrieveResult &loadResult,
		uint32_t resourceType, const rkit::CIPathView &path)
	{
		rkit::Future<AnoxResourceRetrieveResult> resLoadResult;

		CORO_CHECK(m_game->GetCaptureHarness()->GetCIPathKeyedResource(resLoadResult, resourceType, path));
		CORO_CHECK(co_await thread.AwaitFuture(resLoadResult));

		loadResult = resLoadResult.GetResult();

		CORO_RETURN_OK;
	}

	rkit::ResultCoroutine AnoxGameLogic::LoadStringKeyedResource(rkit::ICoroThread &thread, AnoxResourceRetrieveResult &loadResult,
		uint32_t resourceType, const rkit::StringView &str)
	{
		rkit::Future<AnoxResourceRetrieveResult> resLoadResult;
		CORO_CHECK(m_game->GetCaptureHarness()->GetStringKeyedResource(resLoadResult, resourceType, str));
		CORO_CHECK(co_await thread.AwaitFuture(resLoadResult));

		loadResult = resLoadResult.GetResult();
		CORO_RETURN_OK;
	}

	rkit::ResultCoroutine AnoxGameLogic::LoadContentIDKeyedResource(rkit::ICoroThread &thread, AnoxResourceRetrieveResult &loadResult,
		uint32_t resourceType, const rkit::data::ContentID &cid)
	{
		rkit::Future<AnoxResourceRetrieveResult> resLoadResult;

		CORO_CHECK(m_game->GetCaptureHarness()->GetContentIDKeyedResource(resLoadResult, resourceType, cid));
		CORO_CHECK(co_await thread.AwaitFuture(resLoadResult));

		loadResult = resLoadResult.GetResult();
		CORO_RETURN_OK;
	}

	rkit::ResultCoroutine AnoxGameLogic::LoadGlobalScripts(rkit::ICoroThread &thread)
	{
		CORO_CHECK(m_sandboxImports.MTAsync_StartGlobalSession(
			m_sandboxMainThreadContext.Get(), m_sandboxEnv.m_gameSessionObjAddr));

		{
			SandboxMainThreadBlocker mtBlocker(this);
			CORO_CHECK(co_await thread.AwaitBlocker(mtBlocker.CreateBlocker()));
		}

		CORO_RETURN_OK;
	}

	rkit::ResultCoroutine AnoxGameLogic::LoadMap(rkit::ICoroThread &thread, const rkit::StringSliceView &mapName)
	{
		AnoxResourceRetrieveResult modelLoadResult;
		rkit::CIPath path;

		m_bspModel.Reset();

		{
			rkit::String fullPathStr;
			CORO_CHECK(fullPathStr.Format(u8"ax_bsp/maps/{}.bsp.bspmodel", mapName));

			CORO_CHECK(path.Set(fullPathStr));
		}

		rkit::log::LogInfo(u8"GameLogic: Loading map");

		CORO_CHECK(co_await LoadCIPathKeyedResource(thread, modelLoadResult, anox::resloaders::kBSPModelResourceTypeCode, path));

		m_bspModel = modelLoadResult.m_resourceHandle.StaticCast<AnoxBSPModelResourceBase>();

		rkit::log::LogInfo(u8"GameLogic: Map loaded successfully");

		CORO_RETURN_OK;
	}

	rkit::ResultCoroutine AnoxGameLogic::LoadMapScripts(rkit::ICoroThread &thread, const rkit::StringSliceView &mapName)
	{
		AnoxResourceRetrieveResult scriptLoadResult;
		rkit::CIPath path;

		{
			rkit::String fullPathStr;
			CORO_CHECK(fullPathStr.Format(u8"ax_bsp/maps/{}.bsp.scripts", mapName));

			CORO_CHECK(path.Set(fullPathStr));
		}

		rkit::log::LogInfo(u8"GameLogic: Loading script package");

		CORO_CHECK(co_await LoadCIPathKeyedResource(thread, scriptLoadResult, anox::resloaders::kCIPathRawFileResourceTypeCode, path));

		rkit::RCPtr<AnoxFileResourceBase> fileResource = scriptLoadResult.m_resourceHandle.StaticCast<AnoxFileResourceBase>();


		SandboxMemObject scriptPackageMO = {};
		CORO_CHECK(CopySpanToSandbox(scriptPackageMO, fileResource->GetContents()));

		CORO_CHECK(m_sandboxImports.MTAsync_LoadMapScriptPackage(
			m_sandboxMainThreadContext.Get(), m_sandboxEnv.m_gameSessionObjAddr,
			scriptPackageMO.m_addr, scriptPackageMO.m_size));

		{
			SandboxMainThreadBlocker mtBlocker(this);
			CORO_CHECK(co_await thread.AwaitBlocker(mtBlocker.CreateBlocker()));
		}

		CORO_CHECK(m_sandbox->ReleaseDynamicMemory(scriptPackageMO.m_mmid));

		rkit::log::LogInfo(u8"GameLogic: Script package loaded successfully");

		CORO_RETURN_OK;
	}

	rkit::ResultCoroutine AnoxGameLogic::SpawnMapInitialObjects(rkit::ICoroThread &thread, const rkit::StringSliceView &mapName)
	{
		AnoxResourceRetrieveResult objectsLoadResult;
		rkit::CIPath path;

		rkit::String fullPathStr;
		CORO_CHECK(fullPathStr.Format(u8"ax_bsp/maps/{}.bsp.objects", mapName));

		CORO_CHECK(path.Set(fullPathStr));

		rkit::log::LogInfo(u8"GameLogic: Loading spawn objects");

		CORO_CHECK(co_await LoadCIPathKeyedResource(thread, objectsLoadResult, anox::resloaders::kSpawnDefsResourceTypeCode, path));

		rkit::log::LogInfo(u8"GameLogic: Spawning objects");

		m_sandboxEnv.m_spawnDefs = objectsLoadResult.m_resourceHandle.StaticCastMove<AnoxSpawnDefsResourceBase>();

		const AnoxSpawnDefsResourceBase *spawnDefs = m_sandboxEnv.m_spawnDefs.Get();

		SandboxMemObject entityTypesMO = {};
		SandboxMemObject spawnDataMO = {};
		SandboxMemObject stringLengthsMO = {};
		SandboxMemObject stringDataMO = {};
		SandboxMemObject entityDefValuesMO = {};
		SandboxMemObject udefDescLengthsMO = {};
		SandboxMemObject udefDescBytesMO = {};

		rkit::Vector<game::UserEntityDefValues> udefValues;
		rkit::Vector<uint8_t> udefDescBytes;

		{
			const rkit::CallbackSpan<AnoxEntityDefResourceBase *, const AnoxSpawnDefsResourceBase *> inUserEntityDefsSpan = spawnDefs->GetUserEntityDefs();

			CORO_CHECK(udefValues.Reserve(inUserEntityDefsSpan.Count()));

			for (AnoxEntityDefResourceBase *edef : inUserEntityDefsSpan)
			{
				CORO_CHECK(udefValues.Append(edef->GetValues()));

				const rkit::ByteString &desc = edef->GetDescription();
				const size_t descLength = desc.Length();
				if (descLength > std::numeric_limits<uint32_t>::max())
					CORO_THROW(rkit::ResultCode::kIntegerOverflow);

				CORO_CHECK(udefDescBytes.Append(desc.ToSpan()));
			}
		}

		const data::EntitySpawnDataChunks &chunks = spawnDefs->GetChunks();
		CORO_CHECK(CopySpanToSandbox(entityTypesMO, chunks.m_entityTypes.ToSpan()));
		CORO_CHECK(CopySpanToSandbox(spawnDataMO, chunks.m_entityData.ToSpan()));
		CORO_CHECK(CopySpanToSandbox(stringLengthsMO, chunks.m_entityStringLengths.ToSpan()));
		CORO_CHECK(CopySpanToSandbox(stringDataMO, chunks.m_entityStringData.ToSpan()));

		CORO_CHECK(CopySpanToSandbox(entityDefValuesMO, udefValues.ToSpan()));
		CORO_CHECK(CopySpanToSandbox(udefDescBytesMO, udefDescBytes.ToSpan()));

		CORO_CHECK(m_sandboxImports.MTAsync_SpawnInitialEntities(
			m_sandboxMainThreadContext.Get(), m_sandboxEnv.m_gameSessionObjAddr,
			entityTypesMO.m_addr, entityTypesMO.m_size / sizeof(uint32_t),
			spawnDataMO.m_addr, spawnDataMO.m_size / sizeof(uint8_t),
			stringLengthsMO.m_addr, stringLengthsMO.m_size / sizeof(uint32_t),
			stringDataMO.m_addr, stringDataMO.m_size / sizeof(uint8_t),
			entityDefValuesMO.m_addr, entityDefValuesMO.m_size / sizeof(game::UserEntityDefValues),
			udefDescBytesMO.m_addr, udefDescBytesMO.m_size / sizeof(uint8_t)
		));

		{
			SandboxMainThreadBlocker mtBlocker(this);
			CORO_CHECK(co_await thread.AwaitBlocker(mtBlocker.CreateBlocker()));
		}

		CORO_CHECK(m_sandbox->ReleaseDynamicMemory(entityTypesMO.m_mmid));
		CORO_CHECK(m_sandbox->ReleaseDynamicMemory(spawnDataMO.m_mmid));
		CORO_CHECK(m_sandbox->ReleaseDynamicMemory(stringLengthsMO.m_mmid));
		CORO_CHECK(m_sandbox->ReleaseDynamicMemory(stringDataMO.m_mmid));
		CORO_CHECK(m_sandbox->ReleaseDynamicMemory(entityDefValuesMO.m_mmid));
		CORO_CHECK(m_sandbox->ReleaseDynamicMemory(udefDescLengthsMO.m_mmid));
		CORO_CHECK(m_sandbox->ReleaseDynamicMemory(udefDescBytesMO.m_mmid));

		CORO_CHECK(m_sandboxImports.MTAsync_PostSpawnInitialEntities(m_sandboxMainThreadContext.Get(), m_sandboxEnv.m_gameSessionObjAddr));

		{
			SandboxMainThreadBlocker mtBlocker(this);
			CORO_CHECK(co_await thread.AwaitBlocker(mtBlocker.CreateBlocker()));
		}

		CORO_RETURN_OK;
	}

	rkit::ResultCoroutine AnoxGameLogic::StartSession(rkit::ICoroThread &thread)
	{
		const IConfigurationState *configState = nullptr;

		m_resManager.Reset();

		RKIT_CHECK(game::GameResourceManager::Create(m_resManager));
		m_resManager->SetCaptureHarness(m_game->GetCaptureHarness());

		RKIT_ASSERT(!m_sandbox.IsValid());

		{
			rkit::UniquePtr<rkit::ISandbox> sandbox;
			rkit::UniquePtr<rkit::sandbox::IThreadContext> mainThreadContext;

			rkit::log::LogInfo(u8"Loading game module");

			CORO_CHECK(rkit::GetDrivers().m_utilitiesDriver->CreateModuleSandbox(sandbox, kAnoxNamespaceID, u8"Game", m_sandboxImports.GetHostAPIDescriptor().m_sysCallCatalog, m_sandboxEnv));

			m_sandboxEnv.m_sandbox = sandbox.Get();
			m_sandboxEnv.m_resManager = m_resManager.Get();

			CORO_CHECK(rkit::GetDrivers().m_utilitiesDriver->LinkSandbox(*sandbox, m_sandboxImports.GetHostAPIDescriptor()));

			rkit::sandbox::ThreadCreationParameters threadParams = {};
			CORO_CHECK(sandbox->CreateThreadContext(mainThreadContext, threadParams));

			CORO_CHECK(sandbox->RunInitializer(*mainThreadContext));

			CORO_CHECK(m_sandboxImports.Initialize(mainThreadContext.Get(), m_sandboxEnv.m_gameSessionObjAddr, m_sandboxEnv.m_gameSessionMemAddr));

			m_sandbox = std::move(sandbox);
			m_sandboxMainThreadContext = std::move(mainThreadContext);
		}

		rkit::log::LogInfo(u8"GameLogic: Starting session");

		CORO_CHECK(m_game->GetCaptureHarness()->GetConfigurationState(configState));

		IConfigurationValueView root = configState->GetRoot();

		IConfigurationKeyValueTableView kvt;
		CORO_CHECK(root.Get(kvt));

		IConfigurationValueView mapNameValue;
		CORO_CHECK(kvt.GetValueFromKey(u8"mapName", mapNameValue));

		rkit::StringSliceView mapName;
		CORO_CHECK(mapNameValue.Get(mapName));

		for (char c : mapName)
		{
			if (c >= 'a' && c <= 'z')
				continue;
			if (c >= '0' && c <= '9')
				continue;

			rkit::log::Error(u8"Invalid map name");
			CORO_THROW(rkit::ResultCode::kDataError);
		}

		CORO_CHECK(co_await LoadGlobalScripts(thread));
		CORO_CHECK(co_await LoadMap(thread, mapName));
		CORO_CHECK(co_await LoadMapScripts(thread, mapName));
		CORO_CHECK(co_await SpawnMapInitialObjects(thread, mapName));

		rkit::log::LogInfo(u8"GameLogic: Entering game session");

		CORO_CHECK(m_sandboxImports.MTAsync_EnterGameSession(m_sandboxMainThreadContext.Get(), m_sandboxEnv.m_gameSessionObjAddr));

		{
			SandboxMainThreadBlocker mtBlocker(this);
			CORO_CHECK(co_await thread.AwaitBlocker(mtBlocker.CreateBlocker()));
		}

		CORO_RETURN_OK;
	}


	template<class T>
	rkit::Result AnoxGameLogic::CopySpanToSandbox(SandboxMemObject &outMemObject, const rkit::Span<T> &span)
	{
		return CopyDataToSandbox(outMemObject, span.Ptr(), span.SizeInBytes());
	}

	rkit::Result AnoxGameLogic::CopyDataToSandbox(SandboxMemObject &outMemObject, const void *data, size_t size)
	{
		rkit::sandbox::Address_t addr = 0;
		uint32_t mmid = 0;
		RKIT_CHECK(m_sandbox->AllocDynamicMemory(addr, mmid, size));

		void *ptr = nullptr;
		RKIT_CHECK(m_sandbox->AccessMemoryRange(ptr, addr, size));

		memcpy(ptr, data, size);

		outMemObject.m_mmid = mmid;
		outMemObject.m_addr = addr;
		outMemObject.m_size = size;
		RKIT_RETURN_OK;
	}

	rkit::ResultCoroutine AnoxGameLogic::AsyncRunFrame(rkit::ICoroThread &thread)
	{
		if (m_sandbox.IsValid())
		{
			CORO_CHECK(m_sandboxImports.MTAsync_RunFrame(m_sandboxMainThreadContext.Get(), m_sandboxEnv.m_gameSessionObjAddr));

			{
				SandboxMainThreadBlocker mtBlocker(this);
				CORO_CHECK(co_await thread.AwaitBlocker(mtBlocker.CreateBlocker()));
			}
		}

		CORO_RETURN_OK;
	}

	rkit::Result IGameLogic::Create(rkit::UniquePtr<IGameLogic> &outGameLoop, IAnoxGame *game)
	{
		return rkit::New<AnoxGameLogic>(outGameLoop, game);
	}
}
