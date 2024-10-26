#include "rkit/BuildSystem/BuildSystem.h"

#include "rkit/Core/DriverModuleStub.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/ModuleGlue.h"

namespace rkit::buildsystem
{
	class BuildVulkanDriver final : public rkit::buildsystem::IBuildSystemAddOnDriver
	{
	public:

	private:
		rkit::Result InitDriver() override;
		void ShutdownDriver() override;

		uint32_t GetDriverNamespaceID() const override { return rkit::IModuleDriver::kDefaultNamespace; }
		rkit::StringView GetDriverName() const override { return "Build_Vulkan"; }
	};

	typedef rkit::CustomDriverModuleStub<BuildVulkanDriver> BuildVulkanModule;

	rkit::Result BuildVulkanDriver::InitDriver()
	{
		return rkit::ResultCode::kOK;
	}

	void BuildVulkanDriver::ShutdownDriver()
	{
	}
}

RKIT_IMPLEMENT_MODULE("RKit", "Build_Vulkan", ::rkit::buildsystem::BuildVulkanModule)
