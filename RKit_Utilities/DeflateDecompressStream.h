#pragma once

#include "rkit/Core/Optional.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/UniquePtr.h"

#include "zlib.h"

#include <stdint.h>
#include <stddef.h>

namespace rkit
{
	struct IMallocDriver;

	class DeflateDecompressStream : public ISeekableReadStream
	{
	public:
		explicit DeflateDecompressStream(UniquePtr<IReadStream> &&stream, ISeekableStream *seekable, Optional<FilePos_t> decompressedSize, IMallocDriver *alloc);
		~DeflateDecompressStream();

		Result ReadPartial(void *data, size_t count, size_t &outCountRead) override;

		Result SeekStart(FilePos_t pos) override;
		Result SeekCurrent(FileOffset_t pos) override;
		Result SeekEnd(FileOffset_t pos) override;

		FilePos_t Tell() const override;
		FilePos_t GetSize() const override;

	private:
		static voidpf AllocCallback(voidpf opaque, uInt items, uInt size);
		static void FreeCallback(voidpf opaque, voidpf address);

		alloc_func zalloc;  /* used to allocate the internal state */
		free_func  zfree;   /* used to free the internal state */
		voidpf     opaque;  /* private data object passed to zalloc and zfree */

		Result SeekRelativeTo(FilePos_t pos, FileOffset_t offset);
		Result RestartDecompression();

		static const size_t kBufferSize = 4096;

		UniquePtr<IReadStream> m_stream;
		ISeekableStream *m_seekable;

		IMallocDriver *m_alloc;
		uint8_t m_buffer[kBufferSize];
		size_t m_compressedSizeRemaining;

		z_stream m_zstream;
		bool m_streamInitialized;

		FilePos_t m_filePos;
		Optional<FilePos_t> m_decompressedSize;
	};
}
