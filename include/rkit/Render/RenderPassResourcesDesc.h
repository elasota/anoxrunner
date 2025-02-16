#pragma once

namespace rkit
{
	template<class T>
	struct ISpan;
}

namespace rkit::render
{
	struct IDepthStencilView;
	struct IRenderTargetView;

	struct RenderPassResourcesDesc
	{
		const ISpan<IRenderTargetView *> *m_renderTargetViews = nullptr;
		IDepthStencilView *m_depthStencilView = nullptr;
	};
}
