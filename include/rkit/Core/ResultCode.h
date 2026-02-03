#pragma once

#include "CoreDefs.h"

#include <cstdint>

namespace rkit
{
#if !RKIT_USE_ENUM_RESULT
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
		kCppException,

		kDataError,

		kIntegerOverflow,
		kDivisionByZero,

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

		kJobAborted,
	};

#if RKIT_USE_ENUM_RESULT != 0
	typedef Result ResultCode;
#endif
}
