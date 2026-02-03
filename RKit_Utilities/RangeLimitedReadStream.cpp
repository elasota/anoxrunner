#include "RangeLimitedReadStream.h"

#include "rkit/Core/Algorithm.h"

#include <utility>

namespace rkit
{
	RangeLimitedReadStream::RangeLimitedReadStream(rkit::UniquePtr<ISeekableReadStream> &&stream, FilePos_t startPos, FilePos_t length)
		: m_stream(std::move(stream))
		, m_filePos(0)
		, m_fileSize(length)
		, m_baseStreamStart(startPos)
	{
	}

	RangeLimitedReadStream::~RangeLimitedReadStream()
	{
	}

	Result RangeLimitedReadStream::ReadPartial(void *data, size_t count, size_t &outCountRead)
	{
		size_t countRead = 0;
		RKIT_CHECK(SeekStart(m_filePos));
		RKIT_CHECK(m_stream->ReadPartial(data, count, countRead));

		outCountRead = countRead;
		m_filePos += static_cast<FilePos_t>(countRead);

		RKIT_RETURN_OK;
	}

	Result RangeLimitedReadStream::SeekStart(FilePos_t pos)
	{
		if (pos > m_fileSize)
			return ResultCode::kIOSeekOutOfRange;

		RKIT_CHECK(m_stream->SeekStart(pos + m_baseStreamStart));

		m_filePos = pos;

		RKIT_RETURN_OK;
	}

	Result RangeLimitedReadStream::SeekCurrent(FileOffset_t offset)
	{
		if (offset < 0)
		{
			FilePos_t backwardDist = rkit::ToUnsignedAbs<FileOffset_t, FilePos_t>(offset);
			if (backwardDist > m_filePos)
				return ResultCode::kIOSeekOutOfRange;

			return SeekStart(m_filePos - backwardDist);
		}
		else
		{
			FilePos_t distRemaining = m_fileSize - m_filePos;

			if (static_cast<FilePos_t>(offset) > distRemaining)
				return ResultCode::kIOSeekOutOfRange;

			return SeekStart(m_filePos - static_cast<FilePos_t>(offset));
		}
	}

	Result RangeLimitedReadStream::SeekEnd(FileOffset_t offset)
	{
		if (offset <= 0)
		{
			FilePos_t backwardDist = rkit::ToUnsignedAbs<FileOffset_t, FilePos_t>(offset);
			if (backwardDist > m_fileSize)
				return ResultCode::kIOSeekOutOfRange;

			return SeekStart(m_filePos - backwardDist);
		}
		else
			return ResultCode::kIOSeekOutOfRange;
	}

	FilePos_t RangeLimitedReadStream::Tell() const
	{
		return m_filePos;
	}

	FilePos_t RangeLimitedReadStream::GetSize() const
	{
		return m_fileSize;
	}
}
