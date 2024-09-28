#include "DeflateDecompressStream.h"

#include <limits>

namespace rkit
{
	DeflateDecompressStream::DeflateDecompressStream(UniquePtr<IReadStream> &&stream, IMallocDriver *alloc)
		: m_stream(std::move(stream))
		, m_alloc(alloc)
		, m_streamInitialized(false)
	{
	}

	DeflateDecompressStream::~DeflateDecompressStream()
	{
		if (m_streamInitialized)
			inflateEnd(&m_zstream);
	}


	Result DeflateDecompressStream::ReadPartial(void *data, size_t count, size_t &outCountRead)
	{
		Bytef *initialOut = static_cast<Bytef *>(data);

		outCountRead = 0;

		if (!m_streamInitialized)
		{
			m_zstream.zalloc = AllocCallback;
			m_zstream.zfree = FreeCallback;
			m_zstream.opaque = m_alloc;

			m_zstream.avail_in = 0;
			m_zstream.next_in = m_buffer;

			if (inflateInit2(&m_zstream, 15) != Z_OK)
				return ResultCode::kOutOfMemory;

			m_streamInitialized = true;
		}

		m_zstream.next_out = initialOut;

		for (;;)
		{
			const uInt outSizeLimit = 64 * 1024;

			Bytef *lastOutPtr = m_zstream.next_out;
			ptrdiff_t amountWritten = lastOutPtr - static_cast<Bytef *>(data);
			size_t availOut = count - static_cast<size_t>(amountWritten);

			if (availOut == 0)
			{
				outCountRead = count;
				return ResultCode::kOK;
			}

			if (availOut > outSizeLimit)
				m_zstream.avail_out = outSizeLimit;
			else
				m_zstream.avail_out = static_cast<uInt>(availOut);

			int inflateResult = inflate(&m_zstream, Z_NO_FLUSH);
			if (inflateResult < 0 && inflateResult != Z_BUF_ERROR)
				return ResultCode::kDecompressionFailed;

			size_t numDecompressedBytes = static_cast<size_t>(m_zstream.next_out - initialOut);
			if (numDecompressedBytes == count)
			{
				outCountRead = count;
				return ResultCode::kOK;
			}

			if (inflateResult == Z_OK)
				continue;

			if (inflateResult == Z_STREAM_END || inflateResult == Z_BUF_ERROR)
			{
				size_t amountCompressedRead = 0;
				RKIT_CHECK(m_stream->ReadPartial(m_buffer, kBufferSize, amountCompressedRead));

				if (amountCompressedRead == 0)
				{
					outCountRead = static_cast<size_t>(m_zstream.next_out - initialOut);
					return ResultCode::kOK;
				}

				m_zstream.next_in = m_buffer;
				m_zstream.avail_in = static_cast<uInt>(amountCompressedRead);
			}
		}
	}

	voidpf DeflateDecompressStream::AllocCallback(voidpf opaque, uInt items, uInt size)
	{
		if (size == 0 || items == 0)
			return Z_NULL;

		if (std::numeric_limits<size_t>::max() / size < items)
			return Z_NULL;

		return static_cast<IMallocDriver *>(opaque)->Alloc(static_cast<size_t>(items) * static_cast<size_t>(size));
	}

	void DeflateDecompressStream::FreeCallback(voidpf opaque, voidpf address)
	{
		if (address)
			static_cast<IMallocDriver *>(opaque)->Free(address);
	}
}
