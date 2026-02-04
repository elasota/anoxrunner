#pragma once

#include "CoreDefs.h"
#include "ResultCode.h"

#include <cstdint>

namespace rkit { namespace priv
{
	enum class EHBodyType
	{
		kCatch,
		kFinally,
	};

	template<EHBodyType TBodyType>
	class RKIT_NODISCARD EHBodyContext
	{
	public:
		EHBodyContext() = delete;
		EHBodyContext(const EHBodyContext &) = default;

		template<class TEHHandlerBody>
		explicit EHBodyContext(const TEHHandlerBody &handlerBody);

		void Invoke() const;

	private:
		EHBodyContext &operator=(const EHBodyContext &) = delete;

		template<class TEHHandlerBody>
		static void CallInvoke(const void *handlerBodyPtr);

		const void *m_handlerBody;
		void (*m_invokeThunk)(const void *);
	};
} }

namespace rkit
{
	enum class PackedResultAndExtCode : uint64_t;

	typedef priv::EHBodyContext<priv::EHBodyType::kCatch> CatchContext;
	typedef priv::EHBodyContext<priv::EHBodyType::kFinally> FinallyContext;
}


namespace rkit { namespace utils
{
	inline ResultCode UnpackResultCode(PackedResultAndExtCode packedResult);
	inline uint32_t UnpackExtCode(PackedResultAndExtCode packedResult);
	inline PackedResultAndExtCode PackResult(ResultCode resultCode);
	inline PackedResultAndExtCode PackResult(ResultCode resultCode, uint32_t extCode);
	inline bool ResultIsOK(PackedResultAndExtCode packedCode);
	inline bool ResultIsOK(ResultCode resultCode);
} } // rkit::utils

#if RKIT_RESULT_BEHAVIOR == RKIT_RESULT_BEHAVIOR_CLASS

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

#elif RKIT_RESULT_BEHAVIOR == RKIT_RESULT_BEHAVIOR_ENUM

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

#define RKIT_CHECK_SOFT(expr) RKIT_CHECK(expr)

#elif RKIT_RESULT_BEHAVIOR == RKIT_RESULT_BEHAVIOR_EXCEPTION

namespace rkit { namespace priv
{
	template<class TTryBody>
	void TryCatchRethrow(const TTryBody &tryBody, const ::rkit::CatchContext &catchContext)
	{
		try
		{
			tryBody();
		}
		catch (...)
		{
			catchContext.Invoke();
			throw;
		}
	}

	template<class TTryBody>
	void TryFinallyRethrow(const TTryBody &tryBody, const ::rkit::FinallyContext &finallyContext)
	{
		try
		{
			tryBody();
		}
		catch (...)
		{
			finallyContext.Invoke();
			throw;
		}

		finallyContext.Invoke();
	}

	template<class TTryBody>
	void TryCatchFinallyRethrow(const TTryBody &tryBody, const ::rkit::CatchContext &catchContext, const ::rkit::FinallyContext &finallyContext)
	{
		try
		{
			tryBody();
		}
		catch (...)
		{
			catchContext.Invoke();
			finallyContext.Invoke();
			throw;
		}

		finallyContext.Invoke();
	}
} }

namespace rkit
{
	class ResultException
	{
	public:
		ResultException() = delete;
		explicit ResultException(ResultCode resultCode);
		explicit ResultException(PackedResultAndExtCode packedResult);
		ResultException(const ResultException &) = default;

		ResultException &operator=(const ResultException &) = default;

		PackedResultAndExtCode GetPackedResult() const;

	private:
		PackedResultAndExtCode m_packedResult;
	};

	inline ResultException::ResultException(ResultCode resultCode)
		: m_packedResult(utils::PackResult(resultCode))
	{
	}

	inline ResultException::ResultException(PackedResultAndExtCode packedResult)
		: m_packedResult(packedResult)
	{
	}

	inline PackedResultAndExtCode ResultException::GetPackedResult() const
	{
		return m_packedResult;
	}
}

namespace rkit { namespace priv {

	template<class TTryBody>
	PackedResultAndExtCode TryCatch(const TTryBody &tryBody)
	{
		try
		{
			tryBody();
			return utils::PackResult(ResultCode::kOK);
		}
		catch (ResultException rex)
		{
			return rex.GetPackedResult();
		}
		catch (...)
		{
			return utils::PackResult(ResultCode::kCppException);
		}
	}
} }

#define RKIT_CHECK(expr) expr
#define RKIT_RETURN_OK return
#define RKIT_THROW(expr) throw (::rkit::ResultException(expr))
#define RKIT_TRY_CATCH_RETHROW(expr, eh) (::rkit::priv::TryCatchRethrow([&] { static_cast<void>(expr); }, (eh)))
#define RKIT_TRY_FINALLY_RETHROW(expr, eh) (::rkit::priv::TryFinallyRethrow([&] { static_cast<void>(expr); }, (eh)))
#define RKIT_TRY_CATCH_FINALLY_RETHROW(expr, catchContext, finallyContext) (::rkit::priv::TryCatchFinallyRethrow([&] { static_cast<void>(expr); }, (eh), (eh)))
#define RKIT_TRY_EVAL(expr) (::rkit::priv::TryCatch([&] { static_cast<void>(expr); }))

#endif

#include <limits>


namespace rkit { namespace utils
{
	inline ResultCode UnpackResultCode(PackedResultAndExtCode packedResult)
	{
		return static_cast<ResultCode>(static_cast<uint64_t>(packedResult) & 0xffffffffu);
	}

	inline uint32_t UnpackExtCode(PackedResultAndExtCode packedResult)
	{
		return static_cast<uint32_t>((static_cast<uint64_t>(packedResult) >> 32) & 0xffffffffu);
	}

	inline PackedResultAndExtCode PackResult(ResultCode resultCode)
	{
		return static_cast<PackedResultAndExtCode>(static_cast<uint64_t>(resultCode));
	}

	inline PackedResultAndExtCode PackResult(ResultCode resultCode, uint32_t extCode)
	{
		const uint64_t extCodeBits = static_cast<uint64_t>(extCode) << 32;
		const uint64_t resultCodeBits = static_cast<uint64_t>(resultCode);
		return static_cast<PackedResultAndExtCode>(extCodeBits | resultCodeBits);
	}

	inline bool ResultIsOK(PackedResultAndExtCode packedCode)
	{
		return static_cast<uint64_t>(packedCode) == static_cast<uint64_t>(ResultCode::kOK);
	}

	inline bool ResultIsOK(ResultCode resultCode)
	{
		return resultCode == ResultCode::kOK;
	}

	inline int ResultToExitCode(PackedResultAndExtCode result)
	{
		return -static_cast<int>(static_cast<uint64_t>(result) & static_cast<uint64_t>(std::numeric_limits<int>::max()));
	}
} } // rkit::utils

namespace rkit { namespace priv
{
	template<EHBodyType TBodyType>
	template<class TCatchBody>
	EHBodyContext<TBodyType>::EHBodyContext(const TCatchBody &catchBody)
		: m_handlerBody(&catchBody)
		, m_invokeThunk(CatchContext::CallInvoke<TCatchBody>)
	{
	}

	template<EHBodyType TBodyType>
	inline void EHBodyContext<TBodyType>::Invoke() const
	{
		m_invokeThunk(m_handlerBody);
	}

	template<EHBodyType TBodyType>
	template<class TEHHandlerBody>
	void EHBodyContext<TBodyType>::CallInvoke(const void *bodyPtr)
	{
		const TEHHandlerBody &handlerBody = *static_cast<const TEHHandlerBody *>(bodyPtr);
		handlerBody();
	}
} }

namespace rkit
{
	inline Result ThrowIfError(PackedResultAndExtCode result)
	{
		if (utils::ResultIsOK(result))
			RKIT_RETURN_OK;

		RKIT_THROW(result);
	}
}
