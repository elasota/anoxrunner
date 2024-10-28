#include "rkit/Data/DataDriver.h"
#include "rkit/Data/RenderDataHandler.h"

#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/ModuleGlue.h"
#include "rkit/Core/DriverModuleStub.h"

#include "RenderDataHandler.h"

namespace rkit::data
{
	class DataDriver final : public rkit::data::IDataDriver
	{
	public:

	private:
		rkit::Result InitDriver() override;
		void ShutdownDriver() override;

		uint32_t GetDriverNamespaceID() const override { return rkit::IModuleDriver::kDefaultNamespace; }
		rkit::StringView GetDriverName() const override { return "Data"; }

		IRenderDataHandler *GetRenderDataHandler() const override;

	private:
		UniquePtr<IRenderDataHandler> m_renderDataHandler;
	};

	typedef rkit::CustomDriverModuleStub<DataDriver> DataModule;

	rkit::Result DataDriver::InitDriver()
	{
		RKIT_CHECK(New<RenderDataHandler>(m_renderDataHandler));

		return rkit::ResultCode::kOK;
	}

	void DataDriver::ShutdownDriver()
	{
		m_renderDataHandler.Reset();
	}


	IRenderDataHandler *DataDriver::GetRenderDataHandler() const
	{
		return m_renderDataHandler.Get();
	}
}

RKIT_IMPLEMENT_MODULE("RKit", "Data", ::rkit::data::DataModule)
