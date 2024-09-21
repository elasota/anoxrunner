#pragma once

#include <cstdint>

namespace rkit
{
	struct Result;

	typedef uint64_t FilePos_t;
	typedef int64_t FileOffset_t;

	struct IReadStream
	{
		virtual ~IReadStream() {}

		virtual Result Read(void *data, size_t count, size_t &outCountRead) = 0;
	};

	struct IWriteStream
	{
		virtual ~IWriteStream() {}

		virtual Result Write(const void *data, size_t count, size_t &outCountWritten) = 0;
	};

	struct ISeekableStream
	{
		virtual ~ISeekableStream() {}

		virtual Result SeekStart(FilePos_t pos) = 0;
		virtual Result SeekCurrent(FileOffset_t pos) = 0;
		virtual Result SeekEnd(FileOffset_t pos) = 0;
	};

	struct IReadWriteStream : public virtual IReadStream, public virtual IWriteStream
	{
	};

	struct ISeekableReadStream : public virtual IReadStream, public virtual ISeekableStream
	{
	};

	struct ISeekableWriteStream : public virtual IWriteStream, public virtual ISeekableStream
	{
	};

	struct ISeekableReadWriteStream : public virtual ISeekableReadStream, public virtual ISeekableWriteStream, public virtual IReadWriteStream
	{
	};
}
