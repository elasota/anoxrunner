#include "rkit/Data/DataDriver.h"
#include "rkit/Data/RenderDataHandler.h"

#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/ModuleGlue.h"
#include "rkit/Core/DriverModuleStub.h"

#include "RenderDataHandler.h"

namespace rkit { namespace data
{
	class DataDriver final : public rkit::data::IDataDriver
	{
	public:

	private:
		rkit::Result InitDriver(const DriverInitParameters *initParams) override;
		void ShutdownDriver() override;

		uint32_t GetDriverNamespaceID() const override { return rkit::IModuleDriver::kDefaultNamespace; }
		rkit::StringView GetDriverName() const override { return u8"Data"; }

		IRenderDataHandler *GetRenderDataHandler() const override;

	private:
		UniquePtr<IRenderDataHandler> m_renderDataHandler;
	};

	typedef rkit::CustomDriverModuleStub<DataDriver> DataModule;

	rkit::Result DataDriver::InitDriver(const DriverInitParameters *initParams)
	{
		RKIT_CHECK(New<RenderDataHandler>(m_renderDataHandler));

		RKIT_RETURN_OK;
	}

	void DataDriver::ShutdownDriver()
	{
		m_renderDataHandler.Reset();
	}


	IRenderDataHandler *DataDriver::GetRenderDataHandler() const
	{
		return m_renderDataHandler.Get();
	}
} } // rkit::data

RKIT_IMPLEMENT_MODULE(RKit, Data, ::rkit::data::DataModule)
