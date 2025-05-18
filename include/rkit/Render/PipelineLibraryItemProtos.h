#pragma once

namespace rkit { namespace render
{
	struct RenderPassDesc;
	struct IInternalRenderPass;

	template<class TDesc, class TCompiled>
	class PipelineLibraryItemRef;

	typedef PipelineLibraryItemRef<RenderPassDesc, IInternalRenderPass> RenderPassRef_t;
} } // rkit::render
