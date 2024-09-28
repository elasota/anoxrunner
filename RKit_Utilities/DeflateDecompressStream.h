#pragma once

#include "rkit/Core/Stream.h"
#include "rkit/Core/UniquePtr.h"

#include "zlib.h"

#include <stdint.h>
#include <stddef.h>

namespace rkit
{
	struct IMallocDriver;

	class DeflateDecompressStream : public IReadStream
	{
	public:
		explicit DeflateDecompressStream(UniquePtr<IReadStream> &&stream, IMallocDriver *alloc);
		~DeflateDecompressStream();

		Result ReadPartial(void *data, size_t count, size_t &outCountRead) override;

	private:
		static voidpf AllocCallback(voidpf opaque, uInt items, uInt size);
		static void FreeCallback(voidpf opaque, voidpf address);

		alloc_func zalloc;  /* used to allocate the internal state */
		free_func  zfree;   /* used to free the internal state */
		voidpf     opaque;  /* private data object passed to zalloc and zfree */

		static const size_t kBufferSize = 4096;

		UniquePtr<IReadStream> m_stream;

		IMallocDriver *m_alloc;
		uint8_t m_buffer[kBufferSize];
		size_t m_compressedSizeRemaining;

		z_stream m_zstream;
		bool m_streamInitialized;
	};
}
