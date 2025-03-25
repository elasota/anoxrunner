#include "AnoxTextureCompiler.h"

#include "rkit/Core/Algorithm.h"
#include "rkit/Core/Endian.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/StaticArray.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/String.h"
#include "rkit/Core/Vector.h"

#include "rkit/Data/DDSFile.h"

namespace anox::buildsystem
{
	namespace priv
	{
		struct PCXHeader
		{
			static const uint8_t kExpectedMagic = 0x0a;

			uint8_t m_magic;
			uint8_t m_version;
			uint8_t m_encoding;
			uint8_t m_bitsPerPlane;

			rkit::endian::LittleUInt16_t m_minX;
			rkit::endian::LittleUInt16_t m_minY;
			rkit::endian::LittleUInt16_t m_maxX;
			rkit::endian::LittleUInt16_t m_maxY;

			rkit::endian::LittleUInt16_t m_hDPI;
			rkit::endian::LittleUInt16_t m_vDPI;

			uint8_t m_egaPalette[16][3];

			uint8_t m_reserved1[1];
			uint8_t m_numColorPlanes;
			rkit::endian::LittleUInt16_t m_pitch;
			rkit::endian::LittleUInt16_t m_paletteMode;

			rkit::endian::LittleUInt16_t m_sourceResWidth;
			rkit::endian::LittleUInt16_t m_sourceResHeight;

			uint8_t m_reserved2[54];
		};

		template<class TElementType, size_t TIndex, size_t TCount, class... TElementValue>
		struct SetAtHelper
		{
		};

		template<class TElementType, size_t TIndex, size_t TCount, class TFirstElementValue, class... TMoreElementValues>
		struct SetAtHelper<TElementType, TIndex, TCount, TFirstElementValue, TMoreElementValues...>
		{
			static void SetAt(rkit::StaticArray<TElementType, TCount> &elements, TFirstElementValue firstValue, TMoreElementValues... values);
		};

		template<class TElementType, size_t TIndex, size_t TCount, class TElementValue>
		struct SetAtHelper<TElementType, TIndex, TCount, TElementValue>
		{
			static void SetAt(rkit::StaticArray<TElementType, TCount> &elements, TElementValue value);
		};

		template<class TElementType, size_t TNumElements>
		class TextureCompilerPixel
		{
		public:
			TextureCompilerPixel();

			template<class... TElementValue>
			TextureCompilerPixel(TElementValue... values);

			const rkit::StaticArray<TElementType, 4> &GetValues() const;
			rkit::StaticArray<TElementType, 4> &ModifyValues();

			template<size_t TIndex>
			TElementType GetAt() const;

			template<size_t TIndex>
			void SetAt(TElementType value);

			template<class... TElementValues>
			void Set(TElementValues... values);

			template<size_t TIndex>
			TElementType &ModifyAt(TElementType value);

			TElementType &ModifyR() const { return ModifyAt<0>(); }
			TElementType &ModifyG() const { return ModifyAt<1>(); }
			TElementType &ModifyB() const { return ModifyAt<2>(); }
			TElementType &ModifyA() const { return ModifyAt<3>(); }

			TElementType GetR() const { return GetAt<0>(); }
			TElementType GetG() const { return GetAt<1>(); }
			TElementType GetB() const { return GetAt<2>(); }
			TElementType GetA() const { return GetAt<3>(); }

			void SetR(TElementType value) { SetAt<0>(value); }
			void SetG(TElementType value) { SetAt<1>(value); }
			void SetB(TElementType value) { SetAt<2>(value); }
			void SetA(TElementType value) { SetAt<3>(value); }

		private:
			rkit::StaticArray<TElementType, 4> m_values;
		};

		template<class TPixelElement, size_t TElementsPerPixel>
		class TextureCompilerImage
		{
		public:
			typedef TextureCompilerPixel<TPixelElement, TElementsPerPixel> Pixel_t;

			TextureCompilerImage();

			rkit::Result Initialize(uint32_t width, uint32_t height);

			rkit::Span<Pixel_t> GetScanline(uint32_t y);
			rkit::Span<const Pixel_t> GetScanline(uint32_t y) const;

			uint32_t GetWidth() const;
			uint32_t GetHeight() const;

		private:
			rkit::Vector<Pixel_t> m_pixelData;

			size_t m_pitchInElements;
			uint32_t m_width;
			uint32_t m_height;
		};

		template<class TElementType, size_t TNumElements, size_t TElementIndex, bool TElementIsInRange>
		struct UsesChannelHelperBase
		{
		};

		template<class TElementType, size_t TNumElements, size_t TElementIndex>
		struct UsesChannelHelperBase<TElementType, TNumElements, TElementIndex, true>
		{
			static bool UsesChannel(const TextureCompilerImage<TElementType, TNumElements> &image);
		};

		template<class TElementType, size_t TNumElements, size_t TElementIndex>
		struct UsesChannelHelperBase<TElementType, TNumElements, TElementIndex, false>
		{
			static bool UsesChannel(const TextureCompilerImage<TElementType, TNumElements> &image);
		};

		template<class TElementType, size_t TNumElements, size_t TElementIndex>
		struct UsesChannelHelper : public UsesChannelHelperBase<TElementType, TNumElements, TElementIndex, (TElementIndex < TNumElements)>
		{
		};

		template<class TElementType, size_t TElementIndex>
		struct ChannelDefaultValueHelper
		{
			static TElementType GetDefaultValue();
		};

		template<class TElementType>
		struct ChannelDefaultValueHelper<TElementType, 3>
		{
			static TElementType GetDefaultValue();
		};

		template<class TElementType>
		struct NormalizedValueHelper
		{
			static TElementType GetNormalizedOne();
		};

		template<>
		struct NormalizedValueHelper<float>
		{
			static float GetNormalizedOne();
		};

		template<>
		struct NormalizedValueHelper<double>
		{
			static double GetNormalizedOne();
		};

		template<class TElementType, size_t TNumElements, size_t TElementIndex, bool TIsInRange>
		struct PixelPackHelperBase
		{
		};

		template<class TElementType, size_t TNumElements, size_t TElementIndex>
		struct PixelPackHelperBase<TElementType, TNumElements, TElementIndex, true>
		{
			static uint64_t Pack(const TextureCompilerPixel<TElementType, TNumElements> &pixel);
		};

		template<class TElementType, size_t TNumElements, size_t TElementIndex>
		struct PixelPackHelperBase<TElementType, TNumElements, TElementIndex, false>
		{
			static uint64_t Pack(const TextureCompilerPixel<TElementType, TNumElements> &pixel);
		};

		template<class TElementType, size_t TNumElements>
		struct PixelPackHelper : public PixelPackHelperBase<TElementType, TNumElements, 0, (TNumElements > 0)>
		{
		};
	}

	class TextureCompiler final : public TextureCompilerBase
	{
	public:
		bool HasAnalysisStage() const override;

		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;

		uint32_t GetVersion() const override;

	private:
		static rkit::Result CompileTGA(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, const rkit::StringView &shortName, ImageImportDisposition disposition);
		static rkit::Result CompilePCX(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, const rkit::StringView &shortName, ImageImportDisposition disposition);
		static rkit::Result CompilePNG(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, const rkit::StringView &shortName, ImageImportDisposition disposition);

		template<class TElementType, size_t TNumElements>
		static rkit::Result GenerateMipMaps(rkit::Vector<priv::TextureCompilerImage<TElementType, TNumElements>> &resultImages, const rkit::Span<priv::TextureCompilerImage<TElementType, TNumElements>> &sourceImages, size_t &outNumLevels, ImageImportDisposition disposition);

		template<class TElementType, size_t TNumElements>
		static rkit::Result Generate2DMipMapChain(const rkit::Span<priv::TextureCompilerImage<TElementType, TNumElements>> &images, ImageImportDisposition disposition);

		template<class TElementType, size_t TNumElements>
		static rkit::Result ExportDDS(const rkit::Span<priv::TextureCompilerImage<TElementType, TNumElements>> &images, size_t numLevels, size_t numLayers, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, const rkit::StringView &shortName, ImageImportDisposition disposition);

		static bool DispositionHasAlpha(ImageImportDisposition disposition);
		static bool DispositionHasMipMaps(ImageImportDisposition disposition);
	};
}

namespace anox::buildsystem::priv
{
	template<class TElementType, size_t TIndex, size_t TCount, class TFirstElementValue, class... TMoreElementValues>
	void SetAtHelper<TElementType, TIndex, TCount, TFirstElementValue, TMoreElementValues...>::SetAt(rkit::StaticArray<TElementType, TCount> &elements, TFirstElementValue firstValue, TMoreElementValues... moreValues)
	{
		SetAtHelper<TElementType, TIndex, TCount, TFirstElementValue>::SetAt(elements, firstValue);
		SetAtHelper<TElementType, TIndex + 1, TCount, TMoreElementValues...>::SetAt(elements, moreValues...);
	}

	template<class TElementType, size_t TIndex, size_t TCount, class TElementValue>
	void SetAtHelper<TElementType, TIndex, TCount, TElementValue>::SetAt(rkit::StaticArray<TElementType, TCount> &elements, TElementValue value)
	{
		static_assert(TIndex < TCount);
		elements[TIndex] = value;
	}

	template<class TElementType, size_t TNumElements>
	TextureCompilerPixel<TElementType, TNumElements>::TextureCompilerPixel()
		: m_values{ }
	{
	}

	template<class TElementType, size_t TNumElements>
	template<class... TElementValue>
	TextureCompilerPixel<TElementType, TNumElements>::TextureCompilerPixel(TElementValue... values)
		: m_values{ values... }
	{
	}

	template<class TElementType, size_t TNumElements>
	const rkit::StaticArray<TElementType, 4> &TextureCompilerPixel<TElementType, TNumElements>::GetValues() const
	{
		return m_values;
	}

	template<class TElementType, size_t TNumElements>
	rkit::StaticArray<TElementType, 4> &TextureCompilerPixel<TElementType, TNumElements>::ModifyValues()
	{
		return m_values;
	}

	template<class TElementType, size_t TNumElements>
	template<size_t TIndex>
	TElementType TextureCompilerPixel<TElementType, TNumElements>::GetAt() const
	{
		static_assert(TIndex < TNumElements);
		return m_values[TIndex];
	}

	template<class TElementType, size_t TNumElements>
	template<size_t TIndex>
	void TextureCompilerPixel<TElementType, TNumElements>::SetAt(TElementType value)
	{
		static_assert(TIndex < TNumElements);
		m_values[TIndex] = value;
	}


	template<class TElementType, size_t TNumElements>
	template<class... TElementValues>
	void TextureCompilerPixel<TElementType, TNumElements>::Set(TElementValues... values)
	{
		SetAtHelper<TElementType, 0, TNumElements, TElementValues...>::SetAt(m_values, values...);
	}

	template<class TElementType, size_t TNumElements>
	template<size_t TIndex>
	TElementType &TextureCompilerPixel<TElementType, TNumElements>::ModifyAt(TElementType value)
	{
		static_assert(TIndex < TNumElements);
		return m_values[TIndex];
	}

	template<class TPixelElement, size_t TElementsPerPixel>
	TextureCompilerImage<TPixelElement, TElementsPerPixel>::TextureCompilerImage()
		: m_pitchInElements(0)
		, m_width(0)
		, m_height(0)
	{
	}

	template<class TPixelElement, size_t TElementsPerPixel>
	rkit::Result TextureCompilerImage<TPixelElement, TElementsPerPixel>::Initialize(uint32_t width, uint32_t height)
	{
		const size_t pitch = width;

		size_t numElements = 0;
		RKIT_CHECK(rkit::SafeMul<size_t>(numElements, pitch, height));

		RKIT_CHECK(m_pixelData.Resize(numElements));

		m_pitchInElements = pitch;
		m_width = width;
		m_height = height;

		return rkit::ResultCode::kOK;
	}

	template<class TPixelElement, size_t TElementsPerPixel>
	rkit::Span<typename TextureCompilerImage<TPixelElement, TElementsPerPixel>::Pixel_t> TextureCompilerImage<TPixelElement, TElementsPerPixel>::GetScanline(uint32_t y)
	{
		return m_pixelData.ToSpan().SubSpan(y * m_pitchInElements, m_width);
	}

	template<class TPixelElement, size_t TElementsPerPixel>
	rkit::Span<const typename TextureCompilerImage<TPixelElement, TElementsPerPixel>::Pixel_t> TextureCompilerImage<TPixelElement, TElementsPerPixel>::GetScanline(uint32_t y) const
	{
		return m_pixelData.ToSpan().SubSpan(y * m_pitchInElements, m_width);
	}

	template<class TPixelElement, size_t TElementsPerPixel>
	uint32_t TextureCompilerImage<TPixelElement, TElementsPerPixel>::GetWidth() const
	{
		return m_width;
	}

	template<class TPixelElement, size_t TElementsPerPixel>
	uint32_t TextureCompilerImage<TPixelElement, TElementsPerPixel>::GetHeight() const
	{
		return m_height;
	}

	template<class TElementType, size_t TNumElements, size_t TElementIndex>
	bool UsesChannelHelperBase<TElementType, TNumElements, TElementIndex, true>::UsesChannel(const TextureCompilerImage<TElementType, TNumElements> &image)
	{
		const uint32_t width = image.GetWidth();
		const uint32_t height = image.GetHeight();

		for (uint32_t y = 0; y < height; y++)
		{
			const rkit::ConstSpan<TextureCompilerPixel<TElementType, TNumElements>> scanline = image.GetScanline(y);

			for (const TextureCompilerPixel<TElementType, TNumElements> &pixel : scanline)
			{
				if (!(pixel.GetAt<TElementIndex>() == ChannelDefaultValueHelper<TElementType, TElementIndex>::GetDefaultValue()))
					return true;
			}
		}

		return false;
	}

	template<class TElementType, size_t TNumElements, size_t TElementIndex>
	bool UsesChannelHelperBase<TElementType, TNumElements, TElementIndex, false>::UsesChannel(const TextureCompilerImage<TElementType, TNumElements> &image)
	{
		return false;
	}

	template<class TElementType, size_t TElementIndex>
	TElementType ChannelDefaultValueHelper<TElementType, TElementIndex>::GetDefaultValue()
	{
		return static_cast<TElementType>(0);
	}

	template<class TElementType>
	TElementType ChannelDefaultValueHelper<TElementType, 3>::GetDefaultValue()
	{
		return NormalizedValueHelper<TElementType>::GetNormalizedOne();
	}

	template<class TElementType>
	TElementType NormalizedValueHelper<TElementType>::GetNormalizedOne()
	{
		return std::numeric_limits<TElementType>::max();
	}

	float NormalizedValueHelper<float>::GetNormalizedOne()
	{
		return 1.0f;
	}

	double NormalizedValueHelper<double>::GetNormalizedOne()
	{
		return 1.0f;
	}


	template<class TElementType, size_t TNumElements, size_t TElementIndex>
	uint64_t PixelPackHelperBase<TElementType, TNumElements, TElementIndex, true>::Pack(const TextureCompilerPixel<TElementType, TNumElements> &pixel)
	{
		uint64_t bits = static_cast<uint64_t>(pixel.GetAt<TElementIndex>()) << (sizeof(TElementType) * 8 * TElementIndex);
		bits |= PixelPackHelperBase<TElementType, TNumElements, TElementIndex + 1, ((TElementIndex + 1) < TNumElements)>::Pack(pixel);
		return bits;
	}

	template<class TElementType, size_t TNumElements, size_t TElementIndex>
	uint64_t PixelPackHelperBase<TElementType, TNumElements, TElementIndex, false>::Pack(const TextureCompilerPixel<TElementType, TNumElements> &pixel)
	{
		return 0;
	}
}

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
		typedef uint8_t RGBTriplet_t[3];

		priv::PCXHeader pcxHeader = {};

		rkit::UniquePtr<rkit::ISeekableReadStream> stream;
		RKIT_CHECK(feedback->OpenInput(depsNode->GetInputFileLocation(), shortName, stream));

		RKIT_CHECK(stream->ReadAll(&pcxHeader, sizeof(pcxHeader)));

		if (pcxHeader.m_numColorPlanes != 1 && pcxHeader.m_numColorPlanes != 3 && pcxHeader.m_numColorPlanes != 4)
		{
			rkit::log::ErrorFmt("PCX file '%s' has an unsupported number of planes", shortName.GetChars());
			return rkit::ResultCode::kMalformedFile;
		}

		if (pcxHeader.m_bitsPerPlane != 8)
		{
			rkit::log::ErrorFmt("PCX file '%s' has an unsupported bits per plane", shortName.GetChars());
			return rkit::ResultCode::kMalformedFile;
		}

		const size_t scanLinePlanePitch = pcxHeader.m_pitch.Get();
		const size_t scanLinePitch = scanLinePlanePitch * pcxHeader.m_numColorPlanes;

		if (scanLinePitch == 0)
		{
			rkit::log::ErrorFmt("PCX file '%s' has zero size", shortName.GetChars());
			return rkit::ResultCode::kMalformedFile;
		}

		if (pcxHeader.m_minX.Get() > pcxHeader.m_maxX.Get() || pcxHeader.m_minY.Get() > pcxHeader.m_maxY.Get())
		{
			rkit::log::ErrorFmt("PCX file '%s' has invalid dimensions", shortName.GetChars());
			return rkit::ResultCode::kMalformedFile;
		}

		const uint32_t width = static_cast<uint32_t>(pcxHeader.m_maxX.Get()) - pcxHeader.m_minX.Get() + 1;
		const uint32_t height = static_cast<uint32_t>(pcxHeader.m_maxY.Get()) - pcxHeader.m_minY.Get() + 1;

		if (scanLinePlanePitch < width)
		{
			rkit::log::ErrorFmt("PCX file '%s' has invalid pitch", shortName.GetChars());
			return rkit::ResultCode::kMalformedFile;
		}

		size_t dataSize = 0;

		RKIT_CHECK(rkit::SafeMul<size_t>(dataSize, height, scanLinePitch));

		rkit::Vector<uint8_t> pcxData;
		RKIT_CHECK(pcxData.Resize(height * scanLinePitch));

		for (size_t y = 0; y < height; y++)
		{
			rkit::Span<uint8_t> scanline = pcxData.ToSpan().SubSpan(y * scanLinePitch, scanLinePitch);

			size_t bytesRemaining = scanLinePitch;

			while (bytesRemaining > 0)
			{
				// Very slow... yikes
				uint8_t data = 0;
				RKIT_CHECK(stream->ReadAll(&data, 1));

				uint8_t count = 1;
				if ((data & 0xc0) == 0xc0)
				{
					count = data & 0x3f;
					RKIT_CHECK(stream->ReadAll(&data, 1));

					if (count > bytesRemaining)
					{
						rkit::log::ErrorFmt("PCX file '%s' RLE data was corrupted", shortName.GetChars());
						return rkit::ResultCode::kMalformedFile;
					}
				}

				const size_t writeStart = scanLinePitch - bytesRemaining;

				for (size_t i = 0; i < count; i++)
					scanline[writeStart + i] = data;

				bytesRemaining -= count;
			}
		}

		uint8_t vgaPalette[256][3];
		bool haveVGAPalette = false;

		priv::TextureCompilerImage<uint8_t, 4> image;
		RKIT_CHECK(image.Initialize(width, height));

		if (pcxHeader.m_numColorPlanes == 1)
		{
			rkit::Span<RGBTriplet_t> palette;

			if (pcxHeader.m_version == 5)
			{
				haveVGAPalette = true;

				RKIT_CHECK(stream->SeekEnd(-769));

				uint8_t checkByte = 0;
				RKIT_CHECK(stream->ReadAll(&checkByte, 1));

				if (checkByte != 12)
				{
					rkit::log::ErrorFmt("PCX file '%s' palette check byte was invalid", shortName.GetChars());
					return rkit::ResultCode::kMalformedFile;
				}

				static_assert(sizeof(vgaPalette) == 768);
				RKIT_CHECK(stream->ReadAll(vgaPalette, sizeof(vgaPalette)));

				palette = rkit::Span<RGBTriplet_t>(vgaPalette, 256);
			}
			else
			{
				palette = rkit::Span<RGBTriplet_t>(pcxHeader.m_egaPalette, 16);
			}

			const bool hasAlpha = DispositionHasAlpha(disposition);

			for (uint32_t y = 0; y < height; y++)
			{
				const rkit::ConstSpan<uint8_t> inScanline = pcxData.ToSpan().SubSpan(y * scanLinePitch, width);
				const rkit::Span<priv::TextureCompilerPixel<uint8_t, 4>> outScanline = image.GetScanline(y);

				for (uint32_t x = 0; x < width; x++)
				{
					uint8_t elementValue = inScanline[x];
					if (elementValue > palette.Count())
					{
						rkit::log::ErrorFmt("PCX file '%s' had an out-of-range value", shortName.GetChars());
						return rkit::ResultCode::kMalformedFile;
					}

					if (elementValue == 255 && hasAlpha)
						outScanline[x].Set(0, 0, 0, 0);
					else
					{
						const RGBTriplet_t &triplet = palette[elementValue];
						outScanline[x].Set(triplet[0], triplet[1], triplet[2], 255);
					}
				}
			}
		}
		else
		{
			return rkit::ResultCode::kNotYetImplemented;
		}

		rkit::Vector<priv::TextureCompilerImage<uint8_t, 4>> images;

		size_t numLevels = 0;
		RKIT_CHECK(GenerateMipMaps(images, rkit::Span<priv::TextureCompilerImage<uint8_t, 4>>(&image, 1), numLevels, disposition));

		return ExportDDS(images.ToSpan(), numLevels, 1, depsNode, feedback, shortName, disposition);
	}

	rkit::Result TextureCompiler::CompilePNG(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, const rkit::StringView &shortName, ImageImportDisposition disposition)
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	template<class TElementType, size_t TNumElements>
	rkit::Result TextureCompiler::GenerateMipMaps(rkit::Vector<priv::TextureCompilerImage<TElementType, TNumElements>> &resultImages, const rkit::Span<priv::TextureCompilerImage<TElementType, TNumElements>> &sourceImages, size_t &outNumLevels, ImageImportDisposition disposition)
	{
		// 3D textures not supported yet
		size_t numLevels = 1;
		if (DispositionHasMipMaps(disposition))
		{
			size_t width = sourceImages[0].GetWidth();
			size_t height = sourceImages[0].GetHeight();

			numLevels = 0;
			while (width != 0 && height != 0)
			{
				numLevels++;
				width /= 2;
				height /= 2;
			}
		}

		RKIT_CHECK(resultImages.Resize(sourceImages.Count() * numLevels));

		for (size_t i = 0; i < sourceImages.Count(); i++)
		{
			resultImages[i * numLevels] = std::move(sourceImages[i]);
			RKIT_CHECK(Generate2DMipMapChain(resultImages.ToSpan().SubSpan(i * numLevels, numLevels), disposition));
		}

		outNumLevels = numLevels;
		return rkit::ResultCode::kOK;
	}

	template<class TElementType, size_t TNumElements>
	rkit::Result TextureCompiler::Generate2DMipMapChain(const rkit::Span<priv::TextureCompilerImage<TElementType, TNumElements>> &images, ImageImportDisposition disposition)
	{
		if (images.Count() == 1)
			return rkit::ResultCode::kOK;

		return rkit::ResultCode::kNotYetImplemented;
	}

	template<class TElementType, size_t TNumElements>
	rkit::Result TextureCompiler::ExportDDS(const rkit::Span<priv::TextureCompilerImage<TElementType, TNumElements>> &images, size_t numMipMaps, size_t numLayers, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, const rkit::StringView &shortName, ImageImportDisposition disposition)
	{
		rkit::String outName;
		RKIT_CHECK(outName.Format("ax_tex/%s.%i.dds", shortName.GetChars(), static_cast<int>(disposition)));

		bool isCompressed = false;
		bool hasPitch = true;
		bool is3D = false;
		bool isExtended = false;
		bool isCubeMap = false;
		uint8_t pixelSize = TNumElements;

		size_t numChannelsToSave = 1;

		for (const priv::TextureCompilerImage<TElementType, TNumElements> &image : images)
		{
			if (numChannelsToSave < 4)
			{
				if (priv::UsesChannelHelper<TElementType, TNumElements, 3>::UsesChannel(image))
					numChannelsToSave = 4;
			}
			if (numChannelsToSave < 3)
			{
				if (priv::UsesChannelHelper<TElementType, TNumElements, 2>::UsesChannel(image))
					numChannelsToSave = 3;
			}
			if (numChannelsToSave < 2)
			{
				if (priv::UsesChannelHelper<TElementType, TNumElements, 1>::UsesChannel(image))
					numChannelsToSave = 2;
			}
		}

		uint32_t ddsFlags = 0;
		ddsFlags |= rkit::data::DDSFlags::kCaps;
		ddsFlags |= rkit::data::DDSFlags::kHeight;
		ddsFlags |= rkit::data::DDSFlags::kWidth;
		ddsFlags |= rkit::data::DDSFlags::kPixelFormat;

		if (hasPitch)
		{
			if (isCompressed)
				ddsFlags |= rkit::data::DDSFlags::kLinearSize;
			else
				ddsFlags |= rkit::data::DDSFlags::kPitch;
		}

		if (is3D)
			ddsFlags |= rkit::data::DDSFlags::kDepth;

		rkit::data::DDSHeader ddsHeader = {};
		ddsHeader.m_magic = rkit::data::DDSHeader::kExpectedMagic;
		ddsHeader.m_headerSizeMinus4 = sizeof(ddsHeader) - 4;
		ddsHeader.m_ddsFlags = ddsFlags;

		ddsHeader.m_height = images[0].GetHeight();
		ddsHeader.m_width = images[0].GetWidth();

		if (isCompressed)
		{
			return rkit::ResultCode::kNotYetImplemented;
		}
		else
		{
			uint32_t pitch = (images[0].GetWidth() * pixelSize + 3) / 4;
			ddsHeader.m_pitchOrLinearSize = pitch;
		}

		if (is3D)
			return rkit::ResultCode::kNotYetImplemented;
		else
			ddsHeader.m_depth = 1;

		ddsHeader.m_mipMapCount = static_cast<uint32_t>(numMipMaps);

		{
			rkit::data::DDSPixelFormat &pixelFormat = ddsHeader.m_pixelFormat;

			pixelFormat.m_pixelFormatSize = sizeof(pixelFormat);

			{
				uint32_t pixelFormatFlags = 0;

				if (!isCompressed)
				{
					pixelFormatFlags |= rkit::data::DDSPixelFormatFlags::kRGB;

					if (numChannelsToSave >= 4)
						pixelFormatFlags |= rkit::data::DDSPixelFormatFlags::kAlphaPixels;
				}

				pixelFormat.m_pixelFormatFlags = pixelFormatFlags;
			}

			if (isExtended)
			{
				pixelFormat.m_fourCC = rkit::data::DDSFourCCs::kExtended;
			}
			else
			{
				if (isCompressed)
					return rkit::ResultCode::kNotYetImplemented;
			}

			pixelFormat.m_rgbBitCount = pixelSize * 8;

			pixelFormat.m_rBitMask = 0x000000ffu;

			if (numChannelsToSave >= 2)
				pixelFormat.m_gBitMask = 0x0000ff00u;
			if (numChannelsToSave >= 3)
				pixelFormat.m_bBitMask = 0x00ff0000u;
			if (numChannelsToSave >= 4)
				pixelFormat.m_aBitMask = 0xff000000u;
		}

		{
			uint32_t caps = 0;
			caps |= rkit::data::DDSCaps::kTexture;

			if (images.Count() > 1)
				caps |= rkit::data::DDSCaps::kComplex;
			if (numMipMaps > 1)
				caps |= rkit::data::DDSCaps::kMipMap;

			ddsHeader.m_caps = caps;
		}

		{
			uint32_t caps2 = 0;

			if (isCubeMap)
			{
				caps2 |= rkit::data::DDSCaps2::kCubeMap;
				caps2 |= rkit::data::DDSCaps2::kCubeMapPositiveX;
				caps2 |= rkit::data::DDSCaps2::kCubeMapNegativeX;
				caps2 |= rkit::data::DDSCaps2::kCubeMapPositiveY;
				caps2 |= rkit::data::DDSCaps2::kCubeMapNegativeY;
				caps2 |= rkit::data::DDSCaps2::kCubeMapPositiveZ;
				caps2 |= rkit::data::DDSCaps2::kCubeMapNegativeZ;
			}

			if (is3D)
				caps2 |= rkit::data::DDSCaps2::kVolume;

			ddsHeader.m_caps2 = caps2;
		}

		rkit::UniquePtr<rkit::ISeekableReadWriteStream> stream;
		RKIT_CHECK(feedback->OpenOutput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, outName, stream));

		RKIT_CHECK(stream->WriteAll(&ddsHeader, sizeof(ddsHeader)));

		rkit::Vector<uint8_t> packedScanline;
		RKIT_CHECK(packedScanline.Resize(pixelSize * images[0].GetWidth()));

		for (const priv::TextureCompilerImage<TElementType, TNumElements> &image : images)
		{
			uint32_t width = image.GetWidth();
			uint32_t height = image.GetHeight();
			uint8_t *scanlineBuffer = packedScanline.GetBuffer();

			for (uint32_t y = 0; y < height; y++)
			{
				uint8_t *scanlineOut = scanlineBuffer;

				const rkit::ConstSpan<priv::TextureCompilerPixel<TElementType, TNumElements>> scanline = image.GetScanline(y);

				for (const priv::TextureCompilerPixel<TElementType, TNumElements> &pixel : scanline)
				{
					rkit::endian::LittleUInt64_t packedPixel(priv::PixelPackHelper<TElementType, TNumElements>::Pack(pixel));

					for (size_t i = 0; i < pixelSize; i++)
						scanlineOut[i] = packedPixel.GetBytes()[i];

					scanlineOut += pixelSize;
				}

				RKIT_CHECK(stream->WriteAll(scanlineBuffer, pixelSize * width));
			}
		}

		return rkit::ResultCode::kOK;
	}

	bool TextureCompiler::DispositionHasAlpha(ImageImportDisposition disposition)
	{
		switch (disposition)
		{
		case ImageImportDisposition::kWorldAlphaBlend:
		case ImageImportDisposition::kWorldAlphaTested:
		case ImageImportDisposition::kGraphicTransparent:
			return true;
		default:
			return false;
		}
	}

	bool TextureCompiler::DispositionHasMipMaps(ImageImportDisposition disposition)
	{
		switch (disposition)
		{
		case ImageImportDisposition::kWorldAlphaBlend:
		case ImageImportDisposition::kWorldAlphaTested:
			return true;
		default:
			return false;
		}
	}

	uint32_t TextureCompiler::GetVersion() const
	{
		return 1;
	}

	rkit::Result TextureCompilerBase::CreateImportIdentifier(rkit::String &result, const rkit::StringView &imagePath, ImageImportDisposition disposition)
	{
		return result.Format("%s.%i", imagePath.GetChars(), static_cast<int>(disposition));
	}

	rkit::Result TextureCompilerBase::Create(rkit::UniquePtr<TextureCompilerBase> &outCompiler)
	{
		return rkit::New<TextureCompiler>(outCompiler);
	}
}
