#pragma once

#include "CoreDefs.h"
#include "StreamProtos.h"

#include <cstddef>

namespace rkit
{
	template<class T>
	class UniquePtr;

	struct IJobQueue;

	typedef void (*AsyncIOCompletionCallback_t)(void *userdata, const Result &result, size_t bytesProcessed);


	// NOTE: Async requests may return immediately
	struct IAsyncReadRequester
	{
		virtual ~IAsyncReadRequester() {}

		virtual void PostReadRequest(IJobQueue &jobQueue, void *readBuffer, FilePos_t pos, size_t amount, void *completionUserData, AsyncIOCompletionCallback_t completionCallback) = 0;
	};

	struct IAsyncWriteRequester
	{
		virtual ~IAsyncWriteRequester() {}

		virtual void PostWriteRequest(IJobQueue &jobQueue, const void *writeBuffer, FilePos_t pos, size_t amount, void *completionUserData, AsyncIOCompletionCallback_t completionCallback) = 0;
		virtual void PostAppendRequest(IJobQueue &jobQueue, const void *writeBuffer, size_t amount, void *completionUserData, AsyncIOCompletionCallback_t completionCallback) = 0;
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

	template<class TFileType>
	struct AsyncFileOpenResult
	{
		UniquePtr<TFileType> m_file;
		FilePos_t m_initialSize = 0;
	};
}
