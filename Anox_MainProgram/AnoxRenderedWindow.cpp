#include "AnoxRenderedWindow.h"

#include "rkit/Render/DisplayManager.h"
#include "rkit/Render/RenderDevice.h"
#include "rkit/Render/SwapChain.h"

#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Vector.h"

namespace anox
{
	class RenderedWindow final : public RenderedWindowBase
	{
	public:
		RenderedWindow(rkit::UniquePtr<rkit::render::IDisplay> &&display, rkit::render::IRenderDevice *device);
		~RenderedWindow();

		rkit::Result Initialize(uint8_t numBackBuffers, rkit::render::IBaseCommandQueue *swapChainQueue);

		rkit::render::IDisplay &GetDisplay() override;
		rkit::render::ISwapChain *GetSwapChain() override;

	private:
		rkit::UniquePtr<rkit::render::IDisplay> m_display;
		rkit::UniquePtr<rkit::render::ISwapChain> m_swapChain;
		rkit::render::IRenderDevice *m_device;
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

	rkit::Result RenderedWindow::Initialize(uint8_t numBackBuffers, rkit::render::IBaseCommandQueue *swapChainQueue)
	{
		if (m_device != nullptr)
		{
			if (numBackBuffers < 1)
				return rkit::ResultCode::kInternalError;

			RKIT_CHECK(m_device->CreateSwapChain(m_swapChain, *m_display, numBackBuffers, rkit::render::RenderTargetFormat::RGBA_UNorm8, rkit::render::SwapChainWriteBehavior::RenderTarget, *swapChainQueue));
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

	rkit::Result RenderedWindowBase::Create(rkit::UniquePtr<RenderedWindowBase> &outWindow, rkit::UniquePtr<rkit::render::IDisplay> &&display, rkit::render::IRenderDevice *device, rkit::render::IBaseCommandQueue *swapChainQueue, uint8_t numBackBuffers)
	{
		rkit::UniquePtr<RenderedWindow> window;
		RKIT_CHECK(rkit::New<RenderedWindow>(window, std::move(display), device));

		RKIT_CHECK(window->Initialize(numBackBuffers, swapChainQueue));

		outWindow = std::move(window);

		return rkit::ResultCode::kOK;
	}
}
