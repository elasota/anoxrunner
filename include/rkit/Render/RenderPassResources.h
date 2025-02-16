#pragma once

#include "rkit/Core/Span.h"

namespace rkit
{
	template<class T>
	struct ISpan;
}

namespace rkit::render
{
	struct IDepthStencilView;
	struct IRenderTargetView;

	struct RenderPassResources
	{
		Span<IRenderTargetView *> m_renderTargetViews;
		IDepthStencilView *m_depthStencilView = nullptr;

		uint32_t m_width = 0;
		uint32_t m_height = 0;
		uint32_t m_arraySize = 1;
	};
}
