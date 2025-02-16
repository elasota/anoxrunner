#pragma once

namespace rkit::render
{
	struct RenderPassDesc;
	struct IInternalRenderPass;

	template<class TDesc, class TCompiled>
	class PipelineLibraryItemRef;

	typedef PipelineLibraryItemRef<RenderPassDesc, IInternalRenderPass> RenderPassRef_t;
}
