#include "anox/AnoxModule.h"
#include "anox/BuildDriver.h"

#include "rkit/BuildSystem/BuildSystem.h"
#include "rkit/BuildSystem/DependencyGraph.h"

#include "rkit/Core/DriverModuleStub.h"
#include "rkit/Core/ModuleGlue.h"

#include "rkit/Render/BackendType.h"

#include "AnoxMaterialCompiler.h"
#include "AnoxTextureCompiler.h"

namespace anox
{
	class BuildDriver final : public rkit::buildsystem::IBuildSystemAddOnDriver
	{
	public:

	private:
		rkit::Result InitDriver(const rkit::DriverInitParameters *) override;
		void ShutdownDriver() override;

		rkit::Result RegisterBuildSystemAddOn(rkit::buildsystem::IBuildSystemInstance *instance) override;

		uint32_t GetDriverNamespaceID() const override { return anox::kAnoxNamespaceID; }
		rkit::StringView GetDriverName() const override { return "Build"; }
	};

	typedef rkit::CustomDriverModuleStub<BuildDriver> BuildModule;
}



rkit::Result anox::BuildDriver::InitDriver(const rkit::DriverInitParameters *)
{
	return rkit::ResultCode::kOK;
}

void anox::BuildDriver::ShutdownDriver()
{
}

rkit::Result anox::BuildDriver::RegisterBuildSystemAddOn(rkit::buildsystem::IBuildSystemInstance *instance)
{
	{
		rkit::UniquePtr<buildsystem::MaterialCompiler> matCompiler;
		RKIT_CHECK(rkit::New<buildsystem::MaterialCompiler>(matCompiler));

		RKIT_CHECK(instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kFontMaterialNodeID, std::move(matCompiler)));

		RKIT_CHECK(instance->RegisterNodeTypeByExtension("fontmaterial", kAnoxNamespaceID, buildsystem::kFontMaterialNodeID));
	}

	{
		rkit::UniquePtr<buildsystem::TextureCompiler> texCompiler;
		RKIT_CHECK(rkit::New<buildsystem::TextureCompiler>(texCompiler));

		RKIT_CHECK(instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kTextureNodeID, std::move(texCompiler)));
	}

	return rkit::ResultCode::kOK;
}


RKIT_IMPLEMENT_MODULE("Anox", "Build", ::anox::BuildModule)
