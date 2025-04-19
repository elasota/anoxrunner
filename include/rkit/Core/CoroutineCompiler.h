#pragma once

#include "Coroutine.h"

namespace rkit::coro::compiler
{
	struct EmptyStruct
	{
	};

	template<class TClassInstance>
	struct CoroOptionalPtrResolver
	{
		typedef TClassInstance *Type_t;
	};

	template<>
	struct CoroOptionalPtrResolver<void>
	{
		typedef EmptyStruct Type_t[1];
	};


	template<class TParams>
	struct CoroOptionalResolver
	{
		typedef TParams Type_t;
	};

	template<>
	struct CoroOptionalResolver<void>
	{
		typedef EmptyStruct Type_t;
	};

	template<class TClassInstance, class TLocals, class TParams>
	struct StackFrameBuilder
	{
		StackFrameBase m_base;	// This must be first
		typename CoroOptionalPtrResolver<TClassInstance>::Type_t m_classInstance;
		typename CoroOptionalResolver<TLocals>::Type_t m_locals;
		typename CoroOptionalResolver<TParams>::Type_t m_params;
	};

	template<class TCoroutine, class TParamList>
	struct FunctionEntryBuilder
	{
	};

	template<class TStackFrame, class TLocals>
	struct FunctionEntryLocalsInitializer
	{
		inline static void InitLocals(TStackFrame *stackFrame)
		{
			new (&stackFrame->m_locals) TLocals();
		}

		inline static void DestructLocals(TStackFrame *stackFrame)
		{
			stackFrame->m_locals.~TLocals();
		}
	};

	template<class TStackFrame>
	struct FunctionEntryLocalsInitializer<TStackFrame, void>
	{
		inline static void InitLocals(TStackFrame *stackFrame)
		{
		}

		inline static void DestructLocals(TStackFrame *stackFrame)
		{
		}
	};

	template<class TStackFrame, class TClassInstance>
	struct FunctionEntryClassInstanceInitializer
	{
		inline static void InitClassInstance(TStackFrame *stackFrame, TClassInstance *classInstancePtr)
		{
			stackFrame->m_classInstance = classInstancePtr;
		}
	};

	template<class TStackFrame>
	struct FunctionEntryClassInstanceInitializer<TStackFrame, void>
	{
		inline static void InitClassInstance(TStackFrame *stackFrame, void *classInstance)
		{
		}
	};

	template<class TCoroutine>
	struct FunctionEntryBuilder<TCoroutine, void>
	{
		inline static void DestructFrame(StackFrameBase *stackFrameBase)
		{
			typedef typename TCoroutine::CoroStackFrame StackFrame_t;

			StackFrame_t *frame = reinterpret_cast<StackFrame_t *>(stackFrameBase);

			FunctionEntryLocalsInitializer<StackFrame_t, typename TCoroutine::Locals>::DestructLocals(frame);
		}

		inline static void EnterFunction(void *stackFrame, StackFrameBase *prevFrame, void *classInstance)
		{
			typedef typename TCoroutine::CoroStackFrame NewStackFrame_t;

			NewStackFrame_t *newFrame = static_cast<NewStackFrame_t *>(stackFrame);
			newFrame->m_base.m_prevFrame = prevFrame;
			newFrame->m_base.m_ip = TCoroutine::CoroFirstInstruction::CoroFunction;

			FunctionEntryLocalsInitializer<NewStackFrame_t, typename TCoroutine::Locals>::InitLocals(newFrame);

			FunctionEntryClassInstanceInitializer<NewStackFrame_t, typename TCoroutine::ClassInstance>::InitClassInstance(newFrame, static_cast<typename TCoroutine::ClassInstance *>(classInstance));

			newFrame->m_base.m_destructFrame = DestructFrame;
		}
	};

	template<class TCoroutine, class... TParams>
	struct FunctionEntryBuilder<TCoroutine, TypeList<TParams...>>
	{
		inline static void DestructFrame(StackFrameBase *stackFrameBase)
		{
			FunctionEntryBuilder<TCoroutine, void>::DestructFrame(stackFrameBase);

			typedef typename TCoroutine::CoroStackFrame StackFrame_t;

			StackFrame_t *frame = reinterpret_cast<StackFrame_t *>(stackFrameBase);

			typedef typename TCoroutine::Params ParamsStruct_t;
			ParamsStruct_t *params = &frame->m_params;

			params->~ParamsStruct_t();
		}

		inline static void EnterFunction(void *stackFrame, StackFrameBase *prevFrame, void *classInstance, TParams... args)
		{
			typedef typename TCoroutine::CoroStackFrame NewStackFrame_t;

			NewStackFrame_t *newFrame = static_cast<NewStackFrame_t *>(stackFrame);

			typedef typename TCoroutine::Params ParamsStruct_t;
			new (&newFrame->m_params) ParamsStruct_t{ std::forward<TParams>(args)... };

			FunctionEntryBuilder<TCoroutine, void>::EnterFunction(stackFrame, prevFrame, classInstance);

			newFrame->m_base.m_destructFrame = DestructFrame;
		}
	};

	template<class T>
	void FrameDestruct(T &item)
	{
		item.~T();
	}

	template<class TStackFrame>
	CodePtr ExitFunction(Context *coroContext, TStackFrame *derivedFrame)
	{
		FrameDestruct(derivedFrame->m_params);
		FrameDestruct(derivedFrame->m_locals);

		StackFrameBase *frame = &derivedFrame->m_base;
		StackFrameBase *prevFrame = frame->m_prevFrame;

		coroContext->m_freeStack(coroContext->m_userdata, frame);
		coroContext->m_frame = prevFrame;

		return { nullptr };
	}

#if 0
		struct CoroStackFrameBase
		{
			CoroFunction_t m_ip;
			CoroStackFrameBase *m_prevFrame;
			void (*m_destructFrame)(CoroStackFrameBase *stackFrame);
		};

		struct CoroFrameMetadataBase;

		struct CoroContext
		{
			typedef void *(*CoroPushStackCallback_t)(void *userdata, const CoroFrameMetadataBase &metadata);
			typedef void (*CoroFreeStackCallback_t)(void *userdata, void *prevFrameBase);

			CoroStackFrameBase *m_frame = nullptr;
			CoroDisposition m_disposition = CoroDisposition::kInvalid;

			void *m_userdata = nullptr;
			CoroPushStackCallback_t m_allocStack = nullptr;
			CoroFreeStackCallback_t m_freeStack = nullptr;

		};

		typedef void (*InitDestructLocalsFunction_t) (void *ptr);


		struct CoroInstruction
		{
			CoroInstructionType m_instrType;

			CoroFunction_t m_func;
		};

		struct CoroDef
		{
			size_t m_localsSize;
			size_t m_localsAlign;

			const CoroInstruction *m_instructions;
			size_t m_numInstructions;

			InitDestructLocalsFunction_t m_initLocals;
			InitDestructLocalsFunction_t m_destructLocals;
		};


#define CORO_CONCAT_2(a, b) a ## b
#define CORO_CONCAT_1(a, b) CORO_CONCAT_2(a, b)
#define CORO_CONCAT_LINE(a) CORO_CONCAT_1(a, __LINE__)

		template<class TFirst, class TSecond>
		struct CoroIsSameType
		{
			static const bool kResult = false;
		};

		template<class TType>
		struct CoroIsSameType<TType, TType>
		{
			static const bool kResult = true;
		};

		template<class... T>
		struct CoroTypeList
		{
		};

		template<class TTypeList>
		struct CoroTypeListSize
		{
		};

		template<class TType>
		struct CoroTypeListSize<CoroTypeList<TType>>
		{
			static const size_t kSize = 1;
		};

		template<class TFirstType, class... TMoreTypes>
		struct CoroTypeListSize<CoroTypeList<TFirstType, TMoreTypes...>>
		{
			static const size_t kSize = CoroTypeListSize<CoroTypeList<TMoreTypes...>>::kSize + 1;
		};

		template<size_t TIndex, class TTypeList>
		struct CoroTypeListElement
		{
		};

		template<class TFirstType, class... TMoreTypes>
		struct CoroTypeListElement<0, CoroTypeList<TFirstType, TMoreTypes...>>
		{
			typedef TFirstType Resolution_t;
		};

		template<class TType>
		struct CoroTypeListElement<0, CoroTypeList<TType>>
		{
			typedef TType Resolution_t;
		};

		template<size_t TIndex, class TFirstType, class... TMoreTypes>
		struct CoroTypeListElement<TIndex, CoroTypeList<TFirstType, TMoreTypes...>>
		{
			typedef typename CoroTypeListElement<TIndex - 1, CoroTypeList<TMoreTypes...>>::Resolution_t Resolution_t;
		};

		template<class TTypeList, class TAdditional>
		struct CoroTypeListAppend
		{
		};

		template<class... TTypes, class TAdditional>
		struct CoroTypeListAppend<CoroTypeList<TTypes...>, TAdditional>
		{
			typedef CoroTypeList<TTypes..., TAdditional> Type_t;
		};

		template<CoroInstructionType TTypeToCheck, CoroInstructionType... TMatches>
		struct CoroIsInstructionType
		{
		};

		template<CoroInstructionType TTypeToCheck, CoroInstructionType TMatch>
		struct CoroIsInstructionType<TTypeToCheck, TMatch>
		{
			static const bool kIsMatch = false;
		};

		template<CoroInstructionType TTypeToCheck>
		struct CoroIsInstructionType<TTypeToCheck, TTypeToCheck>
		{
			static const bool kIsMatch = true;
		};

		template<CoroInstructionType TTypeToCheck, CoroInstructionType TFirstMatch, CoroInstructionType... TMatches>
		struct CoroIsInstructionType<TTypeToCheck, TFirstMatch, TMatches...>
		{
			static const bool kIsMatch = CoroIsInstructionType<TTypeToCheck, TMatches...>::kIsMatch;
		};

		template<CoroInstructionType TTypeToCheck, CoroInstructionType... TMatches>
		struct CoroIsInstructionType<TTypeToCheck, TTypeToCheck, TMatches...>
		{
			static const bool kIsMatch = true;
		};

		template<bool TIsFirstMatch, template<class> class TEvaluator, class TInstructionList>
		struct CoroFindScopeClosingInstructionIncrement
		{
		};

		template<template<class> class TEvaluator, class TFirstInstruction, class... TMoreInstructions>
		struct CoroFindScopeClosingInstructionIncrement<true, TEvaluator, CoroTypeList<TFirstInstruction, TMoreInstructions...>>
		{
			typedef TFirstInstruction Resolution_t;
		};

		template<template<class> class TEvaluator, class TInstruction>
		struct CoroFindScopeClosingInstructionIncrement<true, TEvaluator, CoroTypeList<TInstruction>>
		{
			typedef TInstruction Resolution_t;
		};

		template<template<class> class TEvaluator, class TFirstInstruction, class TSecondInstruction, class... TMoreInstructions>
		struct CoroFindScopeClosingInstructionIncrement<false, TEvaluator, CoroTypeList<TFirstInstruction, TSecondInstruction, TMoreInstructions...>>
		{
			typedef typename CoroFindScopeClosingInstructionIncrement<TEvaluator<TSecondInstruction>::kIsMatch, TEvaluator, CoroTypeList<TSecondInstruction, TMoreInstructions...>>::Resolution_t Resolution_t;
		};

		template<template<class> class TEvaluator, class TFirstInstruction, class TSecondInstruction>
		struct CoroFindScopeClosingInstructionIncrement<false, TEvaluator, CoroTypeList<TFirstInstruction, TSecondInstruction>>
		{
			typedef typename CoroFindScopeClosingInstructionIncrement<TEvaluator<TSecondInstruction>::kIsMatch, TEvaluator, CoroTypeList<TSecondInstruction>>::Resolution_t Resolution_t;
		};

		template<template<class> class TEvaluator, class TInstructionList>
		struct CoroFindScopeClosingInstruction
		{
		};

		template<template<class> class TEvaluator, class... TInstructions>
		struct CoroFindScopeClosingInstruction<TEvaluator, CoroTypeList<TInstructions...>>
		{
			typedef typename CoroFindScopeClosingInstructionIncrement<false, TEvaluator, CoroTypeList<void, TInstructions...>>::Resolution_t Resolution_t;
		};

		template<class TCandidate, class TInstruction, class TScopeInstruction, CoroInstructionType... TCloseInstrTypes>
		struct CoroScopeCloseChecker
		{
			static const bool kIsMatch = (TCandidate::kCoroInstrIndex > TInstruction::kCoroInstrIndex)
				&& CoroIsInstructionType<TCandidate::kCoroInstrType, TCloseInstrTypes...>::kIsMatch
				&&CoroIsSameType<TScopeInstruction, typename TCandidate::CoroClosesScope_t>::kResult;
		};

		template<CoroInstructionType TInstrType, class TInstr, class TCoroTerminator, int TInstrRefIndex>
		struct CoroResolveInstrRef
		{
		};

		// If instruction ref0 jumps to the first else condition, if present, otherwise to the end
		template<class TInstr, class TCoroTerminator>
		struct CoroResolveInstrRef<CoroInstructionType::kIf, TInstr, TCoroTerminator, 0>
		{
			template<class TCandidate>
			using Evaluator = CoroScopeCloseChecker<TCandidate, TInstr, TInstr, CoroInstructionType::kElse, CoroInstructionType::kElseIf, CoroInstructionType::kEndIf>;

			typedef typename CoroFindScopeClosingInstruction<Evaluator, typename TCoroTerminator::InstrList_t>::Resolution_t Resolution_t;
		};

		// Else ref0 jumps to the end if the if block
		template<class TInstr, class TCoroTerminator>
		struct CoroResolveInstrRef<CoroInstructionType::kElse, TInstr, TCoroTerminator, 0>
		{
			template<class TCandidate>
			using Evaluator = CoroScopeCloseChecker<TCandidate, TInstr, typename TInstr::CoroBodyParentScope_t, CoroInstructionType::kEndIf>;

			typedef typename CoroFindScopeClosingInstruction<Evaluator, typename TCoroTerminator::InstrList_t>::Resolution_t Resolution_t;
		};

		// Else If ref0 exits the "if" block
		template<class TInstr, class TCoroTerminator>
		struct CoroResolveInstrRef<CoroInstructionType::kElseIf, TInstr, TCoroTerminator, 0>
		{
			template<class TCandidate>
			using Evaluator = CoroScopeCloseChecker<TCandidate, TInstr, typename TInstr::CoroBodyParentScope_t, CoroInstructionType::kEndIf>;

			typedef typename CoroFindScopeClosingInstruction<Evaluator, typename TCoroTerminator::InstrList_t>::Resolution_t Resolution_t;
		};

		// Else If ref1 jumps to the next condition or else
		template<class TInstr, class TCoroTerminator>
		struct CoroResolveInstrRef<CoroInstructionType::kElseIf, TInstr, TCoroTerminator, 1>
		{
			template<class TCandidate>
			using Evaluator = CoroScopeCloseChecker<TCandidate, TInstr, typename TInstr::CoroBodyParentScope_t, CoroInstructionType::kElse, CoroInstructionType::kElseIf, CoroInstructionType::kEndIf>;

			typedef typename CoroFindScopeClosingInstruction<Evaluator, typename TCoroTerminator::InstrList_t>::Resolution_t Resolution_t;
		};

		// For ref0 jumps to the end of the for block
		template<class TInstr, class TCoroTerminator>
		struct CoroResolveInstrRef<CoroInstructionType::kFor, TInstr, TCoroTerminator, 0>
		{
			template<class TCandidate>
			using Evaluator = CoroScopeCloseChecker<TCandidate, TInstr, typename TInstr::CoroBodyParentScope_t, CoroInstructionType::kEndFor>;

			typedef typename CoroFindScopeClosingInstruction<Evaluator, typename TCoroTerminator::InstrList_t>::Resolution_t Resolution_t;
		};

		// While ref0 jumps to the end of the for block
		template<class TInstr, class TCoroTerminator>
		struct CoroResolveInstrRef<CoroInstructionType::kWhile, TInstr, TCoroTerminator, 0>
		{
			template<class TCandidate>
			using Evaluator = CoroScopeCloseChecker<TCandidate, TInstr, typename TInstr::CoroBodyParentScope_t, CoroInstructionType::kEndWhile>;

			typedef typename CoroFindScopeClosingInstruction<Evaluator, typename TCoroTerminator::InstrList_t>::Resolution_t Resolution_t;
		};


		template<bool TIsCorrectInstruction, class TInstr>
		struct CoroCheckCloseScope
		{
		};

		template<class TInstr>
		struct CoroCheckCloseScope<true, TInstr>
		{
			typedef TInstr Type_t;
		};

		template<class TInstr>
		struct CoroCloseIf
		{
			typedef typename CoroCheckCloseScope<TInstr::kCoroInstrType == CoroInstructionType::kIf, TInstr>::Type_t Type_t;
		};

		template<class TInstr>
		struct CoroCloseFor
		{
			typedef typename CoroCheckCloseScope<TInstr::kCoroInstrType == CoroInstructionType::kFor, TInstr>::Type_t Type_t;
		};

		template<class TInstr>
		struct CoroCloseWhile
		{
			typedef typename CoroCheckCloseScope<TInstr::kCoroInstrType == CoroInstructionType::kWhile, TInstr>::Type_t Type_t;
		};

		template<class TCoro, class TCoroInstrList>
		struct CompiledCoroInstructions
		{
		};

		template<class TCoro, class... TInstr>
		struct CompiledCoroInstructions<TCoro, CoroTypeList<TInstr...>>
		{
			static const size_t kNumInstructions = CoroTypeListSize<CoroTypeList<TInstr...>>::kSize;
			static const CoroInstruction ms_instructions[CoroTypeListSize<CoroTypeList<TInstr...>>::kSize];
		};


		template<class TCoro>
		struct CompiledCoro : public CompiledCoroInstructions<TCoro, typename TCoro::CoroTerminator::InstrList_t>
		{
			static void InitLocals(void *ptr)
			{
				new (ptr) typename TCoro::Locals();
			}

			static void DestructLocals(void *ptr)
			{
				typedef typename TCoro::Locals Locals_t;

				static_cast<Locals_t *>(ptr)->~Locals_t();
			}
		};

		template<class TCoro, class... TInstr>
		const CoroInstruction CompiledCoroInstructions<TCoro, CoroTypeList<TInstr...>>::ms_instructions[CoroTypeListSize<CoroTypeList<TInstr...>>::kSize] =
		{
			{
				TInstr::kCoroInstrType,
				TInstr::CoroFunction,
			}...
		};

		template<class TCoro>
		struct CoroCreateDef
		{
			static const CoroDef ms_def;
		};

		template<class TCoro>
		const CoroDef CoroCreateDef<TCoro>::ms_def =
		{
			sizeof(typename TCoro::Locals),
			alignof(typename TCoro::Locals),
			CompiledCoro<TCoro>::ms_instructions,
			CompiledCoro<TCoro>::kNumInstructions,
			CompiledCoro<TCoro>::InitLocals,
			CompiledCoro<TCoro>::DestructLocals,
		};

		enum class NextInstructionDisposition
		{
			kContinue,
			kNextRef0,
			kThisRef0,
			kThisRef1,
			kRepeatLoop,
		};

		template<class TCoroTerminator, size_t TInstructionIndex>
		struct CoroInstructionResolver
		{
			typedef typename CoroTypeListElement<TInstructionIndex, typename TCoroTerminator::InstrList_t>::Resolution_t Resolution_t;
		};

		template<class TCoroTerminatorLookup, class TInstruction, NextInstructionDisposition TNextDisposition>
		struct CoroNextInstructionResolver
		{
		};

		template<class TCoroTerminatorLookup, class TInstruction>
		struct CoroNextInstructionResolver<TCoroTerminatorLookup, TInstruction, NextInstructionDisposition::kContinue>
			: public CoroInstructionResolver<typename TCoroTerminatorLookup::Resolution_t, TInstruction::kCoroInstrIndex + 1>
		{
			typedef typename CoroInstructionResolver<typename TCoroTerminatorLookup::Resolution_t, TInstruction::kCoroInstrIndex + 1>::Resolution_t NextInstruction_t;

			static inline CoroFunctionPtr Resolve()
			{
				return { NextInstruction_t::CoroFunction };
			}
		};

		template<class TCoroTerminatorLookup, class TInstruction>
		struct CoroNextInstructionResolver<TCoroTerminatorLookup, TInstruction, NextInstructionDisposition::kNextRef0>
		{
			typedef typename CoroInstructionResolver<typename TCoroTerminatorLookup::Resolution_t, TInstruction::kCoroInstrIndex + 1>::Resolution_t NextInstruction_t;

			typedef typename CoroResolveInstrRef<NextInstruction_t::kCoroInstrType, NextInstruction_t, typename TCoroTerminatorLookup::Resolution_t, 0>::Resolution_t ReferencedInstr_t;

			static inline CoroFunctionPtr Resolve()
			{
				return { ReferencedInstr_t::CoroFunction };
			}
		};

		template<class TCoroTerminatorLookup, class TInstruction>
		struct CoroNextInstructionResolver<TCoroTerminatorLookup, TInstruction, NextInstructionDisposition::kThisRef0>
		{
			typedef typename CoroResolveInstrRef<TInstruction::kCoroInstrType, TInstruction, typename TCoroTerminatorLookup::Resolution_t, 0>::Resolution_t ReferencedInstr_t;

			static inline CoroFunctionPtr Resolve()
			{
				return { ReferencedInstr_t::CoroFunction };
			}
		};

		template<class TCoroTerminatorLookup, class TInstruction>
		struct CoroNextInstructionResolver<TCoroTerminatorLookup, TInstruction, NextInstructionDisposition::kThisRef1>
		{
			typedef typename CoroResolveInstrRef<TInstruction::kCoroInstrType, TInstruction, typename TCoroTerminatorLookup::Resolution_t, 1>::Resolution_t ReferencedInstr_t;

			static inline CoroFunctionPtr Resolve()
			{
				return { ReferencedInstr_t::CoroFunction };
			}
		};

		template<class TCoroTerminatorLookup, class TInstruction>
		struct CoroNextInstructionResolver<TCoroTerminatorLookup, TInstruction, NextInstructionDisposition::kRepeatLoop>
		{
			static CoroFunctionPtr RepeatLoopFunction(CoroContext *CORO_INTERNAL_coroContext, CoroStackFrameBase *CORO_INTERNAL_coroStackFrame)
			{
				TInstruction::CoroStepFunction(CORO_INTERNAL_coroContext, CORO_INTERNAL_coroStackFrame);
				return TInstruction::CoroFunction(CORO_INTERNAL_coroContext, CORO_INTERNAL_coroStackFrame);
			}

			static inline CoroFunctionPtr Resolve()
			{
				return { RepeatLoopFunction };
			}
		};

		template<class TLocals>
		struct CoroStackFrameLocals
		{
			TLocals m_locals;

			inline TLocals *GetLocals()
			{
				return &m_locals;
			}
		};

		template<>
		struct CoroStackFrameLocals<void>
		{
			inline static void *GetLocals()
			{
				return nullptr;
			}
		};


		template<class TCoroutine>
		struct CoroMetadataResolver
		{
			static const CoroFrameMetadata<typename TCoroutine::ParameterList> ms_metadata;
		};

		template<class TSignature>
		struct CoroSignatureAnalyzer
		{
		};

		template<class... TParameters>
		struct CoroSignatureAnalyzer<void(TParameters...)>
		{
			typedef CoroTypeList<TParameters...> Parameters_t;
		};

		template<>
		struct CoroSignatureAnalyzer<void()>
		{
			typedef void Parameters_t;
		};

		template<class TParamList>
		struct CoroParamList
		{
		};

		template<class... TParams>
		struct CoroParamList<CoroTypeList<TParams...>>
		{
			template<size_t TIndex>
			using CoroParam = typename CoroTypeListElement<TIndex, CoroTypeList<TParams...>>::Resolution_t;
		};

		template<class TClass, class TSignature>
		struct MethodCoroutine : public CoroParamList<typename CoroSignatureAnalyzer<TSignature>::Parameters_t>
		{
			typedef TClass ClassInstance;
			typedef typename CoroSignatureAnalyzer<TSignature>::Parameters_t ParameterList;
		};

		template<class TSignature>
		struct FunctionCoroutine : public CoroParamList<typename CoroSignatureAnalyzer<TSignature>::Parameters_t>
		{
			typedef CoroEmptyStruct ClassInstance;
			typedef typename CoroSignatureAnalyzer<TSignature>::Parameters_t ParameterList;
		};

		template<class TSignature>
		class CoroMethodStarter
		{
		public:
			template<class TClass, class TCoroutine>
			explicit CoroMethodStarter(TClass &instance, const TCoroutine &coroutine)
				: m_instance(&instance)
				, m_metadata(CoroMetadataResolver<TCoroutine>::ms_metadata)
			{
				this->CheckType<TClass>(coroutine);
			}

			const CoroFrameMetadata<typename CoroSignatureAnalyzer<TSignature>::Parameters_t> &GetMetadata() const
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
			const CoroFrameMetadata<typename CoroSignatureAnalyzer<TSignature>::Parameters_t> &m_metadata;
		};

		template<class TSignature>
		class CoroFunctionStarter
		{
		public:
			template<class TCoroutine>
			explicit CoroFunctionStarter(const TCoroutine &coroutine)
				: m_metadata(CoroMetadataResolver<TCoroutine>::ms_metadata)
			{
				this->CheckType(coroutine);
			}

			const CoroFrameMetadata<typename CoroSignatureAnalyzer<TSignature>::Parameters_t> &GetMetadata() const
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

			const CoroFrameMetadata<typename CoroSignatureAnalyzer<TSignature>::Parameters_t> &m_metadata;
		};

		class CoroRunner;

		class MyClass
		{
		public:
			struct MyCoroutineCode;
			typedef void MyCoroutineSignature(int a, int b);
			virtual CoroMethodStarter<MyCoroutineSignature> MyCoroutine();

			struct MyCoroutine2Code;
			typedef void MyCoroutine2Signature(int &a);
			static CoroFunctionStarter<MyCoroutine2Signature> MyCoroutine2();
		};

		struct MyClass::MyCoroutineCode final : public MethodCoroutine<MyClass, MyCoroutineSignature>
		{
			struct Params
			{
				CoroParam<0> a;
				CoroParam<1> b;
			};

			struct Locals
			{
				int m_a = 2;
			};

			CORO_BEGIN
				printf("Whatever");

			locals.m_a = params.a + params.b;

			CORO_IF(locals.m_a == 1)
				printf("A was 1");
			CORO_ELSE_IF(locals.m_a == 2)
				printf("A was 2");
			CORO_ELSE_IF(locals.m_a == 3)
				printf("A was 3");
			CORO_ELSE
				printf("A was something else");
			CORO_END_IF

				locals.m_a = 0;
			CORO_WHILE(locals.m_a < 4)
				printf("A: %i\n", locals.m_a);
			CORO_CALL(MyCoroutine2, locals.m_a);
			CORO_END_WHILE

				printf("Whatever");
			CORO_END
		};

		struct MyClass::MyCoroutine2Code final : public FunctionCoroutine<MyCoroutine2Signature>
		{
			struct Params
			{
				CoroParam<0> a;
			};

			struct Locals
			{
			};

			CORO_BEGIN
				params.a++;
			CORO_END
		};

		CoroMethodStarter<MyClass::MyCoroutineSignature> MyClass::MyCoroutine()
		{
			return CoroMethodStarter<MyClass::MyCoroutineSignature>(*this, MyCoroutineCode());
		}

		CoroFunctionStarter<MyClass::MyCoroutine2Signature> MyClass::MyCoroutine2()
		{
			return CoroFunctionStarter<MyClass::MyCoroutine2Signature>(MyCoroutine2Code());
		}


		void ResumeCoro(CoroContext *context)
		{
			for (;;)
			{
				CoroStackFrameBase *frame = context->m_frame;

				if (!frame)
					break;

				CoroFunction_t ip = frame->m_ip;

				while (ip != nullptr)
				{
					ip = ip(context, frame).m_function;
				}
			}

			int n = 0;
		}

		struct StackFrameAllocator
		{
			static void *AllocStackFrame(void *userdata, const CoroFrameMetadataBase &metadata)
			{
				return static_cast<CoroStackFrameBase *>(malloc(metadata.m_frameSize));
			}

			static void FreeStackFrame(void *userdata, void *frame)
			{
				free(frame);
			}
		};

		template<class TCoroStarter, class... TArgs>
		void CoroCallFunction(CoroContext &context, const TCoroStarter &starter, TArgs... args)
		{
			CoroStackFrameBase *prevFrame = context.m_frame;
			void *newFrame = context.m_allocStack(context.m_userdata, starter.GetMetadata().m_base);
			starter.GetMetadata().m_enterFunction(newFrame, prevFrame, starter.GetInstance(), std::forward<TArgs>(args)...);
			context.m_frame = static_cast<CoroStackFrameBase *>(newFrame);
		}

		template<class TCoroStarter, class... TArgs>
		void CoroCallFunction(CoroContext &context, const TCoroStarter &starter)
		{
			CoroStackFrameBase *prevFrame = context.m_frame;
			CoroStackFrameBase *newFrame = context.m_allocStack(context.m_userdata, starter.GetMetadata().m_base);
			starter.GetMetadata().m_enterFunction(newFrame, prevFrame, starter.GetInstance());
			context.m_frame = newFrame;
		}
#endif
}

namespace rkit::coro
{
	template<class TCoroutine>
	const FrameMetadata<typename TCoroutine::ParameterList> CoroMetadataResolver<TCoroutine>::ms_metadata =
	{
		{
			sizeof(typename TCoroutine::CoroStackFrame),
			alignof(typename TCoroutine::CoroStackFrame),
		},
		::rkit::coro::compiler::FunctionEntryBuilder<TCoroutine, typename TCoroutine::ParameterList>::EnterFunction,
	};
}

#define CORO_INSTR_CONSTANTS(prevInstrTag, thisInstrTag, instrType)	\
	typedef thisInstrTag ThisInstr_t; \
	typedef CORO_CONCAT_LINE(prevInstrTag) PrevInstr_t; \
	typedef CoroTypeListAppend<PrevInstr_t::InstrList_t, ThisInstr_t>::Type_t InstrList_t; \
	static const CoroInstructionType kCoroInstrType = CoroInstructionType::instrType; \
	static const size_t kCoroInstrIndex = PrevInstr_t::kCoroInstrIndex + 1;

#define CORO_CONTINUE_BODY_SCOPE	\
	typedef PrevInstr_t::CoroBodyParentScope_t CoroBodyParentScope_t;\
	typedef PrevInstr_t::CoroBodyParentLoop_t CoroBodyParentLoop_t;\
	typedef PrevInstr_t::CoroAvailableElseScope_t CoroAvailableElseScope_t;\
	typedef void CoroClosesScope_t;\
	typedef void CoroClosesLoop_t;\

#define CORO_NO_LABEL	\
	static const size_t kCoroLabelFor = PrevInstr_t::kCoroInstrIndex + 1; \

#define CORO_NO_FUNCTION	\
	static constexpr std::nullptr_t CoroFunction = nullptr;\

#define CORO_LOCALS	\
	Params &params = reinterpret_cast<CoroStackFrame *>(CORO_INTERNAL_coroStackFrame)->m_params;\
	Locals &locals = reinterpret_cast<CoroStackFrame *>(CORO_INTERNAL_coroStackFrame)->m_locals;\
	ClassInstance *self = reinterpret_cast<CoroStackFrame *>(CORO_INTERNAL_coroStackFrame)->m_classInstance;

#define CORO_FUNCTION_DEF	\
	static ::rkit::coro::CodePtr CoroFunction(::rkit::coro::Context *CORO_INTERNAL_coroContext, ::rkit::coro::StackFrameBase *CORO_INTERNAL_coroStackFrame)\
	{\
		CORO_LOCALS

#define CORO_FUNCTION_END	\
		return CoroNextInstructionResolver<CoroTerminatorLookup, ThisInstr_t, NextInstructionDisposition::kContinue>::Resolve();\
	}

#define CORO_BEGIN \
	typedef ::rkit::coro::compiler::StackFrameBuilder<ClassInstance, Locals, Params> CoroStackFrame;\
	struct CoroTerminatorLookup; \
	typedef struct CoroFirstInstruction\
	{\
		typedef CoroFirstInstruction ThisInstr_t; \
		typedef ::rkit::TypeList<ThisInstr_t> InstrList_t; \
		static const ::rkit::coro::InstructionType kCoroInstrType = ::rkit::coro::InstructionType::kCode; \
		static const size_t kCoroInstrIndex = 0; \
		typedef void CoroBodyParentScope_t; \
		typedef void CoroBodyParentLoop_t; \
		typedef void CoroAvailableElseScope_t; \
		typedef void CoroClosesScope_t; \
		typedef void CoroClosesLoop_t; \
		CORO_FUNCTION_DEF

#define CORO_END	\
			return ::rkit::coro::compiler::ExitFunction(CORO_INTERNAL_coroContext, reinterpret_cast<CoroStackFrame *>(CORO_INTERNAL_coroStackFrame));\
		}\
	} CoroTerminator;\
	struct CoroTerminatorLookup\
	{\
		typedef CoroTerminator Resolution_t;\
	};

#define CORO_RETURN	\
			return CoroExitFunction(CORO_INTERNAL_coroContext, reinterpret_cast<CoroStackFrame *>(CORO_INTERNAL_coroStackFrame));\
		}\
	} CORO_CONCAT_LINE(CoroEndAt);\
	typedef struct CORO_CONCAT_LINE(CoroReturn)\
	{\
		CORO_INSTR_CONSTANTS(CoroEndAt, CORO_CONCAT_LINE(CoroReturn), kReturn)\
		CORO_CONTINUE_BODY_SCOPE\
		CORO_FUNCTION_DEF\
		((void)0)

// If/ElseIf/Else/EndIf chains work on the following assumptions
// Code under the "If", "Else If", and "Else" instructions all have the "If" instruction
// as their parent scope.
// The "End If" instruction closes the "If" instruction scope.
// The "Available Else Scope" is the "If" instruction, unless an "Else" has already been
// declared, in which case it is void.
// "Else If", "Else" and "End If" instructions are marked as closing the "If" scope
#define CORO_IF(condition)	\
		if (!(condition))\
			return CoroNextInstructionResolver<CoroTerminatorLookup, ThisInstr_t, NextInstructionDisposition::kNextRef0>::Resolve();\
		CORO_FUNCTION_END\
	} CORO_CONCAT_LINE(CoroEndAt);\
	typedef struct CORO_CONCAT_LINE(CoroIf)\
	{\
		CORO_INSTR_CONSTANTS(CoroEndAt, CORO_CONCAT_LINE(CoroIf), kIf)\
		typedef CORO_CONCAT_LINE(CoroIf) CoroBodyParentScope_t;\
		typedef PrevInstr_t::CoroBodyParentLoop_t CoroBodyParentLoop_t;\
		typedef CORO_CONCAT_LINE(CoroIf) CoroAvailableElseScope_t;\
		typedef void CoroClosesScope_t;\
		typedef void CoroClosesLoop_t;\
		CORO_FUNCTION_DEF



#define CORO_ELSE_IF(condition)	\
			return CoroNextInstructionResolver<CoroTerminatorLookup, ThisInstr_t, NextInstructionDisposition::kNextRef0>::Resolve();\
		}\
	} CORO_CONCAT_LINE(CoroEndAt);\
	typedef struct CORO_CONCAT_LINE(CoroElseIf)\
	{\
		CORO_INSTR_CONSTANTS(CoroEndAt, CORO_CONCAT_LINE(CoroElseIf), kElseIf)\
		typedef PrevInstr_t::CoroAvailableElseScope_t CoroBodyParentScope_t;\
		typedef PrevInstr_t::CoroBodyParentLoop_t CoroBodyParentLoop_t;\
		typedef PrevInstr_t::CoroAvailableElseScope_t CoroAvailableElseScope_t;\
		typedef PrevInstr_t::CoroAvailableElseScope_t CoroClosesScope_t;\
		typedef void CoroClosesLoop_t;\
		CORO_FUNCTION_DEF\
			if (!(condition))\
				return CoroNextInstructionResolver<CoroTerminatorLookup, ThisInstr_t, NextInstructionDisposition::kThisRef1>::Resolve();


#define CORO_ELSE	\
			return CoroNextInstructionResolver<CoroTerminatorLookup, ThisInstr_t, NextInstructionDisposition::kNextRef0>::Resolve();\
		}\
	} CORO_CONCAT_LINE(CoroEndAt);\
	typedef struct CORO_CONCAT_LINE(CoroElse)\
	{\
		CORO_INSTR_CONSTANTS(CoroEndAt, CORO_CONCAT_LINE(CoroElse), kElse)\
		typedef PrevInstr_t::CoroAvailableElseScope_t CoroBodyParentScope_t;\
		typedef PrevInstr_t::CoroBodyParentLoop_t CoroBodyParentLoop_t;\
		typedef void CoroAvailableElseScope_t;\
		typedef PrevInstr_t::CoroAvailableElseScope_t CoroClosesScope_t;\
		typedef void CoroClosesLoop_t;\
		CORO_FUNCTION_DEF

#define CORO_END_IF	\
		CORO_FUNCTION_END\
	} CORO_CONCAT_LINE(CoroEndAt);\
	typedef struct CORO_CONCAT_LINE(CoroEndIf)\
	{\
		CORO_INSTR_CONSTANTS(CoroEndAt, CORO_CONCAT_LINE(CoroEndIf), kEndIf)\
		typedef CoroCloseIf<PrevInstr_t::CoroBodyParentScope_t>::Type_t::PrevInstr_t::CoroBodyParentScope_t CoroBodyParentScope_t;\
		typedef PrevInstr_t::CoroBodyParentLoop_t CoroBodyParentLoop_t;\
		typedef CoroCloseIf<PrevInstr_t::CoroBodyParentScope_t>::Type_t::PrevInstr_t::CoroAvailableElseScope_t CoroAvailableElseScope_t;\
		typedef PrevInstr_t::CoroBodyParentScope_t CoroClosesScope_t;\
		typedef void CoroClosesLoop_t;\
		CORO_FUNCTION_DEF

#define CORO_FOR(initializer, condition, step)	\
		(initializer);\
		CORO_FUNCTION_END\
	} CORO_CONCAT_LINE(CoroEndAt);\
	typedef struct CORO_CONCAT_LINE(CoroFor)\
	{\
		CORO_INSTR_CONSTANTS(CoroEndAt, CORO_CONCAT_LINE(CoroFor), kFor)\
		typedef CORO_CONCAT_LINE(CoroFor) CoroBodyParentScope_t;\
		typedef CORO_CONCAT_LINE(CoroFor) CoroBodyParentLoop_t;\
		typedef void CoroAvailableElseScope_t;\
		typedef void CoroClosesScope_t;\
		typedef void CoroClosesLoop_t;\
		static void CoroStepFunction(CoroContext *CORO_INTERNAL_coroContext, CoroStackFrameBase *CORO_INTERNAL_coroStackFrame)\
		{\
			CORO_LOCALS\
			(step);\
		}\
		CORO_FUNCTION_DEF\
			if (!(condition))\
				return CoroNextInstructionResolver<CoroTerminatorLookup, ThisInstr_t, NextInstructionDisposition::kThisRef0>::Resolve();\

#define CORO_END_FOR	\
			return CoroNextInstructionResolver<CoroTerminatorLookup, CoroBodyParentLoop_t, NextInstructionDisposition::kRepeatLoop>::Resolve();\
		}\
	} CORO_CONCAT_LINE(CoroEndAt);\
	typedef struct CORO_CONCAT_LINE(CoroEndFor)\
	{\
		CORO_INSTR_CONSTANTS(CoroEndAt, CORO_CONCAT_LINE(CoroEndFor), kEndFor)\
		typedef CoroCloseFor<PrevInstr_t::CoroBodyParentScope_t>::Type_t::PrevInstr_t::CoroBodyParentScope_t CoroBodyParentScope_t;\
		typedef CoroCloseFor<PrevInstr_t::CoroBodyParentScope_t>::Type_t::PrevInstr_t::CoroBodyParentLoop_t CoroBodyParentLoop_t;\
		typedef CoroCloseFor<PrevInstr_t::CoroBodyParentScope_t>::Type_t::PrevInstr_t::CoroAvailableElseScope_t CoroAvailableElseScope_t;\
		typedef PrevInstr_t::CoroBodyParentScope_t CoroClosesScope_t;\
		typedef PrevInstr_t::CoroBodyParentScope_t CoroClosesLoop_t;\
		CORO_FUNCTION_DEF

#define CORO_WHILE(condition)	\
		CORO_FUNCTION_END\
	} CORO_CONCAT_LINE(CoroEndAt);\
	typedef struct CORO_CONCAT_LINE(CoroWhile)\
	{\
		CORO_INSTR_CONSTANTS(CoroEndAt, CORO_CONCAT_LINE(CoroWhile), kWhile)\
		typedef CORO_CONCAT_LINE(CoroWhile) CoroBodyParentScope_t;\
		typedef CORO_CONCAT_LINE(CoroWhile) CoroBodyParentLoop_t;\
		typedef void CoroAvailableElseScope_t;\
		typedef void CoroClosesScope_t;\
		typedef void CoroClosesLoop_t;\
		static void CoroStepFunction(CoroContext *, CoroStackFrameBase *)\
		{\
		}\
		CORO_FUNCTION_DEF\
			if (!(condition))\
				return CoroNextInstructionResolver<CoroTerminatorLookup, ThisInstr_t, NextInstructionDisposition::kThisRef0>::Resolve();\

#define CORO_END_WHILE	\
			return CoroNextInstructionResolver<CoroTerminatorLookup, CoroBodyParentLoop_t, NextInstructionDisposition::kRepeatLoop>::Resolve();\
		}\
	} CORO_CONCAT_LINE(CoroEndAt);\
	typedef struct CORO_CONCAT_LINE(CoroEndWhile)\
	{\
		CORO_INSTR_CONSTANTS(CoroEndAt, CORO_CONCAT_LINE(CoroEndWhile), kEndWhile)\
		typedef CoroCloseWhile<PrevInstr_t::CoroBodyParentScope_t>::Type_t::PrevInstr_t::CoroBodyParentScope_t CoroBodyParentScope_t;\
		typedef CoroCloseWhile<PrevInstr_t::CoroBodyParentScope_t>::Type_t::PrevInstr_t::CoroBodyParentLoop_t CoroBodyParentLoop_t;\
		typedef CoroCloseWhile<PrevInstr_t::CoroBodyParentScope_t>::Type_t::PrevInstr_t::CoroAvailableElseScope_t CoroAvailableElseScope_t;\
		typedef PrevInstr_t::CoroBodyParentScope_t CoroClosesScope_t;\
		typedef PrevInstr_t::CoroBodyParentScope_t CoroClosesLoop_t;\
		CORO_FUNCTION_DEF

#define CORO_BREAK \
			return CoroNextInstructionResolver<CoroTerminatorLookup, CoroBodyParentLoop_t, NextInstructionDisposition::kThisRef0>::Resolve();\
		}\
	} CORO_CONCAT_LINE(CoroEndAt);\
	typedef struct CORO_CONCAT_LINE(CoroBreak)\
	{\
		CORO_INSTR_CONSTANTS(CoroEndAt, CORO_CONCAT_LINE(CoroBreak), kBreak)\
		CORO_CONTINUE_BODY_SCOPE\
		CORO_FUNCTION_DEF\
		((void)0)

#define CORO_CONTINUE \
			return CoroNextInstructionResolver<CoroTerminatorLookup, CoroBodyParentLoop_t, NextInstructionDisposition::kRepeatLoop>::Resolve();\
		}\
	} CORO_CONCAT_LINE(CoroEndAt);\
	typedef struct CORO_CONCAT_LINE(CoroContinue)\
	{\
		CORO_INSTR_CONSTANTS(CoroEndAt, CORO_CONCAT_LINE(CoroContinue), kContinue)\
		CORO_CONTINUE_BODY_SCOPE\
		CORO_FUNCTION_DEF\
		((void)0)

#define CORO_CALL(coro, ...)	\
			{\
				auto CORO_CONCAT_LINE(CORO_INTERNAL_coroStarter) = (coro)();\
				void *CORO_CONCAT_LINE(CORO_INTERNAL_newStackFrame) = CORO_INTERNAL_coroContext->m_allocStack(CORO_INTERNAL_coroContext->m_userdata, CORO_CONCAT_LINE(CORO_INTERNAL_coroStarter).GetMetadata().m_base);\
				if (!CORO_CONCAT_LINE(CORO_INTERNAL_newStackFrame))\
					CORO_INTERNAL_coroContext->m_disposition = CoroDisposition::kStackOverflow;\
				else\
				{\
					CORO_INTERNAL_coroStackFrame->m_ip = CoroNextInstructionResolver<CoroTerminatorLookup, ThisInstr_t, NextInstructionDisposition::kContinue>::Resolve().m_function; \
					CORO_CONCAT_LINE(CORO_INTERNAL_coroStarter).GetMetadata().m_enterFunction(CORO_CONCAT_LINE(CORO_INTERNAL_newStackFrame), CORO_INTERNAL_coroStackFrame, CORO_CONCAT_LINE(CORO_INTERNAL_coroStarter).GetInstance(), ## __VA_ARGS__); \
					CORO_INTERNAL_coroContext->m_frame = static_cast<CoroStackFrameBase*>(CORO_CONCAT_LINE(CORO_INTERNAL_newStackFrame)); \
					CORO_INTERNAL_coroContext->m_disposition = CoroDisposition::kResume; \
				}\
				return { nullptr };\
			}\
		}\
	} CORO_CONCAT_LINE(CoroEndAt);\
	typedef struct CORO_CONCAT_LINE(CoroCall)\
	{\
		CORO_INSTR_CONSTANTS(CoroEndAt, CORO_CONCAT_LINE(CoroCall), kContinue)\
		CORO_CONTINUE_BODY_SCOPE\
		CORO_FUNCTION_DEF\
		((void)0)
