#pragma once

#include "rkit/BuildSystem/DependencyGraph.h"

#include "rkit/Core/FourCC.h"
#include "rkit/Core/StringProto.h"

namespace rkit { namespace png {
	struct IPngDriver;
} } // rkit::png

namespace anox { namespace buildsystem
{
	static const uint32_t kTextureNodeID = RKIT_FOURCC('A', 'T', 'E', 'X');

	enum class ImageImportDisposition : int
	{
		kWorldAlphaBlend,
		kWorldAlphaTested,
		kWorldAlphaBlendNoMip,
		kWorldAlphaTestedNoMip,
		kGraphicTransparent,
		kGraphicOpaque,
		kInterformPalette,
		kInterformFrame,
		kModel,

		kCount,
	};

	class TextureCompilerBase : public rkit::buildsystem::IDependencyNodeCompiler
	{
	public:
		static rkit::Result Create(rkit::UniquePtr<TextureCompilerBase> &outCompiler, rkit::png::IPngDriver &pngDriver);

		static rkit::Result CreateImportIdentifier(rkit::String &identifier, const rkit::StringView &imagePath, ImageImportDisposition disposition);

		static rkit::Result ResolveIntermediatePath(rkit::String &outString, const rkit::StringView &identifierWithDisposition);

		static rkit::Result GetImageMetadata(rkit::utils::ImageSpec &imageSpec, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, rkit::png::IPngDriver &pngDriver, rkit::buildsystem::BuildFileLocation buildFileLocation, const rkit::CIPathView &shortName);
		static rkit::Result GetImage(rkit::UniquePtr<rkit::utils::IImage> &image,
			rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, rkit::png::IPngDriver &pngDriver,
			rkit::buildsystem::BuildFileLocation buildFileLocation, const rkit::CIPathView &shortName, ImageImportDisposition disposition);

		static rkit::Result CompileImage(const rkit::utils::IImage &image, const rkit::CIPathView &outPath,
			rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, ImageImportDisposition disposition);

	protected:
		TextureCompilerBase() {}
	};
} } // anox::buildsystem
