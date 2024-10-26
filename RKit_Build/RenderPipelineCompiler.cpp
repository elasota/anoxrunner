#include "RenderPipelineCompiler.h"

#include "rkit/Render/RenderDynamicDefs.h"

#include "rkit/Core/HashTable.h"
#include "rkit/Core/Vector.h"

namespace rkit::buildsystem::rpc_interchange
{
}

namespace rkit::buildsystem::rpc_analyzer
{
	class StaticSamplerDef
	{
	public:

	};
}

namespace rkit::buildsystem
{
	RenderPipelineCompiler::RenderPipelineCompiler()
	{
	}

	bool RenderPipelineCompiler::HasAnalysisStage() const
	{
		return true;
	}

	Result RenderPipelineCompiler::RunAnalysis(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result RenderPipelineCompiler::RunCompile(IDependencyNode *depsNode, IDependencyNodeCompilerFeedback *feedback)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result RenderPipelineCompiler::CreatePrivateData(UniquePtr<IDependencyNodePrivateData> &outPrivateData)
	{
		return ResultCode::kOK;
	}

	uint32_t RenderPipelineCompiler::GetVersion() const
	{
		return 1;
	}
}
