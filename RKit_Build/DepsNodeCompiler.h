#pragma once

#include "rkit/BuildSystem/DependencyGraph.h"

namespace rkit::buildsystem
{
	class DepsNodeCompiler final : public IDependencyNodeCompiler
	{
	public:
		DepsNodeCompiler();

		bool HasAnalysisStage() const override;
		Result RunAnalysis(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) override;
		Result RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback) override;

		Result CreatePrivateData(UniquePtr<IDependencyNodePrivateData> &outPrivateData) override;

		uint32_t GetVersion() const override;

	private:
		String m_path;
	};
}
