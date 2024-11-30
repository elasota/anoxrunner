#pragma once

#include "rkit/BuildSystem/DependencyGraph.h"
#include "rkit/Render/RenderDefs.h"

#include "rkit/Core/FourCC.h"

namespace rkit::buildsystem
{
	static const uint32_t kRenderPipelineLibraryNodeID = RKIT_FOURCC('R', 'P', 'L', 'L');

	class RenderPipelineLibraryCompiler final : public IDependencyNodeCompiler
	{
	public:
		RenderPipelineLibraryCompiler();

		bool HasAnalysisStage() const override;
		Result RunAnalysis(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) override;
		Result RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) override;

		Result CreatePrivateData(UniquePtr<IDependencyNodePrivateData> &outPrivateData) override;

		uint32_t GetVersion() const override;
	};
}
