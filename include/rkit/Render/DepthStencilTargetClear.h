#pragma once

#include "rkit/Render/ImagePlane.h"

#include "rkit/Core/EnumMask.h"
#include "rkit/Core/Optional.h"

#include <cstdint>

namespace rkit::render
{
	struct DepthStencilTargetClear
	{
		Optional<ImagePlaneMask_t> m_planes;

		float m_depth = 0.0f;
		uint32_t m_stencil = 0;
	};
}
