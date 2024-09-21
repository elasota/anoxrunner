#include "anox/AnoxModule.h"
#include "anox/UtilitiesDriver.h"

#include "rkit/Core/DriverModuleStub.h"
#include "rkit/Core/UtilitiesDriver.h"
#include "rkit/Core/ModuleGlue.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/String.h"


namespace anox
{
	class UtilitiesDriver final : public anox::IUtilitiesDriver
	{
	public:

	private:
		rkit::Result InitDriver() override;
		void ShutdownDriver() override;

		uint32_t GetDriverNamespaceID() const override { return anox::kAnoxNamespaceID; }
		rkit::StringView GetDriverName() const override { return "Utilities"; }
	};

	typedef rkit::CustomDriverModuleStub<UtilitiesDriver> UtilitiesModule;
}

rkit::Result anox::UtilitiesDriver::InitDriver()
{
	return rkit::ResultCode::kOK;
}

void anox::UtilitiesDriver::ShutdownDriver()
{
}


RKIT_IMPLEMENT_MODULE("Anox", "Utilities", ::anox::UtilitiesModule)
