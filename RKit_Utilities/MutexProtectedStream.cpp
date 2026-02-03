#include "MutexProtectedStream.h"

#include "rkit/Core/Algorithm.h"
#include "rkit/Core/Mutex.h"
#include "rkit/Core/MutexLock.h"

#include <utility>

namespace rkit
{
	MutexProtectedStreamWrapper::MutexProtectedStreamWrapper(UniquePtr<IBaseStream> &&baseStream, UniquePtr<IMutex> &&mutex, ISeekableStream *seek, IReadStream *read, IWriteStream *write, ISeekableWriteStream *seekableWrite)
		: m_baseStream(std::move(baseStream))
		, m_seek(seek)
		, m_read(read)
		, m_write(write)
		, m_seekableWrite(seekableWrite)
		, m_rcTracker(nullptr)
		, m_mutex(std::move(mutex))
	{
	}

	Result MutexProtectedStreamWrapper::ReadPartial(FilePos_t startPos, void *data, size_t count, size_t &outCountRead)
	{
		MutexLock lock(*m_mutex);

		RKIT_CHECK(m_seek->SeekStart(startPos));
		return m_read->ReadPartial(data, count, outCountRead);
	}

	Result MutexProtectedStreamWrapper::WritePartial(FilePos_t startPos, const void *data, size_t count, size_t &outCountWritten)
	{
		MutexLock lock(*m_mutex);

		RKIT_CHECK(m_seek->SeekStart(startPos));
		return m_write->WritePartial(data, count, outCountWritten);
	}

	Result MutexProtectedStreamWrapper::Flush()
	{
		MutexLock lock(*m_mutex);

		return m_write->Flush();
	}

	FilePos_t MutexProtectedStreamWrapper::GetSize() const
	{
		MutexLock lock(*m_mutex);

		return m_seek->GetSize();
	}

	Result MutexProtectedStreamWrapper::Truncate(FilePos_t newSize)
	{
		MutexLock lock(*m_mutex);

		return m_seekableWrite->Truncate(newSize);
	}

	Result MutexProtectedStreamWrapper::CreateReadStream(UniquePtr<ISeekableReadStream> &outStream)
	{
		return New<MutexProtectedStream>(outStream, SharedPtr<MutexProtectedStreamWrapper>(this, m_rcTracker));
	}

	Result MutexProtectedStreamWrapper::CreateWriteStream(UniquePtr<ISeekableWriteStream> &outStream)
	{
		return New<MutexProtectedStream>(outStream, SharedPtr<MutexProtectedStreamWrapper>(this, m_rcTracker));
	}

	Result MutexProtectedStreamWrapper::CreateReadWriteStream(UniquePtr<ISeekableReadWriteStream> &outStream)
	{
		return New<MutexProtectedStream>(outStream, SharedPtr<MutexProtectedStreamWrapper>(this, m_rcTracker));
	}

	void MutexProtectedStreamWrapper::SetTracker(BaseRefCountTracker *tracker)
	{
		m_rcTracker = tracker;
	}

	MutexProtectedStream::MutexProtectedStream(SharedPtr<MutexProtectedStreamWrapper> &&streamWrapper)
		: m_filePos(0)
		, m_baseStream(std::move(streamWrapper))
	{
	}

	MutexProtectedStream::~MutexProtectedStream()
	{
	}

	Result MutexProtectedStream::ReadPartial(void *data, size_t count, size_t &outCountRead)
	{
		size_t countRead = 0;
		RKIT_CHECK(m_baseStream->ReadPartial(m_filePos, data, count, countRead));

		outCountRead = countRead;
		m_filePos += static_cast<FilePos_t>(countRead);

		RKIT_RETURN_OK;
	}

	Result MutexProtectedStream::WritePartial(const void *data, size_t count, size_t &outCountWritten)
	{
		size_t countWritten = 0;
		RKIT_CHECK(m_baseStream->WritePartial(m_filePos, data, count, countWritten));

		outCountWritten = countWritten;
		m_filePos += static_cast<FilePos_t>(countWritten);

		RKIT_RETURN_OK;
	}

	Result MutexProtectedStream::Flush()
	{
		return m_baseStream->Flush();
	}

	Result MutexProtectedStream::SeekStart(FilePos_t pos)
	{
		FilePos_t fileSize = m_baseStream->GetSize();

		if (pos > fileSize)
			RKIT_THROW(ResultCode::kIOSeekOutOfRange);

		m_filePos = pos;

		RKIT_RETURN_OK;
	}

	Result MutexProtectedStream::SeekCurrent(FileOffset_t offset)
	{
		if (offset < 0)
		{
			FilePos_t backwardDist = rkit::ToUnsignedAbs<FileOffset_t, FilePos_t>(offset);
			if (backwardDist > m_filePos)
				RKIT_THROW(ResultCode::kIOSeekOutOfRange);

			m_filePos -= backwardDist;
		}
		else
		{
			FilePos_t fileSize = m_baseStream->GetSize();

			FilePos_t distRemaining = fileSize - m_filePos;

			if (static_cast<FilePos_t>(offset) > distRemaining)
				RKIT_THROW(ResultCode::kIOSeekOutOfRange);

			m_filePos += static_cast<FilePos_t>(offset);
		}

		RKIT_RETURN_OK;
	}

	Result MutexProtectedStream::SeekEnd(FileOffset_t offset)
	{
		if (offset <= 0)
		{
			FilePos_t fileSize = m_baseStream->GetSize();

			FilePos_t backwardDist = rkit::ToUnsignedAbs<FileOffset_t, FilePos_t>(offset);
			if (backwardDist > fileSize)
				RKIT_THROW(ResultCode::kIOSeekOutOfRange);

			m_filePos = fileSize - backwardDist;

			RKIT_RETURN_OK;
		}
		else
			RKIT_THROW(ResultCode::kIOSeekOutOfRange);
	}

	FilePos_t MutexProtectedStream::Tell() const
	{
		return m_filePos;
	}

	FilePos_t MutexProtectedStream::GetSize() const
	{
		return m_baseStream->GetSize();
	}

	Result MutexProtectedStream::Truncate(FilePos_t newSize)
	{
		return m_baseStream->Truncate(newSize);
	}
}
