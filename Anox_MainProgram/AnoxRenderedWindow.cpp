#include "AnoxRenderedWindow.h"

#include "rkit/Render/DisplayManager.h"
#include "rkit/Render/RenderDevice.h"
#include "rkit/Render/SwapChain.h"
#include "rkit/Render/SwapChainFrame.h"

#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Vector.h"

namespace anox
{
	class RenderedWindow final : public RenderedWindowBase
	{
	public:
		RenderedWindow(rkit::UniquePtr<rkit::render::IDisplay> &&display, rkit::render::IRenderDevice *device);
		~RenderedWindow();

		rkit::Result Initialize(uint8_t numBackBuffers, uint8_t numSyncPoints, rkit::UniquePtr<rkit::render::ISwapChainPrototype> &&prototype, rkit::render::IBaseCommandQueue *presentQueue);

		rkit::render::IDisplay &GetDisplay() override;
		rkit::render::ISwapChain *GetSwapChain() override;

		rkit::Result BeginFrame(GraphicsSubsystem &graphicsSubsystem) override;
		rkit::Result EndFrame() override;

	private:
		struct SwapChainFrameState
		{
			bool m_isInitialized = false;
		};

		rkit::UniquePtr<rkit::render::IDisplay> m_display;
		rkit::UniquePtr<rkit::render::ISwapChain> m_swapChain;
		rkit::Vector<SwapChainFrameState> m_swapChainFrameStates;
		rkit::Vector<rkit::UniquePtr<rkit::render::ISwapChainSyncPoint>> m_syncPoints;
		rkit::render::IRenderDevice *m_device;

		uint8_t m_currentSyncPoint = 0;
	};

	RenderedWindow::RenderedWindow(rkit::UniquePtr<rkit::render::IDisplay> &&display, rkit::render::IRenderDevice *device)
		: m_display(std::move(display))
		, m_device(device)
	{
	}

	RenderedWindow::~RenderedWindow()
	{
		m_swapChain.Reset();
		m_display.Reset();
	}

	rkit::Result RenderedWindow::Initialize(uint8_t numBackBuffers, uint8_t numSyncPoints, rkit::UniquePtr<rkit::render::ISwapChainPrototype> &&prototypeRef, rkit::render::IBaseCommandQueue *presentQueue)
	{
		rkit::UniquePtr<rkit::render::ISwapChainPrototype> prototype = std::move(prototypeRef);

		if (m_device != nullptr)
		{
			if (numBackBuffers < 1 || numSyncPoints < 1)
				return rkit::ResultCode::kInternalError;

			const uint8_t numFrames = numBackBuffers + 1;

			RKIT_CHECK(m_syncPoints.Resize(numSyncPoints));
			for (size_t i = 0; i < numSyncPoints; i++)
			{
				RKIT_CHECK(m_device->CreateSwapChainSyncPoint(m_syncPoints[i]));
			}

			RKIT_CHECK(m_swapChainFrameStates.Resize(numFrames));

			RKIT_CHECK(m_device->CreateSwapChain(m_swapChain, std::move(prototype), numFrames, rkit::render::RenderTargetFormat::RGBA_UNorm8, rkit::render::SwapChainWriteBehavior::RenderTarget, *presentQueue));
		}

		return rkit::ResultCode::kOK;
	}

	rkit::render::IDisplay &RenderedWindow::GetDisplay()
	{
		return *m_display;
	}

	rkit::render::ISwapChain *RenderedWindow::GetSwapChain()
	{
		return m_swapChain.Get();
	}

	rkit::Result RenderedWindow::BeginFrame(GraphicsSubsystem &graphicsSubsystem)
	{
		if (!m_device)
			return rkit::ResultCode::kOK;

		rkit::render::ISwapChainSyncPoint &syncPoint = *m_syncPoints[m_currentSyncPoint];

		RKIT_CHECK(m_swapChain->AcquireFrame(syncPoint));

		const size_t frameIndex = syncPoint.GetFrameIndex();

		SwapChainFrameState &frameState = m_swapChainFrameStates[frameIndex];

		if (!frameState.m_isInitialized)
		{
			frameState.m_isInitialized = true;

			return rkit::ResultCode::kNotYetImplemented;
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result RenderedWindow::EndFrame()
	{
		if (!m_device)
			return rkit::ResultCode::kOK;

		rkit::render::ISwapChainSyncPoint &syncPoint = *m_syncPoints[m_currentSyncPoint];

		RKIT_CHECK(m_swapChain->Present(syncPoint));

		m_currentSyncPoint++;

		if (m_currentSyncPoint == m_syncPoints.Count())
			m_currentSyncPoint = 0;

		return rkit::ResultCode::kOK;
	}

	rkit::Result RenderedWindowBase::Create(rkit::UniquePtr<RenderedWindowBase> &outWindow, rkit::UniquePtr<rkit::render::IDisplay> &&displayRef, rkit::UniquePtr<rkit::render::ISwapChainPrototype> &&prototypeRef, rkit::render::IRenderDevice *device, rkit::render::IBaseCommandQueue *swapChainQueue, uint8_t numBackBuffers, uint8_t numSyncPoints)
	{
		rkit::UniquePtr<rkit::render::IDisplay> display = std::move(displayRef);
		rkit::UniquePtr<rkit::render::ISwapChainPrototype> prototype = std::move(prototypeRef);

		rkit::UniquePtr<RenderedWindow> window;
		RKIT_CHECK(rkit::New<RenderedWindow>(window, std::move(display), device));

		RKIT_CHECK(window->Initialize(numBackBuffers, numSyncPoints, std::move(prototype), swapChainQueue));

		outWindow = std::move(window);

		return rkit::ResultCode::kOK;
	}
}
