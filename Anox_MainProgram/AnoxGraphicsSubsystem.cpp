#include "AnoxGraphicsResourceManager.h"

#include "anox/AnoxGraphicsSubsystem.h"

#include "rkit/Render/DisplayManager.h"
#include "rkit/Render/RenderDriver.h"
#include "rkit/Render/RenderDevice.h"

#include "rkit/Core/DriverModuleInitParams.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/NoCopy.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/UniquePtr.h"
#include "rkit/Core/Vector.h"


namespace anox
{
	class GraphicsSubsystem final : public IGraphicsSubsystem, public rkit::NoCopy
	{
	public:
		explicit GraphicsSubsystem(anox::RenderBackend desiredBackend);
		~GraphicsSubsystem();

		rkit::Result Initialize();

		void SetDesiredRenderBackend(RenderBackend renderBackend) override;

		void SetDesiredDisplayMode(rkit::render::DisplayMode displayMode) override;
		rkit::Result TransitionDisplayState() override;

	private:
		enum class DeviceSetupStep
		{
			kCheckingPipelines,
			kCompilingPipelines,
			kMergingPipelines,
			kLoadingResources,
			kFinished,
		};

		rkit::Result TransitionDisplayMode();
		rkit::Result TransitionBackend();

		rkit::Result CreateGameDisplayAndDevice(const rkit::StringView &renderModule);

		rkit::Optional<rkit::render::DisplayMode> m_currentDisplayMode;
		rkit::render::DisplayMode m_desiredDisplayMode = rkit::render::DisplayMode::kSplash;

		rkit::Optional<anox::RenderBackend> m_backend;
		anox::RenderBackend m_desiredBackend;

		rkit::UniquePtr<rkit::render::IDisplay> m_mainDisplay;

		rkit::UniquePtr<rkit::render::IRenderDevice> m_renderDevice;

		rkit::UniquePtr<anox::IGraphicsResourceManager> m_resourceManager;

		DeviceSetupStep m_setupStep = DeviceSetupStep::kFinished;
	};


	GraphicsSubsystem::GraphicsSubsystem(anox::RenderBackend desiredBackend)
		: m_desiredBackend(desiredBackend)
	{
	}

	GraphicsSubsystem::~GraphicsSubsystem()
	{
		m_mainDisplay.Reset();
	}

	rkit::Result GraphicsSubsystem::Initialize()
	{
		return rkit::ResultCode::kOK;
	}

	rkit::Result GraphicsSubsystem::CreateGameDisplayAndDevice(const rkit::StringView &renderModuleName)
	{
		rkit::render::IDisplayManager *displayManager = rkit::GetDrivers().m_systemDriver->GetDisplayManager();

		m_currentDisplayMode = rkit::render::DisplayMode::kSplash;
		RKIT_CHECK(displayManager->CreateDisplay(m_mainDisplay, m_currentDisplayMode.Get()));

		rkit::render::IProgressMonitor *progressMonitor = m_mainDisplay->GetProgressMonitor();

		if (progressMonitor)
		{
			// FIXME: Localize
			RKIT_CHECK(progressMonitor->SetText("Starting graphics system..."));
		}

		// Create render driver
		rkit::render::RenderDriverInitProperties driverParams = {};

#if RKIT_IS_DEBUG
		driverParams.m_validationLevel = rkit::render::ValidationLevel::kAggressive;
		driverParams.m_enableLogging = true;
#endif

		rkit::DriverModuleInitParameters moduleParams = {};
		moduleParams.m_driverInitParams = &driverParams;

		rkit::IModule *renderModule = rkit::GetDrivers().m_moduleDriver->LoadModule(rkit::IModuleDriver::kDefaultNamespace, renderModuleName.GetChars(), &moduleParams);
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

		RKIT_CHECK(progressMonitor->SetText("Checking shader cache..."));

		m_setupStep = DeviceSetupStep::kCheckingPipelines;

		return rkit::ResultCode::kOK;
	}

	rkit::Result GraphicsSubsystem::TransitionDisplayMode()
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result GraphicsSubsystem::TransitionBackend()
	{
		if (m_resourceManager.IsValid())
			m_resourceManager->UnloadAll();

		if (m_renderDevice.IsValid())
			m_renderDevice.Reset();

		if (m_mainDisplay.IsValid())
			m_mainDisplay.Reset();

		m_backend = m_desiredBackend;

		rkit::StringView backendModule;

		switch (m_backend.Get())
		{
		case anox::RenderBackend::kVulkan:
			backendModule = "Render_Vulkan";
			break;

		default:
			return rkit::ResultCode::kInternalError;
		}

		return CreateGameDisplayAndDevice(backendModule);
	}

	void GraphicsSubsystem::SetDesiredRenderBackend(RenderBackend renderBackend)
	{
		m_desiredBackend = renderBackend;
	}

	void GraphicsSubsystem::SetDesiredDisplayMode(rkit::render::DisplayMode displayMode)
	{
		m_desiredDisplayMode = displayMode;
	}

	rkit::Result GraphicsSubsystem::TransitionDisplayState()
	{
		if (m_setupStep != DeviceSetupStep::kFinished)
		{
			return rkit::ResultCode::kOK;
		}

		if (!m_backend.IsSet() || m_backend.Get() != m_desiredBackend)
			return TransitionBackend();

		if (!m_currentDisplayMode.IsSet() || m_currentDisplayMode.Get() != m_desiredDisplayMode)
			return TransitionDisplayMode();

		return rkit::ResultCode::kOK;
	}
}

rkit::Result anox::IGraphicsSubsystem::Create(rkit::UniquePtr<anox::IGraphicsSubsystem> &outSubsystem, anox::RenderBackend defaultBackend)
{
	rkit::UniquePtr<anox::GraphicsSubsystem> subsystem;
	RKIT_CHECK(rkit::New<anox::GraphicsSubsystem>(subsystem, defaultBackend));

	RKIT_CHECK(subsystem->Initialize());

	outSubsystem = std::move(subsystem);

	return rkit::ResultCode::kOK;
}
