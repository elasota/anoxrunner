#include "CoreDefs.h"
#include "Coroutine.h"

#include "Result.h"

namespace rkit { namespace coro { namespace priv
{
	template<class TClass, class TMethodPtrType, TMethodPtrType TMethodPtr, class... TParams>
	struct MethodCaller
	{
		static rkit::Result Call(TClass &instance, TParams... params);
	};

	template<class TCaller, class TClass, class... TParams>
	struct MethodWrapperThunk final
	{
		static const FrameMetadata<typename SignatureAnalyzer<void(TParams...)>::Parameters_t> ms_metadata;

		static void EnterFunction(Context &context, void *stackFrameRoot, StackFrameBase *prevFrame, void *classInstance, TParams... params);
	};

} } } // rkit::coro::priv

namespace rkit { namespace coro
{
	template<class TClass, class TSignature>
	struct MethodWrapper
	{
	};

	template<class TClass, class... TParams>
	struct MethodWrapper<TClass, void(TParams...)>
	{
		explicit MethodWrapper(TClass *instance);

		template<rkit::Result(TClass:: *TMethodPtr)(TParams...)>
		MethodStarter<void(TParams...)> Create() const;

	private:
		TClass *m_instance;
	};

} } // rkit::coro

namespace rkit { namespace coro
{
	template<class TClass, class... TParams>
	MethodWrapper<TClass, void(TParams...)>::MethodWrapper(TClass *instance)
		: m_instance(instance)
	{
	}

	template<class TClass, class... TParams>
	template<rkit::Result(TClass:: *TMethodPtr)(TParams...)>
	MethodStarter<void(TParams...)> MethodWrapper<TClass, void(TParams...)>::Create() const
	{
		typedef typename SignatureAnalyzer<void(TParams...)>::Parameters_t Parameters_t;
		typedef rkit::Result(TClass:: *MethodPtrType_t)(TParams...) ;
		typedef priv::MethodCaller<TClass, MethodPtrType_t, TMethodPtr, TParams...> Caller_t;
		typedef priv::MethodWrapperThunk<Caller_t, TClass, TParams...> Thunk_t;

		return MethodStarter<void(TParams...)>::CreateCustom(m_instance, &Thunk_t::ms_metadata);
	}
} } // rkit::coro

namespace rkit { namespace coro { namespace priv {
	template<class TCaller, class TClass, class... TParams>
	const FrameMetadata<typename SignatureAnalyzer<void(TParams...)>::Parameters_t> MethodWrapperThunk<TCaller, TClass, TParams...>::ms_metadata =
	{
		{
			0,
			1,
		},
		MethodWrapperThunk<TCaller, TClass, TParams...>::EnterFunction
	};


	template<class TClass, class TMethodPtrType, TMethodPtrType TMethodPtr, class... TParams>
	rkit::Result MethodCaller<TClass, TMethodPtrType, TMethodPtr, TParams...>::Call(TClass &instance, TParams... params)
	{
		return (instance.*TMethodPtr)(std::forward<TParams>(params)...);
	}

	template<class TCaller, class TClass, class... TParams>
	void MethodWrapperThunk<TCaller, TClass, TParams...>::EnterFunction(Context &context, void *stackFrameRoot, StackFrameBase *prevFrame, void *classInstance, TParams... params)
	{
		rkit::Result result = TCaller::Call(*static_cast<TClass *>(classInstance), std::forward<TParams>(params)...);

		if (!result.IsOK())
		{
			context.m_disposition = Disposition::kFailResult;
			context.m_result = result;
		}

		context.m_freeStack(context.m_userdata, stackFrameRoot, prevFrame);
	}
} } } // rkit::coro::priv
