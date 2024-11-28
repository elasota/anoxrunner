#pragma once

#include "rkit/BuildSystem/DependencyGraph.h"
#include "rkit/Render/RenderDefs.h"

#include "rkit/Core/FourCC.h"

namespace rkit::buildsystem
{
	static const uint32_t kRenderPipelineLibraryID = RKIT_FOURCC('R', 'P', 'L', 'L');
	static const uint32_t kRenderGraphicsPipelineID = RKIT_FOURCC('R', 'G', 'P', 'L');

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

	class RenderPipelineCompiler final : public IDependencyNodeCompiler
	{
	public:
		enum class PipelineType
		{
			Graphics,
		};

		explicit RenderPipelineCompiler(PipelineType stage);

		bool HasAnalysisStage() const override;
		Result RunAnalysis(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) override;
		Result RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) override;

		Result CreatePrivateData(UniquePtr<IDependencyNodePrivateData> &outPrivateData) override;

		uint32_t GetVersion() const override;

	private:
		RenderPipelineCompiler() = delete;

		PipelineType m_pipelineType;
	};
}
