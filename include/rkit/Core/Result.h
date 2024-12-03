#pragma once

#include "CoreDefs.h"
#include "ResultCode.h"

namespace rkit
{
	struct RKIT_NODISCARD Result
	{
	public:
		Result();
		Result(ResultCode resultCode);

		bool IsOK() const;
		int ToExitCode() const;
		ResultCode GetResultCode() const;

	private:
#if RKIT_IS_DEBUG
		void FirstChanceResultFailure() const;
#endif

		ResultCode m_resultCode;
	};
}


#define RKIT_CHECK(expr) do {\
	::rkit::Result RKIT_PP_CONCAT(exprResult_, __LINE__) = (expr);\
	if (!RKIT_PP_CONCAT(exprResult_, __LINE__).IsOK())\
		return RKIT_PP_CONCAT(exprResult_, __LINE__);\
} while (false)

inline rkit::Result::Result()
	: m_resultCode(ResultCode::kOK)
{
}

inline rkit::Result::Result(ResultCode resultCode)
	: m_resultCode(resultCode)
{
#if RKIT_IS_DEBUG
	if (!this->IsOK())
		this->FirstChanceResultFailure();
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

