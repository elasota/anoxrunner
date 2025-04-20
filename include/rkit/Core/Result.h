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
		struct ResultAndExtCode
		{
			ResultCode m_resultCode;
			uint32_t m_extCode;
		};

		union Union
		{
			ResultAndExtCode m_re;
			uint64_t m_u64;
		};

		Union m_u;
	};
}


#define RKIT_CHECK(expr) do {\
	::rkit::Result RKIT_PP_CONCAT(exprResult_, __LINE__) = (expr);\
	if (!RKIT_PP_CONCAT(exprResult_, __LINE__).IsOK())\
		return RKIT_PP_CONCAT(exprResult_, __LINE__);\
} while (false)

inline rkit::Result::Result()
	: m_u { { ResultCode::kOK, 0 } }
{
}

inline rkit::Result::Result(ResultCode resultCode)
	: Result(resultCode, static_cast<uint32_t>(0))
{
}

inline rkit::Result::Result(ResultCode resultCode, uint32_t extCode)
	: m_u{ { resultCode, extCode } }
{
#if RKIT_IS_DEBUG
	if (!this->IsOK())
		this->FirstChanceResultFailure();
#endif
}

inline rkit::Result::Result(ResultCode resultCode, const SoftFaultTag &)
	: m_u{ { resultCode, static_cast<uint32_t>(0) } }
{
}

inline bool rkit::Result::IsOK() const
{
	return m_u.m_u64 == 0;
}

inline int rkit::Result::ToExitCode() const
{
	return -static_cast<int>(m_u.m_re.m_resultCode);
}

inline rkit::ResultCode rkit::Result::GetResultCode() const
{
	return m_u.m_re.m_resultCode;
}


inline uint32_t rkit::Result::GetExtendedCode() const
{
	return m_u.m_re.m_extCode;
}

inline rkit::Result rkit::Result::SoftFault(rkit::ResultCode resultCode)
{
	return Result(resultCode, SoftFaultTag());
}
