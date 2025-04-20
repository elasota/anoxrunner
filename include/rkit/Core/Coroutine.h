#pragma once

#include "TypeList.h"
#include "TypeTraits.h"

namespace rkit::coro
{
	enum class Disposition
	{
		kResume,

		kStackOverflow,
	};

	enum class InstructionType
	{
		kCode,

		kIf,
		kElseIf,
		kElse,
		kEndIf,

		kWhile,
		kEndWhile,

		kFor,
		kEndFor,

		kBreak,
		kContinue,
		kReturn,

		kExit,
	};


	struct CodePtr;
	struct Context;
	struct StackFrameBase;

	typedef CodePtr(*Code_t) (Context *coroContext, StackFrameBase *coroStackFrame);

	struct CodePtr
	{
		Code_t m_code;
	};

	struct StackFrameBase
	{
		Code_t m_ip;
		StackFrameBase *m_prevFrame;
		void (*m_destructFrame)(StackFrameBase *stackFrame);
	};

	struct FrameMetadataBase;

	struct Context
	{
		typedef void *(*PushStackCallback_t)(void *userdata, const FrameMetadataBase &metadata);
		typedef void (*FreeStackCallback_t)(void *userdata, void *frame, void *prevFrame);

		StackFrameBase *m_frame = nullptr;
		Disposition m_disposition = Disposition::kResume;

		void *m_userdata = nullptr;
		PushStackCallback_t m_allocStack = nullptr;
		FreeStackCallback_t m_freeStack = nullptr;
	};

	struct FrameMetadataBase
	{
		size_t m_frameSize;
		size_t m_frameAlign;
	};

	template<class TParamList>
	struct EnterFunctionCallbackResolver
	{
	};

	template<>
	struct EnterFunctionCallbackResolver<void>
	{
		typedef void (*Func_t)(void *stackFrameRoot, StackFrameBase *prevFrame, void *classInstance);
	};

	template<class... TParams>
	struct EnterFunctionCallbackResolver<TypeList<TParams...>>
	{
		typedef void (*Func_t)(void *stackFrameRoot, StackFrameBase *prevFrame, void *classInstance, TParams... args);
	};

	template<class TParamList>
	struct FrameMetadata
	{
		typedef typename EnterFunctionCallbackResolver<TParamList>::Func_t EnterFunctionCallback_t;

		FrameMetadataBase m_base;
		EnterFunctionCallback_t m_enterFunction;
	};

	template<class TSignature>
	struct SignatureAnalyzer
	{
	};

	template<class... TParameters>
	struct SignatureAnalyzer<void(TParameters...)>
	{
		typedef TypeList<TParameters...> Parameters_t;
	};

	template<>
	struct SignatureAnalyzer<void()>
	{
		typedef void Parameters_t;
	};

	template<class TParamList>
	struct CoroParamList
	{
	};

	template<class... TParams>
	struct CoroParamList<TypeList<TParams...>>
	{
		template<size_t TIndex>
		using CoroParam = typename TypeListElement<TIndex, TypeList<TParams...>>::Resolution_t;
	};

	struct EmptyStruct
	{
	};

	template<class TClass, class TSignature>
	struct MethodCoroutine : public CoroParamList<typename SignatureAnalyzer<TSignature>::Parameters_t>
	{
		typedef TClass ClassInstance;
		typedef typename SignatureAnalyzer<TSignature>::Parameters_t ParameterList;
	};

	template<class TSignature>
	struct FunctionCoroutine : public CoroParamList<typename SignatureAnalyzer<TSignature>::Parameters_t>
	{
		typedef EmptyStruct ClassInstance;
		typedef typename SignatureAnalyzer<TSignature>::Parameters_t ParameterList;
	};

	template<class TCoroutine>
	struct CoroMetadataResolver
	{
		static const FrameMetadata<typename TCoroutine::ParameterList> ms_metadata;
	};

	template<class TSignature>
	class MethodStarter
	{
	public:
		template<class TClass, class TCoroutine>
		explicit MethodStarter(TClass &instance, const TCoroutine &coroutine)
			: m_instance(&instance)
			, m_metadata(CoroMetadataResolver<TCoroutine>::ms_metadata)
		{
			this->CheckType<TClass>(coroutine);
		}

		const FrameMetadata<typename SignatureAnalyzer<TSignature>::Parameters_t> &GetMetadata() const
		{
			return m_metadata;
		}

		void *GetInstance() const
		{
			return m_instance;
		}

	private:
		template<class TClass>
		inline void CheckType(const MethodCoroutine<TClass, TSignature> &coroutine) const
		{
		}

		void *m_instance;
		const FrameMetadata<typename SignatureAnalyzer<TSignature>::Parameters_t> &m_metadata;
	};

	template<class TSignature>
	class FunctionStarter
	{
	public:
		template<class TCoroutine>
		explicit FunctionStarter(const TCoroutine &coroutine)
			: m_metadata(CoroMetadataResolver<TCoroutine>::ms_metadata)
		{
			this->CheckType(coroutine);
		}

		const FrameMetadata<typename SignatureAnalyzer<TSignature>::Parameters_t> &GetMetadata() const
		{
			return m_metadata;
		}

		std::nullptr_t GetInstance() const
		{
			return nullptr;
		}

	private:
		inline void CheckType(const FunctionCoroutine<TSignature> &coroutine) const
		{
		}

		const FrameMetadata<typename SignatureAnalyzer<TSignature>::Parameters_t> &m_metadata;
	};
}

#include "Result.h"

#include <utility>

namespace rkit::coro
{
	enum class ThreadState
	{
		kInactive,
		kSuspended,
		kFaulted,
	};

	class Thread
	{
	public:
		virtual ~Thread() {}

		template<class TCoroStarter>
		Result EnterFunction(const TCoroStarter &starter);

		template<class TCoroStarter, class... TArgs>
		Result EnterFunction(const TCoroStarter &starter, TArgs... args);

		virtual ThreadState GetState() const = 0;

		virtual Result Resume() = 0;

	protected:
		virtual Context &GetContext() = 0;
	};

	template<class TCoroStarter, class... TArgs>
	Result Thread::EnterFunction(const TCoroStarter &starter, TArgs... args)
	{
		Context &context = this->GetContext();

		StackFrameBase *prevFrame = context.m_frame;
		void *newFrame = context.m_allocStack(context.m_userdata, starter.GetMetadata().m_base);
		if (!newFrame)
			return ResultCode::kCoroStackOverflow;

		starter.GetMetadata().m_enterFunction(newFrame, prevFrame, starter.GetInstance(), std::forward<TArgs>(args)...);
		context.m_frame = static_cast<StackFrameBase *>(newFrame);

		return ResultCode::kOK;
	}

	template<class TCoroStarter>
	Result Thread::EnterFunction(const TCoroStarter &starter)
	{
		Context &context = this->GetContext();

		StackFrameBase *prevFrame = context.m_frame;
		void *newFrame = context.m_allocStack(context.m_userdata, starter.GetMetadata().m_base);
		if (!newFrame)
			return ResultCode::kCoroStackOverflow;

		starter.GetMetadata().m_enterFunction(newFrame, prevFrame, starter.GetInstance());
		context.m_frame = static_cast<StackFrameBase *>(newFrame);

		return ResultCode::kOK;
	}
}
