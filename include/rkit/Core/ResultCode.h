#pragma once

namespace rkit
{
	enum class ResultCode
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
		kTextEncodingError,

		kGraphicsAPIException,

		kFormatError,
	};
}
