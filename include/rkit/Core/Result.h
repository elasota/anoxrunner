#pragma once

#include "CoreDefs.h"
#include "ResultCode.h"

#include <cstdint>

namespace rkit
{
	struct RKIT_NODISCARD Result
	{
	public:

		Result();
		Result(ResultCode resultCode);
		explicit Result(ResultCode resultCode, uint32_t extCode);

		bool IsOK() const;
		int ToExitCode() const;
		ResultCode GetResultCode() const;
		uint32_t GetExtendedCode() const;

		static Result SoftFault(ResultCode resultCode);

	private:
		struct SoftFaultTag {};

		Result(ResultCode resultCode, const SoftFaultTag &);

#if RKIT_IS_DEBUG
		void FirstChanceResultFailure() const;
#endif

		ResultCode m_resultCode;
		uint32_t m_extCode;
	};
}


#define RKIT_CHECK(expr) do {\
	::rkit::Result RKIT_PP_CONCAT(exprResult_, __LINE__) = (expr);\
	if (!RKIT_PP_CONCAT(exprResult_, __LINE__).IsOK())\
		return RKIT_PP_CONCAT(exprResult_, __LINE__);\
} while (false)

inline rkit::Result::Result()
	: m_resultCode(ResultCode::kOK)
	, m_extCode(0)
{
}

inline rkit::Result::Result(ResultCode resultCode)
	: Result(resultCode, static_cast<uint32_t>(0))
{
}

inline rkit::Result::Result(ResultCode resultCode, uint32_t extCode)
	: m_resultCode(resultCode)
	, m_extCode(extCode)
{
#if RKIT_IS_DEBUG
	if (!this->IsOK())
		this->FirstChanceResultFailure();
#endif
}

inline rkit::Result::Result(ResultCode resultCode, const SoftFaultTag &)
	: m_resultCode(resultCode)
	, m_extCode(0)
{
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


inline uint32_t rkit::Result::GetExtendedCode() const
{
	return m_extCode;
}

inline rkit::Result rkit::Result::SoftFault(rkit::ResultCode resultCode)
{
	return Result(resultCode, SoftFaultTag());
}
