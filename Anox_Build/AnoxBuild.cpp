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

#include "AnoxAPEScriptCompiler.h"
#include "AnoxBSPMapCompiler.h"
#include "AnoxModelCompiler.h"
#include "AnoxEntityDefCompiler.h"
#include "AnoxMaterialCompiler.h"
#include "AnoxSceneCompiler.h"
#include "AnoxTextureCompiler.h"

#include "anox/Build/NodeIDs.h"

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
		rkit::UniquePtr<buildsystem::MaterialCompiler> matCompiler = rkit::New<buildsystem::MaterialCompiler>(*m_pngDriver);

		instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kFontMaterialNodeID, std::move(matCompiler));

		instance->RegisterNodeTypeByExtension(buildsystem::MaterialCompiler::GetFontMaterialExtension(), kAnoxNamespaceID, buildsystem::kFontMaterialNodeID);
	}

	{
		rkit::UniquePtr<buildsystem::MaterialCompiler> matCompiler = rkit::New<buildsystem::MaterialCompiler>(*m_pngDriver);

		instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kWorldMaterialNodeID, std::move(matCompiler));

		instance->RegisterNodeTypeByExtension(buildsystem::MaterialCompiler::GetWorldMaterialExtension(), kAnoxNamespaceID, buildsystem::kWorldMaterialNodeID);
	}

	{
		rkit::UniquePtr<buildsystem::MaterialCompiler> matCompiler = rkit::New<buildsystem::MaterialCompiler>(*m_pngDriver);

		instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kModelMaterialNodeID, std::move(matCompiler));

		instance->RegisterNodeTypeByExtension(buildsystem::MaterialCompiler::GetModelMaterialExtension(), kAnoxNamespaceID, buildsystem::kModelMaterialNodeID);
	}

	{
		rkit::UniquePtr<buildsystem::MaterialCompiler> matCompiler = rkit::New<buildsystem::MaterialCompiler>(*m_pngDriver);

		instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kInterfaceMaterialNodeID, std::move(matCompiler));

		instance->RegisterNodeTypeByExtension(buildsystem::MaterialCompiler::GetInterfaceMaterialExtension(), kAnoxNamespaceID, buildsystem::kInterfaceMaterialNodeID);
	}

	{
		rkit::UniquePtr<buildsystem::TextureCompilerBase> texCompiler;
		buildsystem::TextureCompilerBase::Create(texCompiler, *m_pngDriver);

		instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kTextureNodeID, std::move(texCompiler));
	}

	{
		rkit::UniquePtr<buildsystem::BSPMapCompilerBase> mapCompiler;
		buildsystem::BSPMapCompilerBase::CreateMapCompiler(mapCompiler);

		instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kBSPMapNodeID, std::move(mapCompiler));

		instance->RegisterNodeTypeByExtension(u8"bsp", kAnoxNamespaceID, buildsystem::kBSPMapNodeID);
	}

	{
		rkit::UniquePtr<buildsystem::BSPMapCompilerBase> lightingCompiler;
		buildsystem::BSPMapCompilerBase::CreateLightingCompiler(lightingCompiler);

		instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kBSPLightmapNodeID, std::move(lightingCompiler));
	}

	{
		rkit::UniquePtr<buildsystem::BSPMapCompilerBase> bspGeoCompiler;
		buildsystem::BSPMapCompilerBase::CreateGeometryCompiler(bspGeoCompiler);

		instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kBSPGeometryID, std::move(bspGeoCompiler));
	}

	{
		rkit::UniquePtr<buildsystem::BSPMapCompilerBase> bspEntCompiler;
		buildsystem::BSPMapCompilerBase::CreateEntityCompiler(bspEntCompiler);

		instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kBSPEntityID, std::move(bspEntCompiler));
	}

	{
		rkit::UniquePtr<buildsystem::APEDepsCompiler> apeDepsCompiler;
		buildsystem::APEDepsCompiler::Create(apeDepsCompiler);

		instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kAPEDepsNodeID, std::move(apeDepsCompiler));

		instance->RegisterNodeTypeByExtension(u8"apedeps", kAnoxNamespaceID, buildsystem::kAPEDepsNodeID);
	}

	{
		rkit::UniquePtr<buildsystem::APEScriptCompiler> apeCompiler;
		buildsystem::APEScriptCompiler::Create(apeCompiler);

		instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kAPEScriptNodeID, std::move(apeCompiler));

		instance->RegisterNodeTypeByExtension(u8"ape", kAnoxNamespaceID, buildsystem::kAPEScriptNodeID);
	}

	{
		rkit::UniquePtr<buildsystem::APEGroupCompiler> apeGroupCompiler;
		buildsystem::APEGroupCompiler::Create(apeGroupCompiler);

		instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kAPEGroupNodeID, std::move(apeGroupCompiler));

		instance->RegisterNodeTypeByExtension(u8"apegroup", kAnoxNamespaceID, buildsystem::kAPEGroupNodeID);
	}

	{
		rkit::UniquePtr<buildsystem::EntityDefCompilerBase> entityDefCompiler;
		buildsystem::EntityDefCompilerBase::Create(entityDefCompiler);

		instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kEntityDefNodeID, std::move(entityDefCompiler));
	}

	{
		rkit::UniquePtr<buildsystem::AnoxMD2CompilerBase> md2Compiler;
		buildsystem::AnoxMD2CompilerBase::Create(md2Compiler);

		instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kMD2ModelNodeID, std::move(md2Compiler));
	}

	{
		rkit::UniquePtr<buildsystem::AnoxMDACompilerBase> mdaCompiler;
		buildsystem::AnoxMDACompilerBase::Create(mdaCompiler);

		instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kMDAModelNodeID, std::move(mdaCompiler));
	}

	{
		rkit::UniquePtr<buildsystem::AnoxCTCCompilerBase> ctcCompiler;
		buildsystem::AnoxCTCCompilerBase::Create(ctcCompiler);

		instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kCTCModelNodeID, std::move(ctcCompiler));
	}

	{
		rkit::UniquePtr<buildsystem::SceneCompilerBase> sceneCompiler;
		buildsystem::SceneCompilerBase::Create(sceneCompiler);

		instance->GetDependencyGraphFactory()->RegisterNodeCompiler(kAnoxNamespaceID, buildsystem::kSceneNodeID, std::move(sceneCompiler));
	}

	instance->RegisterNodeTypeByExtension(u8"cfg", rkit::buildsystem::kDefaultNamespace, rkit::buildsystem::kCopyFileNodeID);

	RKIT_RETURN_OK;
}


RKIT_IMPLEMENT_MODULE(Anox, Build, ::anox::BuildModule)
