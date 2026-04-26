#pragma once

#include "rkit/BuildSystem/DependencyGraph.h"
#include "rkit/Core/PathProto.h"
#include "rkit/Core/Opaque.h"

namespace anox::buildsystem
{
	class APEScriptCompilerImpl;
	class APEGroupCompilerImpl;

	class APEScriptCompiler final : public rkit::buildsystem::IDependencyNodeCompiler, public rkit::Opaque<APEScriptCompilerImpl>
	{
	public:
		bool HasAnalysisStage() const override;
		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;

		uint32_t GetVersion() const override;

		static rkit::Result FormatOutputPath(rkit::CIPath &outPath, const rkit::StringView &identifier);

		static rkit::Result Create(rkit::UniquePtr<APEScriptCompiler> &outCompiler);
	};

	class APEGroupCompiler final : public rkit::buildsystem::IDependencyNodeCompiler, public rkit::Opaque<APEGroupCompilerImpl>
	{
	public:
		bool HasAnalysisStage() const override;
		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;

		uint32_t GetVersion() const override;

		static rkit::Result Create(rkit::UniquePtr<APEGroupCompiler> &outCompiler);
	};
} // anox::buildsystem
