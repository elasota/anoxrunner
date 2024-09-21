#include "anox/AnoxModule.h"

#include "rkit/Core/Drivers.h"
#include "rkit/Core/Module.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/ModuleGlue.h"
#include "rkit/Core/ProgramDriver.h"
#include "rkit/Core/DriverModuleStub.h"
#include "rkit/Core/ProgramStub.h"
#include "rkit/Core/Result.h"

namespace anox
{
	class ExtractDATProgram final : public rkit::ISimpleProgram
	{
	public:
		rkit::Result Run() override;

	private:
	};

	typedef rkit::DriverModuleStub<rkit::ProgramStubDriver<ExtractDATProgram>, rkit::IProgramDriver, &rkit::Drivers::m_programDriver> ExtractDATModule;
}

rkit::Result anox::ExtractDATProgram::Run()
{
	rkit::IModule *anoxUtilsModule = rkit::GetDrivers().m_moduleDriver->LoadModule(anox::kAnoxNamespaceID, "Utilities");
	if (!anoxUtilsModule)
		return rkit::ResultCode::kModuleLoadFailed;

	RKIT_CHECK(anoxUtilsModule->Init(nullptr));
	

	return rkit::ResultCode::kOK;
}

RKIT_IMPLEMENT_MODULE("Tool", "ExtractDAT", ::anox::ExtractDATModule)
