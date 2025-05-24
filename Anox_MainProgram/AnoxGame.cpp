#include "anox/AnoxGame.h"
#include "anox/AnoxGraphicsSubsystem.h"
#include "anox/AnoxFileSystem.h"

#include "AnoxCaptureHarness.h"
#include "AnoxCommandRegistry.h"
#include "AnoxGameLogic.h"
#include "AnoxGameFileSystem.h"
#include "AnoxKeybindManager.h"
#include "AnoxResourceManager.h"

#include "rkit/Data/DataDriver.h"

#include "rkit/Core/Drivers.h"
#include "rkit/Core/DriverModuleInitParams.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Module.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/Vector.h"

#include "rkit/Utilities/ThreadPool.h"

#include "rkit/Render/DisplayManager.h"

#include "rkit/Render/RenderDevice.h"
#include "rkit/Render/RenderDriver.h"

namespace anox
{
	class AnoxGame final : public IAnoxGame
	{
	public:
		explicit AnoxGame(const rkit::Optional<uint16_t> &numThreads);
		~AnoxGame();

		rkit::Result Start() override;
		rkit::Result RunFrame() override;
		bool IsExiting() const override;

		ICaptureHarness *GetCaptureHarness() const override;
		rkit::utils::IThreadPool *GetThreadPool() const override;
		AnoxCommandRegistryBase *GetCommandRegistry() const override;
		AnoxKeybindManagerBase *GetKeybindManager() const override;

	private:
		bool m_isExiting = false;

		rkit::UniquePtr<rkit::utils::IThreadPool> m_threadPool;
		rkit::UniquePtr<AnoxGameFileSystemBase> m_fileSystem;
		rkit::UniquePtr<AnoxResourceManagerBase> m_resourceManager;
		rkit::UniquePtr<AnoxCommandRegistryBase> m_commandRegistry;
		rkit::UniquePtr<AnoxKeybindManagerBase> m_keybindManager;
		rkit::UniquePtr<IGraphicsSubsystem> m_graphicsSubsystem;
		rkit::UniquePtr<IGameLogic> m_gameLogic;
		rkit::UniquePtr<ICaptureHarness> m_captureHarness;

		rkit::data::IDataDriver *m_dataDriver = nullptr;

		rkit::Optional<uint16_t> m_numThreadsOverride;
	};
}

namespace anox
{
	AnoxGame::AnoxGame(const rkit::Optional<uint16_t> &numThreads)
		: m_numThreadsOverride(numThreads)
	{
	}

	AnoxGame::~AnoxGame()
	{
		if (m_threadPool.IsValid())
		{
			// Flush all thread pool tasks before tearing down subsystems in case there are any leftover jobs
			// from a subsystem
			(void) m_threadPool->Close();
		}

		m_graphicsSubsystem.Reset();
		m_resourceManager.Reset();
		m_threadPool.Reset();
	}

	rkit::Result AnoxGame::Start()
	{
		rkit::IModule *dataModule = rkit::GetDrivers().m_moduleDriver->LoadModule(rkit::IModuleDriver::kDefaultNamespace, "Data");
		if (!dataModule)
		{
			rkit::log::Error("Couldn't load data module");
			return rkit::ResultCode::kModuleLoadFailed;
		}

		m_dataDriver = static_cast<rkit::data::IDataDriver *>(rkit::GetDrivers().FindDriver(rkit::IModuleDriver::kDefaultNamespace, "Data"));

		if (!m_dataDriver)
		{
			rkit::log::Error("Data driver wasn't available");
			return rkit::ResultCode::kModuleLoadFailed;
		}

		uint32_t numWorkThreads = rkit::GetDrivers().m_systemDriver->GetProcessorCount() - 1;

		if (m_numThreadsOverride.IsSet())
			numWorkThreads = m_numThreadsOverride.Get() - 1;

		RKIT_CHECK(rkit::GetDrivers().m_utilitiesDriver->CreateThreadPool(m_threadPool, numWorkThreads));

		RKIT_CHECK(AnoxGameFileSystemBase::Create(m_fileSystem, *m_threadPool->GetJobQueue()));
		RKIT_CHECK(AnoxResourceManagerBase::Create(m_resourceManager, m_fileSystem.Get(), m_threadPool->GetJobQueue()));
		RKIT_CHECK(AnoxCommandRegistryBase::Create(m_commandRegistry));

		RKIT_CHECK(AnoxKeybindManagerBase::Create(m_keybindManager, *this));
		RKIT_CHECK(m_keybindManager->Register(*m_commandRegistry));

		RKIT_CHECK(ICaptureHarness::CreateRealTime(m_captureHarness, *this, *m_resourceManager));

		RKIT_CHECK(IGraphicsSubsystem::Create(m_graphicsSubsystem, *m_fileSystem, *m_dataDriver, *m_threadPool, anox::RenderBackend::kVulkan));

		RKIT_CHECK(IGameLogic::Create(m_gameLogic, this));
		RKIT_CHECK(m_gameLogic->Start());


		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxGame::RunFrame()
	{
		RKIT_CHECK(m_graphicsSubsystem->TransitionDisplayState());

		RKIT_CHECK(m_graphicsSubsystem->RetireOldestFrame());

		RKIT_CHECK(m_gameLogic->RunFrame());

		RKIT_CHECK(m_graphicsSubsystem->StartRendering());

		RKIT_CHECK(m_graphicsSubsystem->DrawFrame());

		RKIT_CHECK(m_graphicsSubsystem->EndFrame());

		// TODO: Run tasks

		return rkit::ResultCode::kOK;
	}

	bool AnoxGame::IsExiting() const
	{
		return m_isExiting;
	}

	ICaptureHarness *AnoxGame::GetCaptureHarness() const
	{
		return m_captureHarness.Get();
	}

	rkit::utils::IThreadPool *AnoxGame::GetThreadPool() const
	{
		return m_threadPool.Get();
	}

	AnoxCommandRegistryBase *AnoxGame::GetCommandRegistry() const
	{
		return m_commandRegistry.Get();
	}

	AnoxKeybindManagerBase *AnoxGame::GetKeybindManager() const
	{
		return m_keybindManager.Get();
	}

	rkit::Result anox::IAnoxGame::Create(rkit::UniquePtr<IAnoxGame> &outGame, const rkit::Optional<uint16_t> &numThreads)
	{
		return rkit::New<AnoxGame>(outGame, numThreads);
	}
}
