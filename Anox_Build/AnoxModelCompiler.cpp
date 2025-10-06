#include "AnoxModelCompiler.h"

#include "rkit/Core/Endian.h"
#include "rkit/Core/FPControl.h"
#include "rkit/Core/HashTable.h"
#include "rkit/Core/BoolVector.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/QuickSort.h"
#include "rkit/Core/Stream.h"

#include "rkit/Data/ContentID.h"

#include "anox/AnoxModule.h"
#include "anox/Data/MDAModel.h"
#include "anox/Data/MaterialData.h"

#include "AnoxMaterialCompiler.h"
#include "AnoxPrecompiledMesh.h"

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

		struct MD2FrameHeader
		{
			rkit::endian::LittleFloat32_t m_scale[3];
			rkit::endian::LittleFloat32_t m_translate[3];
			char m_name[16];
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
			char m_animCategory[8];
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
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);

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
	};

	class AnoxMD2Compiler final : public AnoxModelCompilerCommon, public AnoxMD2CompilerBase
	{
	public:
		bool HasAnalysisStage() const override;
		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback);

		virtual uint32_t GetVersion() const override;
	};

	rkit::Result AnoxModelCompilerCommon::ConstructOutputPathByType(rkit::CIPath &outPath, const rkit::StringView &outputType, const rkit::StringView &identifier)
	{
		rkit::String str;
		RKIT_CHECK(str.Format("ax_mdl_{}/{}", outputType, identifier));

		return outPath.Set(str);
	}


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
			RKIT_CHECK(ResolveTexturePath(texturePath, md2Path, textureDef, true));

			RKIT_CHECK(feedback->AddNodeDependency(kAnoxNamespaceID, kModelMaterialNodeID, rkit::buildsystem::BuildFileLocation::kSourceDir, texturePath.ToString()));
		}

		return rkit::ResultCode::kOK;
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

		rkit::StringSliceView materialPathView(textureDef.m_textureName, strLength);
		rkit::String materialPathTemp;
		if (constructFullMaterialPath)
		{
			RKIT_CHECK(materialPathTemp.Format("{}.{}", materialPathView, MaterialCompiler::GetModelMaterialExtension()));
			materialPathView = materialPathTemp;
		}

		RKIT_CHECK(textureDefPath.Set(md2Path.AbsSlice(md2Path.NumComponents() - 1)));
		RKIT_CHECK(textureDefPath.AppendComponent(materialPathView));

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
		rkit::UniquePtr<rkit::ISeekableReadStream> inputFile;
		rkit::Vector<uint8_t> mdaBytes;

		rkit::IUtilitiesDriver *utils = rkit::GetDrivers().m_utilitiesDriver;

		{
			rkit::CIPath path;
			RKIT_CHECK(path.Set(depsNode->GetIdentifier()));

			RKIT_CHECK(feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, path, inputFile));

			RKIT_CHECK(utils->ReadEntireFile(*inputFile, mdaBytes));
		}

		rkit::ConstSpan<char> fileSpan = mdaBytes.ToSpan().ReinterpretCast<char>();

		UncompiledMDAData mdaData = {};

		rkit::ConstSpan<char> line;
		if (!ParseOneLine(line, fileSpan) || TokenIs(line, "MDA1"))
		{
			rkit::log::Error("Missing MDA header");
			return rkit::ResultCode::kDataError;
		}

		rkit::Vector<MDABase64Chunk> base64Chunks;

		while (ParseOneLine(line, fileSpan))
		{
			if (line[0] == '$')
			{
				// Data chunk start
				if (line.Count() != 32)
				{
					rkit::log::Error("Malformed MDA chunk");
					return rkit::ResultCode::kDataError;
				}

				uint32_t dwords[3] = { 0, 0, 0 };

				MDABase64Chunk base64Chunk;
				base64Chunk.m_fourCC = rkit::utils::ComputeFourCC(line[1], line[2], line[3], line[4]);

				for (size_t dwordIndex = 0; dwordIndex < 3; dwordIndex++)
				{
					const size_t startPos = dwordIndex * 9 + 5;

					if (line[startPos] != ':')
					{
						rkit::log::Error("Malformed MDA chunk");
						return rkit::ResultCode::kDataError;
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
							rkit::log::Error("Malformed MDA chunk");
							return rkit::ResultCode::kDataError;
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
					rkit::log::Error("MDA base64 data appeared after header");
					return rkit::ResultCode::kDataError;
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
						|| !utils->ParseUInt32(rkit::StringSliceView(triVert0Token), 10, triVerts[0])
						|| !utils->ParseUInt32(rkit::StringSliceView(triVert1Token), 10, triVerts[1])
						|| !utils->ParseUInt32(rkit::StringSliceView(triVert2Token), 10, triVerts[2])
						)
					{
						rkit::log::Error("Malformed headtri directive");
						return rkit::ResultCode::kDataError;
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
							rkit::log::Error("Malformed profile directive");
							return rkit::ResultCode::kDataError;
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
											else
											{
												rkit::log::Error("Unknown alphafunc");
												return rkit::ResultCode::kDataError;
											}
										}
										else if (TokenIs(token, "depthwrite"))
										{
											RKIT_CHECK(ExpectToken(token, line));

											uint32_t depthWriteFlag = 0;
											if (!utils->ParseUInt32(rkit::StringSliceView(token), 10, depthWriteFlag))
											{
												rkit::log::Error("Invalid depthwrite value");
												return rkit::ResultCode::kDataError;
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
												rkit::log::Error("Unknown uvgen type");
												return rkit::ResultCode::kDataError;
											}
										}
										else if (TokenIs(token, "blendmode"))
										{
											RKIT_CHECK(ExpectToken(token, line));

											if (TokenIs(token, "add"))
												pass.m_blendMode = data::MDABlendMode::kAdd;
											else if (TokenIs(token, "none"))
												pass.m_blendMode = data::MDABlendMode::kDisabled;
											else
											{
												rkit::log::Error("Unknown blendmode");
												return rkit::ResultCode::kDataError;
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
												rkit::log::Error("Unknown depthfunc");
												return rkit::ResultCode::kDataError;
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
												rkit::log::Error("Unknown cull type");
												return rkit::ResultCode::kDataError;
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
												rkit::log::Error("Unknown rgbgen type");
												return rkit::ResultCode::kDataError;
											}
										}
										else
										{
											rkit::log::Error("Unknown directive in pass");
											return rkit::ResultCode::kDataError;
										}
									}

									if (!haveMap)
									{
										rkit::log::Error("Pass missing map");
										return rkit::ResultCode::kDataError;
									}

									RKIT_CHECK(skin.m_passes.Append(std::move(pass)));
								}
								else
								{
									rkit::log::Error("Unknown directive in skin");
									return rkit::ResultCode::kDataError;
								}
							}

							RKIT_CHECK(profile.m_skins.Append(std::move(skin)));
						}
						else
						{
							rkit::log::Error("Unknown directive in profile");
							return rkit::ResultCode::kDataError;
						}
					}


					RKIT_CHECK(mdaData.m_profiles.Append(std::move(profile)));
				}
				else
				{
					rkit::log::Error("Unknown directive");
					return rkit::ResultCode::kDataError;
				}
			}
		}

		for (const MDABase64Chunk &base64Chunk : base64Chunks)
		{
			if (base64Chunk.m_base64Chars.Count() % 4u != 0
				|| base64Chunk.m_base64Chars.Count() / 4u * 3u != base64Chunk.m_paddedSize
				|| base64Chunk.m_size > base64Chunk.m_paddedSize)
			{
				rkit::log::Error("Malformed base64 chunk");
				return rkit::ResultCode::kDataError;
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
						rkit::log::Error("Malformed base64 chunk");
						return rkit::ResultCode::kDataError;
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
				rkit::log::Error("Unknown MDA chunk type");
				return rkit::ResultCode::kDataError;
			}
		}

		rkit::CIPath outputPath;
		RKIT_CHECK(ConstructOutputPath(outputPath, depsNode->GetIdentifier()));

		return CompileMDA(outputPath, mdaData, false, depsNode, feedback);
	}

	uint32_t AnoxMDACompiler::GetVersion() const
	{
		return 1;
	}

	rkit::Result AnoxMDACompiler::ExpectLine(rkit::ConstSpan<char> &outLine, rkit::ConstSpan<char> &fileSpan)
	{
		if (!ParseOneLine(outLine, fileSpan))
		{
			rkit::log::Error("Unexpected end of file");
			return rkit::ResultCode::kDataError;
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxMDACompiler::ExpectTokenIs(rkit::ConstSpan<char> &token, const rkit::AsciiStringView &candidate)
	{
		if (!TokenIs(token, candidate))
		{
			rkit::log::ErrorFmt("Expected token '{}'", candidate);
			return rkit::ResultCode::kDataError;
		}

		return rkit::ResultCode::kOK;
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
			rkit::log::ErrorFmt("Expected token");
			return rkit::ResultCode::kDataError;
		}

		return rkit::ResultCode::kOK;
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
		UncompiledMDAData mdaData;
		RKIT_CHECK(mdaData.m_baseModel.Set(depsNode->GetIdentifier()));

		rkit::CIPath outputPath;
		RKIT_CHECK(ConstructOutputPath(outputPath, depsNode->GetIdentifier()));

		return CompileMDA(outputPath, mdaData, true, depsNode, feedback);
	}

	rkit::Result AnoxModelCompilerCommon::CompileMDA(rkit::CIPath &outputPath, UncompiledMDAData &mdaData, bool autoSkin, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::FloatingPointStateScope scope(rkit::FloatingPointState::GetCurrent().AppendIEEEStrict());

		rkit::UniquePtr<rkit::ISeekableReadStream> inputFile;
		RKIT_CHECK(feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, mdaData.m_baseModel, inputFile));

		static const size_t kNumHardCodedNormals = 2048;

		rkit::Vector<rkit::endian::LittleFloat32_t> normalFloats;
		RKIT_CHECK(normalFloats.Resize(kNumHardCodedNormals * 3));

		{
			rkit::Vector<uint8_t> anoxGfxContents;

			rkit::UniquePtr<rkit::ISeekableReadStream> anoxGfxDll;
			RKIT_CHECK(feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kAuxDir0, "anoxgfx.dll", anoxGfxDll));

			const rkit::FilePos_t fileSize = anoxGfxDll->GetSize();
			if (anoxGfxDll->GetSize() > std::numeric_limits<size_t>::max())
			{
				rkit::log::Error("anoxgfx.dll is too big");
				return rkit::ResultCode::kDataError;
			}

			RKIT_CHECK(anoxGfxContents.Resize(static_cast<size_t>(fileSize)));
			RKIT_CHECK(anoxGfxDll->ReadAll(anoxGfxContents.GetBuffer(), static_cast<size_t>(fileSize)));

			const size_t kNormalBlobSize = 2048 * 3 * 4;

			if (anoxGfxContents.Count() < kNormalBlobSize)
			{
				rkit::log::Error("anoxgfx.dll size was smaller than expected");
				return rkit::ResultCode::kDataError;
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
				rkit::log::Error("Couldn't find normal blob in anoxgfx.dll");
				return rkit::ResultCode::kDataError;
			}

			rkit::CopySpanNonOverlapping(normalFloats.ToSpan().ReinterpretCast<uint8_t>(), fileSpan.SubSpan(startPos, kNormalBlobSize));
		}

		MD2Header header;
		RKIT_CHECK(inputFile->ReadAll(&header, sizeof(MD2Header)));

		if (header.m_numTextureBlocks.Get() != header.m_numTextures.Get())
		{
			rkit::log::Error("Texture block count doesn't match texture count");
			return rkit::ResultCode::kDataError;
		}

		const uint32_t numTextures = header.m_numTextures.Get();

		if (!autoSkin)
		{
			for (const UncompiledMDAProfile &profile : mdaData.m_profiles)
			{
				if (profile.m_skins.Count() != numTextures)
				{
					rkit::log::Error("Skin count didn't match MD2 texture count");
					return rkit::ResultCode::kDataError;
				}
			}
		}

		if (header.m_magic.Get() != MD2Header::kExpectedMagic
			|| header.m_majorVersion.Get() != MD2Header::kExpectedMajorVersion
			|| header.m_minorVersion.Get() > MD2Header::kMaxMinorVersion)
		{
			rkit::log::Error("MD2 file appears to be invalid");
			return rkit::ResultCode::kDataError;
		}

		const uint32_t numFrames = header.m_numFrames.Get();
		const uint32_t numXYZ = header.m_numXYZ.Get();

		if (numFrames == 0 || numXYZ == 0)
		{
			rkit::log::Error("MD2 file has no frames or verts");
			return rkit::ResultCode::kDataError;
		}

		const uint32_t frameSizeBytes = header.m_frameSizeBytes.Get();

		if (frameSizeBytes < sizeof(MD2FrameHeader))
		{
			rkit::log::Error("MD2 frame size is invalid");
			return rkit::ResultCode::kDataError;
		}

		uint32_t vertSize = (frameSizeBytes - sizeof(MD2FrameHeader));

		if (vertSize % numXYZ != 0)
		{
			rkit::log::Error("Couldn't determine vert size");
			return rkit::ResultCode::kDataError;
		}

		vertSize /= numXYZ;

		if (vertSize != 8 && vertSize != 6 && vertSize != 5)
		{
			rkit::log::Error("Unknown vert size");
			return rkit::ResultCode::kDataError;
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
				rkit::log::Error("Texture blocks desynced");
				return rkit::ResultCode::kDataError;
			}

			textureBlockSizes[currentTextureBlock] = textureBlockSizes[currentTextureBlock].Get() - 1;

			rkit::Vector<ModelProtoVert> &vertList = triLists[currentTextureBlock].m_verts;

			const bool isTriFan = ((baseCount & 0x80000000u) != 0);

			const uint32_t realCount = isTriFan ? static_cast<uint32_t>(~static_cast<uint32_t>(baseCount - 1)) : baseCount;
			if (realCount < 3 || realCount > (glCommands.Count() / 3u))
			{
				rkit::log::Error("Invalid GL command");
				return rkit::ResultCode::kDataError;
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
					rkit::log::Error("Invalid GL command vert");
					return rkit::ResultCode::kDataError;
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
				rkit::log::Error("Anim data too small");
				return rkit::ResultCode::kDataError;
			}

			rkit::CopySpanNonOverlapping(rkit::Span<MDAAnimationChunkData>(&animData, 1).ReinterpretCast<uint8_t>(), animDataBytes.SubSpan(0, sizeof(animData)));

			animDataBytes = animDataBytes.SubSpan(sizeof(animData));

			const uint32_t numAnimations = animData.m_numAnimInfos.Get();

			if (animDataBytes.Count() / sizeof(MDAAnimInfoData) < numAnimations)
			{
				rkit::log::Error("Anim data was truncated");
				return rkit::ResultCode::kDataError;
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
					rkit::log::Error("Anim frame range was invalid");
					return rkit::ResultCode::kDataError;
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
				rkit::log::Error("Morph data too small");
				return rkit::ResultCode::kDataError;
			}

			rkit::CopySpanNonOverlapping(rkit::Span<MDAMorphChunkData>(&morphData, 1).ReinterpretCast<uint8_t>(), morphDataBytes.SubSpan(0, sizeof(morphData)));

			const rkit::ConstSpan<uint8_t> morphKeyBytes = morphDataBytes.SubSpan(sizeof(morphData));

			const uint32_t headBoneFourCC = morphData.m_headBoneID.Get();
			const uint32_t numKeys = morphData.m_numKeys.Get();

			if (morphKeyBytes.Count() / sizeof(MDAMorphKey) < numKeys)
			{
				rkit::log::Error("Morph key data too small");
				return rkit::ResultCode::kDataError;
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
					rkit::log::Error("Morph data too small");
					return rkit::ResultCode::kDataError;
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
						rkit::log::Error("Vert morph vertex was invalid");
						return rkit::ResultCode::kDataError;
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

		rkit::Vector<data::MDAModelBone> bones;
		rkit::Vector<rkit::Vector<data::MDAModelBoneFrame>> boneFrames;

		if (mdaData.m_boneChunk.Count() > 0)
		{
			const rkit::ConstSpan<uint8_t> boneDataBytes = mdaData.m_boneChunk.ToSpan();

			MDABoneChunkData boneData;
			if (boneDataBytes.Count() < sizeof(boneData))
			{
				rkit::log::Error("Bone data too small");
				return rkit::ResultCode::kDataError;
			}

			rkit::CopySpanNonOverlapping(rkit::Span<MDABoneChunkData>(&boneData, 1).ReinterpretCast<uint8_t>(), boneDataBytes.SubSpan(0, sizeof(boneData)));

			const rkit::ConstSpan<uint8_t> boneFrameBytes = boneDataBytes.SubSpan(sizeof(boneData));

			if (boneFrameBytes.Count() / sizeof(MDABoneFrame) < numFrames)
			{
				rkit::log::Error("Bone frame data was too small");
				return rkit::ResultCode::kDataError;
			}

			RKIT_CHECK(bones.Resize(1));
			RKIT_CHECK(boneFrames.Resize(1));

			bones[0].m_boneIDFourCC = boneData.m_boneID.Get();

			rkit::Vector<data::MDAModelBoneFrame> &outFrames = boneFrames[0];

			RKIT_CHECK(outFrames.Resize(numFrames));
			for (uint32_t frameIndex = 0; frameIndex < numFrames; frameIndex++)
			{
				MDABoneFrame inFrame = {};
				data::MDAModelBoneFrame &outFrame = outFrames[frameIndex];

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
					rkit::log::Error("Normal index was out of range");
					return rkit::ResultCode::kDataError;
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
					return rkit::ResultCode::kInternalError;
				}

				for (size_t axis = 0; axis < 3; axis++)
					outPoint.m_point[axis] = scale[axis] * static_cast<float>(vertUIntCoords[axis]) + translate[axis];

				float normal[3];
				for (size_t axis = 0; axis < 3; axis++)
					normal[axis] = normalFloats[normalIndex * 3 + axis].Get();

				outPoint.m_compressedNormal = data::CompressNormal(normal[0], normal[1], normal[2]);
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
					rkit::log::Error("Skin condition length too long");
					return rkit::ResultCode::kDataError;
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
						rkit::log::Error("Too many passes on one skin");
						return rkit::ResultCode::kDataError;
					}

					RKIT_CHECK(outPasses.Resize(numPasses));

					outSkin.m_numPasses = static_cast<uint8_t>(numPasses);

					for (size_t passIndex = 0; passIndex < numPasses; passIndex++)
					{
						data::MDASkinPass &outPass = outPasses[passIndex];
						const UncompiledMDAPass &inPass = inSkin.m_passes[passIndex];

						rkit::String materialPath;
						RKIT_CHECK(materialPath.Format("{}.{}", inPass.m_map.ToString(), MaterialCompiler::GetModelMaterialExtension()));

						rkit::CIPath compiledMaterialPath;
						RKIT_CHECK(MaterialCompiler::ConstructOutputPath(compiledMaterialPath, data::MaterialResourceType::kModel, materialPath));

						RKIT_CHECK(feedback->IndexCAS(rkit::buildsystem::BuildFileLocation::kIntermediateDir, compiledMaterialPath, outPass.m_materialContentID));

						bool needMaterialData = false;
						outPass.m_clampFlag = (inPass.m_clamp ? 1 : 0);

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
								rkit::log::Error("Unknown material color type");
								return rkit::ResultCode::kDataError;
							}

							if (haveAlpha)
							{
								if (!inPass.m_alphaTestMode.IsSet())
									outPass.m_alphaTestMode = static_cast<uint8_t>(data::MDAAlphaTestMode::kGT0);

								if (inPass.m_depthWrite.IsSet())
									outPass.m_depthWriteFlag = false;

								if (inPass.m_blendMode.IsSet())
									outPass.m_blendMode = static_cast<uint8_t>(data::MDABlendMode::kAlphaBlend);
							}
						}
					}
				}
			}
		}

		// Construct final header
		if (numTextures > 0xff)
		{
			rkit::log::Error("Too many materials");
			return rkit::ResultCode::kDataError;
		}

		if (numFrames > 0xffff)
		{
			rkit::log::Error("Too many frames");
			return rkit::ResultCode::kDataError;
		}

		if (numXYZ > 0xffff)
		{
			rkit::log::Error("Too many verts");
			return rkit::ResultCode::kDataError;
		}

		if (animations.Count() > 0xffu)
		{
			rkit::log::Error("Too many animations");
			return rkit::ResultCode::kDataError;
		}

		if (morphKeys.Count() > 0xffffu)
		{
			rkit::log::Error("Too many morph keys");
			return rkit::ResultCode::kDataError;
		}

		if (bones.Count() > 0xffffu)
		{
			rkit::log::Error("Too many bones");
			return rkit::ResultCode::kDataError;
		}

		data::MDAModelHeader outHeader = {};

		for (const UncompiledTriList &triList : triLists)
		{
			const size_t maxSubModels = 0xff;
			if (triList.m_submodels.Count() > maxSubModels || (maxSubModels - outHeader.m_numSubModels) < triList.m_submodels.Count())
			{
				rkit::log::Error("Too many submodels");
				return rkit::ResultCode::kDataError;
			}

			outHeader.m_numSubModels += static_cast<uint8_t>(triList.m_submodels.Count());
		}

		outHeader.m_numSkins = numTextures;
		outHeader.m_numMorphKeys = static_cast<uint16_t>(morphKeys.Count());
		outHeader.m_numBones = static_cast<uint16_t>(bones.Count());
		outHeader.m_numFrames = static_cast<uint16_t>(numFrames);
		outHeader.m_numPoints = static_cast<uint16_t>(numXYZ);
		outHeader.m_numMorphedPoints = static_cast<uint16_t>(numMorphedPoints);
		outHeader.m_numAnimations = static_cast<uint8_t>(animations.Count());

		rkit::UniquePtr<rkit::ISeekableReadWriteStream> outFile;
		RKIT_CHECK(feedback->OpenOutput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, outputPath, outFile));

		// Write header
		RKIT_CHECK(outFile->WriteAll(&outHeader, sizeof(outHeader)));

		// Write profiles
		RKIT_CHECK(outFile->WriteAll(profiles.GetBuffer(), sizeof(profiles[0]) * profiles.Count()));

		// Write profile condition chars
		RKIT_CHECK(outFile->WriteAll(profileConditionStrings.GetBuffer(), sizeof(profileConditionStrings[0]) * profileConditionStrings.Count()));

		// Write profile skins
		for (const rkit::Vector<data::MDASkin> &skinList : profileSkins)
		{
			RKIT_CHECK(outFile->WriteAll(skinList.GetBuffer(), sizeof(skinList[0]) * skinList.Count()));
		}

		// Write profile skin passes
		for (const rkit::Vector<rkit::Vector<data::MDASkinPass>> &skinPassesList : profileSkinPasses)
		{
			for (const rkit::Vector<data::MDASkinPass> &passesList : skinPassesList)
			{
				RKIT_CHECK(outFile->WriteAll(passesList.GetBuffer(), sizeof(passesList[0]) * passesList.Count()));
			}
		}

		// Write animations
		RKIT_CHECK(outFile->WriteAll(animations.GetBuffer(), sizeof(animations[0]) * animations.Count()));

		// Write animation names
		RKIT_CHECK(outFile->WriteAll(animNameChars.GetBuffer(), sizeof(animNameChars[0]) * animNameChars.Count()));

		// Write morph keys
		RKIT_CHECK(outFile->WriteAll(morphKeys.GetBuffer(), sizeof(morphKeys[0]) * morphKeys.Count()));

		// Write bones
		RKIT_CHECK(outFile->WriteAll(bones.GetBuffer(), sizeof(bones[0]) * bones.Count()));

		// Write bone frames
		for (const rkit::Vector<data::MDAModelBoneFrame> &boneFramesList : boneFrames)
		{
			RKIT_CHECK(outFile->WriteAll(boneFramesList.GetBuffer(), sizeof(boneFramesList[0]) * boneFramesList.Count()));
		}

		// Write submodels
		for (size_t textureIndex = 0; textureIndex < triLists.Count(); textureIndex++)
		{
			const UncompiledTriList &triList = triLists[textureIndex];

			for (const UncompiledMDASubmodel &subModel : triList.m_submodels)
			{
				data::MDAModelSubModel outSubModel = {};
				outSubModel.m_materialID = static_cast<uint8_t>(textureIndex);
				outSubModel.m_numTris = static_cast<uint32_t>(subModel.m_tris.Count());
				outSubModel.m_numVerts = static_cast<uint16_t>(subModel.m_verts.Count());

				RKIT_CHECK(outFile->WriteAll(&outSubModel, sizeof(outSubModel)));
			}
		}

		// Write tris
		for (const UncompiledTriList &triList : triLists)
		{
			for (const UncompiledMDASubmodel &subModel : triList.m_submodels)
			{
				RKIT_CHECK(outFile->WriteAll(subModel.m_tris.GetBuffer(), sizeof(subModel.m_tris[0]) * subModel.m_tris.Count()));
			}
		}

		// Write verts
		for (const UncompiledTriList &triList : triLists)
		{
			for (const UncompiledMDASubmodel &subModel : triList.m_submodels)
			{
				RKIT_CHECK(outFile->WriteAll(subModel.m_verts.GetBuffer(), sizeof(subModel.m_verts[0]) * subModel.m_verts.Count()));
			}
		}

		// Write points
		RKIT_CHECK(outFile->WriteAll(points.GetBuffer(), sizeof(points[0]) * points.Count()));

		// Write vert morphs
		RKIT_CHECK(outFile->WriteAll(compiledVertMorphs.GetBuffer(), sizeof(compiledVertMorphs[0]) * compiledVertMorphs.Count()));

		return rkit::ResultCode::kOK;
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

		return rkit::ResultCode::kOK;
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
				}
				else
					vertIndex = it.Value();

				data::MDAModelVert mdaVert;
				mdaVert.m_texCoordU = CompressUV(protoVert.m_uBits);
				mdaVert.m_texCoordV = CompressUV(protoVert.m_vBits);
				mdaVert.m_pointID = xyzToPointIndex[protoVert.m_xyzIndex];

				RKIT_CHECK(outSubmodel.m_verts.Append(mdaVert));
			}

			triEmitted[i] = true;
			emittedAnything = true;
		}

		outEmittedAnything = emittedAnything;

		return rkit::ResultCode::kOK;
	}

	uint16_t AnoxModelCompilerCommon::CompressUV(uint32_t floatBits)
	{
		float f = 0.f;
		memcpy(&f, &floatBits, 4);

		if (!(f >= 0.f))
			f = 0.f;
		else if (!(f <= 1.0f))
			f = 1.0f;

		return static_cast<uint16_t>(floorf((f + 0.5f) * 65535.0f));
	}

	uint32_t AnoxMD2Compiler::GetVersion() const
	{
		return 1;
	}

	rkit::Result AnoxMDACompilerBase::ConstructOutputPath(rkit::CIPath &outPath, const rkit::StringView &identifier)
	{
		return AnoxModelCompilerCommon::ConstructOutputPathByType(outPath, "mda", identifier);
	}

	rkit::Result AnoxMDACompilerBase::Create(rkit::UniquePtr<AnoxMDACompilerBase> &outCompiler)
	{
		return rkit::New<AnoxMDACompiler>(outCompiler);
	}

	rkit::Result AnoxMD2CompilerBase::ConstructOutputPath(rkit::CIPath &outPath, const rkit::StringView &identifier)
	{
		return AnoxModelCompilerCommon::ConstructOutputPathByType(outPath, "md2", identifier);
	}

	rkit::Result AnoxMD2CompilerBase::Create(rkit::UniquePtr<AnoxMD2CompilerBase> &outCompiler)
	{
		return rkit::New<AnoxMD2Compiler>(outCompiler);
	}
} }
