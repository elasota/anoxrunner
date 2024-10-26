#pragma once

#include "rkit/BuildSystem/DependencyGraph.h"
#include "rkit/Render/RenderDefs.h"

namespace rkit::buildsystem
{
	static const uint32_t kRenderPipelineID = 0x50495052;	// RPIP

	class RenderPipelineCompiler final : public IDependencyNodeCompiler
	{
	public:
		RenderPipelineCompiler();

		bool HasAnalysisStage() const override;
		Result RunAnalysis(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) override;
		Result RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) override;

		Result CreatePrivateData(UniquePtr<IDependencyNodePrivateData> &outPrivateData) override;

		uint32_t GetVersion() const override;
	};
}
