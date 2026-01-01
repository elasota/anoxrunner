#pragma once

#include "TypeList.h"
#include "IntList.h"

#include <type_traits>
#include <utility>

/*
// General syntax for traits:

enum class TraitMethods
{
	kSomeMethod,
};

RKIT_TRAIT_FUNC(TraitMethods, MethodName, signature)

using MyTrait = rkit::traits::TraitInfo<TraitMethods>;

Overloads are not supported for the same const/volatile/static member qualifiers.

*/
namespace rkit { namespace traits {
	template<class TMethodEnum, size_t TMethodID>
	struct MethodIDMetadata;

	template<class TTypeTag, size_t TMethodID>
	struct MethodDispatchMetadata;

	template<class... TTraits>
	class TraitRef;

	template<class TMethodEnum>
	struct MethodCount;

	enum class TraitRefType
	{
		kNone,
		kConst,
		kConstVolatile,
		kVolatile,
		kStatic,
	};
} }

namespace rkit { namespace traits { namespace priv {
	typedef void (*AbstractFunctionPtr_t)(void);

	template<class TReturnType, class... TParams>
	struct TraitCallbackTypeResolver
	{
		typedef TReturnType (*Type_t)(void *, TParams...);
	};

	template<class TReturnType, class TParamList>
	struct TraitMethodInvoker
	{
	};

	template<class TReturnType, class... TParamTypes>
	struct TraitMethodInvoker<TReturnType, TypeList<TParamTypes...>>
	{
		typedef TReturnType(*CallbackStatic_t)(TParamTypes...);
		typedef TReturnType(*Callback_t)(TParamTypes...);
		typedef TReturnType(*CallbackConst_t)(const void *, TParamTypes...);
		typedef TReturnType(*CallbackVolatile_t)(volatile void *, TParamTypes...);
		typedef TReturnType(*CallbackConstVolatile_t)(const void *, TParamTypes...);
	};

	template<class TMethodEnum, bool TExists, size_t TCount>
	struct MethodListCounter
	{
	};

	template<class TMethodEnum, size_t TCount>
	struct MethodListCounter<TMethodEnum, false, TCount>
	{
		static constexpr size_t kValue = TCount;
	};

	template<class TMethodEnum, size_t TCount>
	struct MethodListCounter<TMethodEnum, true, TCount>
		: public MethodListCounter<TMethodEnum, MethodIDMetadata<TMethodEnum, TCount + 1>::kExists, TCount + 1>
	{
	};

	template<class TSignature>
	struct AutoMethodMetadata
	{
	};

	template<class TReturnType, class... TParams>
	struct AutoMethodMetadata<TReturnType(TParams...)>
	{
		typedef TReturnType ReturnType_t;
		typedef TypeList<TParams...> ParamList_t;
		typedef TReturnType Signature_t(TParams...);
		typedef typename TraitCallbackTypeResolver<ReturnType_t, TParams...>::Type_t CallbackType_t;
	};

	template<size_t TNumFunctions>
	class TraitVTableReference
	{
	public:
		TraitVTableReference();
		explicit TraitVTableReference(const AbstractFunctionPtr_t (&functions)[TNumFunctions]);

		template<size_t TIndex>
		AbstractFunctionPtr_t GetAt() const;

	private:
		const AbstractFunctionPtr_t *m_functions;
	};

	template<>
	struct TraitVTableReference<1>
	{
	public:
		TraitVTableReference();
		explicit TraitVTableReference(AbstractFunctionPtr_t function);

		template<size_t TIndex>
		AbstractFunctionPtr_t GetAt() const;

	private:
		AbstractFunctionPtr_t m_function;
	};

	template<TraitRefType TRefType, class TTargetType, class TDispatchMetadataType, class TReturnType, class TParamList>
	struct TraitVTableSlotBinderBase2
	{
	};

	template<class TTargetType, class TDispatchMetadataType, class TReturnType, class... TParams>
	struct TraitVTableSlotBinderBase2<TraitRefType::kNone, TTargetType, TDispatchMetadataType, TReturnType, TypeList<TParams...>>
		: public TDispatchMetadataType::template InstanceDispatcher<TTargetType, TTargetType, TReturnType, TParams...>
	{
	};

	template<class TTargetType, class TDispatchMetadataType, class TReturnType, class... TParams>
	struct TraitVTableSlotBinderBase2<TraitRefType::kConst, TTargetType, TDispatchMetadataType, TReturnType, TypeList<TParams...>>
		: public TDispatchMetadataType::template InstanceDispatcher<TTargetType, const TTargetType, TReturnType, TParams...>
	{
	};

	template<class TTargetType, class TDispatchMetadataType, class TReturnType, class... TParams>
	struct TraitVTableSlotBinderBase2<TraitRefType::kVolatile, TTargetType, TDispatchMetadataType, TReturnType, TypeList<TParams...>>
		: public TDispatchMetadataType::template InstanceDispatcher<TTargetType, volatile TTargetType, TReturnType, TParams...>
	{
	};

	template<class TTargetType, class TDispatchMetadataType, class TReturnType, class... TParams>
	struct TraitVTableSlotBinderBase2<TraitRefType::kConstVolatile, TTargetType, TDispatchMetadataType, TReturnType, TypeList<TParams...>>
		: public TDispatchMetadataType::template InstanceDispatcher<TTargetType, const volatile TTargetType, TReturnType, TParams...>
	{
	};

	template<class TTargetType, class TDispatchMetadataType, class TReturnType, class... TParams>
	struct TraitVTableSlotBinderBase2<TraitRefType::kStatic, TTargetType, TDispatchMetadataType, TReturnType, TypeList<TParams...>>
		: public TDispatchMetadataType::template StaticDispatcher<TTargetType, TReturnType, TParams...>
	{
	};

	template<TraitRefType TRefType, class TTargetType, class TDispatchMetadataType>
	struct TraitVTableSlotBinderBase1
		: public TraitVTableSlotBinderBase2<TRefType, TTargetType, TDispatchMetadataType, typename TDispatchMetadataType::ReturnType_t, typename TDispatchMetadataType::ParamList_t>
	{
	};


	template<class TDispatchTag, TraitRefType TRefType, class TTargetType, size_t TSlotIndex>
	struct TraitVTableSlotBinder
		: public TraitVTableSlotBinderBase1<TRefType, TTargetType, MethodDispatchMetadata<TDispatchTag, TSlotIndex> >
	{
	};

	template<class TDispatchTag, TraitRefType TRefType, class TTargetType, size_t TNumFunctions, class TSequence>
	struct TraitVTableMultiBinder
	{
	};

	template<class TDispatchTag, TraitRefType TRefType, class TTargetType, size_t TNumFunctions, size_t... TIndexes>
	struct TraitVTableMultiBinder<TDispatchTag, TRefType, TTargetType, TNumFunctions, IntList<size_t, TIndexes...>>
	{
		static const AbstractFunctionPtr_t ms_functions[TNumFunctions];
	};

	template<class TDispatchTag, TraitRefType TRefType, class TTargetType, size_t TNumFunctions, bool TIsOneFunction>
	struct TraitVTableBinderBase
	{
	};

	template<class TDispatchTag, TraitRefType TRefType, class TTargetType>
	struct TraitVTableBinderBase<TDispatchTag, TRefType, TTargetType, 1, true>
	{
		static TraitVTableReference<1> Bind();
	};

	template<class TDispatchTag, TraitRefType TRefType, class TTargetType, size_t TNumFunctions>
	struct TraitVTableBinderBase<TDispatchTag, TRefType, TTargetType, TNumFunctions, false>
	{
		static TraitVTableReference<TNumFunctions> Bind();
	};

	template<class TDispatchTag, TraitRefType TRefType, class TType, size_t TNumFunctions>
	struct TraitVTableBinder : TraitVTableBinderBase<TDispatchTag, TRefType, TType, TNumFunctions, TNumFunctions == 1>
	{
	};

	template<class TTypeTag, class TMethodID>
	class TraitVTableReferenceForTraitImpl
	{
	public:
		template<TraitRefType TRefType, class TType>
		static TraitVTableReferenceForTraitImpl<TTypeTag, TMethodID> Bind();

		template<size_t>
		AbstractFunctionPtr_t GetAt() const;

	private:
		TraitVTableReferenceForTraitImpl(const TraitVTableReference<MethodCount<TMethodID>::kValue> &vtableRef);

		TraitVTableReference<MethodCount<TMethodID>::kValue> m_vtableRef;
	};

	template<class TTrait>
	using TraitVTableReferenceForTrait = TraitVTableReferenceForTraitImpl<typename TTrait::TypeTag_t, typename TTrait::MethodID_t>;

	template<class... TTraits>
	struct TraitVTablesReferenceCollection
	{
	};

	template<class TTrait>
	struct TraitVTablesReferenceCollection<TTrait>
		: public TraitVTableReferenceForTrait<TTrait>
	{
		TraitVTablesReferenceCollection() = default;

		template<TraitRefType TRefType, class TType>
		static TraitVTablesReferenceCollection<TTrait> Bind();

	private:
		explicit TraitVTablesReferenceCollection(const TraitVTableReferenceForTrait<TTrait> &trait);
	};

	template<class TFirstTrait, class... TMoreTraits>
	struct TraitVTablesReferenceCollection<TFirstTrait, TMoreTraits...>
		: public TraitVTableReferenceForTrait<TFirstTrait>
		, public TraitVTablesReferenceCollection<TMoreTraits...>
	{
		TraitVTablesReferenceCollection() = default;

		template<TraitRefType TRefType, class TType>
		static TraitVTablesReferenceCollection<TFirstTrait, TMoreTraits...> Bind();

	private:
		TraitVTablesReferenceCollection(const TraitVTableReferenceForTrait<TFirstTrait> &firstTrait,
			const TraitVTablesReferenceCollection<TMoreTraits...> &moreTraits);
	};

	template<class TMethodEnum, class TTypeTag>
	struct TraitMethodLookup
	{
	};

	template<class TTypeTag, size_t TMethodID, class TDerived, class TParamList>
	struct TraitRefInlineMethod
	{
		int badBase;
	};

	template<class TTypeTag, size_t TMethodID, class TDerived, class... TParams>
	struct TraitRefInlineMethod<TTypeTag, TMethodID, TDerived, TypeList<TParams...>>
		: public MethodDispatchMetadata<TTypeTag, TMethodID>::template InlineMethods<TDerived, TParams...>
	{
	};

	template<class TTypeTag, size_t TMethodCount, bool TEmpty, class TDerived>
	struct TraitRefInlineMethods
	{
	};

	template<class TTypeTag, class TDerived>
	struct TraitRefInlineMethods<TTypeTag, 0, true, TDerived>
	{
	};

	template<class TTypeTag, size_t TMethodCount, class TDerived>
	struct TraitRefInlineMethods<TTypeTag, TMethodCount, false, TDerived>
		: public TraitRefInlineMethod<TTypeTag, TMethodCount - 1, TDerived, typename MethodDispatchMetadata<TTypeTag, TMethodCount - 1>::ParamList_t>
		, public TraitRefInlineMethods<TTypeTag, TMethodCount - 1, TMethodCount == 1, TDerived>
	{
	};


	template<class TDerived, class TTrait>
	struct TraitRefEmptyBase
		: public TraitRefInlineMethods<typename TTrait::TypeTag_t, MethodCount<typename TTrait::MethodID_t>::kValue, MethodCount<typename TTrait::MethodID_t>::kValue == 0, TDerived>
	{
	};

	template<class TDerived, class... TTraits>
	struct TraitRefEmptyBases
	{
	};

	template<class TDerived, class TFirstTrait, class... TMoreTraits>
	struct TraitRefEmptyBases<TDerived, TFirstTrait, TMoreTraits...>
		: public TraitRefEmptyBase<TDerived, TFirstTrait>
		, public TraitRefEmptyBases<TDerived, TMoreTraits...>
	{
	};

	template<class TDerived, class... TTraits>
	struct TraitRefBases
		: public TraitRefEmptyBases<TDerived, TTraits...>
	{
	};

	template<class... TTraits>
	struct TraitFunctionFinder
	{
	};

	template<class... TTraits>
	class TraitVTableRef
	{
	};

	struct TraitPrivateReader
	{
		template<class... TTraits>
		static void *ReadObjRef(const TraitRef<TTraits...> &traitRef);

		template<class TTrait, class... TTraits>
		static const TraitVTableReferenceForTrait<TTrait> &VTableForTrait(const TraitRef<TTraits...> &traitRef);
	};

	struct TraitMethodCallerFactory
	{
	};

	template<class TSignature>
	class TraitMethodCaller
	{
	};

	template<class TReturnType, class... TParams>
	class TraitMethodCaller<TReturnType(TParams...)>
	{
	public:
		TReturnType operator()(TParams...) const;

	private:
		friend struct TraitMethodCallerFactory;

		typedef typename TraitCallbackTypeResolver<TReturnType, TParams...>::Type_t Callback_t;

		TraitMethodCaller() = delete;
		explicit TraitMethodCaller(void *obj, Callback_t callback);

		void *m_obj;
		Callback_t m_callback;
	};
} } }


namespace rkit { namespace traits { namespace priv {
	template<size_t TNumFunctions>
	inline TraitVTableReference<TNumFunctions>::TraitVTableReference()
		: m_functions(nullptr)
	{
	}

	template<size_t TNumFunctions>
	inline TraitVTableReference<TNumFunctions>::TraitVTableReference(const AbstractFunctionPtr_t(&functions)[TNumFunctions])
		: m_functions(functions)
	{
	}

	template<size_t TNumFunctions>
	template<size_t TIndex>
	inline AbstractFunctionPtr_t TraitVTableReference<TNumFunctions>::GetAt() const
	{
		static_assert(TIndex < TNumFunctions, "Invalid index");
		return m_functions[TIndex];
	}

	inline TraitVTableReference<1>::TraitVTableReference()
		: m_function(nullptr)
	{
	}

	inline TraitVTableReference<1>::TraitVTableReference(AbstractFunctionPtr_t function)
		: m_function(function)
	{
	}

	template<size_t TIndex>
	AbstractFunctionPtr_t TraitVTableReference<1>::GetAt() const
	{
		static_assert(TIndex == 0, "Invalid index");
		return m_function;
	}

	template<class TDispatchTag, TraitRefType TRefType, class TTargetType, size_t TNumFunctions, size_t... TIndexes>
	const AbstractFunctionPtr_t TraitVTableMultiBinder<TDispatchTag, TRefType, TTargetType, TNumFunctions, IntList<size_t, TIndexes...>>::ms_functions[TNumFunctions] =
	{
		reinterpret_cast<AbstractFunctionPtr_t>(
			static_cast<typename MethodDispatchMetadata<TDispatchTag, TIndexes>::CallbackType_t>(
					TraitVTableSlotBinder<TDispatchTag, TRefType, TTargetType, TIndexes>::Dispatch
				)
			)...
	};

	template<class TDispatchTag, TraitRefType TRefType, class TTargetType>
	TraitVTableReference<1> TraitVTableBinderBase<TDispatchTag, TRefType, TTargetType, 1, true>::Bind()
	{
		const typename MethodDispatchMetadata<TDispatchTag, 0>::CallbackType_t cb = TraitVTableSlotBinder<TDispatchTag, TRefType, TTargetType, 0>::Dispatch;
		return TraitVTableReference<1>(reinterpret_cast<AbstractFunctionPtr_t>(cb));
	}

	template<class TDispatchTag, TraitRefType TRefType, class TTargetType, size_t TNumFunctions>
	TraitVTableReference<TNumFunctions> TraitVTableBinderBase<TDispatchTag, TRefType, TTargetType, TNumFunctions, false>::Bind()
	{
		typedef TraitVTableMultiBinder<TDispatchTag, TRefType, TTargetType, TNumFunctions, typename rkit::IntListCreateSequence<size_t, TNumFunctions>::Type_t> MultiBinder_t;

		return TraitVTableReference<TNumFunctions>(MultiBinder_t::ms_functions);
	}

	template<class TTypeTag, class TMethodID>
	template<TraitRefType TRefType, class TType>
	TraitVTableReferenceForTraitImpl<TTypeTag, TMethodID> TraitVTableReferenceForTraitImpl<TTypeTag, TMethodID>::Bind()
	{
		constexpr size_t kMethodCount = MethodCount<TMethodID>::kValue;
		typedef TraitVTableReference<kMethodCount> VTableRef_t;

		return TraitVTableReferenceForTraitImpl<TTypeTag, TMethodID>(TraitVTableBinder<TTypeTag, TRefType, TType, kMethodCount>::Bind());
	}


	template<class TTypeTag, class TMethodID>
	template<size_t TIndex>
	AbstractFunctionPtr_t TraitVTableReferenceForTraitImpl<TTypeTag, TMethodID>::GetAt() const
	{
		return this->m_vtableRef.template GetAt<TIndex>();
	}

	template<class TTypeTag, class TMethodID>
	TraitVTableReferenceForTraitImpl<TTypeTag, TMethodID>::TraitVTableReferenceForTraitImpl(const TraitVTableReference<MethodCount<TMethodID>::kValue> &vtableRef)
		: m_vtableRef(vtableRef)
	{
	}

	template<class TTrait>
	template<TraitRefType TRefType, class TType>
	TraitVTablesReferenceCollection<TTrait> TraitVTablesReferenceCollection<TTrait>::Bind()
	{
		return TraitVTablesReferenceCollection<TTrait>(TraitVTableReferenceForTrait<TTrait>::template Bind<TRefType, TType>());
	}

	template<class TTrait>
	TraitVTablesReferenceCollection<TTrait>::TraitVTablesReferenceCollection(const TraitVTableReferenceForTrait<TTrait> &trait)
		: TraitVTableReferenceForTrait<TTrait>(trait)
	{
	}

	template<class TFirstTrait, class... TMoreTraits>
	template<TraitRefType TRefType, class TType>
	TraitVTablesReferenceCollection<TFirstTrait, TMoreTraits...> TraitVTablesReferenceCollection<TFirstTrait, TMoreTraits...>::Bind()
	{
		return TraitVTablesReferenceCollection(
			TraitVTableReferenceForTrait<TFirstTrait>::template Bind<TRefType, TType>(),
			TraitVTablesReferenceCollection<TMoreTraits...>::template Bind<TRefType, TType>());
	}

	template<class TFirstTrait, class... TMoreTraits>
	TraitVTablesReferenceCollection<TFirstTrait, TMoreTraits...>::TraitVTablesReferenceCollection(const TraitVTableReferenceForTrait<TFirstTrait> &firstTrait,
		const TraitVTablesReferenceCollection<TMoreTraits...> &moreTraits)
		: TraitVTableReferenceForTrait<TFirstTrait>(firstTrait)
		, TraitVTablesReferenceCollection<TMoreTraits...>(moreTraits)
	{
	}


	template<class... TTraits>
	void *TraitPrivateReader::ReadObjRef(const TraitRef<TTraits...> &traitRef)
	{
		return traitRef.m_obj;
	}

	template<class TTrait, class... TTraits>
	const TraitVTableReferenceForTrait<TTrait> &TraitPrivateReader::VTableForTrait(const TraitRef<TTraits...> &traitRef)
	{
		return traitRef.m_vtables;
	}
} } }

namespace rkit { namespace traits {

	template<class TTypeTag, size_t TMethodID>
	struct MethodDispatchMetadata
	{
		// typedef TReturnType ReturnType_t;
		// typedef TypeList<...> ParameterTypes_t;
		// typedef TReturnType(...) Signature_t
	};

	template<class TMethodEnum, size_t TMethodID>
	struct MethodIDMetadata
	{
		static constexpr bool kExists = false;
	};

	template<class TMethodEnum>
	struct MethodCount
	{
		static constexpr size_t kValue = priv::MethodListCounter<TMethodEnum, MethodIDMetadata<TMethodEnum, 0>::kExists, 0>::kValue;
	};

	template<class TMethodEnum, class TTypeTag>
	struct Trait
	{
		typedef priv::TraitMethodLookup<TMethodEnum, TTypeTag> MethodLookup_t;
		typedef TMethodEnum MethodID_t;
		typedef TTypeTag TypeTag_t;
	};

	template<class... TTraits>
	class TraitRef final : public priv::TraitRefBases<TraitRef<TTraits...>, TTraits...>
	{
	public:
		friend struct priv::TraitPrivateReader;

		TraitRef() = default;

		template<class T>
		TraitRef(T &obj);

		template<class T>
		TraitRef(const T &obj);

		template<class T>
		TraitRef(const volatile T &obj);

		template<class T>
		TraitRef(volatile T &obj);

		template<class T>
		static TraitRef ForClass();

		template<class T>
		static TraitRef Nullable(T *obj);

		template<class T>
		static TraitRef Nullable(const T *obj);

		template<class T>
		static TraitRef Nullable(const volatile T *obj);

		template<class T>
		static TraitRef Nullable(volatile T *obj);

	private:
		explicit TraitRef(void *obj, const priv::TraitVTablesReferenceCollection<TTraits...> &vtables);

		void *m_obj;
		priv::TraitVTablesReferenceCollection<TTraits...> m_vtables;
	};
} }

namespace rkit { namespace traits {

	template<class... TTraits>
	template<class T>
	inline TraitRef<TTraits...>::TraitRef(T &obj)
		: m_obj(&obj)
		, m_vtables(priv::TraitVTablesReferenceCollection<TTraits...>::template Bind<TraitRefType::kNone, T>())
	{
	}

	template<class... TTraits>
	template<class T>
	inline TraitRef<TTraits...>::TraitRef(const T &obj)
		: m_obj(&obj)
		, m_vtables(priv::TraitVTablesReferenceCollection<TTraits...>::template Bind<TraitRefType::kConst, T>())
	{
	}

	template<class... TTraits>
	template<class T>
	inline TraitRef<TTraits...>::TraitRef(const volatile T &obj)
		: m_obj(&obj)
		, m_vtables(priv::TraitVTablesReferenceCollection<TTraits...>::template Bind<TraitRefType::kConstVolatile, T>())
	{
	}

	template<class... TTraits>
	template<class T>
	inline TraitRef<TTraits...>::TraitRef(volatile T &obj)
		: m_obj(&obj)
		, m_vtables(priv::TraitVTablesReferenceCollection<TTraits...>::template Bind<TraitRefType::kVolatile, T>())
	{
	}

	template<class... TTraits>
	template<class T>
	TraitRef<TTraits...> TraitRef<TTraits...>::ForClass()
	{
		return TraitRef<TTraits...>(nullptr, priv::TraitVTablesReferenceCollection<TTraits...>::template Bind<TraitRefType::kStatic, T>());
	}

	template<class... TTraits>
	template<class T>
	TraitRef<TTraits...> TraitRef<TTraits...>::Nullable(T *obj)
	{
		return (obj == nullptr) ? TraitRef() : TraitRef(*obj);
	}

	template<class... TTraits>
	template<class T>
	TraitRef<TTraits...> TraitRef<TTraits...>::Nullable(const T *obj)
	{
		return (obj == nullptr) ? TraitRef() : TraitRef(*obj);
	}

	template<class... TTraits>
	template<class T>
	TraitRef<TTraits...> TraitRef<TTraits...>::Nullable(const volatile T *obj)
	{
		return (obj == nullptr) ? TraitRef() : TraitRef(*obj);
	}

	template<class... TTraits>
	template<class T>
	TraitRef<TTraits...> TraitRef<TTraits...>::Nullable(volatile T *obj)
	{
		return (obj == nullptr) ? TraitRef() : TraitRef(*obj);
	}
} }

#define RKIT_TRAIT_FUNC_BIND_TEMPLATED(typeTag, methodEnum, methodID, methodName, signature)	\
	struct ::rkit::traits::MethodDispatchMetadata<typeTag, static_cast<size_t>(methodEnum::methodID)> : public ::rkit::traits::priv::AutoMethodMetadata<signature>\
	{\
		typedef ::rkit::traits::priv::AutoMethodMetadata<signature> AutoMetadata_t;\
		typedef typeTag TypeTag_t;\
		typedef methodEnum IDType_t;\
		static constexpr size_t kMethodID = static_cast<size_t>(IDType_t::methodID);\
		typedef ::rkit::traits::MethodDispatchMetadata<TypeTag_t, kMethodID> DispatchMetadataType_t;\
		typedef ::rkit::traits::MethodIDMetadata<IDType_t, kMethodID> IDMetadataType_t;\
		template<class TType, class TReturnType, class... TParams>\
		struct StaticDispatcher\
		{\
			static TReturnType Dispatch(void *obj, TParams... params)\
			{\
				return TType::methodName(std::forward<TParams>(params)...);\
			}\
		};\
		template<class TThisType, class TCVQualifiedThisType, class TReturnType, class... TParams>\
		struct InstanceDispatcher\
		{\
			static TReturnType Dispatch(void *obj, TParams... params)\
			{\
				TCVQualifiedThisType &self = const_cast<TCVQualifiedThisType &>(*static_cast<TThisType *>(obj));\
				return self.methodName(std::forward<TParams>(params)...);\
			}\
		};\
		template<class TDerived, class... TParams>\
		struct InlineMethods\
		{\
			inline ::rkit::traits::priv::TraitMethodCaller<signature> GetInvokerFor_ ## methodID() const\
			{\
				return ::rkit::traits::priv::TraitMethodCallerFactory::template Create<TDerived, TypeTag_t, kMethodID>(*static_cast<const TDerived *>(this));\
			}\
			inline typename AutoMetadata_t::ReturnType_t methodID(TParams... params) const\
			{\
				typedef ::rkit::traits::Trait<IDType_t, TypeTag_t> Trait_t;\
				const TDerived &derived = *static_cast<const TDerived *>(this);\
				typedef typename ::rkit::traits::priv::TraitCallbackTypeResolver<typename AutoMetadata_t::ReturnType_t, TParams...>::Type_t CallbackType_t;\
				void *obj = ::rkit::traits::priv::TraitPrivateReader::ReadObjRef(derived);\
				::rkit::traits::priv::AbstractFunctionPtr_t fptr = ::rkit::traits::priv::TraitPrivateReader::VTableForTrait<Trait_t>(derived).template GetAt<kMethodID>();\
				const CallbackType_t cb = reinterpret_cast<CallbackType_t>(fptr);\
				return cb(obj, std::forward<TParams>(params)...);\
			}\
		};\
	};\
	template<>\
	struct ::rkit::traits::MethodIDMetadata<methodEnum, static_cast<size_t>(methodEnum::methodID)>\
	{\
		static constexpr size_t kMethodID = static_cast<size_t>(methodEnum::methodID);\
		static constexpr bool kExists = true;\
	}

#define RKIT_TRAIT_FUNC_BIND(methodEnum, methodID, methodName, signature)	\
	template<> RKIT_TRAIT_FUNC_BIND_TEMPLATED(methodEnum, methodEnum, methodID, methodName, signature)

#define RKIT_TRAIT_FUNC_TEMPLATED(typeTag, methodEnum, methodName, signature)	\
	RKIT_TRAIT_FUNC_BIND_TEMPLATED(typeTag, methodEnum, methodName, methodName, signature)

#define RKIT_TRAIT_FUNC(methodEnum, methodName, signature)	\
	template<> RKIT_TRAIT_FUNC_TEMPLATED(methodEnum, methodEnum, methodName, methodName, signature)
