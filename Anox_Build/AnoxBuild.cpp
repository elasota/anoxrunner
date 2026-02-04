#include "anox/AnoxModule.h"
#include "anox/BuildDriver.h"

#include "rkit/BuildSystem/BuildSystem.h"
#include "rkit/BuildSystem/DependencyGraph.h"

#include "rkit/Core/DriverModuleStub.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/ModuleGlue.h"

#include "rkit/Png/PngDriver.h"

#include "rkit/Render/BackendType.h"

#include "AnoxBSPMapCompiler.h"
#include "AnoxModelCompiler.h"
#include "AnoxEntityDefCompiler.h"
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
		rkit::StringView GetDriverName() const override { return u8"Build"; }

		rkit::png::IPngDriver *m_pngDriver = nullptr;
	};

	typedef rkit::CustomDriverModuleStub<BuildDriver> BuildModule;
}



rkit::Result anox::BuildDriver::InitDriver(const rkit::DriverInitParameters *)
{
	if (!rkit::GetDrivers().m_moduleDriver->LoadModule(rkit::IModuleDriver::kDefaultNamespace, u8"PNG"))
	{
		rkit::log::Error(u8"PNG module missing");
		RKIT_THROW(rkit::ResultCode::kModuleLoadFailed);
	}

	rkit::ICustomDriver *pngDriver = rkit::GetDrivers().FindDriver(rkit::IModuleDriver::kDefaultNamespace, u8"PNG");

	if (!pngDriver)
	{
		rkit::log::Error(u8"PNG driver failed to load");
		RKIT_THROW(rkit::ResultCode::kModuleLoadFailed);
	}

	m_pngDriver = static_cast<rkit::png::IPngDriver *>(pngDriver);

	RKIT_RETURN_OK;
}

void anox::BuildDriver::ShutdownDriver()
{
}

rkit::Result anox::BuildDriver::RegisterBuildSystemAddOn(rkit::buildsystem::IBuildSystemInstance *instance)
{
	{
		rkit::UniquePtr<buildsystem::MaterialCompiler> matCompiler;
		RKIT_CHECK(rkit::New<buildsystem::MaterialCompiler>(matCompiler, *m_pngDriver));

		RKIT_CHECK(instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kFontMaterialNodeID, std::move(matCompiler)));

		RKIT_CHECK(instance->RegisterNodeTypeByExtension(buildsystem::MaterialCompiler::GetFontMaterialExtension(), kAnoxNamespaceID, buildsystem::kFontMaterialNodeID));
	}

	{
		rkit::UniquePtr<buildsystem::MaterialCompiler> matCompiler;
		RKIT_CHECK(rkit::New<buildsystem::MaterialCompiler>(matCompiler, *m_pngDriver));

		RKIT_CHECK(instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kWorldMaterialNodeID, std::move(matCompiler)));

		RKIT_CHECK(instance->RegisterNodeTypeByExtension(buildsystem::MaterialCompiler::GetWorldMaterialExtension(), kAnoxNamespaceID, buildsystem::kWorldMaterialNodeID));
	}

	{
		rkit::UniquePtr<buildsystem::MaterialCompiler> matCompiler;
		RKIT_CHECK(rkit::New<buildsystem::MaterialCompiler>(matCompiler, *m_pngDriver));

		RKIT_CHECK(instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kModelMaterialNodeID, std::move(matCompiler)));

		RKIT_CHECK(instance->RegisterNodeTypeByExtension(buildsystem::MaterialCompiler::GetModelMaterialExtension(), kAnoxNamespaceID, buildsystem::kModelMaterialNodeID));
	}

	{
		rkit::UniquePtr<buildsystem::TextureCompilerBase> texCompiler;
		RKIT_CHECK(buildsystem::TextureCompilerBase::Create(texCompiler, *m_pngDriver));

		RKIT_CHECK(instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kTextureNodeID, std::move(texCompiler)));
	}

	{
		rkit::UniquePtr<buildsystem::BSPMapCompilerBase> mapCompiler;
		RKIT_CHECK(buildsystem::BSPMapCompilerBase::CreateMapCompiler(mapCompiler));

		RKIT_CHECK(instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kBSPMapNodeID, std::move(mapCompiler)));

		RKIT_CHECK(instance->RegisterNodeTypeByExtension(u8"bsp", kAnoxNamespaceID, buildsystem::kBSPMapNodeID));
	}

	{
		rkit::UniquePtr<buildsystem::BSPMapCompilerBase> lightingCompiler;
		RKIT_CHECK(buildsystem::BSPMapCompilerBase::CreateLightingCompiler(lightingCompiler));

		RKIT_CHECK(instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kBSPLightmapNodeID, std::move(lightingCompiler)));
	}

	{
		rkit::UniquePtr<buildsystem::BSPMapCompilerBase> bspGeoCompiler;
		RKIT_CHECK(buildsystem::BSPMapCompilerBase::CreateGeometryCompiler(bspGeoCompiler));

		RKIT_CHECK(instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kBSPGeometryID, std::move(bspGeoCompiler)));
	}

	{
		rkit::UniquePtr<buildsystem::BSPMapCompilerBase> bspEntCompiler;
		RKIT_CHECK(buildsystem::BSPMapCompilerBase::CreateEntityCompiler(bspEntCompiler));

		RKIT_CHECK(instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kBSPEntityID, std::move(bspEntCompiler)));
	}

	{
		rkit::UniquePtr<buildsystem::EntityDefCompilerBase> entityDefCompiler;
		RKIT_CHECK(buildsystem::EntityDefCompilerBase::Create(entityDefCompiler));

		RKIT_CHECK(instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kEntityDefNodeID, std::move(entityDefCompiler)));
	}

	{
		rkit::UniquePtr<buildsystem::AnoxMD2CompilerBase> md2Compiler;
		RKIT_CHECK(buildsystem::AnoxMD2CompilerBase::Create(md2Compiler));

		RKIT_CHECK(instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kMD2ModelNodeID, std::move(md2Compiler)));
	}

	{
		rkit::UniquePtr<buildsystem::AnoxMDACompilerBase> mdaCompiler;
		RKIT_CHECK(buildsystem::AnoxMDACompilerBase::Create(mdaCompiler));

		RKIT_CHECK(instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kMDAModelNodeID, std::move(mdaCompiler)));
	}

	{
		rkit::UniquePtr<buildsystem::AnoxCTCCompilerBase> ctcCompiler;
		RKIT_CHECK(buildsystem::AnoxCTCCompilerBase::Create(ctcCompiler));

		RKIT_CHECK(instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kCTCModelNodeID, std::move(ctcCompiler)));
	}

	RKIT_CHECK(instance->RegisterNodeTypeByExtension(u8"cfg", rkit::buildsystem::kDefaultNamespace, rkit::buildsystem::kCopyFileNodeID));

	RKIT_RETURN_OK;
}


RKIT_IMPLEMENT_MODULE("Anox", "Build", ::anox::BuildModule)
