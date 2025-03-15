#include "AnoxTextureCompiler.h"

#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/String.h"

namespace anox::buildsystem
{
	bool TextureCompiler::HasAnalysisStage() const
	{
		return false;
	}

	rkit::Result TextureCompiler::RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		return rkit::ResultCode::kInternalError;
	}

	rkit::Result TextureCompiler::RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		const rkit::StringView identifier = depsNode->GetIdentifier();

		uint32_t dispositionUInt = 0;

		size_t dotPosition = identifier.Length();
		for (;;)
		{
			if (dotPosition == 0)
			{
				rkit::log::ErrorFmt("Texture job '%s' was invalid", identifier.GetChars());
				return rkit::ResultCode::kInternalError;
			}

			dotPosition--;

			if (identifier[dotPosition] == '.')
				break;

			char digit = identifier[dotPosition];
			if (digit < '0' || digit > '9')
			{
				rkit::log::ErrorFmt("Texture job '%s' was invalid", identifier.GetChars());
				return rkit::ResultCode::kInternalError;
			}

			dispositionUInt = dispositionUInt * 10 + (identifier[dotPosition] - '0');

			if (dispositionUInt >= static_cast<uint32_t>(ImageImportDisposition::kCount))
			{
				rkit::log::ErrorFmt("Texture job '%s' was invalid", identifier.GetChars());
				return rkit::ResultCode::kInternalError;
			}
		}

		ImageImportDisposition disposition = static_cast<ImageImportDisposition>(dispositionUInt);

		rkit::String shortName;
		RKIT_CHECK(shortName.Set(identifier.SubString(0, dotPosition)));

		const size_t dispositionDotPos = dotPosition;
		for (;;)
		{
			if (dotPosition == 0)
			{
				rkit::log::ErrorFmt("Texture job '%s' was invalid", identifier.GetChars());
				return rkit::ResultCode::kInternalError;
			}

			dotPosition--;

			if (identifier[dotPosition] == '.')
				break;
		}

		rkit::StringSliceView extension = identifier.SubString(dotPosition, dispositionDotPos - dotPosition);

		if (extension == ".pcx")
			return CompilePCX(depsNode, feedback, shortName, disposition);

		if (extension == ".png")
			return CompilePNG(depsNode, feedback, shortName, disposition);

		if (extension == ".tga")
			return CompileTGA(depsNode, feedback, shortName, disposition);

		rkit::log::ErrorFmt("Texture job '%s' used an unsupported format", identifier.GetChars());
		return rkit::ResultCode::kOperationFailed;
	}

	rkit::Result TextureCompiler::CompileTGA(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, const rkit::StringView &shortName, ImageImportDisposition disposition)
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result TextureCompiler::CompilePCX(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, const rkit::StringView &shortName, ImageImportDisposition disposition)
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result TextureCompiler::CompilePNG(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, const rkit::StringView &shortName, ImageImportDisposition disposition)
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	uint32_t TextureCompiler::GetVersion() const
	{
		return 1;
	}

	rkit::Result TextureCompiler::CreateImportIdentifier(rkit::String &result, const rkit::StringView &imagePath, ImageImportDisposition disposition)
	{
		return result.Format("%s.%i", imagePath.GetChars(), static_cast<int>(disposition));
	}
}
