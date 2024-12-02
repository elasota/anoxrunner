#include "rkit/BuildSystem/BuildSystem.h"

#include "BuildSystemInstance.h"
#include "PackageBuilder.h"

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
		Result CreatePackageObjectWriter(UniquePtr<IPackageObjectWriter> &outWriter) const override;
		Result CreatePackageBuilder(data::IRenderDataHandler *dataHandler, IPackageObjectWriter *objWriter, bool allowTempStrings, UniquePtr<IPackageBuilder> &outBuilder) const override;
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

	Result BuildSystemDriver::CreatePackageObjectWriter(UniquePtr<IPackageObjectWriter> &outWriter) const
	{
		return PackageObjectWriterBase::Create(outWriter);
	}

	Result BuildSystemDriver::CreatePackageBuilder(data::IRenderDataHandler *dataHandler, IPackageObjectWriter *objWriter, bool allowTempStrings, UniquePtr<IPackageBuilder> &outBuilder) const
	{
		return PackageBuilderBase::Create(dataHandler, objWriter, allowTempStrings, outBuilder);
	}
}

RKIT_IMPLEMENT_MODULE("RKit", "Build", ::rkit::buildsystem::BuildSystemModule)
