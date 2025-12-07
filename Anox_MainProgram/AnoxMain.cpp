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
	return rkit::ResultCode::kOK;
}

void anox::MainProgramDriver::ShutdownDriver()
{
}

rkit::Result anox::MainProgramDriver::InitProgram()
{
	rkit::ISystemDriver *sysDriver = rkit::GetDrivers().m_systemDriver;
	rkit::IUtilitiesDriver *utilsDriver = rkit::GetDrivers().m_utilitiesDriver;

	RKIT_CHECK(rkit::GetDrivers().m_utilitiesDriver->SetProgramName("Anox Runner"));

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
		if (arg == "-idir")
		{
			i++;

			if (i == args.Count())
			{
				rkit::log::Error("Expected path after -idir");
				return rkit::ResultCode::kInvalidParameter;
			}

			if (!rkit::utils::ResultIsOK(buildIntermediateDirectory.SetFromUTF8(args[i])))
			{
				rkit::log::Error("-idir path was invalid");
				return rkit::ResultCode::kInvalidParameter;
			}
		}
		else if (arg == "-ddir")
		{
			i++;

			if (i == args.Count())
			{
				rkit::log::Error("Expected path after -ddir");
				return rkit::ResultCode::kInvalidParameter;
			}

			if (!rkit::utils::ResultIsOK(dataDirectory.SetFromUTF8(args[i])))
			{
				rkit::log::Error("-ddir path was invalid");
				return rkit::ResultCode::kInvalidParameter;
			}
		}
		else if (arg == "-dsrcdir")
		{
			i++;

			if (i == args.Count())
			{
				rkit::log::Error("Expected path after -ddir");
				return rkit::ResultCode::kInvalidParameter;
			}

			if (!rkit::utils::ResultIsOK(dataSourceDirectory.SetFromUTF8(args[i])))
			{
				rkit::log::Error("-ddir path was invalid");
				return rkit::ResultCode::kInvalidParameter;
			}
		}
		else if (arg == "-sdir")
		{
			i++;

			if (i == args.Count())
			{
				rkit::log::Error("Expected path after -sdir");
				return rkit::ResultCode::kInvalidParameter;
			}

			if (!rkit::utils::ResultIsOK(buildSourceDirectory.SetFromUTF8(args[i])))
			{
				rkit::log::Error("-sdir path was invalid");
				return rkit::ResultCode::kInvalidParameter;
			}
		}
		else if (arg == "-build")
		{
			i++;

			if (i == args.Count())
			{
				rkit::log::Error("Expected build target after -build");
				return rkit::ResultCode::kInvalidParameter;
			}

			buildTarget = args[i];

			autoBuild = true;
		}
		else if (arg == "-run")
			run = true;
		else if (arg == "-threads")
		{
			i++;

			if (i == args.Count())
			{
				rkit::log::Error("Expected build target after -build");
				return rkit::ResultCode::kInvalidParameter;
			}

			long numThreadsArg = atol(args[i].GetChars());
			if (numThreadsArg < 1 || numThreadsArg > 512)
			{
				rkit::log::Error("Invalid thread count for -threads");
				return rkit::ResultCode::kInvalidParameter;
			}

			numThreads = static_cast<uint16_t>(numThreadsArg);
		}
		else
		{
			rkit::log::ErrorFmt("Unknown argument {}", arg.GetChars());
			return rkit::ResultCode::kInvalidParameter;
		}
#endif
	}

	if (dataDirectory.Length() > 0)
	{
		RKIT_CHECK(sysDriver->SetGameDirectoryOverride(dataDirectory));
	}

	// FIXME: Move this to RKit Config
	RKIT_CHECK(sysDriver->SetSettingsDirectory("AnoxRunner"));

	if (autoBuild)
	{
		rkit::IModule *utilsModule = rkit::GetDrivers().m_moduleDriver->LoadModule(kAnoxNamespaceID, "Utilities");
		if (!utilsModule)
		{
			rkit::log::Error("Couldn't load utilities module");
			return rkit::ResultCode::kModuleLoadFailed;
		}

		IUtilitiesDriver *utilsDriver = static_cast<IUtilitiesDriver *>(rkit::GetDrivers().FindDriver(kAnoxNamespaceID, "Utilities"));

		RKIT_CHECK(utilsDriver->RunDataBuild(buildTarget, buildSourceDirectory, buildIntermediateDirectory, dataDirectory, dataSourceDirectory, renderBackendType));
	}

#if RKIT_IS_FINAL
	run = true;
#endif

	if (run)
	{
		RKIT_CHECK(IAnoxGame::Create(m_game, numThreads));

		RKIT_CHECK(m_game->Start());
	}

	return rkit::ResultCode::kOK;
}

rkit::Result anox::MainProgramDriver::RunFrame(bool &outIsExiting)
{
	outIsExiting = true;

	if (IAnoxGame *game = m_game.Get())
	{
		RKIT_CHECK(game->RunFrame());

		outIsExiting = game->IsExiting();
	}

	return rkit::ResultCode::kOK;
}

void anox::MainProgramDriver::ShutdownProgram()
{
	m_game.Reset();
}

RKIT_IMPLEMENT_MODULE("Anox", "MainProgram", ::anox::MainProgramModule)
