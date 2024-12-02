#pragma once

#include "Stream.h"
#include "Vector.h"

#include <cstdint>

namespace rkit
{
	struct Result;

	class BufferStream final : public ISeekableReadWriteStream
	{
	public:
		Result ReadPartial(void *data, size_t count, size_t &outCountRead) override;
		Result WritePartial(const void *data, size_t count, size_t &outCountWritten) override;
		Result Flush() override;

		Result SeekStart(FilePos_t pos) override;
		Result SeekCurrent(FileOffset_t pos) override;
		Result SeekEnd(FileOffset_t pos) override;

		FilePos_t Tell() const override;
		FilePos_t GetSize() const override;

		const Vector<uint8_t> &GetBuffer() const;
		Vector<uint8_t> TakeBuffer();

	private:
		Vector<uint8_t> m_buffer;
		size_t m_pos = 0;
	};
}

#include "Result.h"

#include <cstring>
#include <utility>

inline rkit::Result rkit::BufferStream::ReadPartial(void *data, size_t count, size_t &outCountRead)
{
	size_t bufferAvailable = m_buffer.Count() - m_pos;

	if (count > bufferAvailable)
		count = bufferAvailable;

	if (count != 0)
		memcpy(data, &m_buffer[m_pos], count);

	outCountRead = count;
	return ResultCode::kOK;
}

inline rkit::Result rkit::BufferStream::WritePartial(const void *data, size_t count, size_t &outCountWritten)
{
	if (count == 0)
	{
		outCountWritten = 0;
		return ResultCode::kOK;
	}

	size_t bufferAvailable = m_buffer.Count() - m_pos;

	if (bufferAvailable > 0)
	{
		size_t bufferToWrite = count;
		if (bufferToWrite > bufferAvailable)
			bufferToWrite = bufferAvailable;

		memcpy(&m_buffer[m_pos], data, bufferToWrite);
		m_pos += bufferToWrite;
		data = static_cast<const uint8_t *>(data) + bufferToWrite;
		count -= bufferToWrite;

		outCountWritten = bufferToWrite;
	}
	else
		outCountWritten = 0;

	if (count > 0)
	{
		RKIT_CHECK(m_buffer.Append(Span<const uint8_t>(static_cast<const uint8_t *>(data), count)));
		outCountWritten += count;
		m_pos += count;
	}

	return ResultCode::kOK;
}

inline rkit::Result rkit::BufferStream::Flush()
{
	return ResultCode::kOK;
}

inline rkit::Result rkit::BufferStream::SeekStart(FilePos_t pos)
{
	if (pos > m_buffer.Count())
		return ResultCode::kIOSeekOutOfRange;

	m_pos = static_cast<size_t>(pos);

	return ResultCode::kOK;
}

inline rkit::Result rkit::BufferStream::SeekCurrent(FileOffset_t offset)
{
	if (offset == 0)
		return ResultCode::kOK;

	if (offset < 0)
	{
		if (offset == std::numeric_limits<FileOffset_t>::min())
			return ResultCode::kIOSeekOutOfRange;

		FileOffset_t negativeOffset = -offset;
		if (static_cast<FilePos_t>(negativeOffset) > m_pos)
			return ResultCode::kIOSeekOutOfRange;

		m_pos -= static_cast<size_t>(negativeOffset);
	}
	else
	{
		// offset > 0
		size_t maxOffset = m_buffer.Count() - m_pos;
		FilePos_t uOffset = static_cast<FilePos_t>(offset);

		if (uOffset > maxOffset)
			return ResultCode::kIOSeekOutOfRange;

		m_pos += static_cast<size_t>(uOffset);
	}

	return ResultCode::kOK;
}

inline rkit::Result rkit::BufferStream::SeekEnd(FileOffset_t offset)
{
	if (offset == 0)
		return ResultCode::kOK;

	if (offset > 0)
		return ResultCode::kOK;

	if (offset == std::numeric_limits<FileOffset_t>::min())
		return ResultCode::kIOSeekOutOfRange;

	FileOffset_t negativeOffset = -offset;
	if (static_cast<FilePos_t>(negativeOffset) > m_buffer.Count())
		return ResultCode::kIOSeekOutOfRange;

	m_pos = m_buffer.Count() - static_cast<size_t>(negativeOffset);
	return ResultCode::kOK;
}

inline rkit::FilePos_t rkit::BufferStream::Tell() const
{
	return static_cast<FilePos_t>(m_pos);
}

inline rkit::FilePos_t rkit::BufferStream::GetSize() const
{
	return static_cast<FilePos_t>(m_buffer.Count());
}

inline const rkit::Vector<uint8_t> &rkit::BufferStream::GetBuffer() const
{
	return m_buffer;
}

rkit::Vector<uint8_t> rkit::BufferStream::TakeBuffer()
{
	m_pos = 0;

	Vector<uint8_t> tempVector = std::move(m_buffer);
	m_buffer = Vector<uint8_t>();

	return Vector<uint8_t>(std::move(tempVector));
}
