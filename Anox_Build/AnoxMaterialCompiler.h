#pragma once

#include "rkit/Core/FourCC.h"
#include "rkit/BuildSystem/DependencyGraph.h"

namespace anox::buildsystem
{
	static const uint32_t kFontMaterialNodeID = RKIT_FOURCC('F', 'M', 'T', 'L');

	struct IDependencyNodeCompilerFeedback;

	class MaterialCompiler final : public rkit::buildsystem::IDependencyNodeCompiler
	{
	public:
		bool HasAnalysisStage() const override;

		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;

		uint32_t GetVersion() const override;

	private:
		enum class MaterialNodeType
		{
			kFont,

			kCount,
		};

		static rkit::Result MaterialNodeTypeFromFourCC(MaterialNodeType &outNodeType, uint32_t nodeTypeFourCC);
		static rkit::Result ConstructAnalysisPath(rkit::String &analysisPath, const rkit::StringView &identifier, MaterialNodeType nodeType);

		rkit::Result RunAnalyzeATD(const rkit::StringView &name, rkit::UniquePtr<rkit::ISeekableReadStream> &&atdStream, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);
		rkit::Result RunAnalyzeImage(const rkit::StringView &name, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);
	};
}
