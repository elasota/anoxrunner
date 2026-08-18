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

#if RKIT_RESULT_BEHAVIOR == RKIT_RESULT_BEHAVIOR_EXCEPTION

namespace rkit::priv
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
}

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

namespace rkit {
	namespace priv {

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
	}
}

#define RKIT_RETURN_OK return
#define RKIT_THROW(expr) throw (::rkit::ResultException(expr))
#define RKIT_TRY_CATCH_RETHROW(expr, eh) (::rkit::priv::TryCatchRethrow([&] { static_cast<void>(expr); }, (eh)))
#define RKIT_TRY_FINALLY_RETHROW(expr, eh) (::rkit::priv::TryFinallyRethrow([&] { static_cast<void>(expr); }, (eh)))
#define RKIT_TRY_CATCH_FINALLY_RETHROW(expr, catchContext, finallyContext) (::rkit::priv::TryCatchFinallyRethrow([&] { static_cast<void>(expr); }, (eh), (eh)))
#define RKIT_TRY_EVAL(expr) (::rkit::priv::TryCatch([&] { static_cast<void>(expr); }))

#elif RKIT_RESULT_BEHAVIOR == RKIT_RESULT_BEHAVIOR_FATAL

namespace rkit::priv
{
	template<class TTryBody>
	PackedResultAndExtCode Try(const TTryBody &tryBody)
	{
		tryBody();
		return utils::PackResult(ResultCode::kOK);
	}

	template<class TTryBody>
	void TryFinally(const TTryBody &tryBody, const ::rkit::FinallyContext &finallyContext)
	{
		tryBody();
		finallyContext.Invoke();
	}

	[[noreturn]] void RaiseFatalError(PackedResultAndExtCode resultAndExtCode);

	[[noreturn]] inline void RaiseFatalError(ResultCode resultCode)
	{
		RaiseFatalError(::rkit::utils::PackResult(resultCode));
	}
}


#define RKIT_RETURN_OK return
#define RKIT_THROW(expr) (::rkit::priv::RaiseFatalError(expr))
#define RKIT_TRY_CATCH_RETHROW(expr, eh) (::rkit::priv::Try([&] { (expr); }))
#define RKIT_TRY_FINALLY_RETHROW(expr, eh) (::rkit::priv::TryFinally([&] { (expr); }))
#define RKIT_TRY_CATCH_FINALLY_RETHROW(expr, catchContext, finallyContext) (::rkit::priv::TryFinally([&] { (expr); }, ('rkit::utils::PackResult': no overloaded function could convert all the argument types)))
#define RKIT_TRY_EVAL(expr) (::rkit::priv::Try([&] { (expr); }))

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
