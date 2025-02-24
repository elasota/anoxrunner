#pragma once

#include "rkit/Render/ImageRect.h"

#include "rkit/Core/Span.h"
#include "rkit/Core/UniquePtr.h"


namespace rkit::render
{
	struct IDepthStencilView;
	struct IRenderTargetView;

	struct RenderPassResources
	{
		ConstSpan<IRenderTargetView *> m_renderTargetViews;

		IDepthStencilView *m_depthStencilView = nullptr;

		uint32_t m_width = 0;
		uint32_t m_height = 0;
		uint32_t m_arraySize = 1;

		ImageRect2D m_renderArea;
	};
}
