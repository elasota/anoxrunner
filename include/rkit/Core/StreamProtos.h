#pragma once

#include <cstdint>

namespace rkit
{
	typedef uint64_t FilePos_t;
	typedef int64_t FileOffset_t;

	struct IAsyncReadFile;
	struct IAsyncWriteFile;
	struct IAsyncReadWriteFile;

	template<class TFileType>
	struct AsyncFileOpenResult;

	typedef AsyncFileOpenResult<IAsyncReadFile> AsyncFileOpenReadResult;
	typedef AsyncFileOpenResult<IAsyncWriteFile> AsyncFileOpenWriteResult;
	typedef AsyncFileOpenResult<IAsyncReadWriteFile> AsyncFileOpenReadWriteResult;
}
