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
		kWorldAlphaBlend,
		kWorldAlphaTested,
		kGraphicTransparent,
		kGraphicOpaque,
		kInterformPalette,

		kCount,
	};

	class TextureCompilerBase : public rkit::buildsystem::IDependencyNodeCompiler
	{
	public:
		static rkit::Result Create(rkit::UniquePtr<TextureCompilerBase> &outCompiler);

		static rkit::Result CreateImportIdentifier(rkit::String &identifier, const rkit::StringView &imagePath, ImageImportDisposition disposition);

	protected:
		TextureCompilerBase() {}
	};
}
