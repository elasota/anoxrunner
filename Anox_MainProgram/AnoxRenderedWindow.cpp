#include "AnoxRenderedWindow.h"

#include "anox/AnoxGraphicsSubsystem.h"

#include "AnoxPeriodicResources.h"
#include "AnoxRecordJobRunner.h"
#include "AnoxSubmitJobRunner.h"

#include "rkit/Render/DisplayManager.h"
#include "rkit/Render/RenderDevice.h"
#include "rkit/Render/SwapChain.h"
#include "rkit/Render/SwapChainFrame.h"

#include "rkit/Core/Job.h"
#include "rkit/Core/JobDependencyList.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Vector.h"

namespace anox
{
	class RenderedWindow final : public RenderedWindowBase
	{
	public:
		RenderedWindow(rkit::UniquePtr<rkit::render::IDisplay> &&display, rkit::UniquePtr<anox::RenderedWindowResources> &&resources, rkit::render::IRenderDevice *device);
		~RenderedWindow();

		rkit::Result Initialize(uint8_t numBackBuffers, uint8_t numSyncPoints, rkit::UniquePtr<rkit::render::ISwapChainPrototype> &&prototype, rkit::render::IBaseCommandQueue *presentQueue);

		rkit::render::IDisplay &GetDisplay() override;
		rkit::render::ISwapChain *GetSwapChain() override;

		rkit::Result BeginFrame(IGraphicsSubsystem &graphicsSubsystem) override;
		rkit::Result EndFrame(IGraphicsSubsystem &graphicsSubsystem) override;

		const rkit::RCPtr<PerFramePerDisplayResources> &GetCurrentFrameResources() const override;

	private:
		struct SwapChainFrameState
		{
			bool m_isInitialized = false;
		};

		class AcquireJobRunner final : public IGraphicsSubmitJobRunner_t
		{
		public:
			explicit AcquireJobRunner(RenderedWindow &window, const rkit::RCPtr<PerFramePerDisplayResources> &resources);

			rkit::Result RunSubmit(rkit::render::IGraphicsCommandQueue &cmdQueue) override;

		private:
			RenderedWindow &m_window;
			rkit::RCPtr<PerFramePerDisplayResources> m_resources;
		};

		class PresentJobRunner final : public IGraphicsSubmitJobRunner_t
		{
		public:
			explicit PresentJobRunner(RenderedWindow &window, const rkit::RCPtr<PerFramePerDisplayResources> &resources);

			rkit::Result RunSubmit(rkit::render::IGraphicsCommandQueue &cmdQueue) override;

		private:
			RenderedWindow &m_window;
			rkit::RCPtr<PerFramePerDisplayResources> m_resources;
		};

		rkit::UniquePtr<rkit::render::IDisplay> m_display;
		rkit::UniquePtr<rkit::render::ISwapChain> m_swapChain;

		rkit::Vector<SwapChainFrameState> m_swapChainFrameStates;
		rkit::Vector<rkit::UniquePtr<rkit::render::ISwapChainSyncPoint>> m_syncPoints;
		rkit::render::IRenderDevice *m_device;

		rkit::RCPtr<PerFramePerDisplayResources> m_currentPerDisplayResources;
		rkit::UniquePtr<RenderedWindowResources> m_resources;

		uint8_t m_currentSyncPoint = 0;

		uint32_t m_width = 0;
		uint32_t m_height = 0;
	};

	RenderedWindow::AcquireJobRunner::AcquireJobRunner(RenderedWindow &window, const rkit::RCPtr<PerFramePerDisplayResources> &resources)
		: m_window(window)
		, m_resources(resources)
	{
	}

	rkit::Result RenderedWindow::AcquireJobRunner::RunSubmit(rkit::render::IGraphicsCommandQueue &cmdQueue)
	{
		rkit::render::ISwapChainSyncPoint &syncPoint = *m_resources->m_swapChainSyncPoint;

		RKIT_CHECK(m_window.m_swapChain->AcquireFrame(syncPoint));

		m_resources->m_swapChainFrameIndex = syncPoint.GetFrameIndex();

		RKIT_RETURN_OK;
	}

	RenderedWindow::PresentJobRunner::PresentJobRunner(RenderedWindow &window, const rkit::RCPtr<PerFramePerDisplayResources> &resources)
		: m_window(window)
		, m_resources(resources)
	{
	}

	rkit::Result RenderedWindow::PresentJobRunner::RunSubmit(rkit::render::IGraphicsCommandQueue &cmdQueue)
	{
		rkit::render::ISwapChainSyncPoint &syncPoint = *m_resources->m_swapChainSyncPoint;

		RKIT_CHECK(m_window.m_swapChain->Present(syncPoint));

		RKIT_RETURN_OK;
	}

	RenderedWindow::RenderedWindow(rkit::UniquePtr<rkit::render::IDisplay> &&display, rkit::UniquePtr<RenderedWindowResources> &&resources, rkit::render::IRenderDevice *device)
		: m_display(std::move(display))
		, m_resources(std::move(resources))
		, m_device(device)
	{
	}

	RenderedWindow::~RenderedWindow()
	{
		m_swapChain.Reset();
		m_display.Reset();
	}

	rkit::Result RenderedWindow::Initialize(uint8_t numSwapChainFrames, uint8_t numSyncPoints, rkit::UniquePtr<rkit::render::ISwapChainPrototype> &&prototypeRef, rkit::render::IBaseCommandQueue *presentQueue)
	{
		rkit::UniquePtr<rkit::render::ISwapChainPrototype> prototype = std::move(prototypeRef);

		if (m_device != nullptr)
		{
			if (numSwapChainFrames < 2 || numSyncPoints < 1)
				return rkit::ResultCode::kInternalError;

			RKIT_CHECK(m_syncPoints.Resize(numSyncPoints));
			for (size_t i = 0; i < numSyncPoints; i++)
			{
				RKIT_CHECK(m_device->CreateSwapChainSyncPoint(m_syncPoints[i]));
			}

			RKIT_CHECK(m_swapChainFrameStates.Resize(numSwapChainFrames));

			RKIT_CHECK(m_device->CreateSwapChain(m_swapChain, std::move(prototype), numSwapChainFrames, rkit::render::RenderTargetFormat::RGBA_UNorm8, rkit::render::SwapChainWriteBehavior::RenderTarget, *presentQueue));

			m_swapChain->GetExtents(m_width, m_height);
		}

		RKIT_RETURN_OK;
	}

	rkit::render::IDisplay &RenderedWindow::GetDisplay()
	{
		return *m_display;
	}

	rkit::render::ISwapChain *RenderedWindow::GetSwapChain()
	{
		return m_swapChain.Get();
	}

	rkit::Result RenderedWindow::BeginFrame(IGraphicsSubsystem &graphicsSubsystem)
	{
		if (!m_device)
			RKIT_RETURN_OK;

		rkit::RCPtr<PerFramePerDisplayResources> displayResources;
		RKIT_CHECK(rkit::New<PerFramePerDisplayResources>(displayResources));

		displayResources->m_windowResources = this->m_resources.Get();
		displayResources->m_swapChainSyncPoint = m_syncPoints[m_currentSyncPoint].Get();

		rkit::UniquePtr<ISubmitJobRunner> acquireJobRunner;
		RKIT_CHECK(rkit::New<AcquireJobRunner>(acquireJobRunner, *this, displayResources));

		RKIT_CHECK(graphicsSubsystem.CreateAndQueueSubmitJob(&displayResources->m_acquireJob, LogicalQueueType::kPresentation, std::move(acquireJobRunner), rkit::JobDependencyList()));

		m_currentPerDisplayResources = displayResources;

		RKIT_RETURN_OK;
	}

	rkit::Result RenderedWindow::EndFrame(IGraphicsSubsystem &graphicsSubsystem)
	{
		if (!m_device)
			RKIT_RETURN_OK;

		rkit::UniquePtr<ISubmitJobRunner> presentJobRunner;
		RKIT_CHECK(rkit::New<PresentJobRunner>(presentJobRunner, *this, m_currentPerDisplayResources));

		rkit::RCPtr<rkit::Job> presentJob;
		RKIT_CHECK(graphicsSubsystem.CreateAndQueueSubmitJob(&presentJob, LogicalQueueType::kPresentation, std::move(presentJobRunner), rkit::JobDependencyList()));

		m_currentPerDisplayResources.Reset();

		m_currentSyncPoint++;

		if (m_currentSyncPoint == m_syncPoints.Count())
			m_currentSyncPoint = 0;

		RKIT_RETURN_OK;
	}

	const rkit::RCPtr<PerFramePerDisplayResources> &RenderedWindow::GetCurrentFrameResources() const
	{
		return m_currentPerDisplayResources;
	}

	rkit::Result RenderedWindowBase::Create(rkit::UniquePtr<RenderedWindowBase> &outWindow, rkit::UniquePtr<rkit::render::IDisplay> &&displayRef, rkit::UniquePtr<rkit::render::ISwapChainPrototype> &&prototypeRef, rkit::UniquePtr<RenderedWindowResources> &&resourcesRef, rkit::render::IRenderDevice *device, rkit::render::IBaseCommandQueue *swapChainQueue, uint8_t numSwapChainFrames, uint8_t numSyncPoints)
	{
		rkit::UniquePtr<rkit::render::IDisplay> display = std::move(displayRef);
		rkit::UniquePtr<rkit::render::ISwapChainPrototype> prototype = std::move(prototypeRef);
		rkit::UniquePtr<RenderedWindowResources> resources = std::move(resourcesRef);

		rkit::UniquePtr<RenderedWindow> window;
		RKIT_CHECK(rkit::New<RenderedWindow>(window, std::move(display), std::move(resources), device));

		RKIT_CHECK(window->Initialize(numSwapChainFrames, numSyncPoints, std::move(prototype), swapChainQueue));

		outWindow = std::move(window);

		RKIT_RETURN_OK;
	}
}
