#pragma once

#include "rkit/BuildSystem/DependencyGraph.h"

#include "rkit/Core/FourCC.h"
#include "rkit/Core/StringProto.h"

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

		kCount,
	};

	class TextureCompilerBase : public rkit::buildsystem::IDependencyNodeCompiler
	{
	public:
		static rkit::Result Create(rkit::UniquePtr<TextureCompilerBase> &outCompiler);

		static rkit::Result CreateImportIdentifier(rkit::String &identifier, const rkit::StringView &imagePath, ImageImportDisposition disposition);

		static rkit::Result ResolveIntermediatePath(rkit::String &outString, const rkit::StringView &identifierWithDisposition);

	protected:
		TextureCompilerBase() {}
	};
} } // anox::buildsystem
