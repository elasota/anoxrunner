#pragma once

#include "rkit/BuildSystem/BuildSystem.h"
#include "rkit/BuildSystem/DependencyGraph.h"
#include "rkit/Render/RenderDefs.h"

namespace rkit { namespace buildsystem
{
	class CopyFileCompiler final : public IDependencyNodeCompiler
	{
	public:
		CopyFileCompiler();

		bool HasAnalysisStage() const override;
		Result RunAnalysis(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) override;
		Result RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) override;

		uint32_t GetVersion() const override;
	};
} } // rkit::buildsystem
