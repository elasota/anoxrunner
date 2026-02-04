#include "anox/AnoxGame.h"
#include "anox/AnoxModule.h"
#include "anox/AnoxUtilitiesDriver.h"

#include "rkit/Core/Drivers.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Module.h"
#include "rkit/Core/ModuleGlue.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/Path.h"
#include "rkit/Core/ProgramStub.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/SystemDriver.h"

#include "rkit/Math/Vec.h"
#include "rkit/Math/Quat.h"

#include "rkit/Core/FPControl.h"

namespace anox
{
	class MainProgramDriver final : public rkit::IProgramDriver
	{
	public:
		rkit::Result InitDriver(const rkit::DriverInitParameters *initParams) override;
		void ShutdownDriver() override;

		rkit::Result InitProgram() override;
		rkit::Result RunFrame(bool &outIsExiting) override;
		void ShutdownProgram() override;

	private:
		rkit::UniquePtr<IAnoxGame> m_game;
	};

	typedef rkit::DriverModuleStub<MainProgramDriver, rkit::IProgramDriver, &rkit::Drivers::m_programDriver> MainProgramModule;
}

rkit::Result anox::MainProgramDriver::InitDriver(const rkit::DriverInitParameters *initParams)
{
	RKIT_RETURN_OK;
}

void anox::MainProgramDriver::ShutdownDriver()
{
}

rkit::Result anox::MainProgramDriver::InitProgram()
{
	rkit::ISystemDriver *sysDriver = rkit::GetDrivers().m_systemDriver;
	rkit::IUtilitiesDriver *utilsDriver = rkit::GetDrivers().m_utilitiesDriver;

	RKIT_CHECK(rkit::GetDrivers().m_utilitiesDriver->SetProgramName(u8"Anox Runner"));

	rkit::Span<const rkit::StringView> args = sysDriver->GetCommandLine();

	bool autoBuild = false;
	bool run = false;

	rkit::StringView buildTarget;
	rkit::OSAbsPath buildSourceDirectory;
	rkit::OSAbsPath buildIntermediateDirectory;
	rkit::OSAbsPath dataSourceDirectory;
	rkit::OSAbsPath dataDirectory;
	rkit::OSAbsPath baseDirectory;

	rkit::render::BackendType renderBackendType = rkit::render::BackendType::Vulkan;

	rkit::Optional<uint16_t> numThreads;

	for (size_t i = 0; i < args.Count(); i++)
	{
		const rkit::StringView &arg = args[i];

#if !RKIT_IS_FINAL
		if (arg == u8"-idir")
		{
			i++;

			if (i == args.Count())
			{
				rkit::log::Error(u8"Expected path after -idir");
				RKIT_THROW(rkit::ResultCode::kInvalidParameter);
			}

			RKIT_TRY_CATCH_RETHROW(buildIntermediateDirectory.SetFromUTF8(args[i]),
				rkit::CatchContext(
					[]
					{
						rkit::log::Error(u8"-idir path was invalid");
					}
				)
			);
		}
		else if (arg == u8"-ddir")
		{
			i++;

			if (i == args.Count())
			{
				rkit::log::Error(u8"Expected path after -ddir");
				RKIT_THROW(rkit::ResultCode::kInvalidParameter);
			}

			RKIT_TRY_CATCH_RETHROW(dataDirectory.SetFromUTF8(args[i]),
				rkit::CatchContext(
					[]
					{
						rkit::log::Error(u8"-ddir path was invalid");
					}
				)
			);
		}
		else if (arg == u8"-dsrcdir")
		{
			i++;

			if (i == args.Count())
			{
				rkit::log::Error(u8"Expected path after -ddir");
				RKIT_THROW(rkit::ResultCode::kInvalidParameter);
			}

			RKIT_TRY_CATCH_RETHROW(dataSourceDirectory.SetFromUTF8(args[i]),
				rkit::CatchContext(
					[]
					{
						rkit::log::Error(u8"-dsrcdir path was invalid");
					}
				)
			);
		}
		else if (arg == u8"-sdir")
		{
			i++;

			if (i == args.Count())
			{
				rkit::log::Error(u8"Expected path after -sdir");
				RKIT_THROW(rkit::ResultCode::kInvalidParameter);
			}

			RKIT_TRY_CATCH_RETHROW(buildSourceDirectory.SetFromUTF8(args[i]),
				rkit::CatchContext(
					[]
					{
						rkit::log::Error(u8"-sdir path was invalid");
					}
				)
			);
		}
		else if (arg == u8"-build")
		{
			i++;

			if (i == args.Count())
			{
				rkit::log::Error(u8"Expected build target after -build");
				RKIT_THROW(rkit::ResultCode::kInvalidParameter);
			}

			buildTarget = args[i];

			autoBuild = true;
		}
		else if (arg == u8"-run")
			run = true;
		else if (arg == u8"-threads")
		{
			i++;

			if (i == args.Count())
			{
				rkit::log::Error(u8"Expected build target after -build");
				RKIT_THROW(rkit::ResultCode::kInvalidParameter);
			}

			// FIXME: Use CoreLib or something instead
			long numThreadsArg = atol(reinterpret_cast<const char *>(args[i].GetChars()));
			if (numThreadsArg < 1 || numThreadsArg > 512)
			{
				rkit::log::Error(u8"Invalid thread count for -threads");
				RKIT_THROW(rkit::ResultCode::kInvalidParameter);
			}

			numThreads = static_cast<uint16_t>(numThreadsArg);
		}
		else
		{
			rkit::log::ErrorFmt(u8"Unknown argument {}", arg.GetChars());
			RKIT_THROW(rkit::ResultCode::kInvalidParameter);
		}
#endif
	}

	if (dataDirectory.Length() > 0)
	{
		RKIT_CHECK(sysDriver->SetGameDirectoryOverride(dataDirectory));
	}

	// FIXME: Move this to RKit Config
	RKIT_CHECK(sysDriver->SetSettingsDirectory(u8"AnoxRunner"));

	if (autoBuild)
	{
		rkit::IModule *utilsModule = rkit::GetDrivers().m_moduleDriver->LoadModule(kAnoxNamespaceID, u8"Utilities");
		if (!utilsModule)
		{
			rkit::log::Error(u8"Couldn't load utilities module");
			RKIT_THROW(rkit::ResultCode::kModuleLoadFailed);
		}

		IUtilitiesDriver *utilsDriver = static_cast<IUtilitiesDriver *>(rkit::GetDrivers().FindDriver(kAnoxNamespaceID, u8"Utilities"));

		RKIT_CHECK(utilsDriver->RunDataBuild(buildTarget, buildSourceDirectory, buildIntermediateDirectory, dataDirectory, dataSourceDirectory, renderBackendType));
	}


#if !!RKIT_IS_FINAL
	run = true;
#endif

	if (run)
	{
		RKIT_CHECK(IAnoxGame::Create(m_game, numThreads));

		RKIT_CHECK(m_game->Start());
	}

	RKIT_RETURN_OK;
}

rkit::Result anox::MainProgramDriver::RunFrame(bool &outIsExiting)
{
	outIsExiting = true;

	if (IAnoxGame *game = m_game.Get())
	{
		RKIT_CHECK(game->RunFrame());

		outIsExiting = game->IsExiting();
	}

	RKIT_RETURN_OK;
}

void anox::MainProgramDriver::ShutdownProgram()
{
	m_game.Reset();
}

RKIT_IMPLEMENT_MODULE("Anox", "MainProgram", ::anox::MainProgramModule)
