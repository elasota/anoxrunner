#pragma once

#include "rkit/BuildSystem/BuildSystem.h"
#include "rkit/BuildSystem/DependencyGraph.h"
#include "rkit/Render/RenderDefs.h"

#include "rkit/Core/FourCC.h"

namespace rkit::buildsystem
{
	class RenderPipelineLibraryCompiler final : public IDependencyNodeCompiler
	{
	public:
		RenderPipelineLibraryCompiler();

		bool HasAnalysisStage() const override;
		Result RunAnalysis(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) override;
		Result RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) override;

		uint32_t GetVersion() const override;
	};

	class PipelineLibraryCombinerBase : public IPipelineLibraryCombiner
	{
	public:
		static Result Create(UniquePtr<IPipelineLibraryCombiner> &outPackageObjectWriter);
	};
}
