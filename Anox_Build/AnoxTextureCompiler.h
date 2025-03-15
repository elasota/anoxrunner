#pragma once

#include "rkit/BuildSystem/DependencyGraph.h"

#include "rkit/Core/FourCC.h"
#include "rkit/Core/StringProto.h"

namespace rkit
{
	struct Result;
}

namespace anox::buildsystem
{
	static const uint32_t kTextureNodeID = RKIT_FOURCC('A', 'T', 'E', 'X');

	enum class ImageImportDisposition
	{
		kWorldTransparent,
		kWorldOpaque,
		kGraphicTransparent,
		kGraphicOpaque,
		kInterformPalette,

		kCount,
	};

	class TextureCompiler final : public rkit::buildsystem::IDependencyNodeCompiler
	{
	public:
		bool HasAnalysisStage() const override;

		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;

		uint32_t GetVersion() const override;

		static rkit::Result CreateImportIdentifier(rkit::String &identifier, const rkit::StringView &imagePath, ImageImportDisposition disposition);

	private:
		static rkit::Result CompileTGA(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, const rkit::StringView &shortName, ImageImportDisposition disposition);
		static rkit::Result CompilePCX(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, const rkit::StringView &shortName, ImageImportDisposition disposition);
		static rkit::Result CompilePNG(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, const rkit::StringView &shortName, ImageImportDisposition disposition);
	};
}
