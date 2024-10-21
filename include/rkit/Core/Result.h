#pragma once

#include "CoreDefs.h"

namespace rkit
{
	enum class ResultCode
	{
		kOK,

		kOutOfMemory,
		kInvalidParameter,
		kNotYetImplemented,
		kIntegerOverflow,

		kKeyNotFound,

		kModuleLoadFailed,
		kInvalidCommandLine,
		kConfigMissing,
		kConfigInvalid,

		kUnknownTool,

		kInvalidUnicode,
		kInvalidJson,

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
	};

	struct RKIT_NODISCARD Result
	{
	public:
		Result();
		Result(ResultCode resultCode);

		bool IsOK() const;
		int ToExitCode() const;
		ResultCode GetResultCode() const;

	private:
		ResultCode m_resultCode;
	};
}


#define RKIT_CHECK(expr) do {\
	::rkit::Result RKIT_PP_CONCAT(exprResult_, __LINE__) = (expr);\
	if (!RKIT_PP_CONCAT(exprResult_, __LINE__).IsOK())\
		return RKIT_PP_CONCAT(exprResult_, __LINE__);\
} while (false)


#if RKIT_IS_DEBUG
#include "Drivers.h"
#include "SystemDriver.h"
#endif

inline rkit::Result::Result()
	: m_resultCode(ResultCode::kOK)
{
}

inline rkit::Result::Result(ResultCode resultCode)
	: m_resultCode(resultCode)
{
#if RKIT_IS_DEBUG
	if (!IsOK())
	{
		ISystemDriver *sysDriver = GetDrivers().m_systemDriver;
		sysDriver->FirstChanceResultFailure(*this);
	}
#endif
}

inline bool rkit::Result::IsOK() const
{
	return m_resultCode == ResultCode::kOK;
}

inline int rkit::Result::ToExitCode() const
{
	return -static_cast<int>(m_resultCode);
}

inline rkit::ResultCode rkit::Result::GetResultCode() const
{
	return m_resultCode;
}

