#include "rkit/Core/CoreDefs.h"

#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Vector.h"

#include "Image.h"

namespace rkit { namespace utils {
	template<class TElement>
	class Image final : public ImageBase
	{
	public:
		Image(const ImageSpec &spec);

		Result Initialize() override;

		const ImageSpec &GetImageSpec() const override;
		uint8_t GetPixelSizeBytes() const override;

		const void *GetScanline(uint32_t row) const override;
		void *ModifyScanline(uint32_t row) override;

	private:
		ImageSpec m_spec;
		Vector<TElement> m_image;
		size_t m_pitchInElements;
	};


	template<class TElement>
	Image<TElement>::Image(const ImageSpec &spec)
		: m_spec(spec)
		, m_pitchInElements(0)
	{
	}

	template<class TElement>
	Result Image<TElement>::Initialize()
	{
		if (m_spec.m_numChannels == 0)
			return ResultCode::kInvalidParameter;

		size_t pitchInBytes = sizeof(TElement) * m_spec.m_numChannels;
		RKIT_CHECK(SafeMul<size_t>(pitchInBytes, pitchInBytes, m_spec.m_width));

		RKIT_CHECK(SafeAdd<size_t>(pitchInBytes, pitchInBytes, RKIT_SIMD_ALIGNMENT - 1));
		pitchInBytes -= pitchInBytes % RKIT_SIMD_ALIGNMENT;

		m_pitchInElements = pitchInBytes / sizeof(TElement);

		size_t bufferSize = m_pitchInElements;
		RKIT_CHECK(SafeMul<size_t>(bufferSize, m_pitchInElements, m_spec.m_height));

		RKIT_CHECK(m_image.Resize(bufferSize));

		memset(m_image.GetBuffer(), 0, sizeof(TElement) * bufferSize);

		return ResultCode::kOK;
	}

	template<class TElement>
	const ImageSpec &Image<TElement>::GetImageSpec() const
	{
		return m_spec;
	}

	template<class TElement>
	uint8_t Image<TElement>::GetPixelSizeBytes() const
	{
		return sizeof(TElement) * m_spec.m_numChannels;
	}

	template<class TElement>
	const void *Image<TElement>::GetScanline(uint32_t row) const
	{
		return const_cast<Image<TElement> *>(this)->ModifyScanline(row);
	}

	template<class TElement>
	void *Image<TElement>::ModifyScanline(uint32_t row)
	{
		RKIT_ASSERT(row < m_spec.m_height);
		return m_image.GetBuffer() + m_pitchInElements * row;
	}

	Result ImageBase::Create(const ImageSpec &spec, UniquePtr<ImageBase> &outImage)
	{
		UniquePtr<ImageBase> image;

		switch (spec.m_pixelPacking)
		{
		case PixelPacking::kUInt8:
			RKIT_CHECK(New<Image<uint8_t>>(image, spec));
			break;

		default:
			return ResultCode::kInvalidParameter;
		}
		RKIT_CHECK(image->Initialize());

		outImage = std::move(image);

		return ResultCode::kOK;
	}
} } // rkit::utils
