#include "rkit/Core/DriverModuleStub.h"
#include "rkit/Core/UtilitiesDriver.h"
#include "rkit/Core/ModuleGlue.h"
#include "rkit/Core/NewDelete.h"

#include "Json.h"

namespace rkit
{
	namespace Utilities
	{
		struct IJsonDocument;
	}

	class UtilitiesDriver final : public IUtilitiesDriver
	{
	public:
		Result InitDriver();
		void ShutdownDriver();

		Result CreateJsonDocument(UniquePtr<Utilities::IJsonDocument> &outDocument, IMallocDriver *alloc, IReadStream *readStream) const override;
	};

	typedef DriverModuleStub<UtilitiesDriver, IUtilitiesDriver, &Drivers::m_utilitiesDriver> UtilitiesModule;

	Result UtilitiesDriver::InitDriver()
	{
		return ResultCode::kOK;
	}

	void UtilitiesDriver::ShutdownDriver()
	{
	}

	Result UtilitiesDriver::CreateJsonDocument(UniquePtr<Utilities::IJsonDocument> &outDocument, IMallocDriver *alloc, IReadStream *readStream) const
	{
		return Utilities::CreateJsonDocument(outDocument, alloc, readStream);
	}
}


RKIT_IMPLEMENT_MODULE("RKit", "Utilities", ::rkit::UtilitiesModule)
