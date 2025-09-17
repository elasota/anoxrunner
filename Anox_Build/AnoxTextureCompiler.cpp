#include "AnoxTextureCompiler.h"

#include "rkit/Core/Algorithm.h"
#include "rkit/Core/Endian.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Path.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/StaticArray.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/String.h"
#include "rkit/Core/UtilitiesDriver.h"
#include "rkit/Core/Vector.h"

#include "rkit/Data/DDSFile.h"

#include "rkit/Png/PngDriver.h"

#include "rkit/Utilities/Image.h"

namespace anox { namespace buildsystem
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
		class TextureCompilerImage final : public rkit::utils::IImage
		{
		public:
			typedef TextureCompilerPixel<TPixelElement, TElementsPerPixel> Pixel_t;

			TextureCompilerImage();

			const rkit::utils::ImageSpec &GetImageSpec() const override;
			uint8_t GetPixelSizeBytes() const override;

			virtual const void *GetScanline(uint32_t row) const override;
			virtual void *ModifyScanline(uint32_t row) override;

			rkit::Result Initialize(uint32_t width, uint32_t height, rkit::utils::PixelPacking pixelPacking);

			rkit::Span<Pixel_t> GetScanlineSpan(uint32_t y);
			rkit::Span<const Pixel_t> GetScanlineSpan(uint32_t y) const;

		private:
			rkit::Vector<Pixel_t> m_pixelData;

			size_t m_pitchInElements;
			rkit::utils::ImageSpec m_imageSpec;
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
	} // anox::buildsystem::priv

	class TextureCompiler final : public TextureCompilerBase
	{
	public:
		explicit TextureCompiler(rkit::png::IPngDriver &pngDriver);

		bool HasAnalysisStage() const override;

		rkit::Result RunAnalysis(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;
		rkit::Result RunCompile(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback) override;

		uint32_t GetVersion() const override;

		static rkit::Result GetImageMetadataDerived(rkit::utils::ImageSpec &imageSpec, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, rkit::png::IPngDriver &pngDriver, rkit::buildsystem::BuildFileLocation buildFileLocation, const rkit::CIPathView &shortName);
		static rkit::Result GetImageDerived(rkit::UniquePtr<rkit::utils::IImage> &image, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, rkit::png::IPngDriver &pngDriver, rkit::buildsystem::BuildFileLocation buildFileLocation, const rkit::CIPathView &shortName, ImageImportDisposition disposition);

	private:
		enum class TGADataType
		{
			kEmpty = 0,
			kUncompressedPalette = 1,
			kUncompressedColor = 2,
			kRLEPalette = 9,
			kRLEColor = 10,
		};

		struct TGAHeader
		{
			uint8_t m_identSize;
			uint8_t m_colorMapType;
			uint8_t m_dataType;
			rkit::endian::LittleUInt16_t m_colorMapStart;
			rkit::endian::LittleUInt16_t m_colorMapLength;
			uint8_t m_colorMapEntrySizeBits;
			rkit::endian::LittleUInt16_t m_imageOriginX;
			rkit::endian::LittleUInt16_t m_imageOriginY;
			rkit::endian::LittleUInt16_t m_imageWidth;
			rkit::endian::LittleUInt16_t m_imageHeight;
			uint8_t m_pixelSizeBits;
			uint8_t m_imageDescriptor;
		};

		static rkit::Result GetTGAMetadata(rkit::utils::ImageSpec &imageSpec, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, const rkit::CIPathView &shortName);
		static rkit::Result GetPCXMetadata(rkit::utils::ImageSpec &imageSpec, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, const rkit::CIPathView &shortName);
		static rkit::Result GetPNGMetadata(rkit::utils::ImageSpec &imageSpec, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, rkit::png::IPngDriver &pngDriver, const rkit::CIPathView &shortName);

		static rkit::Result GetTGA(rkit::UniquePtr<rkit::utils::IImage> &image, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, rkit::buildsystem::BuildFileLocation buildFileLocation, const rkit::CIPathView &shortName, ImageImportDisposition disposition);
		static rkit::Result GetPCX(rkit::UniquePtr<rkit::utils::IImage> &image, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, rkit::buildsystem::BuildFileLocation buildFileLocation, const rkit::CIPathView &shortName, ImageImportDisposition disposition);
		static rkit::Result GetPNG(rkit::UniquePtr<rkit::utils::IImage> &image, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, rkit::png::IPngDriver &pngDriver, rkit::buildsystem::BuildFileLocation buildFileLocation, const rkit::CIPathView &shortName, ImageImportDisposition disposition);

		static rkit::Result CompileTGA(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, const rkit::CIPathView &shortName, ImageImportDisposition disposition);
		static rkit::Result CompilePCX(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, const rkit::CIPathView &shortName, ImageImportDisposition disposition);
		static rkit::Result CompilePNG(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, rkit::png::IPngDriver &pngDriver, const rkit::CIPathView &shortName, ImageImportDisposition disposition);
		static rkit::Result CompileImage(const rkit::utils::IImage &image, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, ImageImportDisposition disposition);

		template<class TElementType, size_t TNumElements>
		static rkit::Result GenerateMipMaps(rkit::Vector<priv::TextureCompilerImage<TElementType, TNumElements>> &resultImages, const rkit::Span<priv::TextureCompilerImage<TElementType, TNumElements>> &sourceImages, size_t &outNumLevels, ImageImportDisposition disposition);

		template<class TElementType, size_t TNumElements>
		static rkit::Result Generate2DMipMapChain(const rkit::Span<priv::TextureCompilerImage<TElementType, TNumElements>> &images, ImageImportDisposition disposition);

		template<class TElementType, size_t TNumElements>
		static rkit::Result ExportDDS(const rkit::Span<priv::TextureCompilerImage<TElementType, TNumElements>> &images, size_t numLevels, size_t numLayers, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, ImageImportDisposition disposition);

		static bool DispositionHasAlpha(ImageImportDisposition disposition);
		static bool DispositionHasMipMaps(ImageImportDisposition disposition);

		rkit::png::IPngDriver &m_pngDriver;
	};
} } // anox::buildsystem::priv

namespace anox { namespace buildsystem { namespace priv
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
		, m_imageSpec{}
	{
	}

	template<class TPixelElement, size_t TElementsPerPixel>
	const rkit::utils::ImageSpec &TextureCompilerImage<TPixelElement, TElementsPerPixel>::GetImageSpec() const
	{
		return m_imageSpec;
	}

	template<class TPixelElement, size_t TElementsPerPixel>
	uint8_t TextureCompilerImage<TPixelElement, TElementsPerPixel>::GetPixelSizeBytes() const
	{
		return sizeof(TPixelElement) * TElementsPerPixel;
	}

	template<class TPixelElement, size_t TElementsPerPixel>
	const void *TextureCompilerImage<TPixelElement, TElementsPerPixel>::GetScanline(uint32_t row) const
	{
		return this->GetScanlineSpan(row).Ptr();
	}

	template<class TPixelElement, size_t TElementsPerPixel>
	void *TextureCompilerImage<TPixelElement, TElementsPerPixel>::ModifyScanline(uint32_t row)
	{
		return this->GetScanlineSpan(row).Ptr();
	}

	template<class TPixelElement, size_t TElementsPerPixel>
	rkit::Result TextureCompilerImage<TPixelElement, TElementsPerPixel>::Initialize(uint32_t width, uint32_t height, rkit::utils::PixelPacking pixelPacking)
	{
		if (m_imageSpec.m_width == 0 || m_imageSpec.m_height == 0)
		{
			rkit::log::Error("Image had 0 dimension");
			return rkit::ResultCode::kDataError;
		}

		const size_t pitch = m_imageSpec.m_width;

		size_t numElements = 0;
		RKIT_CHECK(rkit::SafeMul<size_t>(numElements, pitch, m_imageSpec.m_height));

		RKIT_CHECK(m_pixelData.Resize(numElements));

		m_pitchInElements = pitch;

		return rkit::ResultCode::kOK;
	}

	template<class TPixelElement, size_t TElementsPerPixel>
	rkit::Span<typename TextureCompilerImage<TPixelElement, TElementsPerPixel>::Pixel_t> TextureCompilerImage<TPixelElement, TElementsPerPixel>::GetScanlineSpan(uint32_t y)
	{
		return m_pixelData.ToSpan().SubSpan(y * m_pitchInElements, m_imageSpec.m_width);
	}

	template<class TPixelElement, size_t TElementsPerPixel>
	rkit::Span<const typename TextureCompilerImage<TPixelElement, TElementsPerPixel>::Pixel_t> TextureCompilerImage<TPixelElement, TElementsPerPixel>::GetScanlineSpan(uint32_t y) const
	{
		return m_pixelData.ToSpan().SubSpan(y * m_pitchInElements, m_imageSpec.m_width);
	}

	template<class TElementType, size_t TNumElements, size_t TElementIndex>
	bool UsesChannelHelperBase<TElementType, TNumElements, TElementIndex, true>::UsesChannel(const TextureCompilerImage<TElementType, TNumElements> &image)
	{
		const uint32_t width = image.GetWidth();
		const uint32_t height = image.GetHeight();

		for (uint32_t y = 0; y < height; y++)
		{
			const rkit::ConstSpan<TextureCompilerPixel<TElementType, TNumElements>> scanline = image.GetScanlineSpan(y);

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
} } } // anox::buildsystem::priv

namespace anox { namespace buildsystem
{
	TextureCompiler::TextureCompiler(rkit::png::IPngDriver &pngDriver)
		: m_pngDriver(pngDriver)
	{
	}

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
				rkit::log::ErrorFmt("Texture job '{}' was invalid", identifier.GetChars());
				return rkit::ResultCode::kInternalError;
			}

			dotPosition--;

			if (identifier[dotPosition] == '.')
				break;

			char digit = identifier[dotPosition];
			if (digit < '0' || digit > '9')
			{
				rkit::log::ErrorFmt("Texture job '{}' was invalid", identifier.GetChars());
				return rkit::ResultCode::kInternalError;
			}

			dispositionUInt = dispositionUInt * 10 + (identifier[dotPosition] - '0');

			if (dispositionUInt >= static_cast<uint32_t>(ImageImportDisposition::kCount))
			{
				rkit::log::ErrorFmt("Texture job '{}' was invalid", identifier.GetChars());
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
				rkit::log::ErrorFmt("Texture job '{}' was invalid", identifier.GetChars());
				return rkit::ResultCode::kInternalError;
			}

			dotPosition--;

			if (identifier[dotPosition] == '.')
				break;
		}

		rkit::StringSliceView extension = identifier.SubString(dotPosition, dispositionDotPos - dotPosition);

		rkit::CIPath path;
		RKIT_CHECK(path.Set(shortName));

		if (extension == ".pcx")
			return CompilePCX(depsNode, feedback, path, disposition);

		if (extension == ".png")
			return CompilePNG(depsNode, feedback, m_pngDriver, path, disposition);

		if (extension == ".tga")
			return CompileTGA(depsNode, feedback, path, disposition);

		rkit::log::ErrorFmt("Texture job '{}' used an unsupported format", identifier.GetChars());
		return rkit::ResultCode::kOperationFailed;
	}

	rkit::Result TextureCompiler::GetTGAMetadata(rkit::utils::ImageSpec &imageSpec, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, const rkit::CIPathView &shortName)
	{
		rkit::UniquePtr<rkit::ISeekableReadStream> stream;
		RKIT_CHECK(feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, shortName, stream));

		TGAHeader tgaHeader;
		RKIT_CHECK(stream->ReadAll(&tgaHeader, sizeof(tgaHeader)));

		if (tgaHeader.m_pixelSizeBits % 8 != 0)
		{
			rkit::log::Error("Unsupported pixel bits");
			return rkit::ResultCode::kDataError;
		}

		imageSpec = {};
		imageSpec.m_width = tgaHeader.m_imageWidth.Get();
		imageSpec.m_height = tgaHeader.m_imageHeight.Get();
		imageSpec.m_numChannels = tgaHeader.m_pixelSizeBits / 8;
		imageSpec.m_pixelPacking = rkit::utils::PixelPacking::kUInt8;

		return rkit::ResultCode::kOK;
	}

	rkit::Result TextureCompiler::GetPCXMetadata(rkit::utils::ImageSpec &imageSpec, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, const rkit::CIPathView &shortName)
	{
		rkit::UniquePtr<rkit::ISeekableReadStream> stream;
		RKIT_CHECK(feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, shortName, stream));

		priv::PCXHeader pcxHeader;
		RKIT_CHECK(stream->ReadAll(&pcxHeader, sizeof(pcxHeader)));

		if (pcxHeader.m_numColorPlanes != 1 && pcxHeader.m_numColorPlanes != 3 && pcxHeader.m_numColorPlanes != 4)
		{
			rkit::log::ErrorFmt("PCX file '{}' has an unsupported number of planes", shortName.GetChars());
			return rkit::ResultCode::kMalformedFile;
		}

		if (pcxHeader.m_bitsPerPlane != 8)
		{
			rkit::log::ErrorFmt("PCX file '{}' has an unsupported bits per plane", shortName.GetChars());
			return rkit::ResultCode::kMalformedFile;
		}

		if (pcxHeader.m_minX.Get() > pcxHeader.m_maxX.Get() || pcxHeader.m_minY.Get() > pcxHeader.m_maxY.Get())
		{
			rkit::log::Error("Invalid PCX dimensions");
			return rkit::ResultCode::kDataError;
		}

		imageSpec = {};
		imageSpec.m_width = pcxHeader.m_maxX.Get() - pcxHeader.m_minX.Get() + 1;
		imageSpec.m_height = pcxHeader.m_maxY.Get() - pcxHeader.m_minY.Get() + 1;
		imageSpec.m_numChannels = 3;
		imageSpec.m_pixelPacking = rkit::utils::PixelPacking::kUInt8;

		return rkit::ResultCode::kOK;
	}

	rkit::Result TextureCompiler::GetPNGMetadata(rkit::utils::ImageSpec &imageSpec, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, rkit::png::IPngDriver &pngDriver, const rkit::CIPathView &shortName)
	{
		rkit::UniquePtr<rkit::ISeekableReadStream> stream;
		RKIT_CHECK(feedback->OpenInput(rkit::buildsystem::BuildFileLocation::kSourceDir, shortName, stream));

		return pngDriver.LoadPNGMetadata(imageSpec, *stream);
	}

	rkit::Result TextureCompiler::GetTGA(rkit::UniquePtr<rkit::utils::IImage> &outImage, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, rkit::buildsystem::BuildFileLocation buildFileLocation, const rkit::CIPathView &shortName, ImageImportDisposition disposition)
	{
		rkit::UniquePtr<rkit::ISeekableReadStream> stream;
		RKIT_CHECK(feedback->OpenInput(buildFileLocation, shortName, stream));

		TGAHeader tgaHeader;
		RKIT_CHECK(stream->ReadAll(&tgaHeader, sizeof(tgaHeader)));

		RKIT_CHECK(stream->SeekCurrent(tgaHeader.m_identSize));

		if (tgaHeader.m_colorMapLength.Get() > 0)
		{
			rkit::log::Error("Color mapped TGA not supported");
			return rkit::ResultCode::kNotYetImplemented;
		}

		uint32_t width = tgaHeader.m_imageWidth.Get();
		uint32_t height = tgaHeader.m_imageHeight.Get();

		rkit::UniquePtr<priv::TextureCompilerImage<uint8_t, 4>> image;
		RKIT_CHECK((rkit::New<priv::TextureCompilerImage<uint8_t, 4>>(image)));
		RKIT_CHECK(image->Initialize(width, height, rkit::utils::PixelPacking::kUInt8));

		if (tgaHeader.m_pixelSizeBits % 8 != 0)
		{
			rkit::log::Error("TGA bit format not supported");
			return rkit::ResultCode::kNotYetImplemented;
		}

		const uint8_t pixelSizeBytes = tgaHeader.m_pixelSizeBits / 8;
		size_t decompressedSizePixels = 0;
		RKIT_CHECK(rkit::SafeMul<size_t>(decompressedSizePixels, width, height));

		size_t decompressedSizeBytes = 0;
		RKIT_CHECK(rkit::SafeMul<size_t>(decompressedSizeBytes, decompressedSizePixels, pixelSizeBytes));

		rkit::Vector<uint8_t> decompressedImageData;
		RKIT_CHECK(decompressedImageData.Resize(decompressedSizeBytes));

		{
			if (tgaHeader.m_dataType == static_cast<uint8_t>(TGADataType::kUncompressedColor))
			{
				RKIT_CHECK(stream->ReadAll(decompressedImageData.GetBuffer(), decompressedImageData.Count()));
			}
			else if (tgaHeader.m_dataType == static_cast<uint8_t>(TGADataType::kRLEColor))
			{
				size_t remainingPixelsToDecompress = decompressedSizePixels;
				rkit::Span<uint8_t> outBytes = decompressedImageData.ToSpan();

				while (remainingPixelsToDecompress > 0)
				{
					uint8_t packetHeaderAndFirstPixel[32];
					RKIT_CHECK(stream->ReadAll(packetHeaderAndFirstPixel, pixelSizeBytes + 1));

					const uint8_t additionalPixelsToOutput = (packetHeaderAndFirstPixel[0] & 0x7f);

					rkit::CopySpanNonOverlapping(outBytes.SubSpan(0, pixelSizeBytes), rkit::ConstSpan<uint8_t>(packetHeaderAndFirstPixel + 1, pixelSizeBytes));

					outBytes = outBytes.SubSpan(pixelSizeBytes);
					remainingPixelsToDecompress--;

					if (remainingPixelsToDecompress < additionalPixelsToOutput)
					{
						rkit::log::Error("Compressed data overran target buffer");
						return rkit::ResultCode::kDataError;
					}

					if (packetHeaderAndFirstPixel[0] & 0x80)
					{
						// RLE packet
						bool isRepeatByte = true;
						for (uint8_t i = 1; i < pixelSizeBytes; i++)
						{
							if (packetHeaderAndFirstPixel[1] != packetHeaderAndFirstPixel[1 + i])
							{
								isRepeatByte = false;
								break;
							}
						}

						if (isRepeatByte)
						{
							rkit::Span<uint8_t> spanToFill = outBytes.SubSpan(0, pixelSizeBytes * additionalPixelsToOutput);
							const uint8_t fillValue = packetHeaderAndFirstPixel[1];
							for (uint8_t &byteToFill : spanToFill)
								byteToFill = fillValue;
						}
						else
						{
							for (uint8_t i = 0; i < additionalPixelsToOutput; i++)
								rkit::CopySpanNonOverlapping(outBytes.SubSpan(i * pixelSizeBytes, pixelSizeBytes), rkit::ConstSpan<uint8_t>(packetHeaderAndFirstPixel + 1, pixelSizeBytes));
						}
					}
					else
					{
						// Raw packet
						rkit::Span<uint8_t> rawPacketSpan = outBytes.SubSpan(0, additionalPixelsToOutput * pixelSizeBytes);
						RKIT_CHECK(stream->ReadAll(rawPacketSpan.Ptr(), rawPacketSpan.Count()));
					}

					outBytes = outBytes.SubSpan(pixelSizeBytes * additionalPixelsToOutput);
					remainingPixelsToDecompress -= additionalPixelsToOutput;
				}
			}
		}

		for (uint32_t y = 0; y < height; y++)
		{
			const rkit::Span<priv::TextureCompilerPixel<uint8_t, 4>> outScanline = image->GetScanlineSpan(height - 1 - y);
			RKIT_ASSERT(outScanline.Count() >= width);

			const uint8_t *inScanlineBytes = decompressedImageData.GetBuffer() + static_cast<size_t>(y) * width * pixelSizeBytes;

			switch (tgaHeader.m_pixelSizeBits)
			{
			case 24:
			{
				for (size_t i = 0; i < width; i++)
				{
					const uint8_t *inBytes = inScanlineBytes + (i * 3);
					outScanline[i].Set(inBytes[0], inBytes[1], inBytes[2], 0xff);
				}
			}
			break;
			case 32:
			{
				for (size_t i = 0; i < width; i++)
				{
					const uint8_t *inBytes = inScanlineBytes + (i * 4);
					outScanline[i].Set(inBytes[0], inBytes[1], inBytes[2], inBytes[3]);
				}
			}
			break;
			default:
				rkit::log::Error("TGA bit size unsupported");
				return rkit::ResultCode::kNotYetImplemented;
			}
		}

		outImage = std::move(image);
		return rkit::ResultCode::kOK;
	}

	rkit::Result TextureCompiler::GetPCX(rkit::UniquePtr<rkit::utils::IImage> &outImage, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, rkit::buildsystem::BuildFileLocation buildFileLocation, const rkit::CIPathView &shortName, ImageImportDisposition disposition)
	{
		typedef uint8_t RGBTriplet_t[3];

		priv::PCXHeader pcxHeader = {};

		rkit::UniquePtr<rkit::ISeekableReadStream> stream;
		RKIT_CHECK(feedback->OpenInput(buildFileLocation, shortName, stream));

		RKIT_CHECK(stream->ReadAll(&pcxHeader, sizeof(pcxHeader)));

		if (pcxHeader.m_numColorPlanes != 1 && pcxHeader.m_numColorPlanes != 3 && pcxHeader.m_numColorPlanes != 4)
		{
			rkit::log::ErrorFmt("PCX file '{}' has an unsupported number of planes", shortName.ToStringView());
			return rkit::ResultCode::kMalformedFile;
		}

		if (pcxHeader.m_bitsPerPlane != 8)
		{
			rkit::log::ErrorFmt("PCX file '{}' has an unsupported bits per plane", shortName.ToStringView());
			return rkit::ResultCode::kMalformedFile;
		}

		const size_t scanLinePlanePitch = pcxHeader.m_pitch.Get();
		const size_t scanLinePitch = scanLinePlanePitch * pcxHeader.m_numColorPlanes;

		if (scanLinePitch == 0)
		{
			rkit::log::ErrorFmt("PCX file '{}' has zero size", shortName.GetChars());
			return rkit::ResultCode::kMalformedFile;
		}

		if (pcxHeader.m_minX.Get() > pcxHeader.m_maxX.Get() || pcxHeader.m_minY.Get() > pcxHeader.m_maxY.Get())
		{
			rkit::log::ErrorFmt("PCX file '{}' has invalid dimensions", shortName.GetChars());
			return rkit::ResultCode::kMalformedFile;
		}

		const uint32_t width = static_cast<uint32_t>(pcxHeader.m_maxX.Get()) - pcxHeader.m_minX.Get() + 1;
		const uint32_t height = static_cast<uint32_t>(pcxHeader.m_maxY.Get()) - pcxHeader.m_minY.Get() + 1;

		if (scanLinePlanePitch < width)
		{
			rkit::log::ErrorFmt("PCX file '{}' has invalid pitch", shortName.GetChars());
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
						rkit::log::ErrorFmt("PCX file '{}' RLE data was corrupted", shortName.GetChars());
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

		rkit::UniquePtr<priv::TextureCompilerImage<uint8_t, 4>> image;
		RKIT_CHECK((rkit::New<priv::TextureCompilerImage<uint8_t, 4>>(image)));
		RKIT_CHECK(image->Initialize(width, height, rkit::utils::PixelPacking::kUInt8));

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
					rkit::log::ErrorFmt("PCX file '{}' palette check byte was invalid", shortName.GetChars());
					return rkit::ResultCode::kMalformedFile;
				}

				static_assert(sizeof(vgaPalette) == 768, "VGA palette is the wrong size");
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
				const rkit::Span<priv::TextureCompilerPixel<uint8_t, 4>> outScanline = image->GetScanlineSpan(y);

				for (uint32_t x = 0; x < width; x++)
				{
					uint8_t elementValue = inScanline[x];
					if (elementValue > palette.Count())
					{
						rkit::log::ErrorFmt("PCX file '{}' had an out-of-range value", shortName.GetChars());
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

		outImage = std::move(image);
		return rkit::ResultCode::kOK;
	}

	rkit::Result TextureCompiler::GetPNG(rkit::UniquePtr<rkit::utils::IImage> &image, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, rkit::png::IPngDriver &pngDriver, rkit::buildsystem::BuildFileLocation buildFileLocation, const rkit::CIPathView &shortName, ImageImportDisposition disposition)
	{
		rkit::UniquePtr<rkit::ISeekableReadStream> stream;
		RKIT_CHECK(feedback->OpenInput(buildFileLocation, shortName, stream));

		return pngDriver.LoadPNG(image, *stream);
	}

	rkit::Result TextureCompiler::CompileTGA(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, const rkit::CIPathView &shortName, ImageImportDisposition disposition)
	{
		rkit::UniquePtr<rkit::utils::IImage> image;
		RKIT_CHECK(GetTGA(image, feedback, rkit::buildsystem::BuildFileLocation::kSourceDir, shortName, disposition));

		return CompileImage(*image, depsNode, feedback, disposition);
	}

	rkit::Result TextureCompiler::CompilePCX(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, const rkit::CIPathView &shortName, ImageImportDisposition disposition)
	{
		rkit::UniquePtr<rkit::utils::IImage> image;
		RKIT_CHECK(GetPCX(image, feedback, rkit::buildsystem::BuildFileLocation::kSourceDir, shortName, disposition));

		return CompileImage(*image, depsNode, feedback, disposition);
	}

	rkit::Result TextureCompiler::CompilePNG(rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, rkit::png::IPngDriver &pngDriver, const rkit::CIPathView &shortName, ImageImportDisposition disposition)
	{
		rkit::UniquePtr<rkit::utils::IImage> image;
		RKIT_CHECK(GetPNG(image, feedback, pngDriver, rkit::buildsystem::BuildFileLocation::kSourceDir, shortName, disposition));

		return CompileImage(*image, depsNode, feedback, disposition);
	}

	rkit::Result TextureCompiler::CompileImage(const rkit::utils::IImage &image, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, ImageImportDisposition disposition)
	{
		if (image.GetPixelPacking() != rkit::utils::PixelPacking::kUInt8)
			return rkit::ResultCode::kDataError;

		const uint32_t width = image.GetWidth();
		const uint32_t height = image.GetHeight();

		priv::TextureCompilerImage<uint8_t, 4> tcImage;
		RKIT_CHECK(tcImage.Initialize(image.GetWidth(), image.GetHeight(), rkit::utils::PixelPacking::kUInt8));

		for (uint32_t y = 0; y < height; y++)
		{
			rkit::Span<priv::TextureCompilerPixel<uint8_t, 4>> outScanline = tcImage.GetScanlineSpan(y);
			const void *inScanline = image.GetScanline(y);

			switch (image.GetNumChannels())
			{
			case 1:
				for (size_t x = 0; x < width; x++)
				{
					const uint8_t grayscaleValue = static_cast<const uint8_t *>(inScanline)[x];
					priv::TextureCompilerPixel<uint8_t, 4> &pixel = outScanline[x];
					pixel.Set(grayscaleValue, grayscaleValue, grayscaleValue, 255);
				}
				break;
			case 3:
				for (size_t x = 0; x < width; x++)
				{
					const uint8_t *rgbValue = static_cast<const uint8_t *>(inScanline) + (x * 3);
					priv::TextureCompilerPixel<uint8_t, 4> &pixel = outScanline[x];
					pixel.Set(rgbValue[0], rgbValue[1], rgbValue[2], 255);
				}
				break;
			case 4:
				for (size_t x = 0; x < width; x++)
				{
					const uint8_t *rgbaValue = static_cast<const uint8_t *>(inScanline) + (x * 4);
					priv::TextureCompilerPixel<uint8_t, 4> &pixel = outScanline[x];
					pixel.Set(rgbaValue[0], rgbaValue[1], rgbaValue[2], rgbaValue[3]);
				}
				break;
			default:
				return rkit::ResultCode::kInternalError;
			}
		}

		rkit::Vector<priv::TextureCompilerImage<uint8_t, 4>> images;

		size_t numLevels = 0;
		RKIT_CHECK(GenerateMipMaps(images, rkit::Span<priv::TextureCompilerImage<uint8_t, 4>>(&tcImage, 1), numLevels, disposition));

		return ExportDDS(images.ToSpan(), numLevels, 1, depsNode, feedback, disposition);
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
	rkit::Result TextureCompiler::ExportDDS(const rkit::Span<priv::TextureCompilerImage<TElementType, TNumElements>> &images, size_t numMipMaps, size_t numLayers, rkit::buildsystem::IDependencyNode *depsNode, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, ImageImportDisposition disposition)
	{
		rkit::String outName;
		RKIT_CHECK(ResolveIntermediatePath(outName, depsNode->GetIdentifier()));

		rkit::CIPath outPath;
		RKIT_CHECK(outPath.Set(outName));

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

		// Convert 24-bit to 32-bit
		if (numChannelsToSave == 3 && !isCompressed)
			numChannelsToSave = 4;

		pixelSize = static_cast<uint8_t>(numChannelsToSave);

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
			uint32_t pitch = (images[0].GetWidth() * pixelSize);
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
		RKIT_CHECK(feedback->OpenOutput(rkit::buildsystem::BuildFileLocation::kIntermediateDir, outPath, stream));

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

				const rkit::ConstSpan<priv::TextureCompilerPixel<TElementType, TNumElements>> scanline = image.GetScanlineSpan(y);

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
		case ImageImportDisposition::kWorldAlphaBlendNoMip:
		case ImageImportDisposition::kWorldAlphaTestedNoMip:
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
		//case ImageImportDisposition::kWorldAlphaBlend:
		//case ImageImportDisposition::kWorldAlphaTested:
		//	return true;
		default:
			return false;
		}
	}

	uint32_t TextureCompiler::GetVersion() const
	{
		return 3;
	}

	rkit::Result TextureCompiler::GetImageMetadataDerived(rkit::utils::ImageSpec &imageSpec, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, rkit::png::IPngDriver &pngDriver, rkit::buildsystem::BuildFileLocation buildFileLocation, const rkit::CIPathView &shortName)
	{
		const rkit::StringSliceView path = shortName.ToStringView();

		size_t extPos = path.Length();
		while (extPos > 0)
		{
			extPos--;
			if (path[extPos] == '.')
				break;
		}

		if (extPos == 0)
		{
			rkit::log::ErrorFmt("Missing file extension: {}", path);
			return rkit::ResultCode::kDataError;
		}

		const rkit::StringSliceView extension = path.SubString(extPos);

		if (extension == ".pcx")
			return GetPCXMetadata(imageSpec, feedback, shortName);

		if (extension == ".png")
			return GetPNGMetadata(imageSpec, feedback, pngDriver, shortName);

		if (extension == ".tga")
			return GetTGAMetadata(imageSpec, feedback, shortName);

		rkit::log::ErrorFmt("Unsupported file format: {}", path);
		return rkit::ResultCode::kDataError;
	}

	rkit::Result TextureCompiler::GetImageDerived(rkit::UniquePtr<rkit::utils::IImage> &image,
		rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, rkit::png::IPngDriver &pngDriver,
		rkit::buildsystem::BuildFileLocation buildFileLocation, const rkit::CIPathView &shortName, ImageImportDisposition disposition)
	{
		const rkit::StringSliceView path = shortName.ToStringView();

		size_t extPos = path.Length();
		while (extPos > 0)
		{
			extPos--;
			if (path[extPos] == '.')
				break;
		}

		if (extPos == 0)
		{
			rkit::log::ErrorFmt("Missing file extension: {}", path);
			return rkit::ResultCode::kDataError;
		}

		const rkit::StringSliceView extension = path.SubString(extPos);

		if (extension == ".pcx")
			return GetPCX(image, feedback, buildFileLocation, shortName, disposition);

		if (extension == ".png")
			return GetPNG(image, feedback, pngDriver, buildFileLocation, shortName, disposition);

		if (extension == ".tga")
			return GetTGA(image, feedback, buildFileLocation, shortName, disposition);

		rkit::log::ErrorFmt("Unsupported file format: {}", path);
		return rkit::ResultCode::kDataError;
	}


	rkit::Result TextureCompilerBase::ResolveIntermediatePath(rkit::String &outString, const rkit::StringView &identifierWithDisposition)
	{
		return outString.Format("ax_tex/{}.dds", identifierWithDisposition.GetChars());
	}

	rkit::Result TextureCompilerBase::GetImageMetadata(rkit::utils::ImageSpec &imageSpec, rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, rkit::png::IPngDriver &pngDriver, rkit::buildsystem::BuildFileLocation buildFileLocation, const rkit::CIPathView &shortName)
	{
		return TextureCompiler::GetImageMetadataDerived(imageSpec, feedback, pngDriver, buildFileLocation, shortName);
	}

	rkit::Result TextureCompilerBase::GetImage(rkit::UniquePtr<rkit::utils::IImage> &image,
		rkit::buildsystem::IDependencyNodeCompilerFeedback *feedback, rkit::png::IPngDriver &pngDriver,
		rkit::buildsystem::BuildFileLocation buildFileLocation, const rkit::CIPathView &shortName, ImageImportDisposition disposition)
	{
		return TextureCompiler::GetImageDerived(image, feedback, pngDriver, buildFileLocation, shortName, disposition);
	}

	rkit::Result TextureCompilerBase::CreateImportIdentifier(rkit::String &result, const rkit::StringView &imagePath, ImageImportDisposition disposition)
	{
		return result.Format("{}.{}", imagePath.GetChars(), static_cast<int>(disposition));
	}

	rkit::Result TextureCompilerBase::Create(rkit::UniquePtr<TextureCompilerBase> &outCompiler, rkit::png::IPngDriver &pngDriver)
	{
		return rkit::New<TextureCompiler>(outCompiler, pngDriver);
	}
} } // anox::buildsystem

