#include "AnoxDataBuilder.h"

#include "anox/AnoxModule.h"

#include "rkit/BuildSystem/BuildSystem.h"
#include "rkit/BuildSystem/DependencyGraph.h"

#include "rkit/Core/Drivers.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Module.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/NewDelete.h"

namespace anox::utils
{
	class AnoxDataBuilder final : public IDataBuilder
	{
	public:
		AnoxDataBuilder();

		rkit::Result Run(const rkit::StringView &targetName, const rkit::StringView &sourceDir, const rkit::StringView &intermedDir, const rkit::StringView &dataDir) override;

	private:
		rkit::buildsystem::IBuildSystemDriver *m_bsDriver;
	};

	AnoxDataBuilder::AnoxDataBuilder()
		: m_bsDriver(nullptr)
	{
	}

	rkit::Result AnoxDataBuilder::Run(const rkit::StringView &targetName, const rkit::StringView &sourceDir, const rkit::StringView &intermedDir, const rkit::StringView &dataDir)
	{
		rkit::IModule *buildModule = rkit::GetDrivers().m_moduleDriver->LoadModule(rkit::IModuleDriver::kDefaultNamespace, "Build");
		if (!buildModule)
		{
			rkit::log::Error("Couldn't load build module");
			return rkit::ResultCode::kModuleLoadFailed;
		}

		RKIT_CHECK(buildModule->Init(nullptr));

		m_bsDriver = static_cast<rkit::buildsystem::IBuildSystemDriver *>(rkit::GetDrivers().FindDriver(rkit::IModuleDriver::kDefaultNamespace, "BuildSystem"));
		if (!m_bsDriver)
		{
			rkit::log::Error("Couldn't find build system driver");
			return rkit::ResultCode::kModuleLoadFailed;
		}

		rkit::UniquePtr<rkit::buildsystem::IBuildSystemInstance> instance;
		RKIT_CHECK(m_bsDriver->CreateBuildSystemInstance(instance));

		RKIT_CHECK(instance->Initialize(targetName, sourceDir, intermedDir, dataDir));

		rkit::buildsystem::IDependencyGraphFactory *graphFactory = instance->GetDependencyGraphFactory();

		rkit::UniquePtr<rkit::buildsystem::IDependencyNode> rootDepsNode;
		RKIT_CHECK(graphFactory->CreateNode(rkit::buildsystem::kDefaultNamespace, rkit::buildsystem::kDepsNodeID, rkit::buildsystem::BuildFileLocation::kSourceDir, "RootFiles.deps", rootDepsNode));

		RKIT_CHECK(instance->Build());

		return rkit::ResultCode::kOK;
	}

	rkit::Result IDataBuilder::Create(rkit::UniquePtr<IDataBuilder> &outDataBuilder)
	{
		return rkit::New<AnoxDataBuilder>(outDataBuilder);
	}
}
