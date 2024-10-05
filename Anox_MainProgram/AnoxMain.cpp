#include "anox/AnoxModule.h"
#include "anox/UtilitiesDriver.h"

#include "rkit/Core/Drivers.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Module.h"
#include "rkit/Core/ModuleGlue.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/ProgramStub.h"
#include "rkit/Core/Stream.h"

namespace anox
{
	class MainProgram final : public rkit::ISimpleProgram
	{
	public:
		rkit::Result Run() override;

	private:
		rkit::Result ExecutePreLaunchBuild();

	};

	typedef rkit::DriverModuleStub<rkit::ProgramStubDriver<MainProgram>, rkit::IProgramDriver, &rkit::Drivers::m_programDriver> MainProgramModule;
}

rkit::Result anox::MainProgram::Run()
{
	rkit::Span<const rkit::StringView> args = rkit::GetDrivers().m_systemDriver->GetCommandLine();

	bool autoBuild = false;

	rkit::StringView buildSourceDirectory;
	rkit::StringView buildIntermediateDirectory;
	rkit::StringView dataDirectory;

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

			buildIntermediateDirectory = args[i];
		}
		else if (arg == "-ddir")
		{
			i++;

			if (i == args.Count())
			{
				rkit::log::Error("Expected path after -ddir");
				return rkit::ResultCode::kInvalidParameter;
			}

			dataDirectory = args[i];
		}
		else if (arg == "-sdir")
		{
			i++;

			if (i == args.Count())
			{
				rkit::log::Error("Expected path after -sdir");
				return rkit::ResultCode::kInvalidParameter;
			}

			buildSourceDirectory = args[i];
		}
		else if (arg == "-build")
			autoBuild = true;
#endif
	}

	if (autoBuild)
	{
		rkit::IModule *utilsModule = rkit::GetDrivers().m_moduleDriver->LoadModule(kAnoxNamespaceID, "Utilities");
		if (!utilsModule)
		{
			rkit::log::Error("Couldn't load utilities module");
			return rkit::ResultCode::kModuleLoadFailed;
		}

		RKIT_CHECK(utilsModule->Init(nullptr));

		IUtilitiesDriver *utilsDriver = static_cast<IUtilitiesDriver *>(rkit::GetDrivers().FindDriver(kAnoxNamespaceID, "Utilities"));

		RKIT_CHECK(utilsDriver->RunDataBuild(buildSourceDirectory, buildIntermediateDirectory, dataDirectory));
	}


	return rkit::ResultCode::kOK;
}

RKIT_IMPLEMENT_MODULE("Anox", "MainProgram", ::anox::MainProgramModule)
