#include "AnoxMaterialCompiler.h"

#include "anox/AnoxModule.h"
#include "anox/Data/MaterialData.h"

#include "rkit/Data/ContentID.h"

#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/Pair.h"
#include "rkit/Core/Path.h"
#include "rkit/Core/StaticArray.h"
#include "rkit/Core/String.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/Vector.h"

#include "rkit/Utilities/Image.h"
#include "rkit/Utilities/TextParser.h"

#include "AnoxTextureCompiler.h"

namespace anox { namespace buildsystem
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
		bool m_clamp;
	};

	struct MaterialAnalysisBitmapDef
	{
		uint32_t m_nameIndex;
	};

	struct MaterialAnalysisImageImport
	{
		rkit::String m_identifier;
		bool m_isGenerated;
		ImageImportDisposition m_disposition;
	};

	enum class InterformMaterialMovement
	{
		kNone,
		kScroll,
		kWander,
	};

	struct MaterialAnalysisInterformHalfData
	{
		MaterialAnalysisBitmapDef m_bitmap;

		InterformMaterialMovement m_movement = InterformMaterialMovement::kNone;
		float m_vx = 0;
		float m_vy = 0;
		float m_rate = 0;
		float m_strength = 0;
	};

	struct MaterialAnalysisInterformData
	{
		MaterialAnalysisInterformHalfData m_halves[2];
		MaterialAnalysisBitmapDef m_palette;
	};


	struct MaterialAnalysisFrameDef
	{
		uint32_t m_bitmap;
		uint32_t m_next;
		uint32_t m_waitMSec;

		int32_t m_xOffset;
		int32_t m_yOffset;
	};

	struct MaterialAnalysisDynamicData
	{
		rkit::Vector<MaterialAnalysisImageImport> m_imageImports;
		rkit::Vector<MaterialAnalysisBitmapDef> m_bitmapDefs;
		rkit::Vector<MaterialAnalysisFrameDef> m_frameDefs;

		rkit::Optional<MaterialAnalysisInterformData> m_interform;

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
			(m_interform.IsSet() ? 1u : 0u),
		};

		RKIT_CHECK(stream.WriteAll(counts, sizeof(counts)));

		for (const MaterialAnalysisImageImport &imageImport : m_imageImports)
		{
			uint64_t strLength = imageImport.m_identifier.Length();

			RKIT_CHECK(stream.WriteAll(&strLength, sizeof(strLength)));
			RKIT_CHECK(stream.WriteAll(imageImport.m_identifier.CStr(), imageImport.m_identifier.Length()));

			const uint8_t isGenerated = imageImport.m_isGenerated ? 1 : 0;
			RKIT_CHECK(stream.WriteAll(&isGenerated, 1));

			RKIT_CHECK(stream.WriteAll(&imageImport.m_disposition, sizeof(imageImport.m_disposition)));
		}

		const MaterialAnalysisBitmapDef *bitmaps = m_bitmapDefs.GetBuffer();
		const MaterialAnalysisFrameDef *frameDefs = m_frameDefs.GetBuffer();

		RKIT_CHECK(stream.WriteAll(bitmaps, sizeof(bitmaps[0]) * m_bitmapDefs.Count()));
		RKIT_CHECK(stream.WriteAll(frameDefs, sizeof(frameDefs[0]) * m_frameDefs.Count()));

		if (m_interform.IsSet())
		{
			RKIT_CHECK(stream.WriteAll(&m_interform.Get(), sizeof(m_interform.Get())));
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result MaterialAnalysisDynamicData::Deserialize(rkit::IReadStream &stream)
	{
		rkit::StaticArray<uint64_t, 4> counts;

		RKIT_CHECK(stream.ReadAll(counts.GetBuffer(), sizeof(counts[0]) * counts.Count()));

		RKIT_CHECK(m_imageImports.Resize(counts[0]));
		RKIT_CHECK(m_bitmapDefs.Resize(counts[1]));
		RKIT_CHECK(m_frameDefs.Resize(counts[2]));


		for (MaterialAnalysisImageImport &imageImport : m_imageImports)
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

			imageImport.m_identifier = rkit::String(std::move(scBuf));

			uint8_t isGenerated = 0;
			RKIT_CHECK(stream.ReadAll(&isGenerated, 1));

			imageImport.m_isGenerated = (isGenerated != 0);

			RKIT_CHECK(stream.ReadAll(&imageImport.m_disposition, sizeof(ImageImportDisposition)));
		}

		MaterialAnalysisBitmapDef *bitmaps = m_bitmapDefs.GetBuffer();
		MaterialAnalysisFrameDef *frameDefs = m_frameDefs.GetBuffer();

		RKIT_CHECK(stream.ReadAll(bitmaps, sizeof(bitmaps[0]) * m_bitmapDefs.Count()));
		RKIT_CHECK(stream.ReadAll(frameDefs, sizeof(frameDefs[0]) * m_frameDefs.Count()));

		if (counts[3] != 0)
		{
			MaterialAnalysisInterformData interformData;
			RKIT_CHECK(stream.ReadAll(&interformData, sizeof(interformData)));

			m_interform = interformData;
		}

		return rkit::ResultCode::kOK;
	}

	MaterialCompiler::MaterialCompiler(rkit::png::IPngDriver &pngDriver)
		: m_pngDriver(pngDriver)
	{
	}

	bool MaterialCompiler::HasAnalysisStage() const
	{
		return true;
	}

	rkit::Result MaterialCompiler::MaterialNodeTypeFromFourCC(data::MaterialResourceType &outNodeType, uint32_t nodeTypeFourCC)
	{
		switch (nodeTypeFourCC)
		{
		case anox::buildsystem::kFontMaterialNodeID:
			outNodeType = data::MaterialResourceType::kFont;
			break;
		case anox::buildsystem::kWorldMaterialNodeID:
			outNodeType = data::MaterialResourceType::kWorld;
			break;
		case anox::buildsystem::kModelMaterialNodeID:
			outNodeType = data::MaterialResourceType::kModel;
			break;
		default:
			return rkit::ResultCode::kInternalError;
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result MaterialCompiler::RunAnalyzeATD(const rkit::StringView &name, rkit::UniquePtr<rkit::ISeekableReadStream> &&atdStreamRef, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) const
	{
		rkit::IUtilitiesDriver &utils = *rkit::GetDrivers().m_utilitiesDriver;

		rkit::UniquePtr<rkit::ISeekableReadStream> atdStream(std::move(atdStreamRef));

		if (atdStream->GetSize() > std::numeric_limits<size_t>::max())
			return rkit::ResultCode::kOutOfMemory;

		rkit::Vector<char> chars;
		RKIT_CHECK(chars.Resize(static_cast<size_t>(atdStream->GetSize())));

		RKIT_CHECK(atdStream->ReadAll(chars.GetBuffer(), chars.Count()));

		atdStream.Reset();

		rkit::UniquePtr<rkit::utils::ITextParser> textParser;
		RKIT_CHECK(utils.CreateTextParser(chars.ToSpan(), rkit::utils::TextParserCommentType::kBash, rkit::utils::TextParserLexerType::kSimple, textParser));

		RKIT_CHECK(textParser->SetSimpleDelimiters(rkit::StringView("=").ToSpan()));

		RKIT_CHECK(textParser->ExpectToken("ATD1"));

		MaterialAnalysisHeader analysisHeader = {};
		MaterialAnalysisDynamicData dynamicData = {};

		analysisHeader.m_magic = MaterialAnalysisHeader::kExpectedMagic;
		analysisHeader.m_version = MaterialAnalysisHeader::kExpectedVersion;
		analysisHeader.m_materialType = data::MaterialType::kAnimation;
		analysisHeader.m_bilinear = true;
		analysisHeader.m_mipMapped = false;
		analysisHeader.m_width = 1;
		analysisHeader.m_height = 1;
		analysisHeader.m_colorType = data::MaterialColorType::kRGB;

		bool interformHalfSet[2] = { false, false };
		bool interformPaletteSet = false;

		const uint8_t kInterformMotherHalf = 0;
		const uint8_t kInterformFatherHalf = 1;

		bool haveType = false;
		for (;;)
		{
			bool haveToken = false;
			rkit::ConstSpan<char> token;
			RKIT_CHECK(textParser->ReadToken(haveToken, token));

			if (!haveToken)
				break;

			if (IsToken(token, "type"))
			{
				RKIT_CHECK(textParser->ExpectToken("="));
				RKIT_CHECK(textParser->RequireToken(token));

				if (IsToken(token, "animation"))
				{
					analysisHeader.m_materialType = data::MaterialType::kAnimation;
				}
				else if (IsToken(token, "interform"))
				{
					analysisHeader.m_materialType = data::MaterialType::kInterform;
					dynamicData.m_interform.Emplace();
				}
				else if (IsToken(token, "whitenoise"))
				{
					analysisHeader.m_materialType = data::MaterialType::kWhiteNoise;
				}
				else
				{
					rkit::log::Error("Unknown texture type");
					return rkit::ResultCode::kDataError;
				}

				if (haveType)
				{
					rkit::log::Error("Type specified multiple times");
					return rkit::ResultCode::kDataError;
				}
				haveType = true;
			}
			else if (IsToken(token, "colortype"))
			{
				RKIT_CHECK(textParser->ExpectToken("="));
				RKIT_CHECK(textParser->RequireToken(token));

				uint32_t ct;
				if (!utils.ParseUInt32(rkit::StringSliceView(token), 10, ct) || ct < 1 || ct > static_cast<uint32_t>(data::MaterialColorType::kCount))
				{
					rkit::log::Error("Invalid colortype");
					return rkit::ResultCode::kDataError;
				}

				analysisHeader.m_colorType = static_cast<data::MaterialColorType>(ct - 1);
			}
			else if (IsToken(token, "width") || IsToken(token, "height"))
			{
				rkit::ConstSpan<char> valueToken;
				RKIT_CHECK(textParser->ExpectToken("="));
				RKIT_CHECK(textParser->RequireToken(valueToken));

				uint32_t v;
				if (!utils.ParseUInt32(rkit::StringSliceView(valueToken), 10, v) || v <= 0)
				{
					rkit::log::Error("Invalid dimensions");
					return rkit::ResultCode::kDataError;
				}

				if (IsToken(token, "width"))
					analysisHeader.m_width = v;
				else
				{
					RKIT_ASSERT(IsToken(token, "height"));
					analysisHeader.m_height = v;
				}
			}
			else if (IsToken(token, "bilinear"))
			{
				rkit::ConstSpan<char> valueToken;
				RKIT_CHECK(textParser->ExpectToken("="));
				RKIT_CHECK(textParser->RequireToken(valueToken));

				uint32_t v;
				if (!utils.ParseUInt32(rkit::StringSliceView(valueToken), 10, v) || v > 1)
				{
					rkit::log::Error("Invalid bilinear flag");
					return rkit::ResultCode::kDataError;
				}

				analysisHeader.m_bilinear = (v != 0);
			}
			else if (IsToken(token, "clamp"))
			{
				rkit::ConstSpan<char> valueToken;
				RKIT_CHECK(textParser->ExpectToken("="));
				RKIT_CHECK(textParser->RequireToken(valueToken));

				uint32_t v;
				if (!utils.ParseUInt32(rkit::StringSliceView(valueToken), 10, v) || v > 1)
				{
					rkit::log::Error("Invalid clamp flag");
					return rkit::ResultCode::kDataError;
				}

				analysisHeader.m_clamp = (v != 0);
			}
			else if (IsToken(token, "!bitmap") && haveType && analysisHeader.m_materialType == data::MaterialType::kAnimation)
			{
				RKIT_CHECK(textParser->ExpectToken("file"));
				RKIT_CHECK(textParser->ExpectToken("="));
				RKIT_CHECK(textParser->RequireToken(token));


				MaterialAnalysisBitmapDef bitmapDef = {};
				RKIT_CHECK(ParseImageImport(token, buildsystem::ImageImportDisposition::kCount, dynamicData, bitmapDef, feedback));

				RKIT_CHECK(dynamicData.m_bitmapDefs.Append(bitmapDef));
			}
			else if (IsToken(token, "!frame") && haveType && analysisHeader.m_materialType == data::MaterialType::kAnimation)
			{
				while (haveToken)
				{
					if (dynamicData.m_frameDefs.Count() == std::numeric_limits<uint32_t>::max())
					{
						rkit::log::Error("Too many framedefs");
						return rkit::ResultCode::kDataError;
					}

					const size_t frameIndex = dynamicData.m_frameDefs.Count();

					RKIT_CHECK(dynamicData.m_frameDefs.Append(MaterialAnalysisFrameDef()));

					MaterialAnalysisFrameDef &frameDef = dynamicData.m_frameDefs[dynamicData.m_frameDefs.Count() - 1];

					RKIT_CHECK(textParser->ReadToken(haveToken, token));

					while (haveToken)
					{
						if (IsToken(token, "bitmap"))
						{
							RKIT_CHECK(textParser->ExpectToken("="));
							RKIT_CHECK(textParser->RequireToken(token));

							int32_t nextIndex = 0;
							if (!utils.ParseInt32(rkit::StringSliceView(token), 10, nextIndex))
							{
								rkit::log::Error("Invalid next frame");
								return rkit::ResultCode::kDataError;
							}
						}
						else if (IsToken(token, "next"))
						{
							RKIT_CHECK(textParser->ExpectToken("="));
							RKIT_CHECK(textParser->RequireToken(token));

							int32_t nextIndex = 0;
							if (!utils.ParseInt32(rkit::StringSliceView(token), 10, nextIndex))
							{
								rkit::log::Error("Invalid next frame");
								return rkit::ResultCode::kDataError;
							}

							if (nextIndex < 0)
								frameDef.m_next = static_cast<uint32_t>(frameIndex);
							else
								frameDef.m_next = static_cast<uint32_t>(nextIndex);
						}
						else if (IsToken(token, "wait"))
						{
							RKIT_CHECK(textParser->ExpectToken("="));
							RKIT_CHECK(textParser->RequireToken(token));

							bool isNegative = true;
							if (token[0] == '-')
								frameDef.m_waitMSec = 0;
							else
							{
								uint32_t msec = 0;

								bool isInFraction = false;
								uint32_t integralMultiplier = 1000;
								for (char c : token)
								{
									if (c >= '0' && c <= '9')
									{
										if (isInFraction)
											integralMultiplier /= 10;

										RKIT_CHECK(rkit::SafeAdd<uint32_t>(msec, msec, static_cast<uint16_t>(c - '0') * integralMultiplier));
									}
									else if (c == '.')
									{
										if (isInFraction)
										{
											rkit::log::Error("Invalid wait value");
											return rkit::ResultCode::kDataError;
										}

										isInFraction = true;
									}
									else
									{
										rkit::log::Error("Invalid wait value");
										return rkit::ResultCode::kDataError;
									}
								}

								frameDef.m_waitMSec = msec;
							}
						}
						else if (IsToken(token, "x"))
						{
							RKIT_CHECK(textParser->ExpectToken("="));
							RKIT_CHECK(textParser->RequireToken(token));

							if (!rkit::GetDrivers().m_utilitiesDriver->ParseInt32(rkit::StringSliceView(token), 10, frameDef.m_xOffset))
							{
								rkit::log::Error("Invalid X offset");
								return rkit::ResultCode::kDataError;
							}
						}
						else if (IsToken(token, "y"))
						{
							RKIT_CHECK(textParser->ExpectToken("="));
							RKIT_CHECK(textParser->RequireToken(token));

							if (!rkit::GetDrivers().m_utilitiesDriver->ParseInt32(rkit::StringSliceView(token), 10, frameDef.m_yOffset))
							{
								rkit::log::Error("Invalid Y offset");
								return rkit::ResultCode::kDataError;
							}
						}
						else if (IsToken(token, "!frame"))
							break;
						else
						{
							rkit::log::Error("Unknown subitem in !frame directive");
							return rkit::ResultCode::kDataError;
						}

						RKIT_CHECK(textParser->ReadToken(haveToken, token));
					}
				}
			}
			else if ((IsToken(token, "mother") || IsToken(token, "father")) && haveType && analysisHeader.m_materialType == data::MaterialType::kInterform)
			{
				rkit::ConstSpan<char> imagePathToken;

				RKIT_CHECK(textParser->ExpectToken("="));
				RKIT_CHECK(textParser->RequireToken(imagePathToken));

				MaterialAnalysisBitmapDef bitmapDef = {};
				RKIT_CHECK(ParseImageImport(imagePathToken, ImageImportDisposition::kInterformFrame, dynamicData, bitmapDef, feedback));
				
				const uint8_t half = IsToken(token, "mother") ? kInterformMotherHalf : kInterformFatherHalf;

				if (interformHalfSet[half])
				{
					rkit::log::Error("Interform half set multiple times");
					return rkit::ResultCode::kDataError;
				}

				interformHalfSet[half] = true;
				dynamicData.m_interform.Get().m_halves[half].m_bitmap = bitmapDef;
			}
			else if ((IsToken(token, "mother_move") || IsToken(token, "father_move")) && haveType && analysisHeader.m_materialType == data::MaterialType::kInterform)
			{
				rkit::ConstSpan<char> moveTypeToken;

				RKIT_CHECK(textParser->ExpectToken("="));
				RKIT_CHECK(textParser->RequireToken(moveTypeToken));

				const uint8_t half = IsToken(token, "mother_move") ? kInterformMotherHalf : kInterformFatherHalf;

				if (IsToken(moveTypeToken, "scroll"))
				{
					dynamicData.m_interform.Get().m_halves[half].m_movement = InterformMaterialMovement::kScroll;
				}
				else if (IsToken(moveTypeToken, "wander"))
				{
					dynamicData.m_interform.Get().m_halves[half].m_movement = InterformMaterialMovement::kWander;
				}
				else
				{
					rkit::log::Error("Unknown interform movement type");
					return rkit::ResultCode::kDataError;
				}
			}
			else if (
				  (IsToken(token, "mother_vx")
				|| IsToken(token, "father_vx")
				|| IsToken(token, "mother_vy")
				|| IsToken(token, "father_vy")
				|| IsToken(token, "mother_rate")
				|| IsToken(token, "father_rate")
				|| IsToken(token, "mother_strength")
				|| IsToken(token, "father_strength")
				) && haveType && analysisHeader.m_materialType == data::MaterialType::kInterform)
			{
				rkit::StringSliceView tokenSlice(token);
				const uint8_t half = tokenSlice.StartsWith("mother") ? kInterformMotherHalf : kInterformFatherHalf;

				rkit::ConstSpan<char> valueSpan;

				RKIT_CHECK(textParser->ExpectToken("="));
				RKIT_CHECK(textParser->RequireToken(valueSpan));

				rkit::String valueTokenStr;
				RKIT_CHECK(valueTokenStr.Set(valueSpan));

				double value = 0.0;
				if (!utils.ParseDouble(valueTokenStr, value))
				{
					rkit::log::Error("Invalid velocity value");
					return rkit::ResultCode::kDataError;
				}

				MaterialAnalysisInterformHalfData &halfData = dynamicData.m_interform.Get().m_halves[half];
				if (tokenSlice.EndsWith("_vx"))
					halfData.m_vx = static_cast<float>(value);
				else if (tokenSlice.EndsWith("_vy"))
					halfData.m_vy = static_cast<float>(value);
				else if (tokenSlice.EndsWith("_rate"))
					halfData.m_rate = static_cast<float>(value);
				else if (tokenSlice.EndsWith("_strength"))
					halfData.m_strength = static_cast<float>(value);
			}
			else if ((IsToken(token, "palette")) && haveType && analysisHeader.m_materialType == data::MaterialType::kInterform)
			{
				rkit::ConstSpan<char> imagePathToken;

				RKIT_CHECK(textParser->ExpectToken("="));
				RKIT_CHECK(textParser->RequireToken(imagePathToken));

				MaterialAnalysisBitmapDef bitmapDef = {};
				RKIT_CHECK(ParseImageImport(imagePathToken, ImageImportDisposition::kInterformPalette, dynamicData, bitmapDef, feedback));

				if (interformPaletteSet)
				{
					rkit::log::Error("Palette set multiple times");
					return rkit::ResultCode::kDataError;
				}

				interformPaletteSet = true;
				dynamicData.m_interform.Get().m_palette = bitmapDef;
			}
			else
			{
				rkit::log::Error("Unknown directive");
				return rkit::ResultCode::kDataError;
			}
		}

		for (MaterialAnalysisFrameDef &frameDef : dynamicData.m_frameDefs)
		{
			if (frameDef.m_bitmap >= dynamicData.m_bitmapDefs.Count())
			{
				rkit::log::Error("Out-of-range bitmap");
				return rkit::ResultCode::kDataError;
			}
			if (frameDef.m_next >= dynamicData.m_frameDefs.Count())
			{
				rkit::log::Error("Out-of-range frame next");
				return rkit::ResultCode::kDataError;
			}
		}

		if (analysisHeader.m_materialType == data::MaterialType::kAnimation)
		{
			ImageImportDisposition disposition = ImageImportDisposition::kWorldAlphaTestedNoMip;

			for (MaterialAnalysisImageImport &imageImport : dynamicData.m_imageImports)
			{
				imageImport.m_disposition = disposition;
			}
		}

		if (analysisHeader.m_materialType == data::MaterialType::kInterform)
		{
			if (!interformHalfSet[0] || !interformHalfSet[1] || !interformPaletteSet)
			{
				rkit::log::Error("Interform is missing an image part");
				return rkit::ResultCode::kDataError;
			}
		}

		RKIT_CHECK(GenerateRealFrames(analysisHeader, dynamicData, depsNode, feedback));

		// Finalize identifiers
		for (MaterialAnalysisImageImport &imageImport : dynamicData.m_imageImports)
		{
			if (!imageImport.m_isGenerated)
			{
				RKIT_CHECK(TextureCompilerBase::CreateImportIdentifier(imageImport.m_identifier, imageImport.m_identifier, imageImport.m_disposition));
			}
		}

		data::MaterialResourceType nodeType = data::MaterialResourceType::kCount;

		RKIT_CHECK(MaterialNodeTypeFromFourCC(nodeType, depsNode->GetDependencyNodeType()));

		rkit::CIPath analysisPath;
		RKIT_CHECK(ConstructAnalysisPath(analysisPath, nodeType, depsNode->GetIdentifier()));
		
		for (MaterialAnalysisImageImport &imageImport : dynamicData.m_imageImports)
		{
			if (!imageImport.m_isGenerated)
			{
				RKIT_CHECK(feedback->AddNodeDependency(kAnoxNamespaceID, kTextureNodeID, rkit::buildsystem::BuildFileLocation::kSourceDir, imageImport.m_identifier));
			}
		}

		rkit::UniquePtr<rkit::ISeekableReadWriteStream> analysisStream;
		RKIT_CHECK(feedback->OpenOutput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, analysisPath, analysisStream));

		RKIT_CHECK(analysisStream->WriteAll(&analysisHeader, sizeof(analysisHeader)));

		RKIT_CHECK(dynamicData.Serialize(*analysisStream));

		return rkit::ResultCode::kOK;
	}

	rkit::Result MaterialCompiler::ConstructAnalysisPath(rkit::CIPath &analysisPath, data::MaterialResourceType nodeType, const rkit::StringView &identifier)
	{
		rkit::String pathStr;
		RKIT_CHECK(pathStr.Format("anox/mat/{}/{}.a", static_cast<int>(nodeType), identifier.GetChars()));

		RKIT_CHECK(analysisPath.Set(pathStr));

		return rkit::ResultCode::kOK;
	}

	rkit::Result MaterialCompiler::ConstructOutputPath(rkit::CIPath &outputPath, data::MaterialResourceType nodeType, const rkit::StringView &identifier)
	{
		rkit::String pathStr;
		RKIT_CHECK(pathStr.Format("anox/mat/{}/{}", static_cast<int>(nodeType), identifier.GetChars()));

		RKIT_CHECK(outputPath.Set(pathStr));

		return rkit::ResultCode::kOK;
	}

	rkit::Result MaterialCompiler::RunAnalyzeImage(const rkit::StringView &longName, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) const
	{
		MaterialAnalysisHeader analysisHeader = {};
		analysisHeader.m_magic = MaterialAnalysisHeader::kExpectedMagic;
		analysisHeader.m_version = MaterialAnalysisHeader::kExpectedVersion;
		analysisHeader.m_bilinear = true;
		analysisHeader.m_mipMapped = true;

		analysisHeader.m_materialType = data::MaterialType::kSingle;
		analysisHeader.m_colorType = data::MaterialColorType::kRGBA;

		ImageImportDisposition disposition = ImageImportDisposition::kCount;

		data::MaterialResourceType nodeType = data::MaterialResourceType::kCount;

		RKIT_CHECK(MaterialNodeTypeFromFourCC(nodeType, depsNode->GetDependencyNodeType()));

		// Don't fill in width and height
		switch (nodeType)
		{
		case data::MaterialResourceType::kFont:
			analysisHeader.m_bilinear = false;
			analysisHeader.m_mipMapped = false;
			disposition = ImageImportDisposition::kGraphicTransparent;
			break;
		case data::MaterialResourceType::kWorld:
			analysisHeader.m_bilinear = true;
			analysisHeader.m_mipMapped = true;
			disposition = ImageImportDisposition::kWorldAlphaTested;
			break;
		case data::MaterialResourceType::kModel:
			analysisHeader.m_bilinear = true;
			analysisHeader.m_mipMapped = true;
			disposition = ImageImportDisposition::kModel;
			break;
		default:
			return rkit::ResultCode::kInternalError;
		}

		MaterialAnalysisDynamicData dynamicData;

		RKIT_CHECK(dynamicData.m_imageImports.Resize(1));

		MaterialAnalysisImageImport &imageImport = dynamicData.m_imageImports[0];
		RKIT_CHECK(TextureCompilerBase::CreateImportIdentifier(imageImport.m_identifier, longName, disposition));
		imageImport.m_disposition = disposition;

		RKIT_CHECK(dynamicData.m_bitmapDefs.Resize(1));

		MaterialAnalysisBitmapDef &bitmapDef = dynamicData.m_bitmapDefs[0];
		bitmapDef.m_nameIndex = 0;

		RKIT_CHECK(dynamicData.m_frameDefs.Resize(1));
		MaterialAnalysisFrameDef &frameDef = dynamicData.m_frameDefs[0];
		frameDef.m_bitmap = 0;
		frameDef.m_next = 0;
		frameDef.m_waitMSec = 0;
		frameDef.m_xOffset = 0;
		frameDef.m_yOffset = 0;

		rkit::CIPath analysisPath;
		RKIT_CHECK(ConstructAnalysisPath(analysisPath, nodeType, depsNode->GetIdentifier()));

		RKIT_CHECK(feedback->AddNodeDependency(kAnoxNamespaceID, kTextureNodeID, rkit::buildsystem::BuildFileLocation::kSourceDir, imageImport.m_identifier));

		rkit::UniquePtr<rkit::ISeekableReadWriteStream> analysisStream;
		RKIT_CHECK(feedback->OpenOutput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, analysisPath, analysisStream));

		RKIT_CHECK(analysisStream->WriteAll(&analysisHeader, sizeof(analysisHeader)));

		RKIT_CHECK(dynamicData.Serialize(*analysisStream));

		return rkit::ResultCode::kOK;
	}

	rkit::Result MaterialCompiler::RunAnalyzeMissing(const rkit::StringView &name, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) const
	{
		MaterialAnalysisHeader analysisHeader = {};
		analysisHeader.m_magic = MaterialAnalysisHeader::kExpectedMagic;
		analysisHeader.m_version = MaterialAnalysisHeader::kExpectedVersion;
		analysisHeader.m_bilinear = true;
		analysisHeader.m_mipMapped = true;

		analysisHeader.m_materialType = data::MaterialType::kMissing;
		analysisHeader.m_colorType = data::MaterialColorType::kRGBA;

		data::MaterialResourceType nodeType = data::MaterialResourceType::kCount;

		RKIT_CHECK(MaterialNodeTypeFromFourCC(nodeType, depsNode->GetDependencyNodeType()));

		MaterialAnalysisDynamicData dynamicData;

		rkit::CIPath analysisPath;
		RKIT_CHECK(ConstructAnalysisPath(analysisPath, nodeType, depsNode->GetIdentifier()));

		rkit::UniquePtr<rkit::ISeekableReadWriteStream> analysisStream;
		RKIT_CHECK(feedback->OpenOutput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, analysisPath, analysisStream));

		RKIT_CHECK(analysisStream->WriteAll(&analysisHeader, sizeof(analysisHeader)));

		RKIT_CHECK(dynamicData.Serialize(*analysisStream));

		return rkit::ResultCode::kOK;
	}

	rkit::Result MaterialCompiler::ResolveShortName(rkit::String &shortName, const rkit::StringView &identifier) const
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
			rkit::log::ErrorFmt("Material '{}' didn't end with a material extension", identifier.GetChars());
			return rkit::ResultCode::kInternalError;
		}

		RKIT_CHECK(shortName.Set(identifier.SubString(0, extPos.Get())));

		return rkit::ResultCode::kOK;
	}


	bool MaterialCompiler::IsToken(const rkit::Span<const char> &span, const rkit::StringView &str)
	{
		return rkit::CompareSpansEqual(span, str.ToSpan());
	}

	rkit::Result MaterialCompiler::ParseImageImport(const rkit::Span<const char> &token, ImageImportDisposition disposition, MaterialAnalysisDynamicData &dynamicData, MaterialAnalysisBitmapDef &bitmapDef, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		rkit::Vector<char> lower;
		RKIT_CHECK(lower.Append(token));

		for (char &c : lower)
		{
			c = rkit::InvariantCharCaseAdjuster<char>::ToLower(c);
			if (c == '\\')
				c = '/';
		}

		uint32_t imageIndex = 0;
		auto findImageIndexFunc = [&imageIndex, &dynamicData, &lower]() -> rkit::Result
			{
				imageIndex = 0;
				for (const MaterialAnalysisImageImport &imageImport : dynamicData.m_imageImports)
				{
					if (rkit::CompareSpansEqual(imageImport.m_identifier.ToSpan(), lower.ToSpan()))
						break;

					if (imageIndex == std::numeric_limits<uint32_t>::max())
					{
						rkit::log::Error("Too many images");
						return rkit::ResultCode::kDataError;
					}
					imageIndex++;
				}

				return rkit::ResultCode::kOK;
			};

		RKIT_CHECK(findImageIndexFunc());

		if (imageIndex == dynamicData.m_imageImports.Count())
		{
			rkit::CIPath imagePath;
			RKIT_CHECK(imagePath.Set(rkit::StringSliceView(lower.ToSpan())));

			// Check if this file actually exists, since some materials like spacebox.atd have the wrong extension
			bool imageExists = false;
			RKIT_CHECK(feedback->CheckInputExists(rkit::buildsystem::BuildFileLocation::kSourceDir, imagePath, imageExists));
			if (!imageExists)
			{
				size_t extPos = lower.Count();
				while (extPos > 0)
				{
					extPos--;
					if (lower[extPos] == '.')
						break;
				}

				if (extPos == 0)
				{
					rkit::log::ErrorFmt("Invalid image path {}", imagePath.ToString());
					return rkit::ResultCode::kDataError;
				}

				rkit::String oldExt;
				RKIT_CHECK(oldExt.Set(lower.ToSpan().SubSpan(extPos)));

				const rkit::StringView exts[] =
				{
					".tga",
					".png",
					".pcx",
				};

				for (const rkit::StringView &ext : exts)
				{
					if (ext == oldExt)
						continue;

					lower.ShrinkToSize(extPos);
					RKIT_CHECK(lower.Append(ext.ToSpan()));
					RKIT_CHECK(imagePath.Set(rkit::StringSliceView(lower.ToSpan())));

					RKIT_CHECK(feedback->CheckInputExists(rkit::buildsystem::BuildFileLocation::kSourceDir, imagePath, imageExists));
					if (imageExists)
						break;
				}

				if (!imageExists)
				{
					rkit::log::ErrorFmt("Image {} couldn't be found", imagePath.ToString());
					return rkit::ResultCode::kDataError;
				}

				RKIT_CHECK(findImageIndexFunc());
			}

			if (imageIndex == dynamicData.m_imageImports.Count())
			{
				MaterialAnalysisImageImport imageImport = {};
				RKIT_CHECK(imageImport.m_identifier.Set(lower.ToSpan()));
				imageImport.m_disposition = disposition;
				RKIT_CHECK(dynamicData.m_imageImports.Append(std::move(imageImport)));
			}
		}

		bitmapDef = {};
		bitmapDef.m_nameIndex = imageIndex;

		return rkit::ResultCode::kOK;
	}

	rkit::Result MaterialCompiler::GenerateRealFrames(MaterialAnalysisHeader &header, MaterialAnalysisDynamicData &dynamicData, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) const
	{
		if (header.m_materialType != data::MaterialType::kAnimation)
			return rkit::ResultCode::kOK;

		MaterialAnalysisHeader newHeader = header;
		MaterialAnalysisDynamicData newDynamicData = {};

		const size_t numBitmaps = dynamicData.m_bitmapDefs.Count();
		const size_t numFrames = dynamicData.m_frameDefs.Count();

		rkit::Vector<bool> isInLoop;
		rkit::Vector<rkit::Span<size_t>> predLists;
		rkit::Vector<size_t> predsArray;

		RKIT_CHECK(predLists.Resize(numFrames));
		RKIT_CHECK(predsArray.Resize(numFrames));

		// Build predecessors list
		{
			rkit::Vector<size_t> predListStarts;
			rkit::Vector<size_t> predListCounts;

			RKIT_CHECK(predListStarts.Resize(numFrames));
			RKIT_CHECK(predListCounts.Resize(numFrames));

			// Collect counts
			for (size_t i = 0; i < numFrames; i++)
			{
				size_t next = dynamicData.m_frameDefs[i].m_next;
				predListCounts[next]++;
			}

			// Calculate starts
			for (size_t i = 1; i < numFrames; i++)
				predListStarts[i] = predListStarts[i - 1] + predListCounts[i - 1];

			// Create spans
			for (size_t i = 0; i < numFrames; i++)
				predLists[i] = predsArray.ToSpan().SubSpan(predListStarts[i], predListCounts[i]);

			// Fill pred lists
			for (size_t &count : predListCounts)
				count = 0;

			for (size_t i = 0; i < numFrames; i++)
			{
				size_t next = dynamicData.m_frameDefs[i].m_next;
				predLists[next][predListCounts[next]++] = i;
			}
		}

		rkit::Vector<bool> bitmapIsFullFrame;
		rkit::Vector<bool> frameIsFullFrame;
		RKIT_CHECK(bitmapIsFullFrame.Resize(numBitmaps));
		RKIT_CHECK(frameIsFullFrame.Resize(numFrames));

		// Find frame metadatas
		for (size_t i = 0; i < numBitmaps; i++)
		{
			rkit::CIPath path;
			RKIT_CHECK(path.Set(dynamicData.m_imageImports[dynamicData.m_bitmapDefs[i].m_nameIndex].m_identifier));

			rkit::utils::ImageSpec imageSpec = {};
			RKIT_CHECK(TextureCompilerBase::GetImageMetadata(imageSpec, feedback, m_pngDriver, rkit::buildsystem::BuildFileLocation::kSourceDir, path));

			bitmapIsFullFrame[i] = (imageSpec.m_width == header.m_width && imageSpec.m_height == header.m_height);
		}

		bool allFullFrame = true;
		for (size_t i = 0; i < numFrames; i++)
		{
			frameIsFullFrame[i] = true;
			const MaterialAnalysisFrameDef &frameDef = dynamicData.m_frameDefs[i];
			if (!bitmapIsFullFrame[dynamicData.m_frameDefs[i].m_bitmap] || frameDef.m_xOffset != 0 || frameDef.m_yOffset != 0)
			{
				bitmapIsFullFrame[i] = false;
				allFullFrame = false;
			}
		}

		if (allFullFrame)
			return rkit::ResultCode::kOK;

		// Okay, 

		header = std::move(newHeader);
		dynamicData = std::move(newDynamicData);

		return rkit::ResultCode::kNotYetImplemented;
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

		return RunAnalyzeMissing(shortName, depsNode, feedback);
	}

	rkit::Result MaterialCompiler::RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback)
	{
		struct DeduplicatedContentID
		{
			rkit::data::ContentID m_contentID = {};
			size_t m_uniqueIndex = 0;
		};

		data::MaterialResourceType nodeType = data::MaterialResourceType::kCount;

		RKIT_CHECK(MaterialNodeTypeFromFourCC(nodeType, depsNode->GetDependencyNodeType()));

		rkit::CIPath analysisPath;
		RKIT_CHECK(ConstructAnalysisPath(analysisPath, nodeType, depsNode->GetIdentifier()));

		MaterialAnalysisHeader analysisHeader;
		MaterialAnalysisDynamicData dynamicData;

		{
			rkit::UniquePtr<rkit::ISeekableReadStream> analysisStream;
			RKIT_CHECK(feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, analysisPath, analysisStream));

			RKIT_CHECK(analysisStream->ReadAll(&analysisHeader, sizeof(analysisHeader)));

			if (analysisHeader.m_magic != MaterialAnalysisHeader::kExpectedMagic || analysisHeader.m_version != MaterialAnalysisHeader::kExpectedVersion)
			{
				rkit::log::ErrorFmt("Material '{}' analysis file was invalid", depsNode->GetIdentifier().GetChars());
				return rkit::ResultCode::kOperationFailed;
			}

			RKIT_CHECK(dynamicData.Deserialize(*analysisStream));
		}

		const size_t numImageImports = dynamicData.m_imageImports.Count();

		rkit::Vector<DeduplicatedContentID> bitmapContentIDs;
		RKIT_CHECK(bitmapContentIDs.Resize(numImageImports));

		for (size_t i = 0; i < numImageImports; i++)
		{
			const MaterialAnalysisImageImport &imageImport = dynamicData.m_imageImports[i];

			rkit::String intermediatePathStr;
			RKIT_CHECK(TextureCompilerBase::ResolveIntermediatePath(intermediatePathStr, imageImport.m_identifier));

			rkit::CIPath intermediatePath;
			RKIT_CHECK(intermediatePath.Set(intermediatePathStr));

			RKIT_CHECK(feedback->IndexCAS(rkit::buildsystem::BuildFileLocation::kIntermediateDir, intermediatePath, bitmapContentIDs[i].m_contentID));
		}

		rkit::Vector<data::MaterialBitmapDef> bitmapDefs;

		for (DeduplicatedContentID &ddContentID : bitmapContentIDs)
		{
			ddContentID.m_uniqueIndex = bitmapDefs.Count();

			for (size_t i = 0; i < bitmapDefs.Count(); i++)
			{
				if (ddContentID.m_contentID == bitmapDefs[i].m_contentID)
				{
					ddContentID.m_uniqueIndex = i;
					break;
				}
			}

			if (ddContentID.m_uniqueIndex == bitmapDefs.Count())
			{
				data::MaterialBitmapDef bitmapDef;
				bitmapDef.m_contentID = ddContentID.m_contentID;

				RKIT_CHECK(bitmapDefs.Append(bitmapDef));
			}
		}

		rkit::Vector<data::MaterialFrameDef> frameDefs;

		for (const MaterialAnalysisFrameDef &inFrameDef : dynamicData.m_frameDefs)
		{
			data::MaterialFrameDef outFrameDef = {};

			outFrameDef.m_bitmap = static_cast<uint32_t>(bitmapContentIDs[inFrameDef.m_bitmap].m_uniqueIndex);
			outFrameDef.m_next = inFrameDef.m_next;
			outFrameDef.m_waitMSec = inFrameDef.m_waitMSec;

			RKIT_CHECK(frameDefs.Append(outFrameDef));
		}

		data::MaterialHeader materialHeader;
		materialHeader.m_magic = data::MaterialHeader::kExpectedMagic;
		materialHeader.m_version = data::MaterialHeader::kExpectedVersion;
		materialHeader.m_width = analysisHeader.m_width;
		materialHeader.m_height = analysisHeader.m_height;

		materialHeader.m_bilinear = analysisHeader.m_bilinear ? 1 : 0;
		materialHeader.m_materialType = static_cast<uint8_t>(analysisHeader.m_materialType);
		materialHeader.m_colorType = static_cast<uint8_t>(analysisHeader.m_colorType);
		materialHeader.m_unused = 0;

		materialHeader.m_numBitmaps = static_cast<uint32_t>(bitmapDefs.Count());
		materialHeader.m_numFrames = static_cast<uint32_t>(frameDefs.Count());

		rkit::CIPath outputPath;
		RKIT_CHECK(ConstructOutputPath(outputPath, nodeType, depsNode->GetIdentifier()));

		{
			rkit::UniquePtr<rkit::ISeekableReadWriteStream> outFile;
			RKIT_CHECK(feedback->OpenOutput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, outputPath, outFile));

			RKIT_CHECK(outFile->WriteAll(&materialHeader, sizeof(materialHeader)));
			RKIT_CHECK(outFile->WriteAll(bitmapDefs.GetBuffer(), bitmapDefs.Count() * sizeof(bitmapDefs[0])));
			RKIT_CHECK(outFile->WriteAll(frameDefs.GetBuffer(), frameDefs.Count() * sizeof(frameDefs[0])));
		}

		return rkit::ResultCode::kOK;
	}

	rkit::StringView MaterialCompiler::GetFontMaterialExtension()
	{
		return "fontmaterial";
	}

	rkit::StringView MaterialCompiler::GetWorldMaterialExtension()
	{
		return "worldmaterial";
	}

	rkit::StringView MaterialCompiler::GetModelMaterialExtension()
	{
		return "modelmaterial";
	}

	uint32_t MaterialCompiler::GetVersion() const
	{
		return 5;
	}
} } // anox::buildsystem
