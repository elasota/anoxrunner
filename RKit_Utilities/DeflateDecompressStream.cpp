#include "DeflateDecompressStream.h"

#include <limits>

namespace rkit
{
	DeflateDecompressStream::DeflateDecompressStream(UniquePtr<IReadStream> &&stream, ISeekableStream *seekable, Optional<FilePos_t> decompressedSize, IMallocDriver *alloc)
		: m_stream(std::move(stream))
		, m_seekable(seekable)
		, m_alloc(alloc)
		, m_streamInitialized(false)
		, m_filePos(0)
		, m_decompressedSize(decompressedSize)
		, m_compressedSizeRemaining(0)
		, m_zstream{}
		, m_buffer{}
	{
	}

	DeflateDecompressStream::~DeflateDecompressStream()
	{
		if (m_streamInitialized)
			inflateEnd(&m_zstream);
	}

	Result DeflateDecompressStream::ReadPartial(void *data, size_t count, size_t &outCountRead)
	{
		if (m_decompressedSize.IsSet())
		{
			const FilePos_t maxRead = m_decompressedSize.Get() - m_filePos;
			if (count > maxRead)
				count = static_cast<size_t>(maxRead);
		}

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
				m_filePos += outCountRead;
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
				m_filePos += outCountRead;
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
					m_filePos += outCountRead;
					return ResultCode::kOK;
				}

				m_zstream.next_in = m_buffer;
				m_zstream.avail_in = static_cast<uInt>(amountCompressedRead);
			}
		}
	}

	Result DeflateDecompressStream::SeekStart(FilePos_t pos)
	{
		if (pos < m_filePos)
		{
			RKIT_CHECK(RestartDecompression());
		}

		if (pos > m_filePos)
		{
			uint8_t scrap[2048];

			rkit::FilePos_t remaining = pos - m_filePos;
			while (remaining > 0)
			{
				rkit::FilePos_t thisChunkSize = remaining;
				if (thisChunkSize > sizeof(scrap))
					thisChunkSize = sizeof(scrap);

				RKIT_CHECK(ReadAll(scrap, static_cast<size_t>(thisChunkSize)));

				remaining -= thisChunkSize;
			}

			RKIT_ASSERT(Tell() == pos);
		}

		return ResultCode::kOK;
	}

	Result DeflateDecompressStream::SeekCurrent(FileOffset_t offset)
	{
		return SeekRelativeTo(m_filePos, offset);
	}

	Result DeflateDecompressStream::SeekEnd(FileOffset_t offset)
	{
		return SeekRelativeTo(m_decompressedSize.Get(), offset);
	}

	Result DeflateDecompressStream::SeekRelativeTo(FilePos_t pos, FileOffset_t offset)
	{
		if (offset < 0)
		{
			const FilePos_t negativeOffset = static_cast<FilePos_t>(-(offset + 1)) + 1;
			if (negativeOffset > pos)
				return ResultCode::kIOSeekOutOfRange;

			return SeekStart(pos - negativeOffset);
		}
		else if (offset > 0)
		{
			const FilePos_t unsignedOffset = static_cast<FilePos_t>(offset);
			const FilePos_t maxOffset = m_decompressedSize.Get() - pos;
			if (unsignedOffset > maxOffset)
				return ResultCode::kIOSeekOutOfRange;

			return SeekStart(pos + unsignedOffset);
		}

		return ResultCode::kOK;
	}

	Result DeflateDecompressStream::RestartDecompression()
	{
		if (m_streamInitialized)
		{
			RKIT_CHECK(m_seekable->SeekStart(0));

			inflateEnd(&m_zstream);
			m_streamInitialized = false;

			m_filePos = 0;
		}

		return ResultCode::kOK;
	}

	FilePos_t DeflateDecompressStream::Tell() const
	{
		return m_filePos;
	}

	FilePos_t DeflateDecompressStream::GetSize() const
	{
		return m_decompressedSize.Get();
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
