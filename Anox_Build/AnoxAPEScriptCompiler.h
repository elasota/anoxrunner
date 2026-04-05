#pragma once

#pragma once

#include "rkit/Core/FourCC.h"
#include "rkit/BuildSystem/DependencyGraph.h"
#include "rkit/Core/PathProto.h"
#include "rkit/Core/Opaque.h"

namespace anox::buildsystem
{
	class APEScriptCompilerImpl;

	static const uint32_t kAPEScriptNodeID = RKIT_FOURCC('A', 'P', 'E', 'S');

	class APEScriptCompiler final : public rkit::buildsystem::IDependencyNodeCompiler, public rkit::Opaque<APEScriptCompilerImpl>
	{
	public:
		bool HasAnalysisStage() const override;
		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;

		uint32_t GetVersion() const override;

		static rkit::Result Create(rkit::UniquePtr<APEScriptCompiler> &outCompiler);
	};
} // anox::buildsystem
