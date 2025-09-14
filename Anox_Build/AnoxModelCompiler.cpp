#include "AnoxModelCompiler.h"

namespace anox { namespace buildsystem
{
	class AnoxModelCompilerCommon
	{
	};

	class AnoxMDACompiler final : public AnoxModelCompilerCommon, public AnoxMDACompilerBase
	{
	public:
		bool HasAnalysisStage() const override;
		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);

		virtual uint32_t GetVersion() const override;
	};

	class AnoxMD2Compiler final : public AnoxModelCompilerCommon, public AnoxMD2CompilerBase
	{
	public:
		bool HasAnalysisStage() const override;
		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);

		virtual uint32_t GetVersion() const override;
	};

	bool AnoxMDACompiler::HasAnalysisStage() const
	{
		return true;
	}

	rkit::Result AnoxMDACompiler::RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result AnoxMDACompiler::RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	uint32_t AnoxMDACompiler::GetVersion() const
	{
		return 1;
	}

	bool AnoxMD2Compiler::HasAnalysisStage() const
	{
		return true;
	}

	rkit::Result AnoxMD2Compiler::RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result AnoxMD2Compiler::RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	uint32_t AnoxMD2Compiler::GetVersion() const
	{
		return 1;
	}

	rkit::Result AnoxMDACompilerBase::Create(rkit::UniquePtr<AnoxMDACompilerBase> &outCompiler)
	{
		return rkit::New<AnoxMDACompiler>(outCompiler);
	}

	rkit::Result AnoxMD2CompilerBase::Create(rkit::UniquePtr<AnoxMD2CompilerBase> &outCompiler)
	{
		return rkit::New<AnoxMD2Compiler>(outCompiler);
	}
} }
