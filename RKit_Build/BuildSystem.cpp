#include "rkit/BuildSystem/BuildSystem.h"

#include "BuildSystemInstance.h"

#include "rkit/Core/DriverModuleStub.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/ModuleGlue.h"

namespace rkit::buildsystem
{
	class BuildSystemDriver final : public rkit::buildsystem::IBuildSystemDriver
	{
	public:

	private:
		rkit::Result InitDriver() override;
		void ShutdownDriver() override;

		uint32_t GetDriverNamespaceID() const override { return rkit::IModuleDriver::kDefaultNamespace; }
		rkit::StringView GetDriverName() const override { return "BuildSystem"; }

		Result CreateBuildSystemInstance(UniquePtr<IBuildSystemInstance> &outInstance) const override;
	};

	typedef rkit::CustomDriverModuleStub<BuildSystemDriver> BuildSystemModule;

	rkit::Result BuildSystemDriver::InitDriver()
	{
		return rkit::ResultCode::kOK;
	}

	void BuildSystemDriver::ShutdownDriver()
	{
	}


	Result BuildSystemDriver::CreateBuildSystemInstance(UniquePtr<IBuildSystemInstance> &outInstance) const
	{
		return IBaseBuildSystemInstance::Create(outInstance);
	}
}

RKIT_IMPLEMENT_MODULE("RKit", "Build", ::rkit::buildsystem::BuildSystemModule)
