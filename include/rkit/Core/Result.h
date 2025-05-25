#pragma once

#include "CoreDefs.h"
#include "ResultCode.h"

#include <cstdint>

#if RKIT_USE_CLASS_RESULT

namespace rkit
{
	enum class ResultCode : uint32_t;

	struct RKIT_NODISCARD Result
	{
	public:
		Result(ResultCode resultCode);
		explicit Result(ResultCode resultCode, uint32_t extCode);

		bool IsOK() const;
		int ToExitCode() const;
		ResultCode GetResultCode() const;
		uint32_t GetExtendedCode() const;

		static Result SoftFault(ResultCode resultCode);
		Result ConvertToHardFault() const;

	private:
		Result();
		struct SoftFaultTag {};

		Result(ResultCode resultCode, const SoftFaultTag &);

#if RKIT_IS_DEBUG
		void FirstChanceResultFailure() const;
#endif

		ResultCode m_resultCode;
		uint32_t m_extCode;
		bool m_softFault;
	};
}

inline rkit::Result::Result()
	: m_resultCode(ResultCode::kOK)
	, m_extCode(0)
	, m_softFault(false)
{
}

inline rkit::Result::Result(ResultCode resultCode)
	: Result(resultCode, static_cast<uint32_t>(0))
{
}

inline rkit::Result::Result(ResultCode resultCode, uint32_t extCode)
	: m_resultCode(resultCode)
	, m_extCode(extCode)
	, m_softFault(false)
{
#if RKIT_IS_DEBUG
	if (!this->IsOK())
		this->FirstChanceResultFailure();
#endif
}

inline rkit::Result::Result(ResultCode resultCode, const SoftFaultTag &)
	: m_resultCode(resultCode)
	, m_extCode(0)
	, m_softFault(true)
{
}

inline bool rkit::Result::IsOK() const
{
	return m_resultCode == ResultCode::kOK && m_extCode == 0;
}

inline int rkit::Result::ToExitCode() const
{
	return -static_cast<int>(m_resultCode);
}

inline rkit::ResultCode rkit::Result::GetResultCode() const
{
	return m_resultCode;
}

inline uint32_t rkit::Result::GetExtendedCode() const
{
	return m_extCode;
}

inline rkit::Result rkit::Result::SoftFault(rkit::ResultCode resultCode)
{
	return Result(resultCode, SoftFaultTag());
}

inline rkit::Result rkit::Result::ConvertToHardFault() const
{
	if (m_softFault)
		return Result(m_resultCode, m_extCode);
	else
		return (*this);
}


namespace rkit
{
	namespace utils
	{
		inline Result CreateResultWithExtCode(ResultCode resultCode, uint32_t extCode)
		{
			return Result(resultCode, extCode);
		}

		inline bool ResultIsOK(const Result &result)
		{
			return result.IsOK();
		}

		inline ResultCode GetResultCode(const Result &result)
		{
			return result.GetResultCode();
		}

		inline int ResultToExitCode(const Result &result)
		{
			return result.ToExitCode();
		}

		inline Result SoftFaultResult(ResultCode resultCode)
		{
			return Result::SoftFault(resultCode);
		}
	}
}

#define RKIT_CHECK(expr) do {\
	::rkit::Result RKIT_PP_CONCAT(exprResult_, __LINE__) = (expr);\
	if (!RKIT_PP_CONCAT(exprResult_, __LINE__).IsOK())\
		return RKIT_PP_CONCAT(exprResult_, __LINE__).ConvertToHardFault();\
} while (false)

#define RKIT_CHECK_SOFT(expr) do {\
	::rkit::Result RKIT_PP_CONCAT(exprResult_, __LINE__) = (expr);\
	if (!RKIT_PP_CONCAT(exprResult_, __LINE__).IsOK())\
		return RKIT_PP_CONCAT(exprResult_, __LINE__);\
} while (false)

#else

namespace rkit
{
	enum class RKIT_NODISCARD Result : uint64_t;

	namespace utils
	{
		inline Result CreateResultWithExtCode(Result result, uint32_t extCode)
		{
			return static_cast<Result>((static_cast<uint64_t>(extCode) << 32) | static_cast<uint64_t>(result));
		}

		inline bool ResultIsOK(Result result)
		{
			return static_cast<uint64_t>(result) == 0;
		}

		inline ResultCode GetResultCode(Result result)
		{
			return static_cast<ResultCode>(static_cast<uint32_t>(result) & 0xffffffffu);
		}

		inline int ResultToExitCode(Result result)
		{
			return -static_cast<int>(GetResultCode(result));
		}

		inline Result SoftFaultResult(Result result)
		{
			return result;
		}
	}
}

#define RKIT_CHECK(expr) do {\
	::rkit::Result RKIT_PP_CONCAT(exprResult_, __LINE__) = (expr);\
	if (static_cast<uint64_t>(RKIT_PP_CONCAT(exprResult_, __LINE__)) != 0)\
		return RKIT_PP_CONCAT(exprResult_, __LINE__);\
} while (false)

#endif
