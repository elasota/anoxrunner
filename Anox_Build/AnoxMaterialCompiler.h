#pragma once

#include "rkit/Core/FourCC.h"
#include "rkit/BuildSystem/DependencyGraph.h"
#include "rkit/Core/PathProto.h"
#include "rkit/Core/StringProto.h"

#include "anox/Data/MaterialResourceType.h"

namespace rkit
{
	template<class T>
	class Span;
}

namespace rkit { namespace png
{
	struct IPngDriver;
} }

namespace anox { namespace buildsystem
{
	static const uint32_t kFontMaterialNodeID = RKIT_FOURCC('F', 'M', 'T', 'L');
	static const uint32_t kWorldMaterialNodeID = RKIT_FOURCC('W', 'M', 'T', 'L');
	static const uint32_t kModelMaterialNodeID = RKIT_FOURCC('M', 'M', 'T', 'L');

	struct IDependencyNodeCompilerFeedback;
	struct MaterialAnalysisHeader;
	struct MaterialAnalysisDynamicData;
	struct MaterialAnalysisBitmapDef;

	enum class ImageImportDisposition : int;

	class MaterialCompiler final : public rkit::buildsystem::IDependencyNodeCompiler
	{
	public:
		MaterialCompiler(rkit::png::IPngDriver &pngDriver);

		bool HasAnalysisStage() const override;

		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;

		static rkit::StringView GetFontMaterialExtension();
		static rkit::StringView GetWorldMaterialExtension();
		static rkit::StringView GetModelMaterialExtension();

		uint32_t GetVersion() const override;

		static rkit::Result ConstructOutputPath(rkit::CIPath &outputPath, data::MaterialResourceType nodeType, const rkit::StringView &identifier);

	private:
		MaterialCompiler() = delete;

		static rkit::Result MaterialNodeTypeFromFourCC(data::MaterialResourceType &outNodeType, uint32_t nodeTypeFourCC);
		static rkit::Result ConstructAnalysisPath(rkit::CIPath &analysisPath, data::MaterialResourceType nodeType, const rkit::StringView &identifier);

		rkit::Result RunAnalyzeATD(const rkit::StringView &name, rkit::UniquePtr<rkit::ISeekableReadStream> &&atdStream, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) const;
		rkit::Result RunAnalyzeImage(const rkit::StringView &longName, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) const;
		rkit::Result RunAnalyzeMissing(const rkit::StringView &name, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) const;
		rkit::Result ResolveShortName(rkit::String &shortName, const rkit::StringView &identifier) const;

		static bool IsToken(const rkit::Span<const char> &span, const rkit::StringView &str);
		static rkit::Result ParseImageImport(const rkit::Span<const char> &token, ImageImportDisposition disposition, MaterialAnalysisDynamicData &dynamicData, MaterialAnalysisBitmapDef &bitmapDef, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);

		static rkit::Result AnalyzeDDSChannelUsage(rkit::IReadStream &stream, bool &rgbUsage, bool &alphaUsage, bool &lumaUsage);

		rkit::Result GenerateRealFrames(MaterialAnalysisHeader &header, MaterialAnalysisDynamicData &dynamicData, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) const;

		rkit::png::IPngDriver &m_pngDriver;
	};
} } // anox::buildsystem
