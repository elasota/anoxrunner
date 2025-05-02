#pragma once

#include "CoreDefs.h"
#include "StreamProtos.h"

#include <cstddef>

namespace rkit
{
	template<class T>
	class UniquePtr;

	// NOTE: Async requests may return immediately
	struct IAsyncReadRequester
	{
		virtual ~IAsyncReadRequester() {}

		typedef void (*ReadSucceedCallback_t)(void *userdata, uint32_t bytesRead);
		typedef void (*ReadFailCallback_t)(void *userdata, uint32_t osErrorCode, uint32_t bytesRead);

		virtual Result PostReadRequest(void *readBuffer, FilePos_t pos, size_t amount, void *completionUserData, ReadSucceedCallback_t succeedCallback, ReadFailCallback_t failCallback) = 0;
	};

	struct IAsyncWriteRequester
	{
		virtual ~IAsyncReadRequester() {}

		typedef void (*WriteSucceedCallback_t)(void *userdata, uint32_t bytesWritten);
		typedef void (*WriteFailCallback_t)(void *userdata, uint32_t osErrorCode, uint32_t bytesWritten);

		virtual Result PostWriteRequest(const void *writeBuffer, FilePos_t pos, size_t amount, void *completionUserData, WriteSucceedCallback_t succeedCallback, WriteFailCallback_t failCallback) = 0;
		virtual Result PostAppendRequest(const void *writeBuffer, size_t amount, void *completionUserData, WriteSucceedCallback_t succeedCallback, WriteFailCallback_t failCallback) = 0;
	};

	struct IAsyncReadFile
	{
		virtual ~IAsyncReadFile() {}

		virtual Result CreateReadRequester(UniquePtr<IAsyncReadRequester> &requester) = 0;
	};

	struct IAsyncWriteFile
	{
		virtual ~IAsyncWriteFile() {}

		virtual Result CreateWriteRequester(UniquePtr<IAsyncWriteRequester> &requester) = 0;
	};

	struct IAsyncReadWriteFile : public IAsyncWriteFile, public IAsyncReadFile
	{
	};
}
