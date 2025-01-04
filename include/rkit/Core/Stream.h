#pragma once

#include "StreamProtos.h"

namespace rkit
{
	struct Result;

	template<class T>
	class SharedPtr;

	template<class T>
	class UniquePtr;

	struct IBaseStream
	{
		virtual ~IBaseStream() {}
	};

	struct IReadStream : public virtual IBaseStream
	{
		virtual ~IReadStream() {}

		virtual Result ReadPartial(void *data, size_t count, size_t &outCountRead) = 0;
		Result ReadAll(void *data, size_t count);
	};

	struct IWriteStream : public virtual IBaseStream
	{
		virtual ~IWriteStream() {}

		virtual Result WritePartial(const void *data, size_t count, size_t &outCountWritten) = 0;
		Result WriteAll(const void *data, size_t count);

		virtual Result Flush() = 0;
	};

	struct ISeekableStream : public virtual IBaseStream
	{
		virtual ~ISeekableStream() {}

		virtual Result SeekStart(FilePos_t pos) = 0;
		virtual Result SeekCurrent(FileOffset_t pos) = 0;
		virtual Result SeekEnd(FileOffset_t pos) = 0;

		virtual FilePos_t Tell() const = 0;
		virtual FilePos_t GetSize() const = 0;
	};

	struct IReadWriteStream : public virtual IReadStream, public virtual IWriteStream
	{
	};

	struct ISeekableReadStream : public virtual IReadStream, public virtual ISeekableStream
	{
	};

	struct ISeekableWriteStream : public virtual IWriteStream, public virtual ISeekableStream
	{
		virtual Result Truncate(FilePos_t size) = 0;
	};

	struct ISeekableReadWriteStream : public virtual ISeekableReadStream, public virtual ISeekableWriteStream, public virtual IReadWriteStream
	{
	};

	struct IMutexProtectedStream
	{
		virtual ~IMutexProtectedStream() {}

		virtual FilePos_t GetSize() const = 0;
	};

	struct IMutexProtectedReadStream : public virtual IMutexProtectedStream
	{
		virtual Result CreateReadStream(UniquePtr<ISeekableReadStream> &outStream) = 0;
		virtual Result ReadPartial(FilePos_t startPos, void *data, size_t count, size_t &outCountRead) = 0;
	};

	struct IMutexProtectedWriteStream : public virtual IMutexProtectedStream
	{
		virtual Result CreateWriteStream(UniquePtr<ISeekableWriteStream> &outStream) = 0;
		virtual Result WritePartial(FilePos_t startPos, const void *data, size_t count, size_t &outCountWritten) = 0;
		virtual Result Flush() = 0;
	};

	struct IMutexProtectedReadWriteStream : public virtual IMutexProtectedReadStream, public virtual IMutexProtectedWriteStream
	{
		virtual Result CreateReadWriteStream(UniquePtr<ISeekableReadWriteStream> &outStream) = 0;
	};
}

#include "Result.h"

inline rkit::Result rkit::IReadStream::ReadAll(void *data, size_t count)
{
	size_t countRead = 0;
	RKIT_CHECK(this->ReadPartial(data, count, countRead));

	if (countRead != count)
		return ResultCode::kIOReadError;

	return ResultCode::kOK;
}

inline rkit::Result rkit::IWriteStream::WriteAll(const void *data, size_t count)
{
	size_t countWritten = 0;
	RKIT_CHECK(this->WritePartial(data, count, countWritten));

	if (countWritten != count)
		return ResultCode::kIOReadError;

	return ResultCode::kOK;
}
