#include "AnoxModelCompiler.h"

#include "rkit/Core/DeduplicatedList.h"
#include "rkit/Core/Endian.h"
#include "rkit/Core/FPControl.h"
#include "rkit/Core/HashTable.h"
#include "rkit/Core/BoolVector.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/Pair.h"
#include "rkit/Core/Platform.h"
#include "rkit/Core/QuickSort.h"
#include "rkit/Core/Stream.h"

#include "rkit/Data/ContentID.h"
#include "rkit/Math/Vec.h"
#include "rkit/Math/Quat.h"

#include "anox/AnoxModule.h"
#include "anox/Data/MDAModel.h"
#include "anox/Data/MaterialData.h"

#include "AnoxMaterialCompiler.h"
#include "AnoxPrecompiledMesh.h"

#include <math.h>

namespace anox { namespace buildsystem
{
	class AnoxModelCompilerCommon
	{
	public:
		static rkit::Result ConstructOutputPathByType(rkit::CIPath &outPath, const rkit::StringView &outputType, const rkit::StringView &identifier);

	protected:
		struct MD2Header
		{
			static const uint32_t kExpectedMagic = RKIT_FOURCC('I', 'D', 'P', '2');
			static const uint16_t kMinMajorVersion = 14;
			static const uint16_t kMaxMajorVersion = 15;
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

		struct MD2FrameHeader
		{
			rkit::endian::LittleFloat32_t m_scale[3];
			rkit::endian::LittleFloat32_t m_translate[3];
			char m_name[16] = {};
		};

		struct MDAChunkHeaderData
		{
			rkit::endian::LittleUInt32_t m_magic;
			rkit::endian::LittleUInt32_t m_modelChecksum;
		};

		struct MDAAnimationChunkData
		{
			static const uint32_t kExpectedMagic = 0xffee1acbu;

			MDAChunkHeaderData m_header;
			rkit::endian::LittleUInt32_t m_numAnimInfos;

			// MDAAnimInfo m_animations[m_numAnimInfos]
		};

		struct MDAAnimInfoData
		{
			char m_animCategory[8] = {};
			rkit::endian::LittleUInt32_t m_animNumber;
			rkit::endian::LittleUInt32_t m_firstFrame;
			rkit::endian::LittleUInt32_t m_numFrames;
		};

		struct MDABoneFrame
		{
			rkit::endian::LittleFloat32_t m_matrix[16];
		};

		struct MDABoneChunkData
		{
			static const uint32_t kExpectedMagic = 0xad1f10edu;

			MDAChunkHeaderData m_header;
			rkit::endian::LittleUInt32_t m_triVerts[3];
			rkit::endian::BigUInt32_t m_boneID;

			// MDAMatrix44 m_frames[NumFrames]   // Not normalized?
		};

		struct MDAVertMorph
		{
			rkit::endian::LittleUInt32_t m_vertIndex;
			rkit::endian::LittleFloat32_t m_offset[3];
		};

		struct MDAMorphChunkData
		{
			static const uint32_t kExpectedMagic = 0xad1f10edu;

			MDAChunkHeaderData m_header;
			rkit::endian::BigUInt32_t m_headBoneID;
			rkit::endian::LittleUInt32_t m_numKeys;

			// MDAMorphKey m_morphKeys[m_numKeys]
			// MDAVertMorph m_morphs[m_numMorphs]
		};

		struct MDAMorphKey
		{
			rkit::endian::BigUInt32_t m_morphID;
			rkit::endian::LittleUInt32_t m_dataPosition;
			rkit::endian::LittleUInt32_t m_numMorphs;
		};

		struct UncompiledMDAPass
		{
			rkit::CIPath m_map;
			bool m_clamp = false;
			float m_scrollU = 0.f;
			float m_scrollV = 0.f;
			rkit::Optional<data::MDAAlphaTestMode> m_alphaTestMode;
			rkit::Optional<data::MDABlendMode> m_blendMode;
			rkit::Optional<data::MDAUVGenMode> m_uvGenMode;
			rkit::Optional<data::MDADepthFunc> m_depthFunc;
			rkit::Optional<data::MDACullType> m_cullType;
			rkit::Optional<data::MDARGBGenMode> m_rgbGenMode;
			rkit::Optional<bool> m_depthWrite;
		};

		struct UncompiledMDASkin
		{
			rkit::Vector<UncompiledMDAPass> m_passes;
			rkit::Optional<data::MDASortMode> m_sortMode;
		};

		struct UncompiledMDAProfile
		{
			rkit::AsciiString m_evaluate;
			rkit::Vector<UncompiledMDASkin> m_skins;
			uint32_t m_fourCC = 0;
		};

		struct UncompiledMDAData
		{
			rkit::CIPath m_baseModel;

			rkit::Optional<rkit::StaticArray<uint32_t, 3>> m_headTri;

			rkit::Vector<UncompiledMDAProfile> m_profiles;

			rkit::Vector<uint8_t> m_animChunk;
			rkit::Vector<uint8_t> m_morphChunk;
			rkit::Vector<uint8_t> m_boneChunk;
		};

		struct UncompiledMDAVertMorph
		{
			uint32_t m_vertIndex = 0;
			rkit::StaticArray<float, 3> m_delta;
		};

		struct UncompiledMDAMorph
		{
			rkit::Vector<UncompiledMDAVertMorph> m_vertMorphs;
		};

		struct MDABase64Chunk
		{
			uint32_t m_fourCC = 0;
			uint32_t m_size = 0;
			uint32_t m_paddedSize = 0;
			uint32_t m_adler32Checksum = 0;

			rkit::Vector<char> m_base64Chars;
		};

		struct MD2TextureDef
		{
			char m_textureName[64];
		};

		struct UncompiledMDASubmodel
		{
			rkit::Vector<data::MDAModelTri> m_tris;
			rkit::Vector<data::MDAModelVert> m_verts;
		};

		struct UncompiledTriList
		{
			rkit::Vector<ModelProtoVert> m_verts;

			rkit::Vector<UncompiledMDASubmodel> m_submodels;
		};

		static rkit::Result AnalyzeMD2File(rkit::ISeekableReadStream &inputFile, const rkit::CIPathView &md2Path, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);
		static rkit::Result AnalyzeMD2(const rkit::CIPathView &md2Path, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);
		static rkit::Result ResolveTexturePath(rkit::CIPath &textureDefPath, const rkit::CIPathView &md2Path, const MD2TextureDef &textureDef, bool constructFullMaterialPath);

		static rkit::Result CompileMDA(rkit::CIPath &outputPath, UncompiledMDAData &mdaData, bool autoSkin, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);
		static rkit::Result CompileMDASubmodels(UncompiledTriList &triList, rkit::Span<uint32_t> xyzToPointID);
		static rkit::Result CompileMDASubmodel(bool &outEmittedAnything, UncompiledMDASubmodel &outSubmodel, UncompiledTriList &triList, rkit::Span<uint32_t> xyzToPointID, rkit::BoolVector &triEmitted);

		static uint16_t CompressUV(uint32_t floatBits);
	};

	class AnoxMDACompiler final : public AnoxModelCompilerCommon, public AnoxMDACompilerBase
	{
	public:
		bool HasAnalysisStage() const override;
		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;

		virtual uint32_t GetVersion() const override;

	private:
		static rkit::Result ExpectLine(rkit::ConstSpan<char> &outLine, rkit::ConstSpan<char> &fileSpan);
		static bool ParseOneLine(rkit::ConstSpan<char> &outLine, rkit::ConstSpan<char> &fileSpan);
		static bool ParseOneLineBase(rkit::ConstSpan<char> &outLine, rkit::ConstSpan<char> &fileSpan);

		static rkit::Result ExpectTokenIs(rkit::ConstSpan<char> &token, const rkit::AsciiStringView &candidate);

		static rkit::Result ExpectToken(rkit::ConstSpan<char> &outToken, rkit::ConstSpan<char> &line);
		static bool ParseToken(rkit::ConstSpan<char> &outToken, rkit::ConstSpan<char> &line);

		static bool TokenIs(const rkit::ConstSpan<char> &token, const rkit::AsciiStringView &candidate);

		static rkit::Result ExtractPath(rkit::CIPath &path, const rkit::ConstSpan<char> &token, bool stripExtension);

		static rkit::Result OpenMDAPath(rkit::UniquePtr<rkit::ISeekableReadStream> &outInputFile, bool &outIsActuallyMD2, rkit::CIPath &inOutPath, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);
	};

	class AnoxMD2Compiler final : public AnoxModelCompilerCommon, public AnoxMD2CompilerBase
	{
	public:
		bool HasAnalysisStage() const override;
		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;

		virtual uint32_t GetVersion() const override;
	};

	class AnoxCTCCompiler final : public AnoxModelCompilerCommon, public AnoxCTCCompilerBase
	{
	public:
		bool HasAnalysisStage() const override;
		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;

		virtual uint32_t GetVersion() const override;

	private:
		struct CTCHeader
		{
			static const uint32_t kExpectedMagic = RKIT_FOURCC('S', 'M', 'D', 'L');
			static const uint32_t kExpectedVersion = 11;

			rkit::endian::BigUInt32_t m_magic;
			rkit::endian::LittleUInt32_t m_version;
			rkit::endian::LittleUInt32_t m_skinWidth;
			rkit::endian::LittleUInt32_t m_skinHeight;
			rkit::endian::LittleUInt32_t m_frameSize;
			rkit::endian::LittleUInt32_t m_numTextures;
			rkit::endian::LittleUInt32_t m_numPoints;
			rkit::endian::LittleUInt32_t m_numTexCoords;
			rkit::endian::LittleUInt32_t m_numTris;
			rkit::endian::LittleUInt32_t m_numGLCommands;
			rkit::endian::LittleUInt32_t m_numFrames;
			rkit::endian::LittleUInt32_t m_numAnimations_Size24;	// ???
			rkit::endian::LittleUInt32_t m_numBones;
			rkit::endian::LittleUInt32_t m_numMorphs;
			rkit::endian::LittleUInt32_t m_numAttachments;
			rkit::endian::LittleUInt32_t m_numGLCommandDWords;
			rkit::endian::LittleUInt32_t m_numUnknown5;
			rkit::endian::LittleFloat32_t m_unknownFloat;
			rkit::endian::LittleUInt32_t m_numVertColors;

			rkit::endian::LittleUInt32_t m_texturesPos;
			rkit::endian::LittleUInt32_t m_pointsPos;
			rkit::endian::LittleUInt32_t m_internalOutputPointsPos;
			rkit::endian::LittleUInt32_t m_triVertIndexesPos;
			rkit::endian::LittleUInt32_t m_triTexCoordsPos;
			rkit::endian::LittleUInt32_t m_bonesPos;
			rkit::endian::LittleUInt32_t m_skeletalDataPos;
			rkit::endian::LittleUInt32_t m_animationPos;
			rkit::endian::LittleUInt32_t m_frameDataPos;
			rkit::endian::LittleUInt32_t m_legacyGLCommandsPos;
			rkit::endian::LittleUInt32_t m_morphsPos;
			rkit::endian::LittleUInt32_t m_attachmentsPos;
			rkit::endian::LittleUInt32_t m_triListSizesPos;
			rkit::endian::LittleUInt32_t m_unknown5Pos;
			rkit::endian::LittleUInt32_t m_unknown5TrisOutputPos;
			rkit::endian::LittleUInt32_t m_vertColorsPos;
			rkit::endian::LittleUInt32_t m_eofPos;
		};

		struct CTCCompoundTriVert
		{
			uint16_t m_inPointIndex;
			rkit::StaticArray<uint16_t, 2> m_compressedUV;

			bool operator==(const CTCCompoundTriVert &other) const;
			bool operator!=(const CTCCompoundTriVert &other) const;
		};

		struct CTCAnimation
		{
			char m_name[16];
			rkit::endian::LittleUInt32_t m_unknown;	// Possibly flags
			rkit::endian::LittleUInt32_t m_frameCount;	// Seems to maybe not be frame count...?
		};

		struct CTCMatrix44
		{
			rkit::StaticArray<rkit::StaticArray<rkit::endian::LittleFloat32_t, 4>, 4> m_rows;
		};

		struct CTCBoneKey
		{
			rkit::StaticArray<rkit::endian::LittleFloat32_t, 3> m_pos;
			rkit::StaticArray<rkit::endian::LittleFloat32_t, 3> m_angles;
		};

		struct CTCBone
		{
			rkit::endian::LittleUInt32_t m_parentIndexPlusOne;
			rkit::endian::LittleUInt32_t m_childCount;
			rkit::endian::LittleUInt32_t m_internal_childListPtr;

			rkit::endian::LittleFloat32_t m_internal_unknown[6];
			CTCMatrix44 m_internal_finalTransform;
			CTCMatrix44 m_baseTransform;
		};

		struct CTCPoint
		{
			rkit::StaticArray<rkit::endian::LittleFloat32_t, 3> m_pos;
			rkit::StaticArray<rkit::endian::LittleFloat32_t, 3> m_normal;
		};

		struct CTCBoneAndWeight
		{
			rkit::endian::LittleUInt32_t m_boneIndex;
			rkit::endian::LittleFloat32_t m_weight;
		};

		struct CTCTriVerts
		{
			rkit::StaticArray<rkit::endian::LittleUInt16_t, 3> m_verts;
		};

		struct CTCTexCoords
		{
			rkit::StaticArray<rkit::endian::LittleFloat32_t, 2> m_uvFloatBits;
		};

		struct CTCTriTexCoords
		{
			rkit::StaticArray<CTCTexCoords, 3> m_texCoords;
		};

		struct CTCMorph
		{
			char m_name[16];

			rkit::endian::LittleUInt32_t m_numVertMorphs;
			rkit::endian::LittleFloat32_t m_internal_lerpFactor;
			rkit::endian::LittleUInt32_t m_internal_vertIndexPtr;
			rkit::endian::LittleUInt32_t m_internal_morphDeltaPtr;
		};

		struct VertMorph
		{
			rkit::endian::LittleUInt32_t m_pointIndex;
			rkit::StaticArray<rkit::endian::LittleFloat32_t, 3> m_targetPosition;
		};

		struct Morph
		{
			char m_name[17];

			rkit::Vector<VertMorph> m_vertMorphs;
		};

		struct MorphData
		{
			rkit::Vector<Morph> m_morphs;
		};

		// This is, functionally, T*R
		struct Matrix3x4
		{
			rkit::StaticArray<rkit::StaticArray<float, 4>, 3> m_rows;
		};

		static rkit::Result AddTriClusters(
			rkit::Vector<data::MDAModelSubModel> &outSubModels,
			rkit::Vector<data::MDAModelTri> &outTris,
			rkit::Vector<data::MDAModelVert> &outVerts,
			uint32_t materialID,
			rkit::ConstSpan<uint32_t> inVertToOutVert,
			rkit::ConstSpan<CTCTriVerts> triVerts, rkit::ConstSpan<CTCTriTexCoords> triTexCoords, size_t numTris);


		static rkit::Result AddOneTriCluster(
			rkit::Vector<data::MDAModelTri> &outTris,
			rkit::Vector<data::MDAModelVert> &outVerts,
			rkit::ConstSpan<uint32_t> inVertToOutVert,
			rkit::ConstSpan<CTCTriVerts> triVerts, rkit::ConstSpan<CTCTriTexCoords> triTexCoords, size_t numTris,
			rkit::BoolSpan triAdded);

		static Matrix3x4 OrthoMatrixMul(const Matrix3x4 &a, const Matrix3x4 &b);
		static Matrix3x4 OrthoInvertMatrix(const Matrix3x4 &inMat);
		static Matrix3x4 BoneKeyToMatrix(const CTCBoneKey &key);
		static Matrix3x4 GenerateRotationMatrix(float degrees, float x, float y, float z);
		static Matrix3x4 GenerateTranslationMatrix(float x, float y, float z);
		static Matrix3x4 GenerateIdentityMatrix();
		static Matrix3x4 QuatToMatrix(float x, float y, float z, float w);
		static void MatrixToQuat(const Matrix3x4 &mat, float &x, float &y, float &z, float &w);
		static data::MDAModelSkeletalBoneFrame ConvertMatrixToFrame(const Matrix3x4 &mat);
		static Matrix3x4 ConvertFrameToMatrix(const data::MDAModelSkeletalBoneFrame &boneFrame);
	};

	rkit::Result AnoxModelCompilerCommon::ConstructOutputPathByType(rkit::CIPath &outPath, const rkit::StringView &outputType, const rkit::StringView &identifier)
	{
		rkit::String str;
		RKIT_CHECK(str.Format(u8"ax_mdl_{}/{}", outputType, identifier));

		return outPath.Set(str);
	}


	rkit::Result AnoxModelCompilerCommon::AnalyzeMD2(const rkit::CIPathView &md2Path, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::UniquePtr<rkit::ISeekableReadStream> inputFile;
		RKIT_CHECK(feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, md2Path, inputFile));

		return AnalyzeMD2File(*inputFile, md2Path, feedback);
	}

	rkit::Result AnoxModelCompilerCommon::AnalyzeMD2File(rkit::ISeekableReadStream &inputFile, const rkit::CIPathView &md2Path, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		MD2Header md2Header;
		RKIT_CHECK(inputFile.ReadAll(&md2Header, sizeof(MD2Header)));

		size_t numTextures = md2Header.m_numTextures.Get();

		rkit::Vector<MD2TextureDef> textures;
		RKIT_CHECK(textures.Resize(numTextures));

		RKIT_CHECK(inputFile.ReadAll(textures.GetBuffer(), sizeof(MD2TextureDef) * numTextures));

		for (const MD2TextureDef &textureDef : textures)
		{
			rkit::CIPath texturePath;
			RKIT_CHECK(ResolveTexturePath(texturePath, md2Path, textureDef, true));

			RKIT_CHECK(feedback->AddNodeDependency(kAnoxNamespaceID, kModelMaterialNodeID, rkit::buildsystem::BuildFileLocation::kSourceDir, texturePath.ToString()));
		}

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxModelCompilerCommon::ResolveTexturePath(rkit::CIPath &textureDefPath, const rkit::CIPathView &md2Path, const MD2TextureDef &textureDef, bool constructFullMaterialPath)
	{
		size_t strLength = 0;
		size_t lastDotPosPlusOne = 0;
		while (strLength < 64)
		{
			const char c = textureDef.m_textureName[strLength];
			if (c == '\0')
				break;
			else
			{
				strLength++;

				if (c == '.')
					lastDotPosPlusOne = strLength;
			}
		}

		if (lastDotPosPlusOne != 0)
			strLength = lastDotPosPlusOne - 1;

		rkit::StringSliceView materialPathView = rkit::AsciiStringSliceView(textureDef.m_textureName, strLength).ToUTF8();
		rkit::String materialPathTemp;
		if (constructFullMaterialPath)
		{
			RKIT_CHECK(materialPathTemp.Format(u8"{}.{}", materialPathView.ToUTF8(), MaterialCompiler::GetModelMaterialExtension()));
			materialPathView = materialPathTemp;
		}

		RKIT_CHECK(textureDefPath.Set(md2Path.AbsSlice(md2Path.NumComponents() - 1)));
		RKIT_CHECK(textureDefPath.AppendComponent(materialPathView.ToUTF8()));

		RKIT_RETURN_OK;
	}

	bool AnoxMDACompiler::HasAnalysisStage() const
	{
		return true;
	}

	rkit::Result AnoxMDACompiler::OpenMDAPath(rkit::UniquePtr<rkit::ISeekableReadStream> &outInputFile, bool &outIsActuallyMD2, rkit::CIPath &inOutPath, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		outIsActuallyMD2 = false;
		outInputFile.Reset();

		RKIT_CHECK(feedback->TryOpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, inOutPath, outInputFile));

		if (!outInputFile.IsValid())
		{
			if (inOutPath.ToString().EndsWithNoCase(u8".mda"))
			{
				rkit::String md2PathStr;
				RKIT_CHECK(md2PathStr.Set(inOutPath.ToString().SubString(0, inOutPath.Length() - 4)));
				RKIT_CHECK(md2PathStr.Append(u8".md2"));

				rkit::CIPath md2Path;
				RKIT_CHECK(md2Path.Set(md2PathStr));

				RKIT_CHECK(feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, md2Path, outInputFile));

				inOutPath = md2Path;
				outIsActuallyMD2 = true;
				RKIT_RETURN_OK;
			}

			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxMDACompiler::RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::Vector<uint8_t> mdaBytes;

		{
			rkit::CIPath mdaPath;
			RKIT_CHECK(mdaPath.Set(depsNode->GetIdentifier()));

			rkit::UniquePtr<rkit::ISeekableReadStream> inputFile;
			RKIT_CHECK(feedback->TryOpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, mdaPath, inputFile));

			bool isActuallyMD2 = false;
			RKIT_CHECK(OpenMDAPath(inputFile, isActuallyMD2, mdaPath, feedback));

			if (isActuallyMD2)
				return AnalyzeMD2File(*inputFile, mdaPath, feedback);

			RKIT_CHECK(rkit::GetDrivers().m_utilitiesDriver->ReadEntireFile(*inputFile, mdaBytes));
		}

		rkit::ConstSpan<char> fileSpan(reinterpret_cast<const char *>(mdaBytes.GetBuffer()), mdaBytes.Count());

		rkit::ConstSpan<char> line;
		if (!ParseOneLine(line, fileSpan) || !rkit::CompareSpansEqual(line, rkit::AsciiStringView("MDA1").ToSpan()))
			RKIT_THROW(rkit::ResultCode::kDataError);

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
						RKIT_CHECK(materialPath.Format(u8"{}.{}", fixedPath.ToString(), MaterialCompiler::GetModelMaterialExtension()));

						RKIT_CHECK(feedback->AddNodeDependency(kAnoxNamespaceID, kModelMaterialNodeID, rkit::buildsystem::BuildFileLocation::kSourceDir, materialPath));
					}
				}
			}
		}

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxMDACompiler::RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::FloatingPointStateScope scope(rkit::FloatingPointState::GetCurrent().AppendIEEEStrict());

		rkit::UniquePtr<rkit::ISeekableReadStream> inputFile;
		rkit::Vector<uint8_t> mdaBytes;

		rkit::IUtilitiesDriver *utils = rkit::GetDrivers().m_utilitiesDriver;

		{
			rkit::CIPath path;
			RKIT_CHECK(path.Set(depsNode->GetIdentifier()));

			bool isActuallyMD2 = false;
			RKIT_CHECK(OpenMDAPath(inputFile, isActuallyMD2, path, feedback));

			if (isActuallyMD2)
			{
				UncompiledMDAData mdaData;
				mdaData.m_baseModel = path;

				// Construct from the identifier since path is changed at this point,
				// we want to keep the original suffix
				rkit::CIPath outputPath;
				RKIT_CHECK(ConstructOutputPath(outputPath, depsNode->GetIdentifier()));

				return CompileMDA(outputPath, mdaData, true, depsNode, feedback);
			}

			RKIT_CHECK(feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, path, inputFile));

			RKIT_CHECK(utils->ReadEntireFile(*inputFile, mdaBytes));
		}

		rkit::ConstSpan<char> fileSpan = mdaBytes.ToSpan().ReinterpretCast<char>();

		UncompiledMDAData mdaData = {};

		rkit::ConstSpan<char> line;
		if (!ParseOneLine(line, fileSpan) || TokenIs(line, "MDA1"))
		{
			rkit::log::Error(u8"Missing MDA header");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		rkit::Vector<MDABase64Chunk> base64Chunks;

		while (ParseOneLine(line, fileSpan))
		{
			if (line[0] == '$')
			{
				// Data chunk start
				if (line.Count() != 32)
				{
					rkit::log::Error(u8"Malformed MDA chunk");
					RKIT_THROW(rkit::ResultCode::kDataError);
				}

				uint32_t dwords[3] = { 0, 0, 0 };

				MDABase64Chunk base64Chunk;
				base64Chunk.m_fourCC = rkit::utils::ComputeFourCC(line[1], line[2], line[3], line[4]);

				for (size_t dwordIndex = 0; dwordIndex < 3; dwordIndex++)
				{
					const size_t startPos = dwordIndex * 9 + 5;

					if (line[startPos] != ':')
					{
						rkit::log::Error(u8"Malformed MDA chunk");
						RKIT_THROW(rkit::ResultCode::kDataError);
					}

					for (size_t nibbleIndex = 0; nibbleIndex < 8; nibbleIndex++)
					{
						const char nibbleChar = line[startPos + 1 + nibbleIndex];

						uint8_t nibble = 0;
						if (nibbleChar >= '0' && nibbleChar <= '9')
							nibble = static_cast<uint8_t>(nibbleChar - '0');
						else if (nibbleChar >= 'a' && nibbleChar <= 'f')
							nibble = static_cast<uint8_t>(nibbleChar - 'a' + 0xa);
						else if (nibbleChar >= 'A' && nibbleChar <= 'F')
							nibble = static_cast<uint8_t>(nibbleChar - 'A' + 0xA);
						else
						{
							rkit::log::Error(u8"Malformed MDA chunk");
							RKIT_THROW(rkit::ResultCode::kDataError);
						}

						dwords[dwordIndex] = dwords[dwordIndex] * 0x10u + nibble;
					}
				}

				base64Chunk.m_size = dwords[0];
				base64Chunk.m_paddedSize = dwords[1];
				base64Chunk.m_adler32Checksum = dwords[2];

				RKIT_CHECK(base64Chunks.Append(std::move(base64Chunk)));

				continue;
			}
			else if (line[0] == '&')
			{
				if (base64Chunks.Count() == 0)
				{
					rkit::log::Error(u8"MDA base64 data appeared after header");
					RKIT_THROW(rkit::ResultCode::kDataError);
				}

				RKIT_CHECK(base64Chunks[base64Chunks.Count() - 1].m_base64Chars.Append(line.SubSpan(1)));

				continue;
			}

			rkit::ConstSpan<char> token;
			if (ParseToken(token, line))
			{
				if (TokenIs(token, "basemodel"))
				{
					if (ParseToken(token, line))
					{
						rkit::CIPath fixedPath;
						RKIT_CHECK(ExtractPath(fixedPath, token, false));

						mdaData.m_baseModel = fixedPath;
					}
				}
				else if (TokenIs(token, "headtri"))
				{
					rkit::ConstSpan<char> triVert0Token;
					rkit::ConstSpan<char> triVert1Token;
					rkit::ConstSpan<char> triVert2Token;

					rkit::StaticArray<uint32_t, 3> triVerts;

					if (!ParseToken(triVert0Token, line)
						|| !ParseToken(triVert1Token, line)
						|| !ParseToken(triVert2Token, line)
						|| !utils->ParseUInt32(rkit::AsciiStringSliceView(triVert0Token).RemoveEncoding(), 10, triVerts[0])
						|| !utils->ParseUInt32(rkit::AsciiStringSliceView(triVert1Token).RemoveEncoding(), 10, triVerts[1])
						|| !utils->ParseUInt32(rkit::AsciiStringSliceView(triVert2Token).RemoveEncoding(), 10, triVerts[2])
						)
					{
						rkit::log::Error(u8"Malformed headtri directive");
						RKIT_THROW(rkit::ResultCode::kDataError);
					}

					mdaData.m_headTri = triVerts;
				}
				else if (TokenIs(token, "profile"))
				{
					uint32_t profileFourCC = RKIT_FOURCC('D', 'F', 'L', 'T');

					UncompiledMDAProfile profile = {};

					if (ParseToken(token, line))
					{
						if (token.Count() != 4)
						{
							rkit::log::Error(u8"Malformed profile directive");
							RKIT_THROW(rkit::ResultCode::kDataError);
						}

						profileFourCC = rkit::utils::ComputeFourCC(token[0], token[1], token[2], token[3]);
					}

					RKIT_CHECK(ExpectLine(line, fileSpan));
					RKIT_CHECK(ExpectTokenIs(line, "{"));

					for (;;)
					{
						RKIT_CHECK(ExpectLine(line, fileSpan));

						if (TokenIs(line, "}"))
							break;

						RKIT_CHECK(ExpectToken(token, line));

						if (TokenIs(token, "skin"))
						{
							UncompiledMDASkin skin = {};

							RKIT_CHECK(ExpectLine(line, fileSpan));
							RKIT_CHECK(ExpectTokenIs(line, "{"));

							for (;;)
							{
								RKIT_CHECK(ExpectLine(line, fileSpan));
								RKIT_CHECK(ExpectToken(token, line));

								if (TokenIs(token, "}"))
									break;

								if (TokenIs(token, "pass"))
								{
									UncompiledMDAPass pass = {};

									RKIT_CHECK(ExpectLine(line, fileSpan));
									RKIT_CHECK(ExpectTokenIs(line, "{"));

									bool haveMap = false;
									for (;;)
									{
										RKIT_CHECK(ExpectLine(line, fileSpan));
										RKIT_CHECK(ExpectToken(token, line));

										if (TokenIs(token, "}"))
											break;

										const bool isMap = TokenIs(token, "map");
										const bool isClampMap = (!isMap) && TokenIs(token, "clampmap");

										if (isMap || isClampMap)
										{
											RKIT_CHECK(ExpectToken(token, line));

											RKIT_CHECK(ExtractPath(pass.m_map, token, true));
											pass.m_clamp = isClampMap;

											haveMap = true;
										}
										else if (TokenIs(token, "alphafunc"))
										{
											RKIT_CHECK(ExpectToken(token, line));

											if (TokenIs(token, "ge128"))
												pass.m_alphaTestMode = data::MDAAlphaTestMode::kGE128;
											else if (TokenIs(token, "lt128"))
												pass.m_alphaTestMode = data::MDAAlphaTestMode::kLT128;
											else if (TokenIs(token, "gt0"))
												pass.m_alphaTestMode = data::MDAAlphaTestMode::kGT0;
											else
											{
												rkit::log::Error(u8"Unknown alphafunc");
												RKIT_THROW(rkit::ResultCode::kDataError);
											}
										}
										else if (TokenIs(token, "depthwrite"))
										{
											RKIT_CHECK(ExpectToken(token, line));

											uint32_t depthWriteFlag = 0;
											if (!utils->ParseUInt32(rkit::AsciiStringSliceView(token).RemoveEncoding(), 10, depthWriteFlag))
											{
												rkit::log::Error(u8"Invalid depthwrite value");
												RKIT_THROW(rkit::ResultCode::kDataError);
											}

											pass.m_depthWrite = (depthWriteFlag != 0);
										}
										else if (TokenIs(token, "uvgen"))
										{
											RKIT_CHECK(ExpectToken(token, line));

											if (TokenIs(token, "sphere"))
												pass.m_uvGenMode = data::MDAUVGenMode::kSphere;
											else
											{
												rkit::log::Error(u8"Unknown uvgen type");
												RKIT_THROW(rkit::ResultCode::kDataError);
											}
										}
										else if (TokenIs(token, "uvmod"))
										{
											RKIT_CHECK(ExpectToken(token, line));

											if (TokenIs(token, "scroll"))
											{
												double uvScroll[2] = { 0.0, 0.0 };

												for (size_t axis = 0; axis < 2; axis++)
												{
													RKIT_CHECK(ExpectToken(token, line));

													rkit::AsciiString tokenStr;
													RKIT_CHECK(tokenStr.Set(token));

													if (!utils->ParseDouble(tokenStr.ToByteView(), uvScroll[axis]))
													{
														rkit::log::Error(u8"Invalid scroll");
														RKIT_THROW(rkit::ResultCode::kDataError);
													}
												}

												pass.m_scrollU = static_cast<float>(uvScroll[0]);
												pass.m_scrollV = static_cast<float>(uvScroll[1]);
											}
											else
											{
												rkit::log::Error(u8"Unknown uvmode type");
												RKIT_THROW(rkit::ResultCode::kDataError);
											}
										}
										else if (TokenIs(token, "blendmode"))
										{
											RKIT_CHECK(ExpectToken(token, line));

											if (TokenIs(token, "add"))
												pass.m_blendMode = data::MDABlendMode::kAdd;
											else if (TokenIs(token, "none"))
												pass.m_blendMode = data::MDABlendMode::kDisabled;
											else if (TokenIs(token, "normal"))
												pass.m_blendMode = data::MDABlendMode::kNormal;
											else
											{
												rkit::log::Error(u8"Unknown blendmode");
												RKIT_THROW(rkit::ResultCode::kDataError);
											}
										}
										else if (TokenIs(token, "depthfunc"))
										{
											RKIT_CHECK(ExpectToken(token, line));

											if (TokenIs(token, "equal"))
												pass.m_depthFunc = data::MDADepthFunc::kEqual;
											else if (TokenIs(token, "less"))
												pass.m_depthFunc = data::MDADepthFunc::kLess;
											else
											{
												rkit::log::Error(u8"Unknown depthfunc");
												RKIT_THROW(rkit::ResultCode::kDataError);
											}
										}
										else if (TokenIs(token, "cull"))
										{
											RKIT_CHECK(ExpectToken(token, line));

											if (TokenIs(token, "back"))
												pass.m_cullType = data::MDACullType::kBack;
											else if (TokenIs(token, "front"))
												pass.m_cullType = data::MDACullType::kFront;
											else if (TokenIs(token, "disable"))
												pass.m_cullType = data::MDACullType::kDisable;
											else
											{
												rkit::log::Error(u8"Unknown cull type");
												RKIT_THROW(rkit::ResultCode::kDataError);
											}
										}
										else if (TokenIs(token, "rgbgen"))
										{
											RKIT_CHECK(ExpectToken(token, line));

											if (TokenIs(token, "diffusezero"))
												pass.m_rgbGenMode = data::MDARGBGenMode::kDiffuseZero;
											else if (TokenIs(token, "identity"))
												pass.m_rgbGenMode = data::MDARGBGenMode::kIdentity;
											else
											{
												rkit::log::Error(u8"Unknown rgbgen type");
												RKIT_THROW(rkit::ResultCode::kDataError);
											}
										}
										else
										{
											rkit::log::Error(u8"Unknown directive in pass");
											RKIT_THROW(rkit::ResultCode::kDataError);
										}
									}

									if (!haveMap)
									{
										rkit::log::Error(u8"Pass missing map");
										RKIT_THROW(rkit::ResultCode::kDataError);
									}

									RKIT_CHECK(skin.m_passes.Append(std::move(pass)));
								}
								else if (TokenIs(token, "sort"))
								{
									RKIT_CHECK(ExpectToken(token, line));

									if (TokenIs(token, "blend"))
										skin.m_sortMode = data::MDASortMode::kBlend;
									else if (TokenIs(token, "opaque"))
										skin.m_sortMode = data::MDASortMode::kOpaque;
									else
									{
										rkit::log::Error(u8"Unknown sort type");
										RKIT_THROW(rkit::ResultCode::kDataError);
									}
								}
								else
								{
									rkit::log::Error(u8"Unknown directive in skin");
									RKIT_THROW(rkit::ResultCode::kDataError);
								}
							}

							RKIT_CHECK(profile.m_skins.Append(std::move(skin)));
						}
						else if (TokenIs(token, "evaluate"))
						{
							RKIT_CHECK(ExpectToken(token, line));

							if (token.Count() < 2 || token[0] != '\"' || token[token.Count() - 1] != '\"')
							{
								rkit::log::Error(u8"Invalid condition");
								RKIT_THROW(rkit::ResultCode::kDataError);
							}

							RKIT_CHECK(profile.m_evaluate.Set(token.SubSpan(1, token.Count() - 2)));
						}
						else
						{
							rkit::log::Error(u8"Unknown directive in profile");
							RKIT_THROW(rkit::ResultCode::kDataError);
						}
					}


					RKIT_CHECK(mdaData.m_profiles.Append(std::move(profile)));
				}
				else
				{
					rkit::log::Error(u8"Unknown directive");
					RKIT_THROW(rkit::ResultCode::kDataError);
				}
			}
		}

		for (const MDABase64Chunk &base64Chunk : base64Chunks)
		{
			if (base64Chunk.m_base64Chars.Count() % 4u != 0
				|| base64Chunk.m_base64Chars.Count() / 4u * 3u != base64Chunk.m_paddedSize
				|| base64Chunk.m_size > base64Chunk.m_paddedSize)
			{
				rkit::log::Error(u8"Malformed base64 chunk");
				RKIT_THROW(rkit::ResultCode::kDataError);
			}

			rkit::Vector<uint8_t> decoded;
			RKIT_CHECK(decoded.Resize(base64Chunk.m_paddedSize));

			const rkit::ConstSpan<char> base64Data = base64Chunk.m_base64Chars.ToSpan();
			const rkit::Span<uint8_t> byteData = decoded.ToSpan();

			const size_t numFourToThreeBlocks = base64Chunk.m_base64Chars.Count() / 4u;
			for (size_t fttBlockIndex = 0; fttBlockIndex < numFourToThreeBlocks; fttBlockIndex++)
			{
				const size_t outPos = fttBlockIndex * 3u;
				const size_t inPos = fttBlockIndex * 4u;

				uint8_t inUInts[4] = {};

				for (size_t subChar = 0; subChar < 4; subChar++)
				{
					const char c = base64Data[inPos + subChar];
					if (c >= '0' && c <= '9')
						inUInts[subChar] = (c - '0') + 0;
					else if (c >= 'A' && c <= 'Z')
						inUInts[subChar] = (c - 'A') + 10;
					else if (c >= 'a' && c <= 'z')
						inUInts[subChar] = (c - 'a') + 36;
					else if (c == '.')
						inUInts[subChar] = 62;
					else if (c == '/')
						inUInts[subChar] = 63;
					else
					{
						rkit::log::Error(u8"Malformed base64 chunk");
						RKIT_THROW(rkit::ResultCode::kDataError);
					}
				}

				const uint8_t b0 = (inUInts[0] << 2) | (inUInts[1] >> 4);
				const uint8_t b1 = ((inUInts[1] & 0xf) << 4) | ((inUInts[2] >> 2) & 0xf);
				const uint8_t b2 = ((inUInts[2] & 0x3) << 6) | inUInts[3];

				byteData[outPos + 0] = b0;
				byteData[outPos + 1] = b1;
				byteData[outPos + 2] = b2;
			}

			decoded.ShrinkToSize(base64Chunk.m_size);

			if (base64Chunk.m_fourCC == RKIT_FOURCC('B', 'O', 'N', 'E'))
				mdaData.m_boneChunk = std::move(decoded);
			else if (base64Chunk.m_fourCC == RKIT_FOURCC('A', 'N', 'I', 'P'))
				mdaData.m_animChunk = std::move(decoded);
			else if (base64Chunk.m_fourCC == RKIT_FOURCC('M', 'R', 'P', 'H'))
				mdaData.m_morphChunk = std::move(decoded);
			else
			{
				rkit::log::Error(u8"Unknown MDA chunk type");
				RKIT_THROW(rkit::ResultCode::kDataError);
			}
		}

		rkit::CIPath outputPath;
		RKIT_CHECK(ConstructOutputPath(outputPath, depsNode->GetIdentifier()));

		return CompileMDA(outputPath, mdaData, false, depsNode, feedback);
	}

	uint32_t AnoxMDACompiler::GetVersion() const
	{
		return 3;
	}

	rkit::Result AnoxMDACompiler::ExpectLine(rkit::ConstSpan<char> &outLine, rkit::ConstSpan<char> &fileSpan)
	{
		if (!ParseOneLine(outLine, fileSpan))
		{
			rkit::log::Error(u8"Unexpected end of file");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxMDACompiler::ExpectTokenIs(rkit::ConstSpan<char> &token, const rkit::AsciiStringView &candidate)
	{
		if (!TokenIs(token, candidate))
		{
			rkit::log::ErrorFmt(u8"Expected token '{}'", candidate.ToUTF8());
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		RKIT_RETURN_OK;
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

		rkit::ConstSpan<char> line = fileSpan.SubSpan(0, endPos);

		// Trim whitespace
		while (line.Count() > 0 && rkit::IsASCIIWhitespace(line[0]))
			line = line.SubSpan(1);

		while (line.Count() > 0 && rkit::IsASCIIWhitespace(line[line.Count() - 1]))
			line = line.SubSpan(0, line.Count() - 1);

		outLine = line;
		fileSpan = fileSpan.SubSpan(endPos + eolCharCount);

		return true;
	}

	rkit::Result AnoxMDACompiler::ExpectToken(rkit::ConstSpan<char> &outToken, rkit::ConstSpan<char> &line)
	{
		if (!ParseToken(outToken, line))
		{
			rkit::log::ErrorFmt(u8"Expected token");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		RKIT_RETURN_OK;
	}

	bool AnoxMDACompiler::ParseToken(rkit::ConstSpan<char> &outToken, rkit::ConstSpan<char> &lineSpan)
	{
		size_t endPos = 0;

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
			RKIT_THROW(rkit::ResultCode::kDataError);

		rkit::StringConstructionBuffer strBuf;
		RKIT_CHECK(strBuf.Allocate(token.Count()));

		const rkit::Span<rkit::Utf8Char_t> strBufChars = strBuf.GetSpan();

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
			RKIT_THROW(rkit::ResultCode::kDataError);

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
		UncompiledMDAData mdaData;
		RKIT_CHECK(mdaData.m_baseModel.Set(depsNode->GetIdentifier()));

		rkit::CIPath outputPath;
		RKIT_CHECK(ConstructOutputPath(outputPath, depsNode->GetIdentifier()));

		return CompileMDA(outputPath, mdaData, true, depsNode, feedback);
	}

	bool AnoxCTCCompiler::HasAnalysisStage() const
	{
		return true;
	}

	rkit::Result AnoxCTCCompiler::RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::CIPath ctcPath;
		RKIT_CHECK(ctcPath.Set(depsNode->GetIdentifier()));

		rkit::UniquePtr<rkit::ISeekableReadStream> inputFile;
		RKIT_CHECK(feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, ctcPath, inputFile));

		CTCHeader header;
		RKIT_CHECK(inputFile->ReadOneBinary(header));

		if (header.m_version.Get() != CTCHeader::kExpectedVersion || header.m_magic.Get() != CTCHeader::kExpectedMagic)
			RKIT_THROW(rkit::ResultCode::kDataError);

		const uint32_t numTextures = header.m_numTextures.Get();

		if (numTextures == 0)
			RKIT_THROW(rkit::ResultCode::kDataError);

		RKIT_CHECK(inputFile->SeekStart(header.m_texturesPos.Get()));

		for (size_t i = 0; i < numTextures; i++)
		{
			MD2TextureDef textureDef;
			RKIT_CHECK(inputFile->ReadOneBinary(textureDef));

			rkit::CIPath texturePath;
			RKIT_CHECK(ResolveTexturePath(texturePath, ctcPath, textureDef, true));

			RKIT_CHECK(feedback->AddNodeDependency(kAnoxNamespaceID, kModelMaterialNodeID, rkit::buildsystem::BuildFileLocation::kSourceDir, texturePath.ToString()));
		}

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxCTCCompiler::RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::DeduplicatedList<rkit::data::ContentID> materialContentIDs;

		rkit::CIPath ctcPath;
		RKIT_CHECK(ctcPath.Set(depsNode->GetIdentifier()));

		rkit::UniquePtr<rkit::ISeekableReadStream> inputFile;
		RKIT_CHECK(feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, ctcPath, inputFile));

		CTCHeader header;
		RKIT_CHECK(inputFile->ReadOneBinary(header));

		const uint32_t numPoints = header.m_numPoints.Get();
		const uint32_t numTotalTris = header.m_numTris.Get();
		const uint32_t numTextures = header.m_numTextures.Get();
		const uint32_t numMorphs = header.m_numMorphs.Get();

		rkit::Vector<rkit::endian::LittleUInt32_t> textureTriListSizes;
		RKIT_CHECK(textureTriListSizes.Resize(numTextures));

		RKIT_CHECK(inputFile->SeekStart(header.m_triListSizesPos.Get()));
		RKIT_CHECK(inputFile->ReadAllSpan(textureTriListSizes.ToSpan()));

		rkit::Vector<Morph> morphs;

		RKIT_CHECK(morphs.Resize(numMorphs));

		RKIT_CHECK(inputFile->SeekStart(header.m_morphsPos.Get()));

		for (uint32_t mi = 0; mi < numMorphs; mi++)
		{
			Morph &outMorph = morphs[mi];

			CTCMorph ctcMorph;
			RKIT_CHECK(inputFile->ReadOneBinary(ctcMorph));

			rkit::CopySpanNonOverlapping(rkit::Span<char>(outMorph.m_name).SubSpan(0, 16), rkit::ConstSpan<char>(ctcMorph.m_name));

			outMorph.m_name[16] = 0;

			const uint32_t numVertMorphs = ctcMorph.m_numVertMorphs.Get();

			RKIT_CHECK(outMorph.m_vertMorphs.Resize(numVertMorphs));
		}

		for (uint32_t mi = 0; mi < numMorphs; mi++)
		{
			for (VertMorph &vertMorph : morphs[mi].m_vertMorphs)
			{
				RKIT_CHECK(inputFile->ReadOneBinary(vertMorph.m_pointIndex));

				RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
			}
			for (VertMorph &vertMorph : morphs[mi].m_vertMorphs)
			{
				RKIT_CHECK(inputFile->ReadOneBinary(vertMorph.m_targetPosition));
			}
		}

		rkit::Vector<CTCTriVerts> triVerts;
		rkit::Vector<CTCTriTexCoords> triTexCoords;

		RKIT_CHECK(triVerts.Resize(numTotalTris));
		RKIT_CHECK(inputFile->SeekStart(header.m_triVertIndexesPos.Get()));
		RKIT_CHECK(inputFile->ReadAllSpan(triVerts.ToSpan()));

		RKIT_CHECK(triTexCoords.Resize(numTotalTris));
		RKIT_CHECK(inputFile->SeekStart(header.m_triTexCoordsPos.Get()));
		RKIT_CHECK(inputFile->ReadAllSpan(triTexCoords.ToSpan()));

		data::MDAProfile outProfile = {};

		rkit::Vector<data::MDAAnimation> outAnimations;
		rkit::Vector<rkit::AsciiString> outAnimationNames;

		rkit::Vector<data::MDASkeletalModelBone> outBones;

		rkit::Vector<data::MDASkin> outSkins;
		rkit::Vector<data::MDASkinPass> outSkinPasses;

		rkit::Vector<data::MDAModelMorphKey> outMorphKeys;

		rkit::Vector<rkit::Vector<data::MDAModelSkeletalBoneFrame>> outFrames;

		rkit::Vector<data::MDAModelSubModel> outSubModels;
		rkit::Vector<data::MDAModelTri> outTris;
		rkit::Vector<data::MDAModelVert> outVerts;
		rkit::Vector<data::MDAModelPoint> outPoints;
		rkit::Vector<data::MDASkeletalModelBoneIndex> outBoneIndexes;
		rkit::Vector<data::MDAModelVertMorph> outVertMorphs;

		uint32_t numMorphedPoints = 0;

		rkit::BoolVector pointHasMorph;
		RKIT_CHECK(pointHasMorph.Resize(numPoints));

		for (const Morph &morph : morphs)
		{
			for (const VertMorph &vertMorph : morph.m_vertMorphs)
			{
				const uint32_t pointIndex = vertMorph.m_pointIndex.Get();
				if (pointIndex >= numPoints)
				{
					rkit::log::Error(u8"Vert morph vert index was out of range");
					RKIT_THROW(rkit::ResultCode::kDataError);
				}

				pointHasMorph.Set(pointIndex, true);
			}
		}

		rkit::Vector<uint32_t> inPointToOutPoint;

		RKIT_CHECK(inPointToOutPoint.Resize(numPoints));

		{
			uint32_t numUnmorphedPoints = 0;

			for (uint32_t inPointIndex = 0; inPointIndex < numPoints; inPointIndex++)
			{
				if (pointHasMorph[inPointIndex])
					inPointToOutPoint[inPointIndex] = numMorphedPoints++;
			}

			for (uint32_t inPointIndex = 0; inPointIndex < numPoints; inPointIndex++)
			{
				if (!pointHasMorph[inPointIndex])
					inPointToOutPoint[inPointIndex] = numMorphedPoints + (numUnmorphedPoints++);
			}
		}

		{
			size_t firstTri = 0;
			for (uint32_t textureIndex = 0; textureIndex < numTextures; textureIndex++)
			{
				const uint32_t numTrisForTexture = textureTriListSizes[textureIndex].Get();
				if (numTrisForTexture > numTotalTris || (numTotalTris - firstTri) < numTrisForTexture)
					RKIT_THROW(rkit::ResultCode::kDataError);

				const rkit::ConstSpan<CTCTriVerts> textureTriVerts = triVerts.ToSpan().SubSpan(firstTri, numTrisForTexture);
				const rkit::ConstSpan<CTCTriTexCoords> textureTriTexCoords = triTexCoords.ToSpan().SubSpan(firstTri, numTrisForTexture);

				RKIT_CHECK(AddTriClusters(outSubModels, outTris, outVerts, textureIndex, inPointToOutPoint.ToSpan(), textureTriVerts, textureTriTexCoords, numTrisForTexture));

				firstTri += numTrisForTexture;
			}
		}

		rkit::Vector<CTCAnimation> inAnimations;
		RKIT_CHECK(inAnimations.Resize(header.m_numAnimations_Size24.Get()));

		RKIT_CHECK(inputFile->SeekStart(header.m_animationPos.Get()));
		RKIT_CHECK(inputFile->ReadAllSpan(inAnimations.ToSpan()));

		const uint32_t numFrames = header.m_numFrames.Get();

		{
			uint32_t animNumber = 0;
			uint32_t startFrame = 0;

			for (const CTCAnimation &inAnim : inAnimations)
			{
				char animNameChars[17];

				rkit::CopySpanNonOverlapping(rkit::Span<char>(animNameChars).SubSpan(0, 16), rkit::ConstSpan<char>(inAnim.m_name));

				animNameChars[16] = 0;

				rkit::AsciiString animName;
				const uint8_t animNameLength = static_cast<uint8_t>(strlen(animNameChars));
				RKIT_CHECK(animName.Set(rkit::ConstSpan<char>(animNameChars, strlen(animNameChars))));

				const uint32_t availableFrames = numFrames - startFrame;
				const uint32_t numFramesForAnim = inAnim.m_frameCount.Get();
				if (availableFrames < numFramesForAnim)
				{
					rkit::log::Error(u8"Too many frames in animations");
					RKIT_THROW(rkit::ResultCode::kDataError);
				}

				data::MDAAnimation outAnim = {};
				outAnim.m_animNumber = animNumber;
				outAnim.m_firstFrame = startFrame;
				outAnim.m_numFrames = numFramesForAnim;
				outAnim.m_categoryLength = animNameLength;

				RKIT_CHECK(outAnimationNames.Append(std::move(animName)));
				RKIT_CHECK(outAnimations.Append(std::move(outAnim)));

				animNumber++;
				startFrame += numFramesForAnim;
			}
		}

		// Convert bones
		rkit::Vector<CTCBone> bones;
		const uint32_t numBones = header.m_numBones.Get();
		RKIT_CHECK(bones.Resize(numBones));

		RKIT_CHECK(inputFile->SeekStart(header.m_bonesPos.Get()));
		RKIT_CHECK(inputFile->ReadAllSpan(bones.ToSpan()));

		for (size_t boneIndex = 0; boneIndex < numBones; boneIndex++)
		{
			const CTCBone &inBone = bones[boneIndex];

			data::MDASkeletalModelBone outBone = {};

			for (size_t row = 0; row < 3; row++)
			{
				for (size_t col = 0; col < 4; col++)
					outBone.m_baseMatrix[row][col] = inBone.m_baseTransform.m_rows[row][col];
			}

			const uint32_t parentIndexPlusOne = inBone.m_parentIndexPlusOne.Get();
			if (parentIndexPlusOne > boneIndex)
			{
				rkit::log::Error(u8"Invalid bone parent index");
				RKIT_THROW(rkit::ResultCode::kDataError);
			}

			outBone.m_parentIndexPlusOne = static_cast<uint16_t>(inBone.m_parentIndexPlusOne.Get());

			RKIT_CHECK(outBones.Append(outBone));
		}

		rkit::Vector<MD2TextureDef> inTextures;

		RKIT_CHECK(inTextures.Resize(numTextures));

		RKIT_CHECK(inputFile->SeekStart(header.m_texturesPos.Get()));
		RKIT_CHECK(inputFile->ReadAllSpan(inTextures.ToSpan()));

		for (const MD2TextureDef &inTexture : inTextures)
		{
			char nameChars[65];
			nameChars[64] = 0;

			rkit::CopySpanNonOverlapping(rkit::Span<char>(nameChars).SubSpan(0, 64), rkit::ConstSpan<char>(inTexture.m_textureName));
			const size_t nameLen = strlen(nameChars);

			data::MDASkin skin = {};
			skin.m_numPasses = 1;
			skin.m_sortMode = static_cast<uint8_t>(data::MDASortMode::kOpaque);

			data::MDASkinPass skinPass = {};

			rkit::CIPath materialPath;
			RKIT_CHECK(ResolveTexturePath(materialPath, ctcPath, inTexture, true));

			rkit::CIPath compiledMaterialPath;
			RKIT_CHECK(MaterialCompiler::ConstructOutputPath(compiledMaterialPath, data::MaterialResourceType::kModel, materialPath.ToString()));

			rkit::data::ContentID materialContentID;
			RKIT_CHECK(feedback->IndexCAS(rkit::buildsystem::BuildFileLocation::kIntermediateDir, compiledMaterialPath, materialContentID));

			size_t materialIndex = 0;
			RKIT_CHECK(materialContentIDs.AddAndGetIndex(materialIndex, materialContentID));

			typedef uint16_t StoredMaterialIndex_t;
			if (materialIndex > std::numeric_limits<StoredMaterialIndex_t>::max())
				RKIT_THROW(rkit::ResultCode::kIntegerOverflow);

			skinPass.m_materialIndex = static_cast<StoredMaterialIndex_t>(materialIndex);

			RKIT_CHECK(outSkins.Append(skin));
			RKIT_CHECK(outSkinPasses.Append(skinPass));
		}

		if (numMorphedPoints > 0)
		{
			RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
		}

		RKIT_CHECK(outPoints.Resize(numPoints));
		RKIT_CHECK(outBoneIndexes.Resize(numPoints));

		{
			rkit::Vector<CTCPoint> inPoints;
			RKIT_CHECK(inPoints.Resize(numPoints));

			RKIT_CHECK(inputFile->SeekStart(header.m_pointsPos.Get()));
			RKIT_CHECK(inputFile->ReadAllSpan(inPoints.ToSpan()));

			for (uint32_t inPointIndex = 0; inPointIndex < numPoints; inPointIndex++)
			{
				data::MDAModelPoint &outPoint = outPoints[inPointToOutPoint[inPointIndex]];
				data::MDASkeletalModelBoneIndex &outBoneIndex = outBoneIndexes[inPointToOutPoint[inPointIndex]];
				const CTCPoint &inPoint = inPoints[inPointIndex];

				rkit::StaticArray<float, 3> pos;
				rkit::StaticArray<float, 3> normal;

				for (size_t axis = 0; axis < 3; axis++)
				{
					pos[axis] = inPoint.m_pos[axis].Get();
					normal[axis] = inPoint.m_normal[axis].Get();
				}

				outBoneIndex.m_boneIndex = 0;
				outPoint.m_compressedNormal.m_value = data::CompressNormal32(normal[0], normal[1], normal[2]);
				for (size_t axis = 0; axis < 3; axis++)
					outPoint.m_point[axis] = inPoint.m_pos[axis];
			}
		}

		// Process skeletal data
		{
			RKIT_CHECK(inputFile->SeekStart(header.m_skeletalDataPos.Get()));

			rkit::endian::BigUInt32_t skeletalDataFourCC;
			RKIT_CHECK(inputFile->ReadOneBinary(skeletalDataFourCC));

			if (skeletalDataFourCC.Get() != RKIT_FOURCC('S', 'T', 'G', 'W'))
			{
				rkit::log::Error(u8"Skeletal data fourcc mismatch");
				RKIT_THROW(rkit::ResultCode::kDataError);
			}

			for (uint32_t inPointIndex = 0; inPointIndex < numPoints; inPointIndex++)
			{
				rkit::endian::LittleUInt32_t numBonesForPointData;
				RKIT_CHECK(inputFile->ReadOneBinary(numBonesForPointData));

				const uint32_t numBonesForPoint = numBonesForPointData.Get();

				if (numBonesForPoint == 0)
				{
					rkit::log::Error(u8"Skeletal data vert had no bones?");
					RKIT_THROW(rkit::ResultCode::kDataError);
				}

				for (uint32_t bpi = 0; bpi < numBonesForPoint; bpi++)
				{
					CTCBoneAndWeight boneAndWeightData;
					RKIT_CHECK(inputFile->ReadOneBinary(boneAndWeightData));

					const uint32_t boneIndex = boneAndWeightData.m_boneIndex.Get();
					const float weight = boneAndWeightData.m_weight.Get();

					if (boneIndex >= bones.Count())
					{
						rkit::log::Error(u8"Bone index was out of range");
						RKIT_THROW(rkit::ResultCode::kDataError);
					}

					// Anachronox never uses multi-weight skinning even though the format
					// supports it
					if (bpi == 0)
						outBoneIndexes[inPointToOutPoint[inPointIndex]].m_boneIndex = boneIndex;
				}
			}
		}

		// Process frames
		{
			RKIT_CHECK(inputFile->SeekStart(header.m_frameDataPos.Get()));

			rkit::Vector<Matrix3x4> absMatrixes;
			RKIT_CHECK(absMatrixes.Resize(numBones));

			RKIT_CHECK(outFrames.Resize(numFrames));

			for (uint32_t frameIndex = 0; frameIndex < numFrames; frameIndex++)
			{
				rkit::Vector<data::MDAModelSkeletalBoneFrame> &frameBones = outFrames[frameIndex];
				RKIT_CHECK(frameBones.Resize(numBones));

				CTCBoneKey baseKey;
				RKIT_CHECK(inputFile->ReadOneBinary(baseKey));

				const Matrix3x4 baseMatrix = BoneKeyToMatrix(baseKey);

				for (uint32_t boneIndex = 0; boneIndex < numBones; boneIndex++)
				{
					const CTCBone &inBone = bones[boneIndex];
					const Matrix3x4 *parentMatrixPtr = &baseMatrix;

					const uint32_t parentIndexPlusOne = inBone.m_parentIndexPlusOne.Get();

					if (parentIndexPlusOne != 0)
						parentMatrixPtr = &absMatrixes[parentIndexPlusOne - 1];

					CTCBoneKey boneKey;
					RKIT_CHECK(inputFile->ReadOneBinary(boneKey));

					const Matrix3x4 relMatrix = BoneKeyToMatrix(baseKey);
					absMatrixes[boneIndex] = OrthoMatrixMul(*parentMatrixPtr, relMatrix);
				}

				// Compress matrices
				for (uint32_t boneIndex = 0; boneIndex < numBones; boneIndex++)
				{
					const CTCBone &inBone = bones[boneIndex];
					const uint32_t parentIndexPlusOne = inBone.m_parentIndexPlusOne.Get();

					Matrix3x4 &absMatrixRef = absMatrixes[boneIndex];

					Matrix3x4 relMatrix = absMatrixRef;
					if (parentIndexPlusOne != 0)
					{
						// absMatrixRef = parentMatrix * relMatrix
						const Matrix3x4 invParentMatrix = OrthoInvertMatrix(absMatrixes[parentIndexPlusOne - 1]);
						relMatrix = OrthoMatrixMul(invParentMatrix, relMatrix);
					}

					const data::MDAModelSkeletalBoneFrame compressedBoneFrame = ConvertMatrixToFrame(relMatrix);

					relMatrix = ConvertFrameToMatrix(compressedBoneFrame);
					if (parentIndexPlusOne == 0)
						absMatrixRef = relMatrix;
					else
						absMatrixRef = OrthoMatrixMul(absMatrixes[parentIndexPlusOne - 1], relMatrix);

					frameBones[boneIndex] = compressedBoneFrame;
				}
			}
		}

		outProfile.m_conditionLength = 0;
		outProfile.m_fourCC = RKIT_FOURCC('D', 'F', 'L', 'T');

		data::MDAModelHeader outHeader = {};
		outHeader.m_numProfiles = 1;

		if (outSubModels.Count() > 255)
		{
			rkit::log::Error(u8"Too many submodels");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		outHeader.m_numSubModels = static_cast<uint8_t>(outSubModels.Count());

		const rkit::ConstSpan<rkit::data::ContentID> outMaterials = materialContentIDs.GetItems();

		if (outMaterials.Count() > std::numeric_limits<uint16_t>::max())
		{
			rkit::log::Error(u8"Too many materials");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		outHeader.m_numMaterials = static_cast<uint16_t>(outMaterials.Count());

		if (outAnimations.Count() > 127)
		{
			rkit::log::Error(u8"Too many animations");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		outHeader.m_numAnimations7_AnimationType1 = static_cast<uint8_t>(0x80 | outAnimations.Count());

		if (outSkins.Count() > 255)
		{
			rkit::log::Error(u8"Too many skins");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		outHeader.m_numSkins = static_cast<uint8_t>(outSkins.Count());

		if (outMorphKeys.Count() > std::numeric_limits<uint16_t>::max())
		{
			rkit::log::Error(u8"Too many morph keys");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		outHeader.m_numMorphKeys = static_cast<uint16_t>(outMorphKeys.Count());

		if (outBones.Count() > std::numeric_limits<uint16_t>::max())
		{
			rkit::log::Error(u8"Too many morph keys");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		outHeader.m_numBones = static_cast<uint16_t>(outBones.Count());

		if (outFrames.Count() > std::numeric_limits<uint16_t>::max())
		{
			rkit::log::Error(u8"Too many frames");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		outHeader.m_numFrames = static_cast<uint16_t>(outFrames.Count());

		if (outPoints.Count() > std::numeric_limits<uint32_t>::max())
		{
			rkit::log::Error(u8"Too many points");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		outHeader.m_numPoints = static_cast<uint32_t>(outPoints.Count());

		outHeader.m_numMorphedPoints = static_cast<uint32_t>(numMorphedPoints);

		rkit::CIPath outputPath;
		RKIT_CHECK(ConstructOutputPath(outputPath, depsNode->GetIdentifier()));

		rkit::UniquePtr<rkit::ISeekableReadWriteStream> outFile;
		RKIT_CHECK(feedback->OpenOutput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, outputPath, outFile));

		// Start writing the file
		RKIT_CHECK(outFile->WriteOneBinary(outHeader));

		RKIT_CHECK(outFile->WriteAllSpan(outMaterials));

		RKIT_CHECK(outFile->WriteOneBinary(outProfile));

		// No conditions to write
		RKIT_CHECK(outFile->WriteAllSpan(outSkins.ToSpan()));
		RKIT_CHECK(outFile->WriteAllSpan(outSkinPasses.ToSpan()));
		RKIT_CHECK(outFile->WriteAllSpan(outAnimations.ToSpan()));

		for (const rkit::AsciiString &animName : outAnimationNames)
		{
			RKIT_CHECK(outFile->WriteAllSpan(animName.ToSpan()));
		}

		RKIT_CHECK(outFile->WriteAllSpan(outMorphKeys.ToSpan()));
		RKIT_CHECK(outFile->WriteAllSpan(outBones.ToSpan()));

		for (const rkit::Vector<data::MDAModelSkeletalBoneFrame> &frame : outFrames)
		{
			RKIT_CHECK(outFile->WriteAllSpan(frame.ToSpan()));
		}

		RKIT_CHECK(outFile->WriteAllSpan(outSubModels.ToSpan()));
		RKIT_CHECK(outFile->WriteAllSpan(outTris.ToSpan()));
		RKIT_CHECK(outFile->WriteAllSpan(outVerts.ToSpan()));
		RKIT_CHECK(outFile->WriteAllSpan(outPoints.ToSpan()));
		RKIT_CHECK(outFile->WriteAllSpan(outBoneIndexes.ToSpan()));
		RKIT_CHECK(outFile->WriteAllSpan(outVertMorphs.ToSpan()));

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxCTCCompiler::AddTriClusters(rkit::Vector<data::MDAModelSubModel> &outSubModels,
		rkit::Vector<data::MDAModelTri> &outTris,
		rkit::Vector<data::MDAModelVert> &outVerts,
		uint32_t materialID,
		rkit::ConstSpan<uint32_t> inVertToOutVert,
		rkit::ConstSpan<CTCTriVerts> triVerts, rkit::ConstSpan<CTCTriTexCoords> triTexCoords, size_t numTris)
	{
		rkit::BoolVector triAdded;
		RKIT_CHECK(triAdded.Resize(numTris));

		for (;;)
		{
			const size_t firstTri = outTris.Count();
			const size_t firstVert = outVerts.Count();

			data::MDAModelSubModel subModel = {};
			subModel.m_materialIndex = static_cast<uint16_t>(materialID);

			RKIT_CHECK(AddOneTriCluster(outTris, outVerts, inVertToOutVert, triVerts, triTexCoords, numTris, triAdded.ToSpan()));

			if (firstTri != outTris.Count())
			{
				RKIT_ASSERT(firstVert != outVerts.Count());

				subModel.m_numTris = static_cast<uint16_t>(outTris.Count() - firstTri);
				subModel.m_numVertsMinusOne = static_cast<uint16_t>(outVerts.Count() - firstVert - 1u);
			}
			else
			{
				RKIT_ASSERT(firstVert == outVerts.Count());
				break;
			}

			RKIT_CHECK(outSubModels.Append(subModel));
		}

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxCTCCompiler::AddOneTriCluster(
		rkit::Vector<data::MDAModelTri> &outTris,
		rkit::Vector<data::MDAModelVert> &outVerts,
		rkit::ConstSpan<uint32_t> inPointToOutPoint,
		rkit::ConstSpan<CTCTriVerts> triVertsSpan, rkit::ConstSpan<CTCTriTexCoords> triTexCoordsSpan, size_t numTris,
		rkit::BoolSpan triAdded)
	{
		rkit::HashMap<CTCCompoundTriVert, uint16_t> compoundVertToVertIndex;

		size_t vertsEmitted = 0;

		for (size_t triIndex = 0; triIndex < numTris; triIndex++)
		{
			if (triAdded[triIndex])
				continue;

			rkit::StaticArray<rkit::HashValue_t, 3> triCompoundVertHashes;
			rkit::StaticArray<CTCCompoundTriVert, 3> triCompoundVerts;

			uint8_t numNewVerts = 0;

			const CTCTriVerts &triVerts = triVertsSpan[triIndex];
			const CTCTriTexCoords &triTexCoords = triTexCoordsSpan[triIndex];

			for (size_t ptIndex = 0; ptIndex < 3; ptIndex++)
			{
				CTCCompoundTriVert &triCompoundVert = triCompoundVerts[ptIndex];

				triCompoundVert.m_inPointIndex = triVerts.m_verts[ptIndex].Get();

				for (size_t axis = 0; axis < 2; axis++)
					triCompoundVert.m_compressedUV[axis] = CompressUV(triTexCoords.m_texCoords[ptIndex].m_uvFloatBits[axis].GetBits());

				const rkit::HashValue_t hashValue = rkit::GetDrivers().m_utilitiesDriver->ComputeHash(0, &triCompoundVert, sizeof(triCompoundVert));

				triCompoundVertHashes[ptIndex] = hashValue;

				if (compoundVertToVertIndex.FindPrehashed(hashValue, triCompoundVert) == compoundVertToVertIndex.end())
					numNewVerts++;
			}

			const size_t availableVertIndexes = 0x10000u - vertsEmitted;

			if (availableVertIndexes > numNewVerts)
			{
				data::MDAModelTri outTri = {};

				for (size_t ptIndex = 0; ptIndex < 3; ptIndex++)
				{
					const CTCCompoundTriVert &compoundVert = triCompoundVerts[ptIndex];
					rkit::HashValue_t hashValue = triCompoundVertHashes[ptIndex];

					uint16_t submodelVertIndex = static_cast<uint16_t>(vertsEmitted);

					const rkit::HashMap<CTCCompoundTriVert, uint16_t>::ConstIterator_t it = compoundVertToVertIndex.FindPrehashed(hashValue, compoundVert);
					if (it == compoundVertToVertIndex.end())
					{
						const size_t inPointIndex = compoundVert.m_inPointIndex;

						if (inPointIndex >= inPointToOutPoint.Count())
						{
							rkit::log::Error(u8"Vert was out of range");
							RKIT_THROW(rkit::ResultCode::kDataError);
						}

						data::MDAModelVert outVert = {};
						outVert.m_pointID = inPointToOutPoint[inPointIndex];
						outVert.m_texCoordU = compoundVert.m_compressedUV[0];
						outVert.m_texCoordV = compoundVert.m_compressedUV[1];

						RKIT_ASSERT(vertsEmitted < 0x10000u);
						RKIT_CHECK(outVerts.Append(outVert));

						RKIT_CHECK(compoundVertToVertIndex.SetPrehashed(hashValue, compoundVert, static_cast<uint16_t>(vertsEmitted)));

						vertsEmitted++;
					}
					else
						submodelVertIndex = it.Value();

					outTri.m_verts[ptIndex] = submodelVertIndex;
				}

				RKIT_CHECK(outTris.Append(outTri));

				triAdded.SetAt(triIndex, true);
			}
		}

		RKIT_RETURN_OK;
	}

	AnoxCTCCompiler::Matrix3x4 AnoxCTCCompiler::OrthoMatrixMul(const Matrix3x4 &a, const Matrix3x4 &b)
	{
		Matrix3x4 result;
		for (size_t outRow = 0; outRow < 3; outRow++)
		{
			for (size_t outCol = 0; outCol < 4; outCol++)
			{
				float coeff = 0.f;
				for (size_t term = 0; term < 3; term++)
					coeff += a.m_rows[outRow][term] * b.m_rows[term][outCol];
				// b.m_rows[3][outCol] is implicitly 0 for term outCol=0..2

				result.m_rows[outRow][outCol] = coeff;
			}

			// b.m_rows[3][outCol] is implicitly 1 for term outCol=3
			result.m_rows[outRow][3] += a.m_rows[outRow][3];
		}

		return result;
	}

	AnoxCTCCompiler::Matrix3x4 AnoxCTCCompiler::OrthoInvertMatrix(const Matrix3x4 &inMat)
	{
		// inMat is T*R, invert is R'*T'
		Matrix3x4 invR = {};
		for (size_t row = 0; row < 3; row++)
		{
			for (size_t col = 0; col < 3; col++)
			{
				invR.m_rows[row][col] = inMat.m_rows[col][row];
			}
		}

		/*
		Matrix3x4 invT = {};
		invT.m_rows[0][0] = 1.f;
		invT.m_rows[1][1] = 1.f;
		invT.m_rows[2][2] = 1.f;
		invT.m_rows[0][3] = -inMat.m_rows[0][3];
		invT.m_rows[1][3] = -inMat.m_rows[1][3];
		invT.m_rows[2][3] = -inMat.m_rows[2][3];

		Matrix3x4 result = OrthoMatrixMul(invR, invT);
		*/

		Matrix3x4 result = invR;
		for (size_t outRow = 0; outRow < 3; outRow++)
		{
			float coeff = 0.f;
			for (size_t term = 0; term < 3; term++)
				coeff -= inMat.m_rows[term][outRow] * inMat.m_rows[term][3];

			result.m_rows[outRow][3] = coeff;
		}

		return result;
	}

	AnoxCTCCompiler::Matrix3x4 AnoxCTCCompiler::BoneKeyToMatrix(const CTCBoneKey &key)
	{
		const float piDiv180 = 0.01745329251994329576923690768489f;
		const float dx = key.m_angles[1].Get();
		const float dy = key.m_angles[2].Get();
		const float dz = key.m_angles[0].Get();

		const float sx = sinf(dx * piDiv180);
		const float cx = cosf(dx * piDiv180);
		const float sy = sinf(dy * piDiv180);
		const float cy = cosf(dy * piDiv180);
		const float sz = sinf(dz * piDiv180);
		const float cz = cosf(dz * piDiv180);

		Matrix3x4 result = {};
		Matrix3x4 t = GenerateTranslationMatrix(key.m_pos[0].Get(), key.m_pos[1].Get(), key.m_pos[2].Get());
		Matrix3x4 rx = GenerateRotationMatrix(key.m_angles[1].Get(), 1.0f, 0.0f, 0.0f);
		Matrix3x4 ry = GenerateRotationMatrix(key.m_angles[0].Get(), 0.0f, 1.0f, 0.0f);
		Matrix3x4 rz = GenerateRotationMatrix(key.m_angles[2].Get(), 0.0f, 0.0f, 1.0f);

		return OrthoMatrixMul(OrthoMatrixMul(OrthoMatrixMul(t, ry), rx), rz);
	}

	AnoxCTCCompiler::Matrix3x4 AnoxCTCCompiler::GenerateRotationMatrix(float degrees, float x, float y, float z)
	{
		const float r = degrees * 0.01745329251994329576923690768489f;
		const float s = sinf(r);
		const float c = cosf(r);

		const float oneMinusC = 1.0f - c;

		Matrix3x4 result = {};
		result.m_rows[0][0] = x * x * oneMinusC + c;
		result.m_rows[0][1] = x * y * oneMinusC - z * s;
		result.m_rows[0][2] = x * z * oneMinusC + y * s;

		result.m_rows[1][0] = y * x * oneMinusC + z * s;
		result.m_rows[1][1] = y * y * oneMinusC + c;
		result.m_rows[1][2] = y * z * oneMinusC - x * s;

		result.m_rows[2][0] = z * x * oneMinusC - y * s;
		result.m_rows[2][1] = z * y * oneMinusC + x * s;
		result.m_rows[2][2] = z * z * oneMinusC + c;

		return result;
	}

	AnoxCTCCompiler::Matrix3x4 AnoxCTCCompiler::GenerateTranslationMatrix(float x, float y, float z)
	{
		Matrix3x4 result = GenerateIdentityMatrix();
		result.m_rows[0][3] = x;
		result.m_rows[1][3] = y;
		result.m_rows[2][3] = z;
		return result;
	}

	AnoxCTCCompiler::Matrix3x4 AnoxCTCCompiler::GenerateIdentityMatrix()
	{
		Matrix3x4 result = {};
		for (size_t axis = 0; axis < 3; axis++)
			result.m_rows[axis][axis] = 1.f;

		return result;
	}

	void AnoxCTCCompiler::MatrixToQuat(const Matrix3x4 &mat, float &outX, float &outY, float &outZ, float &outW)
	{
		const float xTerm = mat.m_rows[0][0];	// (1 - 2yy - 2zz)
		const float yTerm = mat.m_rows[1][1];	// (1 - 2xx - 2zz)
		const float zTerm = mat.m_rows[2][2];	// (1 - 2xx - 2yy)

		const float axisTerms[] =
		{
			xTerm - yTerm - zTerm,	// 4xx - 1
			yTerm - xTerm - zTerm,	// 4yy - 1
			zTerm - xTerm - yTerm,	// 4zz - 1
		};

		float sqResults[4] = {};
		for (size_t axis = 0; axis < 3; axis++)
			sqResults[axis] = (axisTerms[axis] + 1.0f) * 0.25f;
		sqResults[3] = 1.0f - sqResults[0] - sqResults[1] - sqResults[2];

		float finalResults[4] = {};
		for (size_t axis = 0; axis < 4; axis++)
		{
			const float coeff = sqResults[axis];
			if (coeff < 0.0f)
				finalResults[axis] = 0.0f;
			else
				finalResults[axis] = sqrtf(coeff);
		}

		outX = finalResults[0];
		outY = finalResults[1];
		outZ = finalResults[2];
		outW = finalResults[3];
	}

	AnoxCTCCompiler::Matrix3x4 AnoxCTCCompiler::QuatToMatrix(float x, float y, float z, float w)
	{
		Matrix3x4 result = {};
		result.m_rows[0][0] = 1.0f - (2.0f * y * y) - (2.0f * z * z);
		result.m_rows[0][1] = (2.0f * x * y) - (2.0f * z * w);
		result.m_rows[0][2] = (2.0f * x * z) + (2.0f * y * w);

		result.m_rows[1][0] = (2.0f * x * y) + (2.0f * z * w);
		result.m_rows[1][1] = 1.0f - (2.0f * x * x) - (2.0f * z * z);
		result.m_rows[1][2] = (2.0f * y * z) - (2.0f * x * w);

		result.m_rows[2][0] = (2.0f * x * z) - (2.0f * y * w);
		result.m_rows[2][1] = (2.0f * y * z) + (2.0f * x * w);
		result.m_rows[2][2] = 1.0f - (2.0f * x * x) - (2.0f * y * y);

		return result;
	}

	data::MDAModelSkeletalBoneFrame AnoxCTCCompiler::ConvertMatrixToFrame(const Matrix3x4 &mat)
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		float w = 0.0f;
		MatrixToQuat(mat, x, y, z, w);

		data::MDAModelSkeletalBoneFrame result = {};

		for (size_t axis = 0; axis < 3; axis++)
			result.m_translation[axis] = mat.m_rows[axis][3];

		if (w < 0.f)
		{
			x = -x;
			y = -y;
			z = -z;
		}

		result.m_rotation = data::CompressNormalizedQuat(x, y, z);

		return result;
	}

	AnoxCTCCompiler::Matrix3x4 AnoxCTCCompiler::ConvertFrameToMatrix(const data::MDAModelSkeletalBoneFrame &boneFrame)
	{
		float x = 0.f;
		float y = 0.f;
		float z = 0.f;
		float w = 0.f;

		data::DecompressQuat(boneFrame.m_rotation, x, y, z, w);

		AnoxCTCCompiler::Matrix3x4 result = QuatToMatrix(x, y, z, w);

		for (size_t axis = 0; axis < 3; axis++)
			result.m_rows[axis][3] = boneFrame.m_translation[axis].Get();

		return result;
	}

	uint32_t AnoxCTCCompiler::GetVersion() const
	{
		return 2;
	}

	rkit::Result AnoxCTCCompilerBase::ConstructOutputPath(rkit::CIPath &outPath, const rkit::StringView &identifier)
	{
		return AnoxModelCompilerCommon::ConstructOutputPathByType(outPath, u8"ctc", identifier);
	}

	bool AnoxCTCCompiler::CTCCompoundTriVert::operator==(const CTCCompoundTriVert &other) const
	{
		if (m_inPointIndex != other.m_inPointIndex)
			return false;

		if (m_compressedUV[0] != other.m_compressedUV[0])
			return false;

		if (m_compressedUV[1] != other.m_compressedUV[1])
			return false;

		return true;
	}

	bool AnoxCTCCompiler::CTCCompoundTriVert::operator!=(const CTCCompoundTriVert &other) const
	{
		return !((*this) == other);
	}

	rkit::Result AnoxModelCompilerCommon::CompileMDA(rkit::CIPath &outputPath, UncompiledMDAData &mdaData, bool autoSkin, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::FloatingPointStateScope scope(rkit::FloatingPointState::GetCurrent().AppendIEEEStrict());

		rkit::DeduplicatedList<rkit::data::ContentID> materialContentIDs;

		rkit::UniquePtr<rkit::ISeekableReadStream> inputFile;
		RKIT_CHECK(feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, mdaData.m_baseModel, inputFile));

		static const size_t kNumHardCodedNormals = 2048;

		rkit::Vector<rkit::endian::LittleFloat32_t> normalFloats;
		RKIT_CHECK(normalFloats.Resize(kNumHardCodedNormals * 3));

		{
			rkit::Vector<uint8_t> anoxGfxContents;

			rkit::UniquePtr<rkit::ISeekableReadStream> anoxGfxDll;
			RKIT_CHECK(feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kAuxDir0, u8"anoxgfx.dll", anoxGfxDll));

			const rkit::FilePos_t fileSize = anoxGfxDll->GetSize();
			if (anoxGfxDll->GetSize() > std::numeric_limits<size_t>::max())
			{
				rkit::log::Error(u8"anoxgfx.dll is too big");
				RKIT_THROW(rkit::ResultCode::kDataError);
			}

			RKIT_CHECK(anoxGfxContents.Resize(static_cast<size_t>(fileSize)));
			RKIT_CHECK(anoxGfxDll->ReadAll(anoxGfxContents.GetBuffer(), static_cast<size_t>(fileSize)));

			const size_t kNormalBlobSize = 2048 * 3 * 4;

			if (anoxGfxContents.Count() < kNormalBlobSize)
			{
				rkit::log::Error(u8"anoxgfx.dll size was smaller than expected");
				RKIT_THROW(rkit::ResultCode::kDataError);
			}

			const uint8_t kSignature[] = { 0x45, 0x7d, 0x74, 0xbf, 0x11, 0x04 };
			const rkit::ConstSpan<uint8_t> signature(kSignature);

			const size_t maxPos = static_cast<size_t>(fileSize - kNormalBlobSize);

			const rkit::ConstSpan<uint8_t> fileSpan = anoxGfxContents.ToSpan();

			size_t startPos = 0;
			for (startPos = 0; startPos < maxPos; startPos++)
			{
				if (rkit::CompareSpansEqual(signature, fileSpan.SubSpan(startPos, signature.Count())))
					break;
			}

			if (startPos == maxPos)
			{
				rkit::log::Error(u8"Couldn't find normal blob in anoxgfx.dll");
				RKIT_THROW(rkit::ResultCode::kDataError);
			}

			rkit::CopySpanNonOverlapping(normalFloats.ToSpan().ReinterpretCast<uint8_t>(), fileSpan.SubSpan(startPos, kNormalBlobSize));
		}

		MD2Header header;
		RKIT_CHECK(inputFile->ReadAll(&header, sizeof(MD2Header)));

		if (header.m_numTextureBlocks.Get() != header.m_numTextures.Get())
		{
			rkit::log::Error(u8"Texture block count doesn't match texture count");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		const uint32_t numTextures = header.m_numTextures.Get();

		if (!autoSkin)
		{
			for (const UncompiledMDAProfile &profile : mdaData.m_profiles)
			{
				if (profile.m_skins.Count() != numTextures)
				{
					rkit::log::Error(u8"Skin count didn't match MD2 texture count");
					RKIT_THROW(rkit::ResultCode::kDataError);
				}
			}
		}

		if (header.m_magic.Get() != MD2Header::kExpectedMagic
			|| header.m_majorVersion.Get() < MD2Header::kMinMajorVersion
			|| header.m_majorVersion.Get() > MD2Header::kMaxMajorVersion
			|| header.m_minorVersion.Get() > MD2Header::kMaxMinorVersion)
		{
			rkit::log::Error(u8"MD2 file appears to be invalid");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		const uint32_t numFrames = header.m_numFrames.Get();
		const uint32_t numXYZ = header.m_numXYZ.Get();

		if (numFrames == 0 || numXYZ == 0)
		{
			rkit::log::Error(u8"MD2 file has no frames or verts");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		const uint32_t frameSizeBytes = header.m_frameSizeBytes.Get();

		if (frameSizeBytes < sizeof(MD2FrameHeader))
		{
			rkit::log::Error(u8"MD2 frame size is invalid");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		uint32_t vertSize = (frameSizeBytes - sizeof(MD2FrameHeader));

		if (vertSize % numXYZ != 0)
		{
			rkit::log::Error(u8"Couldn't determine vert size");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		vertSize /= numXYZ;

		if (vertSize != 8 && vertSize != 6 && vertSize != 5)
		{
			rkit::log::Error(u8"Unknown vert size");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		rkit::Vector<rkit::endian::LittleUInt32_t> glCommandsArray;
		RKIT_CHECK(glCommandsArray.Resize(header.m_numGLCalls.Get()));

		rkit::Vector<rkit::endian::LittleUInt16_t> textureBlockSizes;
		RKIT_CHECK(textureBlockSizes.Resize(header.m_numTextureBlocks.Get()));

		RKIT_CHECK(inputFile->SeekStart(header.m_glCommandPos.Get()));
		RKIT_CHECK(inputFile->ReadAll(glCommandsArray.GetBuffer(), glCommandsArray.Count() * 4u));

		RKIT_CHECK(inputFile->SeekStart(header.m_textureBlockPos.Get()));
		RKIT_CHECK(inputFile->ReadAll(textureBlockSizes.GetBuffer(), textureBlockSizes.Count() * 2u));

		rkit::Vector<UncompiledTriList> triLists;
		RKIT_CHECK(triLists.Resize(numTextures));

		size_t currentTextureBlock = 0;
		rkit::ConstSpan<rkit::endian::LittleUInt32_t> glCommands = glCommandsArray.ToSpan();
		while (glCommands.Count() > 0)
		{
			const uint32_t baseCount = glCommands[0].Get();
			glCommands = glCommands.SubSpan(1);

			if (baseCount == 0)
				break;

			while (currentTextureBlock < textureBlockSizes.Count())
			{
				if (textureBlockSizes[currentTextureBlock].Get() != 0)
					break;

				currentTextureBlock++;
			}

			if (currentTextureBlock == textureBlockSizes.Count())
			{
				rkit::log::Error(u8"Texture blocks desynced");
				RKIT_THROW(rkit::ResultCode::kDataError);
			}

			textureBlockSizes[currentTextureBlock] = textureBlockSizes[currentTextureBlock].Get() - 1;

			rkit::Vector<ModelProtoVert> &vertList = triLists[currentTextureBlock].m_verts;

			const bool isTriFan = ((baseCount & 0x80000000u) != 0);

			const uint32_t realCount = isTriFan ? static_cast<uint32_t>(~static_cast<uint32_t>(baseCount - 1)) : baseCount;
			if (realCount < 3 || realCount > (glCommands.Count() / 3u))
			{
				rkit::log::Error(u8"Invalid GL command");
				RKIT_THROW(rkit::ResultCode::kDataError);
			}

			const rkit::ConstSpan<rkit::endian::LittleUInt32_t> commandArgs = glCommands.SubSpan(0, realCount * 3);
			glCommands = glCommands.SubSpan(realCount * 3);

			size_t firstOutVert = vertList.Count();
			for (size_t i = 0; i < realCount; i++)
			{
				ModelProtoVert vert;
				vert.m_uBits = commandArgs[i * 3 + 0].Get();
				vert.m_vBits = commandArgs[i * 3 + 1].Get();
				vert.m_xyzIndex = commandArgs[i * 3 + 2].Get();

				// Canonicalize -0.0 tex coords into +0.0
				if (vert.m_uBits == 0x80000000u)
					vert.m_uBits = 0;

				if (vert.m_vBits == 0x80000000u)
					vert.m_vBits = 0;

				if (vert.m_xyzIndex >= numXYZ)
				{
					rkit::log::Error(u8"Invalid GL command vert");
					RKIT_THROW(rkit::ResultCode::kDataError);
				}

				if (i >= 3)
				{
					if (isTriFan)
					{
						RKIT_CHECK(vertList.Append(vertList[firstOutVert]));
						RKIT_CHECK(vertList.Append(vertList[vertList.Count() - 2]));
					}
					else
					{
						// Tristrip
						if ((i & 1) != 0)
						{
							RKIT_CHECK(vertList.Append(vertList[vertList.Count() - 1]));
							RKIT_CHECK(vertList.Append(vertList[vertList.Count() - 3]));
						}
						else
						{
							RKIT_CHECK(vertList.Append(vertList[vertList.Count() - 3]));
							RKIT_CHECK(vertList.Append(vertList[vertList.Count() - 2]));
						}
					}
				}

				RKIT_CHECK(vertList.Append(vert));
			}
		}

		rkit::Vector<char> animNameChars;
		rkit::Vector<data::MDAAnimation> animations;
		rkit::Vector<data::MDAModelMorphKey> morphKeys;

		rkit::Vector<UncompiledMDAMorph> morphs;

		rkit::BoolVector xyzHasMorph;
		RKIT_CHECK(xyzHasMorph.Resize(numXYZ));

		if (mdaData.m_animChunk.Count() > 0)
		{
			rkit::ConstSpan<uint8_t> animDataBytes = mdaData.m_animChunk.ToSpan();

			MDAAnimationChunkData animData;
			if (animDataBytes.Count() < sizeof(animData))
			{
				rkit::log::Error(u8"Anim data too small");
				RKIT_THROW(rkit::ResultCode::kDataError);
			}

			rkit::CopySpanNonOverlapping(rkit::Span<MDAAnimationChunkData>(&animData, 1).ReinterpretCast<uint8_t>(), animDataBytes.SubSpan(0, sizeof(animData)));

			animDataBytes = animDataBytes.SubSpan(sizeof(animData));

			const uint32_t numAnimations = animData.m_numAnimInfos.Get();

			if (animDataBytes.Count() / sizeof(MDAAnimInfoData) < numAnimations)
			{
				rkit::log::Error(u8"Anim data was truncated");
				RKIT_THROW(rkit::ResultCode::kDataError);
			}

			for (uint32_t animIndex = 0; animIndex < numAnimations; animIndex++)
			{
				MDAAnimInfoData inAnim;
				data::MDAAnimation outAnim = {};

				rkit::CopySpanNonOverlapping(rkit::Span<MDAAnimInfoData>(&inAnim, 1).ReinterpretCast<uint8_t>(), animDataBytes.SubSpan(0, sizeof(inAnim)));

				const uint32_t animFirstFrame = inAnim.m_firstFrame.Get();
				const uint32_t animNumFrames = inAnim.m_numFrames.Get();

				if (numFrames == 0 || animFirstFrame > numFrames || (numFrames - animFirstFrame) < animNumFrames)
				{
					rkit::log::Error(u8"Anim frame range was invalid");
					RKIT_THROW(rkit::ResultCode::kDataError);
				}

				uint8_t nameLength = sizeof(inAnim.m_animCategory);
				for (uint8_t checkChar = 0; checkChar < nameLength; checkChar++)
				{
					if (inAnim.m_animCategory[checkChar] == 0)
					{
						nameLength = checkChar;
						break;
					}
				}

				outAnim.m_animNumber = inAnim.m_animNumber;
				outAnim.m_firstFrame = animFirstFrame;
				outAnim.m_numFrames = animNumFrames;
				outAnim.m_categoryLength = nameLength;

				RKIT_CHECK(animations.Append(std::move(outAnim)));
				RKIT_CHECK(animNameChars.Append(rkit::ConstSpan<char>(inAnim.m_animCategory, nameLength)));

				animDataBytes = animDataBytes.SubSpan(sizeof(inAnim));
			}
		}

		if (mdaData.m_morphChunk.Count() > 0)
		{
			const rkit::ConstSpan<uint8_t> morphDataBytes = mdaData.m_morphChunk.ToSpan();

			MDAMorphChunkData morphData;
			if (morphDataBytes.Count() < sizeof(morphData))
			{
				rkit::log::Error(u8"Morph data too small");
				RKIT_THROW(rkit::ResultCode::kDataError);
			}

			rkit::CopySpanNonOverlapping(rkit::Span<MDAMorphChunkData>(&morphData, 1).ReinterpretCast<uint8_t>(), morphDataBytes.SubSpan(0, sizeof(morphData)));

			const rkit::ConstSpan<uint8_t> morphKeyBytes = morphDataBytes.SubSpan(sizeof(morphData));

			const uint32_t headBoneFourCC = morphData.m_headBoneID.Get();
			const uint32_t numKeys = morphData.m_numKeys.Get();

			if (morphKeyBytes.Count() / sizeof(MDAMorphKey) < numKeys)
			{
				rkit::log::Error(u8"Morph key data too small");
				RKIT_THROW(rkit::ResultCode::kDataError);
			}

			RKIT_CHECK(morphKeys.Resize(numKeys));
			RKIT_CHECK(morphs.Resize(numKeys));

			for (uint32_t keyIndex = 0; keyIndex < numKeys; keyIndex++)
			{
				MDAMorphKey inKey = {};

				rkit::CopySpanNonOverlapping(rkit::Span<MDAMorphKey>(&inKey, 1).ReinterpretCast<uint8_t>(), morphKeyBytes.SubSpan(keyIndex * sizeof(inKey), sizeof(inKey)));

				const uint32_t vertMorphDataPos = inKey.m_dataPosition.Get();
				const uint32_t numVertMorphs = inKey.m_numMorphs.Get();

				morphKeys[keyIndex].m_morphKeyFourCC = inKey.m_morphID;

				rkit::Vector<UncompiledMDAVertMorph> &vertMorphs = morphs[keyIndex].m_vertMorphs;

				if (vertMorphDataPos > morphDataBytes.Count() || (morphDataBytes.Count() - vertMorphDataPos) / sizeof(MDAVertMorph) < numVertMorphs)
				{
					rkit::log::Error(u8"Morph data too small");
					RKIT_THROW(rkit::ResultCode::kDataError);
				}

				RKIT_CHECK(vertMorphs.Resize(numVertMorphs));
				for (uint32_t vertMorphIndex = 0; vertMorphIndex < numVertMorphs; vertMorphIndex++)
				{
					UncompiledMDAVertMorph &outVertMorph = vertMorphs[vertMorphIndex];

					MDAVertMorph inVertMorph = {};

					rkit::CopySpanNonOverlapping(rkit::Span<MDAVertMorph>(&inVertMorph, 1).ReinterpretCast<uint8_t>(), morphDataBytes.SubSpan(vertMorphDataPos + vertMorphIndex * sizeof(inVertMorph), sizeof(inVertMorph)));

					const uint32_t vertIndex = inVertMorph.m_vertIndex.Get();

					if (vertIndex >= numXYZ)
					{
						rkit::log::Error(u8"Vert morph vertex was invalid");
						RKIT_THROW(rkit::ResultCode::kDataError);
					}

					outVertMorph.m_vertIndex = vertIndex;
					for (size_t axis = 0; axis < 3; axis++)
						outVertMorph.m_delta[axis] = inVertMorph.m_offset[axis].Get();

					xyzHasMorph[vertIndex] = true;
				}
			}
		}

		// Compute point remappings
		uint32_t numMorphedPoints = 0;

		for (bool hasMorph : xyzHasMorph)
		{
			if (hasMorph)
				numMorphedPoints++;
		}

		rkit::Vector<uint32_t> xyzToPointIndex;
		RKIT_CHECK(xyzToPointIndex.Resize(numXYZ));

		rkit::Vector<uint32_t> pointToXYZIndex;
		RKIT_CHECK(pointToXYZIndex.Resize(numXYZ));

		for (uint32_t i = 0; i < numXYZ; i++)
			pointToXYZIndex[i] = i;

		rkit::QuickSort(pointToXYZIndex.begin(), pointToXYZIndex.end(), [&xyzHasMorph](uint32_t a, uint32_t b)
			{
				const bool aHasMorph = xyzHasMorph[a];
				const bool bHasMorph = xyzHasMorph[b];

				if (aHasMorph != bHasMorph)
					return aHasMorph;

				return a < b;
			});

		for (uint32_t i = 0; i < numXYZ; i++)
			xyzToPointIndex[pointToXYZIndex[i]] = i;

		rkit::Vector<data::MDAVertexModelBone> vertexBones;
		rkit::Vector<rkit::Vector<data::MDAModelTagBoneFrame>> boneFrames;

		if (mdaData.m_boneChunk.Count() > 0)
		{
			const rkit::ConstSpan<uint8_t> boneDataBytes = mdaData.m_boneChunk.ToSpan();

			MDABoneChunkData boneData;
			if (boneDataBytes.Count() < sizeof(boneData))
			{
				rkit::log::Error(u8"Bone data too small");
				RKIT_THROW(rkit::ResultCode::kDataError);
			}

			rkit::CopySpanNonOverlapping(rkit::Span<MDABoneChunkData>(&boneData, 1).ReinterpretCast<uint8_t>(), boneDataBytes.SubSpan(0, sizeof(boneData)));

			const rkit::ConstSpan<uint8_t> boneFrameBytes = boneDataBytes.SubSpan(sizeof(boneData));

			if (boneFrameBytes.Count() / sizeof(MDABoneFrame) < numFrames)
			{
				rkit::log::Error(u8"Bone frame data was too small");
				RKIT_THROW(rkit::ResultCode::kDataError);
			}

			RKIT_CHECK(vertexBones.Resize(1));
			RKIT_CHECK(boneFrames.Resize(1));

			vertexBones[0].m_boneIDFourCC = boneData.m_boneID.Get();

			rkit::Vector<data::MDAModelTagBoneFrame> &outFrames = boneFrames[0];

			RKIT_CHECK(outFrames.Resize(numFrames));
			for (uint32_t frameIndex = 0; frameIndex < numFrames; frameIndex++)
			{
				MDABoneFrame inFrame = {};
				data::MDAModelTagBoneFrame &outFrame = outFrames[frameIndex];

				rkit::CopySpanNonOverlapping(rkit::Span<MDABoneFrame>(&inFrame, 1).ReinterpretCast<uint8_t>(), boneFrameBytes.SubSpan(frameIndex * sizeof(inFrame), sizeof(inFrame)));

				float matrix[4][4] = {};

				for (size_t matrixElement = 0; matrixElement < 16; matrixElement++)
				{
					matrix[matrixElement % 4][matrixElement / 4] = inFrame.m_matrix[matrixElement].Get();
				}

				for (size_t row = 0; row < 3; row++)
				{
					for (size_t col = 0; col < 4; col++)
						outFrame.m_matrix[row][col] = matrix[row][col];
				}
			}
		}

		for (UncompiledTriList &triList : triLists)
		{
			RKIT_CHECK(CompileMDASubmodels(triList, xyzToPointIndex.ToSpan()));
		}

		rkit::Vector<data::MDAModelPoint> points;
		RKIT_CHECK(points.Resize(static_cast<size_t>(numXYZ) * numFrames));

		rkit::Vector<uint8_t> frameData;
		RKIT_CHECK(frameData.Resize(frameSizeBytes));

		RKIT_CHECK(inputFile->SeekStart(header.m_framePos.Get()));

		for (size_t frameIndex = 0; frameIndex < numFrames; frameIndex++)
		{
			RKIT_CHECK(inputFile->ReadAll(frameData.GetBuffer(), frameSizeBytes));

			MD2FrameHeader frameHeader;
			rkit::CopySpanNonOverlapping(rkit::Span<MD2FrameHeader>(&frameHeader, 1).ReinterpretCast<uint8_t>(), frameData.ToSpan().SubSpan(0, sizeof(MD2FrameHeader)));

			float translate[3] =
			{
				frameHeader.m_translate[0].Get(),
				frameHeader.m_translate[1].Get(),
				frameHeader.m_translate[2].Get()
			};

			float scale[3] =
			{
				frameHeader.m_scale[0].Get(),
				frameHeader.m_scale[1].Get(),
				frameHeader.m_scale[2].Get()
			};

			for (size_t xyzIndex = 0; xyzIndex < numXYZ; xyzIndex++)
			{
				data::MDAModelPoint &outPoint = points[frameIndex * numXYZ + xyzToPointIndex[xyzIndex]];

				const rkit::ConstSpan<uint8_t> vertData = frameData.ToSpan().SubSpan(sizeof(MD2FrameHeader) + xyzIndex * vertSize);

				const uint16_t normalIndex = vertData[vertSize - 2] + (static_cast<uint16_t>(vertData[vertSize - 1]) << 8);

				if (normalIndex >= 2048)
				{
					rkit::log::Error(u8"Normal index was out of range");
					RKIT_THROW(rkit::ResultCode::kDataError);
				}

				uint16_t vertUIntCoords[3];

				switch (vertSize)
				{
				case 5:
					for (size_t axis = 0; axis < 3; axis++)
						vertUIntCoords[axis] = vertData[axis];
					break;
				case 6:
					{
						rkit::endian::LittleUInt32_t vertU32Data;
						rkit::CopySpanNonOverlapping(vertU32Data.GetBytes().ToSpan(), vertData.SubSpan(0, 4));

						const uint32_t packedVert = vertU32Data.Get();

						vertUIntCoords[0] = (packedVert & 0x7ff);
						vertUIntCoords[1] = ((packedVert >> 11) & 0x3ff);
						vertUIntCoords[2] = ((packedVert >> 21) & 0x7ff);
					}
					break;
				case 8:
					for (size_t axis = 0; axis < 3; axis++)
						vertUIntCoords[axis] = vertData[axis * 2] + static_cast<uint16_t>(vertData[axis * 2 + 1] << 8);
					break;
				default:
					RKIT_THROW(rkit::ResultCode::kInternalError);
				}

				for (size_t axis = 0; axis < 3; axis++)
					outPoint.m_point[axis] = scale[axis] * static_cast<float>(vertUIntCoords[axis]) + translate[axis];

				float normal[3];
				for (size_t axis = 0; axis < 3; axis++)
					normal[axis] = normalFloats[normalIndex * 3 + axis].Get();

				outPoint.m_compressedNormal.m_value = data::CompressNormal32(normal[0], normal[1], normal[2]);
			}
		}

		// Generate vert morphs
		rkit::Vector<data::MDAModelVertMorph> compiledVertMorphs;

		{
			size_t morphKeyListSize = 0;
			RKIT_CHECK(rkit::SafeMul<size_t>(morphKeyListSize, morphs.Count(), numMorphedPoints));

			RKIT_CHECK(compiledVertMorphs.Resize(morphKeyListSize));
		}

		if (numMorphedPoints > 0)
		{
			for (size_t morphIndex = 0; morphIndex < morphs.Count(); morphIndex++)
			{
				rkit::Span<data::MDAModelVertMorph> keyVertMorphs = compiledVertMorphs.ToSpan().SubSpan(morphIndex * numMorphedPoints, numMorphedPoints);

				for (data::MDAModelVertMorph &vertMorph : keyVertMorphs)
					vertMorph = {};

				const UncompiledMDAMorph &uncompiledMorph = morphs[morphIndex];
				for (const UncompiledMDAVertMorph &inVertMorph : morphs[morphIndex].m_vertMorphs)
				{
					const uint32_t pointIndex = xyzToPointIndex[inVertMorph.m_vertIndex];

					data::MDAModelVertMorph &outVertMorph = keyVertMorphs[pointIndex];

					rkit::StaticArray<float, 3> vertMorphBase;
					for (size_t axis = 0; axis < 3; axis++)
						vertMorphBase[axis] = inVertMorph.m_delta[axis];

					for (size_t axis = 0; axis < 3; axis++)
						outVertMorph.m_delta[axis] = inVertMorph.m_delta[axis];
				}
			}
		}

		rkit::Vector<data::MDAProfile> profiles;
		rkit::Vector<rkit::Vector<data::MDASkin>> profileSkins;
		rkit::Vector<rkit::Vector<rkit::Vector<data::MDASkinPass>>> profileSkinPasses;

		rkit::Vector<char> profileConditionStrings;

		UncompiledMDAProfile autoSkinProfile;
		rkit::ConstSpan<UncompiledMDAProfile> mdaProfiles;

		if (autoSkin)
		{
			rkit::Vector<MD2TextureDef> md2Textures;
			RKIT_CHECK(md2Textures.Resize(numTextures));

			RKIT_CHECK(inputFile->SeekStart(header.m_texturePos.Get()));
			RKIT_CHECK(inputFile->ReadAll(md2Textures.GetBuffer(), sizeof(MD2TextureDef) * numTextures));

			autoSkinProfile.m_fourCC = RKIT_FOURCC('D', 'F', 'L', 'T');
			RKIT_CHECK(autoSkinProfile.m_skins.Resize(numTextures));

			for (size_t texIndex = 0; texIndex < numTextures; texIndex++)
			{
				const MD2TextureDef &textureDef = md2Textures[texIndex];

				UncompiledMDASkin &skin = autoSkinProfile.m_skins[texIndex];
				RKIT_CHECK(skin.m_passes.Resize(1));

				UncompiledMDAPass &pass = skin.m_passes[0];

				RKIT_CHECK(ResolveTexturePath(pass.m_map, mdaData.m_baseModel, textureDef, false));
			}

			mdaProfiles = rkit::ConstSpan<UncompiledMDAProfile>(&autoSkinProfile, 1);
		}
		else
			mdaProfiles = mdaData.m_profiles.ToSpan();

		{
			const size_t numProfiles = mdaProfiles.Count();

			RKIT_CHECK(profiles.Resize(numProfiles));
			RKIT_CHECK(profileSkins.Resize(numProfiles));
			RKIT_CHECK(profileSkinPasses.Resize(numProfiles));

			for (size_t profileIndex = 0; profileIndex < numProfiles; profileIndex++)
			{
				const UncompiledMDAProfile &inProfile = mdaProfiles[profileIndex];

				data::MDAProfile &outProfile = profiles[profileIndex];

				if (inProfile.m_evaluate.Length() > std::numeric_limits<uint32_t>::max())
				{
					rkit::log::Error(u8"Skin condition length too long");
					RKIT_THROW(rkit::ResultCode::kDataError);
				}

				outProfile.m_fourCC = inProfile.m_fourCC;
				outProfile.m_conditionLength = static_cast<uint32_t>(inProfile.m_evaluate.Length());

				RKIT_CHECK(profileConditionStrings.Append(inProfile.m_evaluate.ToSpan()));

				rkit::Vector<data::MDASkin> &outSkins = profileSkins[profileIndex];
				rkit::Vector<rkit::Vector<data::MDASkinPass>> &outSkinPasses = profileSkinPasses[profileIndex];

				RKIT_CHECK(outSkins.Resize(numTextures));
				RKIT_CHECK(outSkinPasses.Resize(numTextures));

				for (size_t skinIndex = 0; skinIndex < numTextures; skinIndex++)
				{
					const UncompiledMDASkin &inSkin = inProfile.m_skins[skinIndex];
					const size_t numPasses = inSkin.m_passes.Count();

					rkit::Vector<data::MDASkinPass> &outPasses = outSkinPasses[skinIndex];
					data::MDASkin &outSkin = outSkins[skinIndex];

					if (numPasses > 0xffu)
					{
						rkit::log::Error(u8"Too many passes on one skin");
						RKIT_THROW(rkit::ResultCode::kDataError);
					}

					RKIT_CHECK(outPasses.Resize(numPasses));

					outSkin.m_numPasses = static_cast<uint8_t>(numPasses);


					if (inSkin.m_sortMode.IsSet())
						outSkin.m_sortMode = static_cast<uint8_t>(inSkin.m_sortMode.Get());

					for (size_t passIndex = 0; passIndex < numPasses; passIndex++)
					{
						data::MDASkinPass &outPass = outPasses[passIndex];
						const UncompiledMDAPass &inPass = inSkin.m_passes[passIndex];

						rkit::String materialPath;
						RKIT_CHECK(materialPath.Format(u8"{}.{}", inPass.m_map.ToString(), MaterialCompiler::GetModelMaterialExtension()));

						rkit::CIPath compiledMaterialPath;
						RKIT_CHECK(MaterialCompiler::ConstructOutputPath(compiledMaterialPath, data::MaterialResourceType::kModel, materialPath));

						rkit::data::ContentID materialContentID;
						RKIT_CHECK(feedback->IndexCAS(rkit::buildsystem::BuildFileLocation::kIntermediateDir, compiledMaterialPath, materialContentID));

						typedef uint16_t StoredMaterialIndex_t;
						size_t materialIndex = 0;
						RKIT_CHECK(materialContentIDs.AddAndGetIndex(materialIndex, materialContentID));

						if (materialIndex > std::numeric_limits<uint16_t>::max())
							RKIT_THROW(rkit::ResultCode::kIntegerOverflow);

						outPass.m_materialIndex = static_cast<StoredMaterialIndex_t>(materialIndex);

						bool needMaterialData = false;
						outPass.m_clampFlag = (inPass.m_clamp ? 1 : 0);
						outPass.m_scrollU = inPass.m_scrollU;
						outPass.m_scrollV = inPass.m_scrollV;

						if (inPass.m_alphaTestMode.IsSet())
							outPass.m_alphaTestMode = static_cast<uint8_t>(inPass.m_alphaTestMode.Get());
						else
							needMaterialData = true;

						if (inPass.m_depthWrite.IsSet())
							outPass.m_depthWriteFlag = inPass.m_depthWrite.Get() ? 1 : 0;
						else
							needMaterialData = true;

						if (inPass.m_blendMode.IsSet())
							outPass.m_blendMode = static_cast<uint8_t>(inPass.m_blendMode.Get());
						else
							needMaterialData = true;

						if (inPass.m_uvGenMode.IsSet())
							outPass.m_uvGenMode = static_cast<uint8_t>(inPass.m_uvGenMode.Get());

						if (inPass.m_rgbGenMode.IsSet())
							outPass.m_rgbGenMode = static_cast<uint8_t>(inPass.m_rgbGenMode.Get());

						if (inPass.m_depthFunc.IsSet())
							outPass.m_depthFunc = static_cast<uint8_t>(inPass.m_depthFunc.Get());

						if (inPass.m_cullType.IsSet())
							outPass.m_cullType = static_cast<uint8_t>(inPass.m_cullType.Get());

						if (needMaterialData)
						{
							rkit::UniquePtr<rkit::ISeekableReadStream> materialStream;
							RKIT_CHECK(feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, compiledMaterialPath, materialStream));

							data::MaterialHeader matHeader;
							RKIT_CHECK(materialStream->ReadAll(&matHeader, sizeof(matHeader)));

							const data::MaterialColorType colorType = static_cast<data::MaterialColorType>(matHeader.m_colorType);

							bool haveAlpha = false;

							switch (colorType)
							{
							case data::MaterialColorType::kLuminance:
							case data::MaterialColorType::kRGB:
								haveAlpha = false;
								break;
							case data::MaterialColorType::kLuminanceAlpha:
							case data::MaterialColorType::kRGBA:
								haveAlpha = true;
								break;
							default:
								rkit::log::Error(u8"Unknown material color type");
								RKIT_THROW(rkit::ResultCode::kDataError);
							}

							if (haveAlpha)
							{
								if (!inPass.m_alphaTestMode.IsSet())
									outPass.m_alphaTestMode = static_cast<uint8_t>(data::MDAAlphaTestMode::kGT0);

								if (!inPass.m_depthWrite.IsSet())
									outPass.m_depthWriteFlag = false;

								if (!inPass.m_blendMode.IsSet())
									outPass.m_blendMode = static_cast<uint8_t>(data::MDABlendMode::kAlphaBlend);
							}
						}
					}
				}
			}
		}

		// Construct final header
		const rkit::ConstSpan<rkit::data::ContentID> materials = materialContentIDs.GetItems();

		if (numTextures > 0xff)
		{
			rkit::log::Error(u8"Too many materials");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		if (numFrames > 0xffff)
		{
			rkit::log::Error(u8"Too many frames");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		if (numXYZ > 0xffff)
		{
			rkit::log::Error(u8"Too many verts");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		if (animations.Count() > 0x7fu)
		{
			rkit::log::Error(u8"Too many animations");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		if (morphKeys.Count() > 0xffffu)
		{
			rkit::log::Error(u8"Too many morph keys");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		if (vertexBones.Count() > 0xffffu)
		{
			rkit::log::Error(u8"Too many bones");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		if (profiles.Count() > 0xffu)
		{
			rkit::log::Error(u8"Too many profiles");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		if (materials.Count() > 0xffffu)
		{
			rkit::log::Error(u8"Too many materials");
			RKIT_THROW(rkit::ResultCode::kDataError);
		}

		data::MDAModelHeader outHeader = {};

		for (const UncompiledTriList &triList : triLists)
		{
			const size_t maxSubModels = 0xff;
			if (triList.m_submodels.Count() > maxSubModels || (maxSubModels - outHeader.m_numSubModels) < triList.m_submodels.Count())
			{
				rkit::log::Error(u8"Too many submodels");
				RKIT_THROW(rkit::ResultCode::kDataError);
			}

			outHeader.m_numSubModels += static_cast<uint8_t>(triList.m_submodels.Count());
		}

		outHeader.m_numSkins = numTextures;
		outHeader.m_numMorphKeys = static_cast<uint16_t>(morphKeys.Count());
		outHeader.m_numBones = static_cast<uint16_t>(vertexBones.Count());
		outHeader.m_numFrames = static_cast<uint16_t>(numFrames);
		outHeader.m_numPoints = static_cast<uint16_t>(numXYZ);
		outHeader.m_numMorphedPoints = static_cast<uint16_t>(numMorphedPoints);
		outHeader.m_numAnimations7_AnimationType1 = static_cast<uint8_t>(animations.Count());
		outHeader.m_numProfiles = static_cast<uint8_t>(profiles.Count());
		outHeader.m_numMaterials = static_cast<uint16_t>(materials.Count());

		rkit::UniquePtr<rkit::ISeekableReadWriteStream> outFile;
		RKIT_CHECK(feedback->OpenOutput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, outputPath, outFile));

		// Write header
		RKIT_CHECK(outFile->WriteOneBinary(outHeader));

		// Write materials
		RKIT_CHECK(outFile->WriteAllSpan(materials));

		// Write profiles
		RKIT_CHECK(outFile->WriteAllSpan(profiles.ToSpan()));

		// Write profile condition chars
		RKIT_CHECK(outFile->WriteAllSpan(profileConditionStrings.ToSpan()));

		// Write profile skins
		for (const rkit::Vector<data::MDASkin> &skinList : profileSkins)
		{
			RKIT_CHECK(outFile->WriteAllSpan(skinList.ToSpan()));
		}

		// Write profile skin passes
		for (const rkit::Vector<rkit::Vector<data::MDASkinPass>> &skinPassesList : profileSkinPasses)
		{
			for (const rkit::Vector<data::MDASkinPass> &passesList : skinPassesList)
			{
				RKIT_CHECK(outFile->WriteAllSpan(passesList.ToSpan()));
			}
		}

		// Write animations
		RKIT_CHECK(outFile->WriteAllSpan(animations.ToSpan()));

		// Write animation names
		RKIT_CHECK(outFile->WriteAllSpan(animNameChars.ToSpan()));

		// Write morph keys
		RKIT_CHECK(outFile->WriteAllSpan(morphKeys.ToSpan()));

		// Write bones
		RKIT_CHECK(outFile->WriteAllSpan(vertexBones.ToSpan()));

		// Write bone frames
		for (const rkit::Vector<data::MDAModelTagBoneFrame> &boneFramesList : boneFrames)
		{
			RKIT_CHECK(outFile->WriteAllSpan(boneFramesList.ToSpan()));
		}

		// Write submodels
		for (size_t textureIndex = 0; textureIndex < triLists.Count(); textureIndex++)
		{
			const UncompiledTriList &triList = triLists[textureIndex];

			for (const UncompiledMDASubmodel &subModel : triList.m_submodels)
			{
				if (subModel.m_verts.Count() == 0)
				{
					rkit::log::Error(u8"Submodel has no verts");
					RKIT_THROW(rkit::ResultCode::kDataError);
				}

				data::MDAModelSubModel outSubModel = {};
				outSubModel.m_materialIndex = static_cast<uint16_t>(textureIndex);
				outSubModel.m_numTris = static_cast<uint32_t>(subModel.m_tris.Count());
				outSubModel.m_numVertsMinusOne = static_cast<uint16_t>(subModel.m_verts.Count() - 1);

				RKIT_CHECK(outFile->WriteOneBinary(outSubModel));
			}
		}

		// Write tris
		for (const UncompiledTriList &triList : triLists)
		{
			for (const UncompiledMDASubmodel &subModel : triList.m_submodels)
			{
				RKIT_CHECK(outFile->WriteAllSpan(subModel.m_tris.ToSpan()));
			}
		}

		// Write verts
		for (const UncompiledTriList &triList : triLists)
		{
			for (const UncompiledMDASubmodel &subModel : triList.m_submodels)
			{
				RKIT_CHECK(outFile->WriteAllSpan(subModel.m_verts.ToSpan()));
			}
		}

		// Write points
		RKIT_CHECK(outFile->WriteAllSpan(points.ToSpan()));

		// Write vert morphs
		RKIT_CHECK(outFile->WriteAllSpan(compiledVertMorphs.ToSpan()));

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxModelCompilerCommon::CompileMDASubmodels(UncompiledTriList &triList, rkit::Span<uint32_t> xyzToPointIndex)
	{
		RKIT_ASSERT(triList.m_verts.Count() % 3 == 0);

		rkit::BoolVector triEmitted;
		RKIT_CHECK(triEmitted.Resize(triList.m_verts.Count() / 3));

		bool compiledAnything = true;
		while (compiledAnything)
		{
			compiledAnything = false;

			UncompiledMDASubmodel submodel;
			RKIT_CHECK(CompileMDASubmodel(compiledAnything, submodel, triList, xyzToPointIndex, triEmitted));

			if (compiledAnything)
			{
				RKIT_CHECK(triList.m_submodels.Append(std::move(submodel)));
			}
		}

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxModelCompilerCommon::CompileMDASubmodel(bool &outEmittedAnything, UncompiledMDASubmodel &outSubmodel,
		UncompiledTriList &triList, rkit::Span<uint32_t> xyzToPointIndex, rkit::BoolVector &triEmitted)
	{
		outEmittedAnything = false;

		bool emittedAnything = false;

		rkit::HashMap<ModelProtoVert, size_t> protoVertToVertIndex;

		rkit::Vector<size_t> uncompiledPoints;

		for (size_t i = 0; i < triEmitted.Count(); i++)
		{
			if (triEmitted[i])
				continue;

			const size_t vertIndex = i * 3;

			const ModelProtoVert verts[3] = {
				triList.m_verts[vertIndex + 0],
				triList.m_verts[vertIndex + 1],
				triList.m_verts[vertIndex + 2]
			};

			size_t numNewVerts = 0;

			for (const ModelProtoVert &vert : verts)
			{
				if (protoVertToVertIndex.FindPrehashed(vert.ComputeHash(), vert) == protoVertToVertIndex.end())
					numNewVerts++;
			}

			// Can't emit this tri
			if (numNewVerts + protoVertToVertIndex.Count() > 0xffff)
				continue;

			// Can emit this tri
			data::MDAModelTri tri;
			for (size_t triVert = 0; triVert < 3; triVert++)
			{
				const ModelProtoVert &protoVert = verts[triVert];

				rkit::HashValue_t hash = protoVert.ComputeHash();

				rkit::HashMap<ModelProtoVert, size_t>::ConstIterator_t it = protoVertToVertIndex.FindPrehashed(hash, protoVert);

				size_t vertIndex = protoVertToVertIndex.Count();
				if (it == protoVertToVertIndex.end())
				{
					RKIT_CHECK(uncompiledPoints.Append(vertIndex));
					RKIT_CHECK(protoVertToVertIndex.SetPrehashed(hash, protoVert, vertIndex));

					data::MDAModelVert mdaVert;
					mdaVert.m_texCoordU = CompressUV(protoVert.m_uBits);
					mdaVert.m_texCoordV = CompressUV(protoVert.m_vBits);
					mdaVert.m_pointID = xyzToPointIndex[protoVert.m_xyzIndex];

					RKIT_CHECK(outSubmodel.m_verts.Append(mdaVert));
				}
				else
					vertIndex = it.Value();

				tri.m_verts[triVert] = static_cast<uint16_t>(vertIndex);
			}

			RKIT_CHECK(outSubmodel.m_tris.Append(tri));

			triEmitted[i] = true;
			emittedAnything = true;
		}

		RKIT_ASSERT(outSubmodel.m_verts.Count() == protoVertToVertIndex.Count());

		outEmittedAnything = emittedAnything;

		RKIT_RETURN_OK;
	}

	uint16_t AnoxModelCompilerCommon::CompressUV(uint32_t floatBits)
	{
		float f = 0.f;
		memcpy(&f, &floatBits, 4);

		const float scale = 16384.0f;
		const float bias = 32768.0f;
		const float maxValue = 65535.0f;

		float scaledF = f * scale + bias;

		if (!(scaledF >= 0.f))
			scaledF = 0.f;
		else if (!(scaledF <= maxValue))
			scaledF = maxValue;

		return static_cast<uint16_t>(floorf(scaledF + 0.5f));
	}

	uint32_t AnoxMD2Compiler::GetVersion() const
	{
		return 2;
	}

	rkit::Result AnoxMDACompilerBase::ConstructOutputPath(rkit::CIPath &outPath, const rkit::StringView &identifier)
	{
		return AnoxModelCompilerCommon::ConstructOutputPathByType(outPath, u8"mda", identifier);
	}

	rkit::Result AnoxMDACompilerBase::Create(rkit::UniquePtr<AnoxMDACompilerBase> &outCompiler)
	{
		return rkit::New<AnoxMDACompiler>(outCompiler);
	}

	rkit::Result AnoxMD2CompilerBase::ConstructOutputPath(rkit::CIPath &outPath, const rkit::StringView &identifier)
	{
		return AnoxModelCompilerCommon::ConstructOutputPathByType(outPath, u8"md2", identifier);
	}

	rkit::Result AnoxMD2CompilerBase::Create(rkit::UniquePtr<AnoxMD2CompilerBase> &outCompiler)
	{
		return rkit::New<AnoxMD2Compiler>(outCompiler);
	}

	rkit::Result AnoxCTCCompilerBase::Create(rkit::UniquePtr<AnoxCTCCompilerBase> &outCompiler)
	{
		return rkit::New<AnoxCTCCompiler>(outCompiler);
	}
} }
