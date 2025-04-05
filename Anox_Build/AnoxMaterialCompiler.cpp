#include "AnoxMaterialCompiler.h"

#include "anox/AnoxModule.h"
#include "anox/Data/MaterialData.h"

#include "rkit/Data/ContentID.h"

#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/Path.h"
#include "rkit/Core/StaticArray.h"
#include "rkit/Core/String.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/Vector.h"

#include "AnoxTextureCompiler.h"

namespace anox::buildsystem
{
	struct MaterialAnalysisHeader
	{
		static const uint32_t kExpectedMagic = RKIT_FOURCC('A', 'M', 'T', 'L');
		static const uint32_t kExpectedVersion = 1;

		uint32_t m_magic;
		uint32_t m_version;
		uint32_t m_width;
		uint32_t m_height;

		data::MaterialType m_materialType;
		data::MaterialColorType m_colorType;
		bool m_mipMapped;
		bool m_bilinear;
	};

	struct MaterialAnalysisDynamicData
	{
		rkit::Vector<rkit::String> m_imageImports;
		rkit::Vector<anox::data::MaterialBitmapDef> m_bitmapDefs;
		rkit::Vector<anox::data::MaterialFrameDef> m_frameDefs;

		rkit::Result Serialize(rkit::IWriteStream &stream) const;
		rkit::Result Deserialize(rkit::IReadStream &stream);
	};

	rkit::Result MaterialAnalysisDynamicData::Serialize(rkit::IWriteStream &stream) const
	{
		uint64_t counts[] =
		{
			m_imageImports.Count(),
			m_bitmapDefs.Count(),
			m_frameDefs.Count(),
		};

		RKIT_CHECK(stream.WriteAll(counts, sizeof(counts)));

		for (const rkit::String &str : m_imageImports)
		{
			uint64_t strLength = str.Length();

			RKIT_CHECK(stream.WriteAll(&strLength, sizeof(strLength)));
			RKIT_CHECK(stream.WriteAll(str.CStr(), str.Length()));
		}

		const data::MaterialBitmapDef *bitmaps = m_bitmapDefs.GetBuffer();
		const data::MaterialFrameDef *frameDefs = m_frameDefs.GetBuffer();

		RKIT_CHECK(stream.WriteAll(bitmaps, sizeof(bitmaps[0]) * m_bitmapDefs.Count()));
		RKIT_CHECK(stream.WriteAll(frameDefs, sizeof(frameDefs[0]) * m_frameDefs.Count()));

		return rkit::ResultCode::kOK;
	}

	rkit::Result MaterialAnalysisDynamicData::Deserialize(rkit::IReadStream &stream)
	{
		rkit::StaticArray<uint64_t, 3> counts;

		RKIT_CHECK(stream.ReadAll(counts.GetBuffer(), sizeof(counts[0]) * counts.Count()));

		RKIT_CHECK(m_imageImports.Resize(counts[0]));
		RKIT_CHECK(m_bitmapDefs.Resize(counts[1]));
		RKIT_CHECK(m_frameDefs.Resize(counts[2]));

		for (rkit::String &str : m_imageImports)
		{
			uint64_t strLength64 = 0;

			RKIT_CHECK(stream.ReadAll(&strLength64, sizeof(strLength64)));

			if (strLength64 > std::numeric_limits<size_t>::max())
				return rkit::ResultCode::kIntegerOverflow;

			const size_t strLength = static_cast<size_t>(strLength64);

			rkit::StringConstructionBuffer scBuf;
			RKIT_CHECK(scBuf.Allocate(strLength));

			rkit::Span<char> scChars = scBuf.GetSpan();
			RKIT_CHECK(stream.ReadAll(scChars.Ptr(), scChars.Count()));

			str = rkit::String(std::move(scBuf));
		}

		data::MaterialBitmapDef *bitmaps = m_bitmapDefs.GetBuffer();
		data::MaterialFrameDef *frameDefs = m_frameDefs.GetBuffer();

		RKIT_CHECK(stream.ReadAll(bitmaps, sizeof(bitmaps[0]) * m_bitmapDefs.Count()));
		RKIT_CHECK(stream.ReadAll(frameDefs, sizeof(frameDefs[0]) * m_frameDefs.Count()));

		return rkit::ResultCode::kOK;
	}

	bool MaterialCompiler::HasAnalysisStage() const
	{
		return true;
	}

	rkit::Result MaterialCompiler::MaterialNodeTypeFromFourCC(MaterialNodeType &outNodeType, uint32_t nodeTypeFourCC)
	{
		switch (nodeTypeFourCC)
		{
		case anox::buildsystem::kFontMaterialNodeID:
			outNodeType = MaterialNodeType::kFont;
			break;
		default:
			return rkit::ResultCode::kInternalError;
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result MaterialCompiler::RunAnalyzeATD(const rkit::StringView &name, rkit::UniquePtr<rkit::ISeekableReadStream> &&atdStream, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result MaterialCompiler::ConstructAnalysisPath(rkit::CIPath &analysisPath, const rkit::StringView &identifier, MaterialNodeType nodeType)
	{
		rkit::String pathStr;
		RKIT_CHECK(pathStr.Format("anox/mat/%s.a%i", identifier.GetChars(), static_cast<int>(nodeType)));

		RKIT_CHECK(analysisPath.Set(pathStr));

		return rkit::ResultCode::kOK;
	}

	rkit::Result MaterialCompiler::RunAnalyzeImage(const rkit::StringView &longName, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		MaterialAnalysisHeader analysisHeader = {};
		analysisHeader.m_magic = MaterialAnalysisHeader::kExpectedMagic;
		analysisHeader.m_version = MaterialAnalysisHeader::kExpectedVersion;
		analysisHeader.m_bilinear = true;
		analysisHeader.m_mipMapped = true;

		analysisHeader.m_materialType = data::MaterialType::kSingle;
		analysisHeader.m_colorType = data::MaterialColorType::kRGBA;

		ImageImportDisposition disposition = ImageImportDisposition::kWorldAlphaTested;

		MaterialNodeType nodeType = MaterialNodeType::kCount;

		RKIT_CHECK(MaterialNodeTypeFromFourCC(nodeType, depsNode->GetDependencyNodeType()));

		// Don't fill in width and height
		switch (nodeType)
		{
		case MaterialNodeType::kFont:
			analysisHeader.m_bilinear = false;
			analysisHeader.m_mipMapped = false;
			disposition = ImageImportDisposition::kGraphicTransparent;
			break;
		default:
			return rkit::ResultCode::kInternalError;
		}

		MaterialAnalysisDynamicData dynamicData;

		RKIT_CHECK(dynamicData.m_imageImports.Resize(1));

		rkit::String &imageImport = dynamicData.m_imageImports[0];
		RKIT_CHECK(TextureCompilerBase::CreateImportIdentifier(imageImport, longName, disposition));

		RKIT_CHECK(dynamicData.m_bitmapDefs.Resize(1));

		data::MaterialBitmapDef &bitmapDef = dynamicData.m_bitmapDefs[0];
		bitmapDef.m_nameIndex = 0;

		RKIT_CHECK(dynamicData.m_frameDefs.Resize(1));
		data::MaterialFrameDef &frameDef = dynamicData.m_frameDefs[0];
		frameDef.m_bitmap = 0;
		frameDef.m_next = 0;
		frameDef.m_waitMSec = 0;
		frameDef.m_xOffset = 0;
		frameDef.m_yOffset = 0;

		rkit::CIPath analysisPath;
		RKIT_CHECK(ConstructAnalysisPath(analysisPath, depsNode->GetIdentifier(), nodeType));

		RKIT_CHECK(feedback->AddNodeDependency(kAnoxNamespaceID, kTextureNodeID, rkit::buildsystem::BuildFileLocation::kSourceDir, imageImport));

		rkit::UniquePtr<rkit::ISeekableReadWriteStream> analysisStream;
		RKIT_CHECK(feedback->OpenOutput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, analysisPath, analysisStream));

		RKIT_CHECK(analysisStream->WriteAll(&analysisHeader, sizeof(analysisHeader)));

		RKIT_CHECK(dynamicData.Serialize(*analysisStream));

		return rkit::ResultCode::kOK;
	}

	rkit::Result MaterialCompiler::ResolveShortName(rkit::String &shortName, const rkit::StringView &identifier)
	{
		// Remove extension
		rkit::Optional<size_t> extPos;
		for (size_t i = 0; i < identifier.Length(); i++)
		{
			const size_t pos = identifier.Length() - 1 - i;

			if (identifier[pos] == '/')
				break;

			if (identifier[pos] == '.')
			{
				extPos = pos;
				break;
			}
		}

		if (!extPos.IsSet())
		{
			rkit::log::ErrorFmt("Material '%s' didn't end with a material extension", identifier.GetChars());
			return rkit::ResultCode::kInternalError;
		}

		RKIT_CHECK(shortName.Set(identifier.SubString(0, extPos.Get())));

		return rkit::ResultCode::kOK;
	}

	rkit::Result MaterialCompiler::RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		const rkit::StringView identifier = depsNode->GetIdentifier();

		rkit::String shortName;
		RKIT_CHECK(ResolveShortName(shortName, identifier));

		// Check for ATD
		{
			rkit::String longName = shortName;
			RKIT_CHECK(longName.Append(".atd"));

			rkit::CIPath ciPath;
			RKIT_CHECK(ciPath.Set(longName));

			rkit::UniquePtr<rkit::ISeekableReadStream> atdStream;
			RKIT_CHECK(feedback->TryOpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, ciPath, atdStream));

			if (atdStream.IsValid())
				return RunAnalyzeATD(longName, std::move(atdStream), depsNode, feedback);
		}

		const rkit::StringView imageExtensions[] =
		{
			".png",
			".tga",
			".pcx",
		};

		for (const rkit::StringView &imageExt : imageExtensions)
		{
			rkit::String longName = shortName;
			RKIT_CHECK(longName.Append(imageExt));

			rkit::CIPath ciPath;
			RKIT_CHECK(ciPath.Set(longName));

			bool imageExists = false;
			RKIT_CHECK(feedback->CheckInputExists(rkit::buildsystem::BuildFileLocation::kSourceDir, ciPath, imageExists));

			if (imageExists)
			{
				return RunAnalyzeImage(longName, depsNode, feedback);
			}
		}

		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result MaterialCompiler::RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		MaterialNodeType nodeType = MaterialNodeType::kCount;

		RKIT_CHECK(MaterialNodeTypeFromFourCC(nodeType, depsNode->GetDependencyNodeType()));

		rkit::CIPath analysisPath;
		RKIT_CHECK(ConstructAnalysisPath(analysisPath, depsNode->GetIdentifier(), nodeType));

		MaterialAnalysisDynamicData dynamicData;

		{
			rkit::UniquePtr<rkit::ISeekableReadStream> analysisStream;
			RKIT_CHECK(feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, analysisPath, analysisStream));

			MaterialAnalysisHeader analysisHeader;
			RKIT_CHECK(analysisStream->ReadAll(&analysisHeader, sizeof(analysisHeader)));

			if (analysisHeader.m_magic != MaterialAnalysisHeader::kExpectedMagic || analysisHeader.m_version != MaterialAnalysisHeader::kExpectedVersion)
			{
				rkit::log::ErrorFmt("Material '%s' analysis file was invalid", depsNode->GetIdentifier().GetChars());
				return rkit::ResultCode::kOperationFailed;
			}

			RKIT_CHECK(dynamicData.Deserialize(*analysisStream));
		}


		return rkit::ResultCode::kNotYetImplemented;
	}

	uint32_t MaterialCompiler::GetVersion() const
	{
		return 1;
	}
}
