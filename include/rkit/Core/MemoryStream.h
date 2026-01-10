#pragma once

#include "Stream.h"

#include <cstddef>

namespace rkit
{
	template<class T>
	class Span;

	class FixedSizeMemoryStream final : public ISeekableReadWriteStream
	{
	public:
		explicit FixedSizeMemoryStream(void *data, size_t size);

		Result ReadPartial(void *data, size_t count, size_t &outCountRead) override;
		Result WritePartial(const void *data, size_t count, size_t &outCountWritten) override;
		Result Flush() override;

		Result SeekStart(FilePos_t pos) override;
		Result SeekCurrent(FileOffset_t pos) override;
		Result SeekEnd(FileOffset_t pos) override;

		Result Truncate(FilePos_t newSize) override;

		FilePos_t Tell() const override;
		FilePos_t GetSize() const override;

		template<class T>
		Result ExtractSpan(Span<T> &outSpan, size_t count);

	private:
		char *m_bytes;
		size_t m_pos;
		size_t m_size;
	};

	class ReadOnlyMemoryStream final : public ISeekableReadStream
	{
	public:
		explicit ReadOnlyMemoryStream(const Span<const uint8_t> &data);
		explicit ReadOnlyMemoryStream(const void *data, size_t size);

		Result ReadPartial(void *data, size_t count, size_t &outCountRead) override;

		Result SeekStart(FilePos_t pos) override;
		Result SeekCurrent(FileOffset_t pos) override;
		Result SeekEnd(FileOffset_t pos) override;

		FilePos_t Tell() const override;
		FilePos_t GetSize() const override;

	private:
		FixedSizeMemoryStream m_stream;
	};
}

#include "Span.h"
#include "Result.h"

#include <cstring>
#include <limits>

inline rkit::FixedSizeMemoryStream::FixedSizeMemoryStream(void *data, size_t size)
	: m_bytes(static_cast<char *>(data))
	, m_pos(0)
	, m_size(size)
{
}

inline rkit::Result rkit::FixedSizeMemoryStream::ReadPartial(void *data, size_t count, size_t &outCountRead)
{
	size_t amountAvailable = m_size - m_pos;

	if (amountAvailable < count)
	{
		memcpy(data, m_bytes + m_pos, amountAvailable);
		outCountRead = amountAvailable;
		m_pos = m_size;

		return ResultCode::kEndOfStream;
	}
	else
	{
		memcpy(data, m_bytes + m_pos, count);
		m_pos += count;
		outCountRead = count;

		return ResultCode::kOK;
	}
}

inline rkit::Result rkit::FixedSizeMemoryStream::WritePartial(const void *data, size_t count, size_t &outCountWritten)
{
	size_t amountAvailable = m_size - m_pos;

	if (amountAvailable < count)
	{
		memcpy(m_bytes + m_pos, data, amountAvailable);
		outCountWritten = amountAvailable;
		m_pos = m_size;

		return ResultCode::kIOWriteError;
	}
	else
	{
		memcpy(m_bytes + m_pos, data, count);
		m_pos += count;
		outCountWritten = count;

		return ResultCode::kOK;
	}
}

inline rkit::Result rkit::FixedSizeMemoryStream::Flush()
{
	return ResultCode::kOK;
}

inline rkit::Result rkit::FixedSizeMemoryStream::SeekStart(FilePos_t pos)
{
	if (pos > m_size)
		return ResultCode::kIOSeekOutOfRange;

	m_pos = static_cast<size_t>(pos);

	return ResultCode::kOK;
}

inline rkit::Result rkit::FixedSizeMemoryStream::SeekCurrent(FileOffset_t pos)
{
	const FileOffset_t minOffset = -static_cast<FileOffset_t>(m_pos);
	const FileOffset_t maxOffset = static_cast<FileOffset_t>(m_size - m_pos);

	if (pos < minOffset || pos > maxOffset)
		return ResultCode::kIOSeekOutOfRange;

	m_pos = static_cast<size_t>(static_cast<FileOffset_t>(m_pos) + pos);

	return ResultCode::kOK;
}

inline rkit::Result rkit::FixedSizeMemoryStream::SeekEnd(FileOffset_t pos)
{
	const FileOffset_t minOffset = -static_cast<FileOffset_t>(m_size);

	if (pos < minOffset || pos > 0)
		return ResultCode::kIOSeekOutOfRange;

	m_pos = static_cast<size_t>(static_cast<FileOffset_t>(m_size) + pos);

	return ResultCode::kOK;
}

inline rkit::Result rkit::FixedSizeMemoryStream::Truncate(FilePos_t newSize)
{
	if (newSize == m_size)
		return ResultCode::kOK;
	else
		return ResultCode::kOperationFailed;
}

inline rkit::FilePos_t rkit::FixedSizeMemoryStream::Tell() const
{
	return m_pos;
}

inline rkit::FilePos_t rkit::FixedSizeMemoryStream::GetSize() const
{
	return m_size;
}

template<class T>
rkit::Result rkit::FixedSizeMemoryStream::ExtractSpan(Span<T> &outSpan, size_t count)
{
	ResultCode result = ResultCode::kOK;

	const size_t maxCount = (m_size - m_pos) / sizeof(T);

	if (maxCount < count)
	{
		count = maxCount;
		result = ResultCode::kEndOfStream;
	}

	outSpan = Span<T>(reinterpret_cast<T *>(m_bytes + m_pos), count);

	m_pos += count * sizeof(T);

	return result;
}

inline rkit::ReadOnlyMemoryStream::ReadOnlyMemoryStream(const Span<const uint8_t> &data)
	: m_stream(const_cast<uint8_t *>(data.Ptr()), data.Count())
{
}

inline rkit::ReadOnlyMemoryStream::ReadOnlyMemoryStream(const void *data, size_t size)
	: m_stream(const_cast<void *>(data), size)
{
}

inline rkit::Result rkit::ReadOnlyMemoryStream::ReadPartial(void *data, size_t count, size_t &outCountRead)
{
	return m_stream.ReadPartial(data, count, outCountRead);
}

inline rkit::Result rkit::ReadOnlyMemoryStream::SeekStart(FilePos_t pos)
{
	return m_stream.SeekStart(pos);
}

inline rkit::Result rkit::ReadOnlyMemoryStream::SeekCurrent(FileOffset_t pos)
{
	return m_stream.SeekCurrent(pos);
}

inline rkit::Result rkit::ReadOnlyMemoryStream::SeekEnd(FileOffset_t pos)
{
	return m_stream.SeekEnd(pos);
}

inline rkit::FilePos_t rkit::ReadOnlyMemoryStream::Tell() const
{
	return m_stream.Tell();
}

inline rkit::FilePos_t rkit::ReadOnlyMemoryStream::GetSize() const
{
	return m_stream.GetSize();
}
