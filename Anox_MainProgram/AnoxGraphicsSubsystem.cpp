#include "AnoxGraphicsResourceManager.h"

#include "anox/AnoxGraphicsSubsystem.h"

#include "rkit/Render/DisplayManager.h"
#include "rkit/Render/RenderDriver.h"
#include "rkit/Render/RenderDevice.h"

#include "rkit/Utilities/ThreadPool.h"

#include "rkit/Core/DriverModuleInitParams.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Mutex.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/NoCopy.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/UniquePtr.h"
#include "rkit/Core/Vector.h"


namespace anox
{
	class GraphicsSubsystem final : public IGraphicsSubsystem, public rkit::NoCopy
	{
	public:
		explicit GraphicsSubsystem(rkit::data::IDataDriver &dataDriver, rkit::utils::IThreadPool &threadPool, anox::RenderBackend desiredBackend);
		~GraphicsSubsystem();

		rkit::Result Initialize();

		void SetDesiredRenderBackend(RenderBackend renderBackend) override;

		void SetDesiredDisplayMode(rkit::render::DisplayMode displayMode) override;
		rkit::Result TransitionDisplayState() override;

	private:
		class CheckPipelinesJob final : public rkit::IJobRunner
		{
		public:
			explicit CheckPipelinesJob(GraphicsSubsystem &graphicsSubsystem, const rkit::StringView &pipelinesFileName);

			rkit::Result Run() override;

		private:
			GraphicsSubsystem &m_graphicsSubsystem;
			rkit::StringView m_pipelinesFileName;
		};

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

		rkit::Result CreateGameDisplayAndDevice(const rkit::StringView &renderModule, const rkit::StringView &pipelinesFile);

		rkit::Optional<rkit::render::DisplayMode> m_currentDisplayMode;
		rkit::render::DisplayMode m_desiredDisplayMode = rkit::render::DisplayMode::kSplash;

		rkit::Optional<anox::RenderBackend> m_backend;
		anox::RenderBackend m_desiredBackend;

		rkit::UniquePtr<rkit::render::IDisplay> m_mainDisplay;

		rkit::UniquePtr<rkit::render::IRenderDevice> m_renderDevice;

		rkit::UniquePtr<anox::IGraphicsResourceManager> m_resourceManager;

		rkit::utils::IThreadPool &m_threadPool;

		rkit::UniquePtr<rkit::IMutex> m_setupMutex;
		DeviceSetupStep m_setupStep = DeviceSetupStep::kFinished;

		rkit::data::IDataDriver &m_dataDriver;
	};


	GraphicsSubsystem::CheckPipelinesJob::CheckPipelinesJob(GraphicsSubsystem &graphicsSubsystem, const rkit::StringView &pipelinesFileName)
		: m_graphicsSubsystem(graphicsSubsystem)
		, m_pipelinesFileName(pipelinesFileName)
	{
	}

	rkit::Result GraphicsSubsystem::CheckPipelinesJob::Run()
	{
		rkit::ISystemDriver *sysDriver = rkit::GetDrivers().m_systemDriver;
		rkit::UniquePtr<rkit::ISeekableReadStream> pipelinesFile = sysDriver->OpenFileRead(rkit::FileLocation::kGameDirectory, m_pipelinesFileName.GetChars());

		if (!pipelinesFile.IsValid())
		{
			rkit::log::Error("Failed to open pipeline package");
			return rkit::ResultCode::kFileOpenError;
		}

		

		return rkit::ResultCode::kNotYetImplemented;
	}

	GraphicsSubsystem::GraphicsSubsystem(rkit::data::IDataDriver &dataDriver, rkit::utils::IThreadPool &threadPool, anox::RenderBackend desiredBackend)
		: m_threadPool(threadPool)
		, m_dataDriver(dataDriver)
		, m_desiredBackend(desiredBackend)
	{
	}

	GraphicsSubsystem::~GraphicsSubsystem()
	{
		m_mainDisplay.Reset();
	}

	rkit::Result GraphicsSubsystem::Initialize()
	{
		rkit::ISystemDriver *sysDriver = rkit::GetDrivers().m_systemDriver;

		RKIT_CHECK(sysDriver->CreateMutex(m_setupMutex));

		return rkit::ResultCode::kOK;
	}

	rkit::Result GraphicsSubsystem::CreateGameDisplayAndDevice(const rkit::StringView &renderModuleName, const rkit::StringView &pipelinesFile)
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

		rkit::render::IRenderDriver *renderDriver = static_cast<rkit::render::IRenderDriver *>(rkit::GetDrivers().FindDriver(rkit::IModuleDriver::kDefaultNamespace, renderModuleName));

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

		m_renderDevice = std::move(device);

		RKIT_CHECK(progressMonitor->SetText("Checking shader cache..."));

		m_setupStep = DeviceSetupStep::kCheckingPipelines;

		rkit::UniquePtr<rkit::IJobRunner> jobRunner;
		RKIT_CHECK(rkit::New<CheckPipelinesJob>(jobRunner, *this, pipelinesFile));

		RKIT_CHECK(m_threadPool.GetJobQueue()->CreateJob(nullptr, rkit::JobType::kIO, std::move(jobRunner), nullptr));

		return rkit::ResultCode::kOK;
	}

	rkit::Result GraphicsSubsystem::TransitionDisplayMode()
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result GraphicsSubsystem::TransitionBackend()
	{
		// TODO: Wait for all tasks to complete
		if (m_resourceManager.IsValid())
			m_resourceManager->UnloadAll();

		if (m_renderDevice.IsValid())
			m_renderDevice.Reset();

		if (m_mainDisplay.IsValid())
			m_mainDisplay.Reset();

		m_backend = m_desiredBackend;

		rkit::StringView backendModule;
		rkit::StringView pipelinesFile;

		switch (m_backend.Get())
		{
		case anox::RenderBackend::kVulkan:
			backendModule = "Render_Vulkan";
			pipelinesFile = "pipelines_vk.rkp";
			break;

		default:
			return rkit::ResultCode::kInternalError;
		}

		return CreateGameDisplayAndDevice(backendModule, pipelinesFile);
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
			rkit::GetDrivers().m_systemDriver->SleepMSec(1000u / 60u);

			rkit::render::IProgressMonitor *progressMonitor = m_mainDisplay->GetProgressMonitor();
			if (progressMonitor)
				progressMonitor->FlushEvents();

			RKIT_CHECK(m_threadPool.GetJobQueue()->CheckFault());

			return rkit::ResultCode::kOK;
		}

		if (!m_backend.IsSet() || m_backend.Get() != m_desiredBackend)
			return TransitionBackend();

		if (!m_currentDisplayMode.IsSet() || m_currentDisplayMode.Get() != m_desiredDisplayMode)
			return TransitionDisplayMode();

		return rkit::ResultCode::kOK;
	}
}

rkit::Result anox::IGraphicsSubsystem::Create(rkit::UniquePtr<IGraphicsSubsystem> &outSubsystem, rkit::data::IDataDriver &dataDriver, rkit::utils::IThreadPool &threadPool, anox::RenderBackend defaultBackend)
{
	rkit::UniquePtr<anox::GraphicsSubsystem> subsystem;
	RKIT_CHECK(rkit::New<anox::GraphicsSubsystem>(subsystem, dataDriver, threadPool, defaultBackend));

	RKIT_CHECK(subsystem->Initialize());

	outSubsystem = std::move(subsystem);

	return rkit::ResultCode::kOK;
}
