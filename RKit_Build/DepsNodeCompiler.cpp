#include "DepsNodeCompiler.h"

namespace rkit::buildsystem
{
	DepsNodeCompiler::DepsNodeCompiler()
	{
	}

	bool DepsNodeCompiler::HasAnalysisStage() const
	{
		return true;
	}

	Result DepsNodeCompiler::RunAnalysis(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result DepsNodeCompiler::RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result DepsNodeCompiler::CreatePrivateData(UniquePtr<IDependencyNodePrivateData> &outPrivateData)
	{
		return ResultCode::kOK;
	}

	uint32_t DepsNodeCompiler::GetVersion() const
	{
		return 1;
	}
}
