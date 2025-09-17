#include "AnoxModelCompiler.h"

#include "rkit/Core/Endian.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/Stream.h"

#include "anox/AnoxModule.h"

#include "AnoxMaterialCompiler.h"

namespace anox { namespace buildsystem
{
	class AnoxModelCompilerCommon
	{
	public:
		struct MD2Header
		{
			static const uint32_t kExpectedMagic = RKIT_FOURCC('I', 'D', 'P', '2');
			static const uint16_t kExpectedMajorVersion = 15;
			static const uint16_t kMaxMinorVersion = 2;

			rkit::endian::BigUInt32_t m_magic;
			rkit::endian::LittleUInt16_t m_majorVersion;
			rkit::endian::LittleUInt16_t m_minorVersion;

			rkit::endian::LittleUInt32_t m_textureWidth;
			rkit::endian::LittleUInt32_t m_textureHeight;

			rkit::endian::LittleUInt32_t m_frameSizeBytes;

			rkit::endian::LittleUInt32_t m_numTextures;

			rkit::endian::LittleUInt32_t m_numXYZ;
			rkit::endian::LittleUInt32_t m_numTexCoord;
			rkit::endian::LittleUInt32_t m_numTri;
			rkit::endian::LittleUInt32_t m_numGLCalls;
			rkit::endian::LittleUInt32_t m_numFrames;

			rkit::endian::LittleUInt32_t m_texturePos;
			rkit::endian::LittleUInt32_t m_texCoordPos;
			rkit::endian::LittleUInt32_t m_triPos;
			rkit::endian::LittleUInt32_t m_framePos;
			rkit::endian::LittleUInt32_t m_glCommandPos;
			rkit::endian::LittleUInt32_t m_eofPos;

			rkit::endian::LittleUInt32_t m_numTextureBlocks;
			rkit::endian::LittleUInt32_t m_textureBlockPos;

			rkit::endian::LittleFloat32_t m_scale[3];

			rkit::endian::LittleUInt32_t m_numTaggedSurf;
			rkit::endian::LittleUInt32_t m_taggedSurfPos;
		};

		struct MD2TextureDef
		{
			char m_textureName[64];
		};

		static rkit::Result AnalyzeMD2(const rkit::CIPathView &md2Path, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);
		static rkit::Result ResolveTexturePath(rkit::CIPath &textureDefPath, const rkit::CIPathView &md2Path, const MD2TextureDef &textureDef);
	};

	class AnoxMDACompiler final : public AnoxModelCompilerCommon, public AnoxMDACompilerBase
	{
	public:
		bool HasAnalysisStage() const override;
		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);

		virtual uint32_t GetVersion() const override;

	private:
		static bool ParseOneLine(rkit::ConstSpan<char> &outLine, rkit::ConstSpan<char> &fileSpan);
		static bool ParseOneLineBase(rkit::ConstSpan<char> &outLine, rkit::ConstSpan<char> &fileSpan);

		static bool ParseToken(rkit::ConstSpan<char> &outToken, rkit::ConstSpan<char> &line);

		static bool TokenIs(const rkit::ConstSpan<char> &token, const rkit::AsciiStringView &candidate);

		static rkit::Result ExtractPath(rkit::CIPath &path, const rkit::ConstSpan<char> &token, bool stripExtension);
	};

	class AnoxMD2Compiler final : public AnoxModelCompilerCommon, public AnoxMD2CompilerBase
	{
	public:
		bool HasAnalysisStage() const override;
		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);

		virtual uint32_t GetVersion() const override;
	};

	rkit::Result AnoxModelCompilerCommon::AnalyzeMD2(const rkit::CIPathView &md2Path, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::UniquePtr<rkit::ISeekableReadStream> inputFile;
		RKIT_CHECK(feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, md2Path, inputFile));

		MD2Header md2Header;
		RKIT_CHECK(inputFile->ReadAll(&md2Header, sizeof(MD2Header)));

		size_t numTextures = md2Header.m_numTextures.Get();

		rkit::Vector<MD2TextureDef> textures;
		RKIT_CHECK(textures.Resize(numTextures));

		RKIT_CHECK(inputFile->ReadAll(textures.GetBuffer(), sizeof(MD2TextureDef) * numTextures));

		for (const MD2TextureDef &textureDef : textures)
		{
			rkit::CIPath texturePath;
			RKIT_CHECK(ResolveTexturePath(texturePath, md2Path, textureDef));

			RKIT_CHECK(feedback->AddNodeDependency(kAnoxNamespaceID, kModelMaterialNodeID, rkit::buildsystem::BuildFileLocation::kSourceDir, texturePath.ToString()));
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxModelCompilerCommon::ResolveTexturePath(rkit::CIPath &textureDefPath, const rkit::CIPathView &md2Path, const MD2TextureDef &textureDef)
	{
		size_t strLength = 0;
		while (strLength < 64)
		{
			const char c = textureDef.m_textureName[strLength];
			if (c == '\0' || c == '.')
				break;
			else
				strLength++;
		}

		rkit::String materialPath;
		RKIT_CHECK(materialPath.Format("{}.{}", rkit::StringSliceView(textureDef.m_textureName, strLength), MaterialCompiler::GetModelMaterialExtension()));

		RKIT_CHECK(textureDefPath.Set(md2Path.AbsSlice(md2Path.NumComponents() - 1)));
		RKIT_CHECK(textureDefPath.AppendComponent(materialPath));

		return rkit::ResultCode::kOK;
	}

	bool AnoxMDACompiler::HasAnalysisStage() const
	{
		return true;
	}

	rkit::Result AnoxMDACompiler::RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::Vector<uint8_t> mdaBytes;

		{
			rkit::CIPath mdaPath;
			RKIT_CHECK(mdaPath.Set(depsNode->GetIdentifier()));

			rkit::UniquePtr<rkit::ISeekableReadStream> inputFile;
			RKIT_CHECK(feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, mdaPath, inputFile));

			RKIT_CHECK(rkit::GetDrivers().m_utilitiesDriver->ReadEntireFile(*inputFile, mdaBytes));
		}

		rkit::ConstSpan<char> fileSpan(reinterpret_cast<const char *>(mdaBytes.GetBuffer()), mdaBytes.Count());

		rkit::ConstSpan<char> line;
		if (!ParseOneLine(line, fileSpan) || !rkit::CompareSpansEqual(line, rkit::AsciiStringView("MDA1").ToSpan()))
			return rkit::ResultCode::kDataError;

		rkit::Optional<rkit::ConstSpan<char>> baseModel;

		while (ParseOneLine(line, fileSpan))
		{
			// Ignore this part for analysis
			if (line[0] == '$')
				continue;

			rkit::ConstSpan<char> token;
			if (ParseToken(token, line))
			{
				if (TokenIs(token, "basemodel"))
				{
					if (ParseToken(token, line))
					{
						rkit::CIPath fixedPath;
						RKIT_CHECK(ExtractPath(fixedPath, token, false));

						RKIT_CHECK(AnalyzeMD2(fixedPath, feedback));
					}
				}
				else if (TokenIs(token, "map") || TokenIs(token, "clampmap"))
				{
					if (ParseToken(token, line))
					{
						rkit::CIPath fixedPath;
						RKIT_CHECK(ExtractPath(fixedPath, token, true));

						rkit::String materialPath;
						RKIT_CHECK(materialPath.Format("{}.{}", fixedPath.ToString(), MaterialCompiler::GetModelMaterialExtension()));

						RKIT_CHECK(feedback->AddNodeDependency(kAnoxNamespaceID, kModelMaterialNodeID, rkit::buildsystem::BuildFileLocation::kSourceDir, materialPath));
					}
				}
			}
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxMDACompiler::RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	uint32_t AnoxMDACompiler::GetVersion() const
	{
		return 1;
	}

	bool AnoxMDACompiler::ParseOneLine(rkit::ConstSpan<char> &outLine, rkit::ConstSpan<char> &fileSpan)
	{
		while (ParseOneLineBase(outLine, fileSpan))
		{
			if (outLine.Count() == 0 || outLine[0] == '#')
				continue;

			return true;
		}

		return false;
	}

	bool AnoxMDACompiler::ParseOneLineBase(rkit::ConstSpan<char> &outLine, rkit::ConstSpan<char> &fileSpan)
	{
		size_t endPos = 0;
		size_t eolCharCount = 0;

		if (endPos == fileSpan.Count())
			return false;

		while (endPos < fileSpan.Count())
		{
			const char c = fileSpan[endPos];
			if (c == '\r')
			{
				eolCharCount++;
				if (endPos < fileSpan.Count() - 1)
				{
					if (fileSpan[endPos + 1] == '\n')
						eolCharCount++;
				}
				break;
			}
			else if (c == '\n')
			{
				eolCharCount++;
				break;
			}

			endPos++;
		}

		outLine = fileSpan.SubSpan(0, endPos);
		fileSpan = fileSpan.SubSpan(endPos + eolCharCount);

		return true;
	}

	bool AnoxMDACompiler::ParseToken(rkit::ConstSpan<char> &outToken, rkit::ConstSpan<char> &lineSpan)
	{
		size_t endPos = 0;
		size_t eolCharCount = 0;

		while (endPos < lineSpan.Count())
		{
			if (!rkit::IsASCIIWhitespace(lineSpan[endPos]))
				break;

			endPos++;
		}

		if (endPos == lineSpan.Count())
			return false;

		const size_t startPos = endPos;

		while (endPos < lineSpan.Count())
		{
			const char c = lineSpan[endPos];
			if (rkit::IsASCIIWhitespace(c))
				break;

			endPos++;
		}

		outToken = lineSpan.SubSpan(startPos, endPos - startPos);
		lineSpan = lineSpan.SubSpan(endPos);

		return true;
	}

	bool AnoxMDACompiler::TokenIs(const rkit::ConstSpan<char> &token, const rkit::AsciiStringView &candidate)
	{
		const size_t tokenLength = token.Count();

		if (tokenLength != candidate.Length())
			return false;

		for (size_t i = 0; i < tokenLength; i++)
		{
			if (rkit::InvariantCharCaseAdjuster<char>::ToLower(token[i]) != candidate[i])
				return false;
		}

		return true;
	}

	rkit::Result AnoxMDACompiler::ExtractPath(rkit::CIPath &path, const rkit::ConstSpan<char> &tokenRef, bool stripExtension)
	{
		rkit::ConstSpan<char> token = tokenRef;

		if (token.Count() >= 2 && token[0] == '\"' && token[token.Count() - 1] == '\"')
			token = token.SubSpan(1, token.Count() - 2);

		if (stripExtension)
		{
			for (size_t ri = 0; ri < token.Count(); ri++)
			{
				const size_t i = token.Count() - 1 - ri;
				if (token[i] == '.')
				{
					token = token.SubSpan(0, i);
					break;
				}
			}
		}

		if (token.Count() == 0)
			return rkit::ResultCode::kDataError;

		rkit::StringConstructionBuffer strBuf;
		RKIT_CHECK(strBuf.Allocate(token.Count()));

		const rkit::Span<char> strBufChars = strBuf.GetSpan();

		for (size_t i = 0; i < token.Count(); i++)
		{
			const char c = token[i];
			if (c == '\\')
				strBufChars[i] = '/';
			else
				strBufChars[i] = c;
		}

		rkit::String str(std::move(strBuf));

		if (rkit::CIPath::Validate(str) == rkit::PathValidationResult::kInvalid)
			return rkit::ResultCode::kDataError;

		return path.Set(str);
	}

	bool AnoxMD2Compiler::HasAnalysisStage() const
	{
		return true;
	}

	rkit::Result AnoxMD2Compiler::RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::CIPath md2Path;
		RKIT_CHECK(md2Path.Set(depsNode->GetIdentifier()));

		return AnoxModelCompilerCommon::AnalyzeMD2(md2Path, feedback);
	}

	rkit::Result AnoxMD2Compiler::RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	uint32_t AnoxMD2Compiler::GetVersion() const
	{
		return 1;
	}

	rkit::Result AnoxMDACompilerBase::Create(rkit::UniquePtr<AnoxMDACompilerBase> &outCompiler)
	{
		return rkit::New<AnoxMDACompiler>(outCompiler);
	}

	rkit::Result AnoxMD2CompilerBase::Create(rkit::UniquePtr<AnoxMD2CompilerBase> &outCompiler)
	{
		return rkit::New<AnoxMD2Compiler>(outCompiler);
	}
} }
