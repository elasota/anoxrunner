#pragma once

#include "rkit/Core/Stream.h"
#include "rkit/Core/UniquePtr.h"

namespace rkit
{
	struct ISeekableStream;
	struct IReadStream;

	class RangeLimitedReadStream : public ISeekableReadStream
	{
	public:
		RangeLimitedReadStream(rkit::UniquePtr<ISeekableReadStream> &&stream, FilePos_t startPos, FilePos_t size);
		~RangeLimitedReadStream();

		Result ReadPartial(void *data, size_t count, size_t &outCountRead) override;
		Result SeekStart(FilePos_t pos) override;
		Result SeekCurrent(FileOffset_t pos) override;
		Result SeekEnd(FileOffset_t pos) override;
		FilePos_t Tell() const override;
		FilePos_t GetSize() const override;

	private:
		rkit::UniquePtr<ISeekableReadStream> m_stream;
		FilePos_t m_filePos;
		FilePos_t m_fileSize;

		FilePos_t m_baseStreamStart;
	};
}
