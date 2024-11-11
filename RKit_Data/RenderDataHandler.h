#pragma once
#pragma once

#include "rkit/Data/RenderDataHandler.h"

namespace rkit::data
{
	class RenderDataHandler final : public IRenderDataHandler
	{
	public:
		const RenderRTTIStructType *GetSamplerDescRTTI() const override;
		const RenderRTTIStructType *GetPushConstantDescRTTI() const override;
		const RenderRTTIEnumType *GetVertexInputSteppingRTTI() const override;
	};
}
