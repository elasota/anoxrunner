#pragma once

#include "rkit/Core/SharedPtr.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/UniquePtr.h"

namespace rkit
{
	struct ISeekableStream;
	struct IReadStream;
	struct IWriteStream;
	struct IBaseStream;
	struct IMutex;

	class MutexProtectedStreamWrapper : public IMutexProtectedReadWriteStream
	{
	public:
		MutexProtectedStreamWrapper(UniquePtr<IBaseStream> &&baseStream, UniquePtr<IMutex> &&mutex, ISeekableStream *seek, IReadStream *read, IWriteStream *write);

		Result ReadPartial(FilePos_t startPos, void *data, size_t count, size_t &outCountRead) override;
		Result WritePartial(FilePos_t startPos, const void *data, size_t count, size_t &outCountWritten) override;
		Result Flush() override;
		FilePos_t GetSize() const override;

		Result CreateReadStream(UniquePtr<ISeekableReadStream> &outStream) override;
		Result CreateWriteStream(UniquePtr<ISeekableWriteStream> &outStream) override;
		Result CreateReadWriteStream(UniquePtr<ISeekableReadWriteStream> &outStream) override;

		void SetTracker(BaseRefCountTracker *tracker);

	private:
		UniquePtr<IMutex> m_mutex;
		UniquePtr<IBaseStream> m_baseStream;
		ISeekableStream *m_seek;
		IReadStream *m_read;
		IWriteStream *m_write;
		BaseRefCountTracker *m_rcTracker;
	};

	class MutexProtectedStream : public ISeekableReadWriteStream
	{
	public:
		explicit MutexProtectedStream(SharedPtr<MutexProtectedStreamWrapper> &&streamWrapper);
		~MutexProtectedStream();

		Result ReadPartial(void *data, size_t count, size_t &outCountRead) override;
		Result WritePartial(const void *data, size_t count, size_t &outCountWritten) override;
		Result Flush() override;
		Result SeekStart(FilePos_t pos) override;
		Result SeekCurrent(FileOffset_t pos) override;
		Result SeekEnd(FileOffset_t pos) override;
		FilePos_t Tell() const override;
		FilePos_t GetSize() const override;

	private:
		SharedPtr<MutexProtectedStreamWrapper> m_baseStream;
		FilePos_t m_filePos;
	};
}
