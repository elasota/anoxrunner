#pragma once

#include "rkit/Core/Result.h"
#include "rkit/Core/TypeList.h"
#include "rkit/Sandbox/SandboxAddress.h"

namespace rkit::sandbox::priv
{
	template<size_t TArgIndex, class TArgType>
	struct ArgIndexAndType
	{
		static constexpr size_t kIndex = TArgIndex;
		typedef TArgType Type_t;
	};

	template<size_t TFirstIndex, class... TArgs>
	struct ArgIndexAndTypeListFromArgs
	{
	};

	template<>
	struct ArgIndexAndTypeListFromArgs<0>
	{
	};

	template<size_t TStartIndex, class TArg>
	struct ArgIndexAndTypeListFromArgs<TStartIndex, TArg>
	{
		typedef TypeList<ArgIndexAndType<TStartIndex, TArg>> Type_t;
	};

	template<size_t TStartIndex, class TArg, class... TMoreArgs>
	struct ArgIndexAndTypeListFromArgs<TStartIndex, TArg, TMoreArgs...>
	{
	private:
		typedef TypeList<ArgIndexAndType<TStartIndex, TArg>> FirstIAT_t;
		typedef typename ArgIndexAndTypeListFromArgs<TStartIndex + 1, TMoreArgs...>::Type_t MoreIAT_t;

	public:
		typedef typename rkit::TypeListConcat<FirstIAT_t, MoreIAT_t>::Type_t Type_t;
	};

	template<class TArgType, size_t TArgSize, bool TFitsInAddress, bool TIsFloatingPoint, bool TIsIntegral, bool TIsUnsigned>
	struct ArgConverterByType
	{
		static TArgType LoadArg(Address_t arg);
	};

	template<class TArgType, size_t TArgSize>
	struct ArgConverterByType<TArgType, TArgSize, true, false, true, true>
	{
		static TArgType LoadArg(Address_t arg);
	};

	template<class TArgType, size_t TArgSize>
	struct ArgConverterByType<TArgType, TArgSize, true, false, true, false>
	{
		static TArgType LoadArg(Address_t arg);
	};

	template<>
	struct ArgConverterByType<double, 8, true, true, false, false>
	{
		static double LoadArg(Address_t arg);
	};

	template<>
	struct ArgConverterByType<float, 4, true, true, false, false>
	{
		static float LoadArg(Address_t arg);
	};

	template<class TArgType>
	struct ArgConverter : public ArgConverterByType <
		TArgType,
		sizeof(TArgType),
		sizeof(TArgType) <= sizeof(Address_t),
		std::is_floating_point<TArgType>::value,
		std::is_integral<TArgType>::value,
		std::is_unsigned<TArgType>::value
	>
	{
	};

	template<class TArgType>
	struct ArgConverter<TArgType *>
	{
		static TArgType *LoadArg(Address_t arg);
	};

	template<class TArgIndexAndType>
	struct FunctionArgPasser
	{
		static typename TArgIndexAndType::Type_t PassArg(const Address_t *args);
	};

	template<class T>
	struct FunctionDispatcher
	{
	};

	template<class... TArgsAndIndexes>
	struct FunctionDispatcher<TypeList<TArgsAndIndexes...>>
	{
		template<class TFunction>
		static PackedResultAndExtCode Dispatch(TFunction function, const Address_t *args);
	};
}

#include <string.h>

namespace rkit::sandbox::priv
{
	template<class TArgType, size_t TArgSize>
	TArgType ArgConverterByType<TArgType, TArgSize, true, false, true, false>::LoadArg(Address_t address)
	{
		// Signed integrals
		typedef typename std::make_unsigned<TArgType>::type UnsignedType_t;

		return static_cast<TArgType>(static_cast<UnsignedType_t>(address & std::numeric_limits<UnsignedType_t>::max()));
	}

	template<class TArgType, size_t TArgSize>
	TArgType ArgConverterByType<TArgType, TArgSize, true, false, true, true>::LoadArg(Address_t address)
	{
		// Unsigned integrals
		return static_cast<TArgType>(address & std::numeric_limits<TArgType>::max());
	}

	inline double ArgConverterByType<double, 8, true, true, false, false>::LoadArg(Address_t arg)
	{
		uint64_t u64 = ArgConverterByType<uint64_t, 8, true, false, true, true>::LoadArg(arg);
		double result = 0.0;
		memcpy(&result, &u64, 8);
		return result;
	}

	inline float ArgConverterByType<float, 4, true, true, false, false>::LoadArg(Address_t arg)
	{
		uint32_t u32 = ArgConverterByType<uint32_t, 4, true, false, true, true>::LoadArg(arg);
		float result = 0.0;
		memcpy(&result, &u32, 4);
		return result;
	}

	template<class TArgType>
	TArgType *ArgConverter<TArgType *>::LoadArg(Address_t arg)
	{
		return reinterpret_cast<TArgType *>(ArgConverter<uintptr_t>::LoadArg(arg));
	}

	template<class TArgIndexAndType>
	typename TArgIndexAndType::Type_t FunctionArgPasser<TArgIndexAndType>::PassArg(const Address_t *args)
	{
		return ArgConverter<typename TArgIndexAndType::Type_t>::LoadArg(args[TArgIndexAndType::kIndex]);
	}

	template<class... TArgsAndIndexes>
	template<class TFunction>
	PackedResultAndExtCode FunctionDispatcher<TypeList<TArgsAndIndexes...>>::Dispatch(TFunction function, const Address_t *args)
	{
		return RKIT_TRY_EVAL(function(
			FunctionArgPasser<TArgsAndIndexes>::PassArg(args)...
		));
	}
}

namespace rkit::sandbox
{
	template<class... TArgs>
	PackedResultAndExtCode DispatchFunction(Result(*functionPtr)(TArgs... args), const Address_t *args) noexcept
	{
		typedef priv::ArgIndexAndTypeListFromArgs<0, TArgs...>::Type_t ArgTypesAndIndexes_t;

		return priv::FunctionDispatcher<ArgTypesAndIndexes_t>::Dispatch(functionPtr, args);
	}
}
