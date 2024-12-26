#include "anox/AnoxGame.h"

#include "rkit/Core/Drivers.h"
#include "rkit/Core/DriverModuleInitParams.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Module.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/Vector.h"

#include "rkit/Render/DisplayManager.h"

#include "rkit/Render/RenderDevice.h"
#include "rkit/Render/RenderDriver.h"

namespace anox
{
	class AnoxGame final : public IAnoxGame
	{
	public:
		~AnoxGame();

		rkit::Result Start() override;
		rkit::Result RunFrame() override;
		bool IsExiting() const override;

	private:
		bool m_isExiting = false;

		rkit::UniquePtr<rkit::render::IDisplay> m_mainDisplay;
	};
}

namespace anox
{
	AnoxGame::~AnoxGame()
	{
	}

	rkit::Result AnoxGame::Start()
	{
		// Create splash
		RKIT_CHECK(rkit::GetDrivers().m_systemDriver->GetDisplayManager()->CreateDisplay(m_mainDisplay, rkit::render::DisplayMode::kSplash));

		rkit::render::IProgressMonitor *progressMonitor = m_mainDisplay->GetProgressMonitor();

		if (progressMonitor)
		{
			// FIXME: Localize
			progressMonitor->SetText("Starting graphics system...");
		}

		// Create render driver
		rkit::render::RenderDriverInitProperties driverParams = {};

#if RKIT_IS_DEBUG
		driverParams.m_validationLevel = rkit::render::ValidationLevel::kAggressive;
		driverParams.m_enableLogging = true;
#endif

		rkit::DriverModuleInitParameters moduleParams = {};
		moduleParams.m_driverInitParams = &driverParams;

		rkit::IModule* renderModule = rkit::GetDrivers().m_moduleDriver->LoadModule(rkit::IModuleDriver::kDefaultNamespace, "Render_Vulkan", &moduleParams);
		if (!renderModule)
			return rkit::ResultCode::kModuleLoadFailed;

		rkit::render::IRenderDriver *renderDriver = static_cast<rkit::render::IRenderDriver *>(rkit::GetDrivers().FindDriver(rkit::IModuleDriver::kDefaultNamespace, "Render_Vulkan"));

		if (!renderDriver)
		{
			rkit::log::Error("Missing render driver");
			return rkit::ResultCode::kModuleLoadFailed;
		}

		rkit::Vector<rkit::UniquePtr<rkit::render::IRenderAdapter>> adapters;
		RKIT_CHECK(renderDriver->EnumerateAdapters(adapters));

		if (adapters.Count() == 0)
		{
			rkit::log::Error("No available adapters");
			return rkit::ResultCode::kOperationFailed;
		}

		rkit::Vector<rkit::render::CommandQueueTypeRequest> queueRequests;

		float fOne = 1.0f;
		float fHalf = 0.5f;

		{
			rkit::render::CommandQueueTypeRequest rq;
			rq.m_numQueues = 1;
			rq.m_queuePriorities = &fOne;
			rq.m_type = rkit::render::CommandQueueType::kGraphicsCompute;

			RKIT_CHECK(queueRequests.Append(rq));

			rq.m_type = rkit::render::CommandQueueType::kCopy;
			RKIT_CHECK(queueRequests.Append(rq));
		}

		rkit::UniquePtr<rkit::render::IRenderDevice> device;
		RKIT_CHECK(renderDriver->CreateDevice(device, queueRequests.ToSpan(), *adapters[0]));

		progressMonitor->SetText("Starting up...");

		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result AnoxGame::RunFrame()
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	bool AnoxGame::IsExiting() const
	{
		return m_isExiting;
	}

	rkit::Result anox::IAnoxGame::Create(rkit::UniquePtr<IAnoxGame> &outGame)
	{
		return rkit::New<AnoxGame>(outGame);
	}
}
