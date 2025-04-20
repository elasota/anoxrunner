#pragma once

#include <cstdint>

namespace rkit
{
#if RKIT_USE_CLASS_RESULT
	enum class ResultCode : uint32_t
#else
	enum class Result : uint64_t
#endif
	{
		kOK,

		kOutOfMemory,
		kInvalidParameter,
		kNotYetImplemented,
		kInternalError,

		kDataError,

		kIntegerOverflow,

		kKeyNotFound,

		kModuleLoadFailed,
		kInvalidCommandLine,
		kConfigMissing,
		kConfigInvalid,

		kUnknownTool,

		kInvalidUnicode,
		kInvalidJson,
		kInvalidCString,
		kInvalidPath,

		kEndOfStream,
		kIOReadError,
		kIOWriteError,
		kIOSeekOutOfRange,
		kIOError,
		kFileOpenError,

		kDecompressionFailed,
		kMalformedFile,

		kOperationFailed,
		kTextParsingFailed,

		kGraphicsAPIException,

		kFormatError,

		kCoroStackOverflow,
	};

#if !RKIT_USE_CLASS_RESULT
	typedef Result ResultCode;
#endif
}
