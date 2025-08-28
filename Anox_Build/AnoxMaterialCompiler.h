#pragma once

#include "rkit/Core/FourCC.h"
#include "rkit/BuildSystem/DependencyGraph.h"
#include "rkit/Core/PathProto.h"
#include "rkit/Core/StringProto.h"

namespace rkit
{
	template<class T>
	class Span;
}

namespace anox { namespace buildsystem
{
	static const uint32_t kFontMaterialNodeID = RKIT_FOURCC('F', 'M', 'T', 'L');
	static const uint32_t kWorldMaterialNodeID = RKIT_FOURCC('W', 'M', 'T', 'L');

	struct IDependencyNodeCompilerFeedback;
	struct MaterialAnalysisDynamicData;
	struct MaterialAnalysisBitmapDef;

	enum class ImageImportDisposition : int;

	class MaterialCompiler final : public rkit::buildsystem::IDependencyNodeCompiler
	{
	public:
		enum class MaterialNodeType
		{
			kFont,
			kWorld,

			kCount,
		};

		bool HasAnalysisStage() const override;

		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;

		static rkit::StringView GetFontMaterialExtension();
		static rkit::StringView GetWorldMaterialExtension();

		uint32_t GetVersion() const override;

		static rkit::Result ConstructOutputPath(rkit::CIPath &analysisPath, MaterialNodeType nodeType, const rkit::StringView &identifier);

	private:
		static rkit::Result MaterialNodeTypeFromFourCC(MaterialNodeType &outNodeType, uint32_t nodeTypeFourCC);
		static rkit::Result ConstructAnalysisPath(rkit::CIPath &analysisPath, MaterialNodeType nodeType, const rkit::StringView &identifier);

		rkit::Result RunAnalyzeATD(const rkit::StringView &name, rkit::UniquePtr<rkit::ISeekableReadStream> &&atdStream, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);
		rkit::Result RunAnalyzeImage(const rkit::StringView &longName, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);
		rkit::Result RunAnalyzeMissing(const rkit::StringView &name, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);
		rkit::Result ResolveShortName(rkit::String &shortName, const rkit::StringView &identifier);

		static bool IsToken(const rkit::Span<const char> &span, const rkit::StringView &str);
		static rkit::Result ParseImageImport(const rkit::Span<const char> &token, ImageImportDisposition disposition, MaterialAnalysisDynamicData &dynamicData, MaterialAnalysisBitmapDef &bitmapDef);
	};
} } // anox::buildsystem
