#include "rkit/Png/PngDriver.h"

#include "rkit/Core/DriverModuleStub.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/ModuleGlue.h"
#include "rkit/Core/Stream.h"

#include "rkit/Utilities/Image.h"

#include "png.h"

namespace rkit { namespace png {
	struct PngCallWrapper
	{
	};

	class PngDriver final : public IPngDriver
	{
	public:

	private:
		rkit::Result InitDriver(const rkit::DriverInitParameters *) override;
		void ShutdownDriver() override;

		Result LoadPNG(ISeekableReadStream &stream, UniquePtr<utils::IImage> &outImage) const override;

		uint32_t GetDriverNamespaceID() const override { return rkit::IModuleDriver::kDefaultNamespace; }
		rkit::StringView GetDriverName() const override { return "PNG"; }
	};

	typedef rkit::CustomDriverModuleStub<PngDriver> PngModule;

	template<class TFirstArg, class... TArgs>
	class PngCallThunk
	{
	public:
		typedef void(*Func_t)(TFirstArg png, TArgs... args);

		PngCallThunk(Func_t func);

		Result operator()(TFirstArg png, TArgs... args) const;

	private:
		Func_t m_func;
	};

	template<class TReturn, class TFirstArg, class... TArgs>
	class PngCallWithReturnValueThunk
	{
	public:
		typedef TReturn(*Func_t)(TFirstArg png, TArgs... args);

		PngCallWithReturnValueThunk(Func_t func);

		Result operator()(TReturn &returnValue, TFirstArg png, TArgs... args) const;

	private:
		Func_t m_func;
	};

	class PngLoader
	{
	public:
		explicit PngLoader(ISeekableReadStream &stream);
		~PngLoader();

		Result Run(UniquePtr<utils::IImage> &outImage);

	private:
		static void PNGCBAPI StaticErrorCB(png_structp png, png_const_charp msg);
		static void PNGCBAPI StaticWarnCB(png_structp png, png_const_charp msg);
		static png_voidp PNGCBAPI StaticAllocCB(png_structp png, png_alloc_size_t sz);
		static void PNGCBAPI StaticFreeCB(png_structp png, png_voidp ptr);
		static void PNGCBAPI StaticReadCB(png_structp png, png_bytep buffer, size_t sz);

		template<class TReturn, class... TArgs>
		PngCallWithReturnValueThunk<TReturn, TArgs...> TrapPNGCall(TReturn(*pngCall)(TArgs... args));

		template<class... TArgs>
		PngCallThunk<TArgs...> TrapPNGCall(void(*pngCall)(TArgs... args));

		ISeekableReadStream &m_stream;
		png_structp m_png = nullptr;
		png_infop m_info = nullptr;
		png_infop m_endInfo = nullptr;
	};

	template<class TFirstArg, class... TArgs>
	PngCallThunk<TFirstArg, TArgs...>::PngCallThunk(Func_t func)
		: m_func(func)
	{
	}

	template<class TFirstArg, class... TArgs>
	Result PngCallThunk<TFirstArg, TArgs...>::operator()(TFirstArg png, TArgs... args) const
	{
		if (setjmp(png_jmpbuf(png)))
		{
			return ResultCode::kDataError;
		}
		else
		{
			m_func(png, args...);
			return ResultCode::kOK;
		}
	}

	template<class TReturn, class TFirstArg, class... TArgs>
	PngCallWithReturnValueThunk<TReturn, TFirstArg, TArgs...>::PngCallWithReturnValueThunk(Func_t func)
		: m_func(func)
	{
	}

	template<class TReturn, class TFirstArg, class... TArgs>
	Result PngCallWithReturnValueThunk<TReturn, TFirstArg, TArgs...>::operator()(TReturn &returnedValue, TFirstArg png, TArgs... args) const
	{
		if (setjmp(png_jmpbuf(png)))
		{
			return ResultCode::kDataError;
		}
		else
		{
			TReturn callReturnValue = m_func(png, args...);
			returnedValue = callReturnValue;
			return ResultCode::kOK;
		}
	}

	PngLoader::PngLoader(ISeekableReadStream &stream)
		: m_stream(stream)
	{
	}

	PngLoader::~PngLoader()
	{
		png_destroy_read_struct(&m_png, &m_info, &m_endInfo);
	}

	Result PngLoader::Run(UniquePtr<utils::IImage> &outImage)
	{
		m_png = png_create_read_struct_2(PNG_LIBPNG_VER_STRING, this, StaticErrorCB, StaticWarnCB, this, StaticAllocCB, StaticFreeCB);

		if (!m_png)
		{
			rkit::log::Error("Creating PNG failed");
			return ResultCode::kDataError;
		}

		m_info = png_create_info_struct(m_png);

		if (!m_png)
		{
			rkit::log::Error("Creating PNG info failed");
			return ResultCode::kDataError;
		}

		png_set_user_limits(m_png, 4096, 4096);
		png_set_read_fn(m_png, this, StaticReadCB);

		int transforms = PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND;

		RKIT_CHECK(TrapPNGCall(png_read_png)(m_png, m_info, transforms, nullptr));

		const png_bytepp rowPtrs = png_get_rows(m_png, m_info);
		const png_byte channels = png_get_channels(m_png, m_info);
		const png_byte bitDepth = png_get_bit_depth(m_png, m_info);

		if (bitDepth != 8)
		{
			rkit::log::Error("Unsupported bit depth");
			return ResultCode::kDataError;
		}

		if (channels != 3 && channels != 4 && channels != 1)
		{
			rkit::log::Error("Unsupported bit depth");
			return ResultCode::kDataError;
		}

		const size_t pngRowSize = png_get_rowbytes(m_png, m_info);

		utils::ImageSpec imageSpec;
		imageSpec.m_width = png_get_image_width(m_png, m_info);
		imageSpec.m_height = png_get_image_height(m_png, m_info);
		imageSpec.m_pixelPacking = utils::PixelPacking::kUInt8;
		imageSpec.m_numChannels = channels;

		UniquePtr<utils::IImage> image;
		RKIT_CHECK(GetDrivers().m_utilitiesDriver->CreateImage(imageSpec, image));

		const size_t copySize = static_cast<size_t>(image->GetPixelSizeBytes()) * imageSpec.m_width;

		if (copySize < pngRowSize)
			return ResultCode::kInternalError;


		for (uint32_t y = 0; y < imageSpec.m_height; y++)
		{
			const png_byte *inScanline = rowPtrs[y];
			void *outScanline = image->ModifyScanline(y);

			memcpy(outScanline, inScanline, copySize);
		}

		outImage = std::move(image);

		return ResultCode::kOK;
	}

	void PNGCBAPI PngLoader::StaticErrorCB(png_structp png, png_const_charp msg)
	{
		rkit::log::Error(msg);
		png_longjmp(png, 1);
	}

	void PNGCBAPI PngLoader::StaticWarnCB(png_structp png, png_const_charp msg)
	{
		rkit::log::Warning(msg);
	}

	png_voidp PNGCBAPI PngLoader::StaticAllocCB(png_structp png, png_alloc_size_t sz)
	{
		return GetDrivers().m_mallocDriver->Alloc(sz);
	}

	void PNGCBAPI PngLoader::StaticFreeCB(png_structp png, png_voidp ptr)
	{
		return GetDrivers().m_mallocDriver->Free(ptr);
	}

	void PNGCBAPI PngLoader::StaticReadCB(png_structp png, png_bytep buffer, size_t sz)
	{
		png_voidp ioPtr = png_get_io_ptr(png);
		Result result = static_cast<PngLoader *>(ioPtr)->m_stream.ReadAll(buffer, sz);

		if (!result.IsOK())
			png_error(png, "Read error");
	}

	template<class TReturn, class... TArgs>
	PngCallWithReturnValueThunk<TReturn, TArgs...> PngLoader::TrapPNGCall(TReturn(*pngCall)(TArgs... args))
	{
		return PngCallWithReturnValueThunk<TReturn, TArgs...>(pngCall);
	}

	template<class... TArgs>
	PngCallThunk<TArgs...> PngLoader::TrapPNGCall(void(*pngCall)(TArgs... args))
	{
		return PngCallThunk<TArgs...>(pngCall);
	}

	rkit::Result PngDriver::InitDriver(const rkit::DriverInitParameters *)
	{
		return rkit::ResultCode::kOK;
	}

	void PngDriver::ShutdownDriver()
	{
	}

	Result PngDriver::LoadPNG(ISeekableReadStream &stream, UniquePtr<utils::IImage> &outImage) const
	{
		PngLoader loader(stream);
		return loader.Run(outImage);
	}

} } // rkit::png


RKIT_IMPLEMENT_MODULE("RKit", "PNG", ::rkit::png::PngModule)
