#pragma once

namespace rkit { namespace render
{
	struct GraphicsPipelineDesc;
	struct RenderPassDesc;
	struct IInternalRenderPass;
	struct IInternalGraphicsPipeline;

	template<class TDesc, class TCompiled>
	class PipelineLibraryItemRef;

	typedef PipelineLibraryItemRef<RenderPassDesc, IInternalRenderPass> RenderPassRef_t;
	typedef PipelineLibraryItemRef<GraphicsPipelineDesc, IInternalGraphicsPipeline> GraphicsPipelineRef_t;
} } // rkit::render
