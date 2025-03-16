#include "AnoxMaterialCompiler.h"

#include "anox/AnoxModule.h"
#include "anox/Data/MaterialData.h"

#include "rkit/Data/ContentID.h"

#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Optional.h"
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

	rkit::Result MaterialCompiler::ConstructAnalysisPath(rkit::String &analysisPath, const rkit::StringView &identifier, MaterialNodeType nodeType)
	{
		return analysisPath.Format("anox/mat/%s.a%i", identifier.GetChars(), static_cast<int>(nodeType));
	}

	rkit::Result MaterialCompiler::RunAnalyzeImage(const rkit::StringView &name, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
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
		RKIT_CHECK(TextureCompilerBase::CreateImportIdentifier(imageImport, name, disposition));

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

		rkit::String analysisPath;
		RKIT_CHECK(ConstructAnalysisPath(analysisPath, name, nodeType));

		RKIT_CHECK(feedback->AddNodeDependency(kAnoxNamespaceID, kTextureNodeID, rkit::buildsystem::BuildFileLocation::kSourceDir, imageImport));

		rkit::UniquePtr<rkit::ISeekableReadWriteStream> analysisStream;
		RKIT_CHECK(feedback->OpenOutput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, analysisPath, analysisStream));

		RKIT_CHECK(analysisStream->WriteAll(&analysisHeader, sizeof(analysisHeader)));

		RKIT_CHECK(dynamicData.Serialize(*analysisStream));

		return rkit::ResultCode::kOK;
	}

	rkit::Result MaterialCompiler::RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		// Remove extension
		rkit::Optional<size_t> extPos;
		const rkit::StringView identifier = depsNode->GetIdentifier();
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
			rkit::log::ErrorFmt("Material '%s' didn't end with a material extension", depsNode->GetIdentifier().GetChars());
			return rkit::ResultCode::kInternalError;
		}

		rkit::String shortName;
		RKIT_CHECK(shortName.Set(identifier.SubString(0, extPos.Get())));

		// Check for ATD
		{
			rkit::String longName = shortName;
			RKIT_CHECK(longName.Append(".atd"));

			rkit::UniquePtr<rkit::ISeekableReadStream> atdStream;
			RKIT_CHECK(feedback->TryOpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, longName, atdStream));

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

			bool imageExists = false;
			RKIT_CHECK(feedback->CheckInputExists(rkit::buildsystem::BuildFileLocation::kSourceDir, longName, imageExists));

			if (imageExists)
			{
				return RunAnalyzeImage(longName, depsNode, feedback);
			}
		}

		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result MaterialCompiler::RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	uint32_t MaterialCompiler::GetVersion() const
	{
		return 1;
	}
}
