#pragma once

#include "CoreDefs.h"
#include "Ordering.h"

#include <stdint.h>
#include <type_traits>

namespace rkit { namespace priv {
	template<class TType, bool TIsFloatingPoint>
	struct StrongComparerHelper
	{
	};

	template<class TType>
	struct StrongComparerHelper<TType, true>
	{
		static Ordering Compare(const TType &a, const TType &b);
	};

	template<class TType>
	struct StrongComparerHelper<TType, false>
	{
		static Ordering Compare(const TType &a, const TType &b);
	};
} }

namespace rkit
{
	template<class T>
	class Span;

	template<size_t TSize>
	struct SIntOfSize
	{
	};

	template<>
	struct SIntOfSize<1>
	{
		typedef int8_t Type_t;
	};

	template<>
	struct SIntOfSize<2>
	{
		typedef int16_t Type_t;
	};

	template<>
	struct SIntOfSize<4>
	{
		typedef int32_t Type_t;
	};

	template<>
	struct SIntOfSize<8>
	{
		typedef int64_t Type_t;
	};

	template<size_t TSize>
	struct UIntOfSize
	{
	};

	template<>
	struct UIntOfSize<1>
	{
		typedef uint8_t Type_t;
	};

	template<>
	struct UIntOfSize<2>
	{
		typedef uint16_t Type_t;
	};

	template<>
	struct UIntOfSize<4>
	{
		typedef uint32_t Type_t;
	};

	template<>
	struct UIntOfSize<8>
	{
		typedef uint64_t Type_t;
	};

	template<class TSubType>
	struct DefaultOperatorsMixin
	{
		bool operator>(const TSubType &other) const;
		bool operator<=(const TSubType &other) const;
		bool operator>=(const TSubType &other) const;
		bool operator!=(const TSubType &other) const;
	};

	template<class TLeft, class TRight>
	struct DefaultComparer
	{
		static Ordering Compare(const TLeft &a, const TRight &b);
		static bool CompareEqual(const TLeft &a, const TRight &b);
		static bool CompareNotEqual(const TLeft &a, const TRight &b);
		static bool CompareLess(const TLeft &a, const TRight &b);
		static bool CompareLessEqual(const TLeft &a, const TRight &b);
		static bool CompareGreater(const TLeft &a, const TRight &b);
		static bool CompareGreaterEqual(const TLeft &a, const TRight &b);
	};

	template<class TSubType, class TComparer>
	struct CompareWithOrderingOperatorsMixin
	{
		template<class TOther>
		bool operator<(const TOther &other) const;

		template<class TOther>
		bool operator>(const TOther &other) const;

		template<class TOther>
		bool operator<=(const TOther &other) const;

		template<class TOther>
		bool operator>=(const TOther &other) const;

		template<class TOther>
		bool operator==(const TOther &other) const;

		template<class TOther>
		bool operator!=(const TOther &other) const;
	};

	template<class TSubType, class TComparer>
	struct CompareWithPredOperatorsMixin
	{
		bool operator<(const TSubType &other) const;
		bool operator>(const TSubType &other) const;
		bool operator<=(const TSubType &other) const;
		bool operator>=(const TSubType &other) const;
		bool operator==(const TSubType &other) const;
		bool operator!=(const TSubType &other) const;
	};

	template<class TType>
	struct StrongComparer : public priv::StrongComparerHelper<TType, std::is_floating_point<TType>::value>
	{
	};

	struct DefaultComparePred
	{
		template<class TLeft, class TRight>
		Ordering operator()(const TLeft &a, const TRight &b) const
		{
			return DefaultComparer<TLeft, TRight>::Compare(a, b);
		}
	};

	struct DefaultCompareEqualPred
	{
		template<class TLeft, class TRight>
		bool operator()(const TLeft &a, const TRight &b) const
		{
			return DefaultComparer<TLeft, TRight>::CompareEqual(a, b);
		}
	};

	struct DefaultCompareNotEqualPred
	{
		template<class TLeft, class TRight>
		bool operator()(const TLeft &a, const TRight &b) const
		{
			return DefaultComparer<TLeft, TRight>::CompareNotEqual(a, b);
		}
	};

	struct DefaultCompareLessPred
	{
		template<class TLeft, class TRight>
		bool operator()(const TLeft &a, const TRight &b) const
		{
			return DefaultComparer<TLeft, TRight>::CompareLess(a, b);
		}
	};

	struct DefaultCompareLessEqualPred
	{
		template<class TLeft, class TRight>
		bool operator()(const TLeft &a, const TRight &b) const
		{
			return DefaultComparer<TLeft, TRight>::CompareLessEqual(a, b);
		}
	};

	struct DefaultCompareGreaterPred
	{
		template<class TLeft, class TRight>
		bool operator()(const TLeft &a, const TRight &b) const
		{
			return DefaultComparer<TLeft, TRight>::CompareGreater(a, b);
		}
	};

	struct DefaultCompareGreaterEqualPred
	{
		template<class TLeft, class TRight>
		bool operator()(const TLeft &a, const TRight &b) const
		{
			return DefaultComparer<TLeft, TRight>::CompareGreaterEqual(a, b);
		}
	};

	struct StrongComparePred
	{
		template<class TType>
		Ordering operator()(const TType &a, const TType &b) const
		{
			return StrongComparer<TType>::Compare(a, b);
		}
	};

	namespace priv
	{
		template<class T, bool TIsIntegral, bool TIsSigned>
		struct SafeMathHelper
		{
		};

		template<class T>
		struct SafeMathHelper<T, true, true>
		{
			static Result Add(T &result, T a, T b);
			static Result Sub(T &result, T a, T b);
			static Result Mul(T &result, T a, T b);
			static Result DivMod(T &divResult, T &modResult, T a, T b);
		};

		template<class T>
		struct SafeMathHelper<T, true, false>
		{
			static Result Add(T &result, T a, T b);
			static Result Sub(T &result, T a, T b);
			static Result Mul(T &result, T a, T b);
			static Result DivMod(T &divResult, T &modResult, T a, T b);
		};

		template<class TSigned, class TUnsigned>
		TUnsigned ToUnsignedAbs(TSigned v);

		template<class T>
		struct AssignSpanOp
		{
			static const bool kMayOverlap = true;

			static void Apply(T *dest, const T *src);
		};

		template<class T>
		struct MoveAssignSpanOp
		{
			static const bool kMayOverlap = false;

			static void Apply(T *dest, T *src);
		};

		template<class T>
		struct CopyConstructSpanOp
		{
			static const bool kMayOverlap = false;

			static void Apply(T *dest, const T *src);
		};

		template<class T>
		struct MoveConstructSpanOp
		{
			static const bool kMayOverlap = false;

			static void Apply(T *dest, T *src);
		};


		template<class T, class TSrc, class TOperator>
		void ApplyToSpanNonOverlapping(const Span<T> &dest, const Span<TSrc> &src);

		template<class T, class TSrc, class TOperator>
		void ApplyToSpan(const Span<T> &dest, const Span<TSrc> &src);

		template<class T, bool TIsTriviallyCopyable>
		struct SpanOpsHelper
		{
		};

		template<class T>
		struct SpanOpsHelper<T, true>
		{
			static void CopyConstructSpan(const Span<T> &dest, const Span<const T> &src);
			static void MoveConstructSpan(const Span<T> &dest, const Span<T> &src);
			static void CopySpanNonOverlapping(const Span<T> &dest, const Span<const T> &src);
			static void CopySpan(const Span<T> &dest, const Span<const T> &src);
			static void MoveSpan(const Span<T> &dest, const Span<T> &src);
		};

		template<class T>
		struct SpanOpsHelper<T, false>
		{
			static void CopyConstructSpan(const Span<T> &dest, const Span<const T> &src);
			static void MoveConstructSpan(const Span<T> &dest, const Span<T> &src);
			static void CopySpanNonOverlapping(const Span<T> &dest, const Span<const T> &src);
			static void CopySpan(const Span<T> &dest, const Span<const T> &src);
			static void MoveSpan(const Span<T> &dest, const Span<T> &src);
		};

		template<class T, bool TIsUnsignedIntegral>
		struct FindBitsHelper
		{
		};

		template<class T>
		struct FindBitsHelper<T, true>
		{
			static int FindLowestSetBit(T value);
			static int FindHighestSetBit(T value);
		};
	}

	template<class T>
	Result SafeAlignUp(T &result, const T &value, const T &alignment);

	template<class T>
	T AlignUp(const T &value, const T &alignment);

	template<class T>
	T DivideRoundUp(const T &dividend, const T &divisor);

	template<class T>
	Result SafeAdd(T &result, const T &a, const T &b);

	template<class T>
	Result SafeSub(T &result, const T &a, const T &b);

	template<class T>
	Result SafeMul(T &result, const T &a, const T &b);

	template<class T>
	Result SafeDiv(T &result, const T &a, const T &b);

	template<class T>
	Result SafeMod(T &result, const T &a, const T &b);

	template<class T>
	Result SafeDivMod(T &divResult, T &modResult, const T &a, const T &b);

	template<class TSigned, class TUnsigned>
	TUnsigned ToUnsignedAbs(TSigned v);

	template<class T>
	T Min(const T &a, const T &b);

	template<class T>
	T Max(const T &a, const T &b);

	template<class T>
	void CopyConstructSpan(const Span<T> &dest, const Span<const T> &src);

	template<class T>
	void MoveConstructSpan(const Span<T> &dest, const Span<T> &src);

	template<class T>
	void CopySpanNonOverlapping(const Span<T> &dest, const Span<const T> &src);

	template<class T>
	void CopySpanNonOverlapping(const Span<T> &dest, const Span<T> &src);

	template<class T>
	void CopySpan(const Span<T> &dest, const Span<const T> &src);

	template<class T>
	void MoveSpan(const Span<T> &dest, const Span<T> &src);

	template<class T, class TComparer = DefaultComparer<T, T>>
	bool CompareSpansEqual(const Span<const T> &dest, const Span<const T> &src);

	template<class T, class TComparer = DefaultComparer<T, T>>
	bool CompareSpansEqual(const Span<const T> &dest, const Span<T> &src);

	template<class T, class TComparer = DefaultComparer<T, T>>
	bool CompareSpansEqual(const Span<T> &dest, const Span<const T> &src);

	template<class T, class TComparer = DefaultComparer<T, T>>
	Ordering CompareSpans(const Span<const T> &dest, const Span<const T> &src);

	template<class T, class TComparer = DefaultComparer<T, T>>
	Ordering CompareSpans(const Span<T> &dest, const Span<const T> &src);

	template<class T, class TComparer = DefaultComparer<T, T>>
	Ordering CompareSpans(const Span<const T> &dest, const Span<T> &src);

	template<class T>
	int FindLowestSetBit(T value);

	template<class T>
	int FindHighestSetBit(T value);

	template<class T>
	void Swap(T &a, T &b);

	template<class T>
	void ReverseSpanOrder(const Span<T> &spanRef);

	bool IsASCIIWhitespace(char c);

	template<class TTo, class TFrom>
	TTo BitCast(const TFrom &from);

	template<class TTo, class TFrom>
	TTo ImplicitCast(const TFrom &from)
	{
		return from;
	}

	template<class TDest, class TSrc, class TConverter>
	void ConvertSpan(const Span<TDest> &dest, const Span<TSrc> &src, const TConverter &converter);

	template<class TDest, class TSrc, class TProcessor>
	void ProcessParallelSpans(const Span<TDest> &dest, const Span<TSrc> &src, const TProcessor &processor);

	template<class TDest, class TSrc, class TProcessor>
	Result CheckedProcessParallelSpans(const Span<TDest> &dest, const Span<TSrc> &src, const TProcessor &processor);
}

#include "Result.h"
#include "Span.h"

#include <limits>
#include <new>
#include <string.h>


template<class TType>
rkit::Ordering rkit::priv::StrongComparerHelper<TType, true>::Compare(const TType &a, const TType &b)
{
	const size_t kBitCount = sizeof(TType) * 8;

	typedef typename rkit::UIntOfSize<sizeof(TType)>::Type_t UFloatBits_t;
	typedef typename rkit::SIntOfSize<sizeof(TType)>::Type_t SFloatBits_t;
	const UFloatBits_t aBits = BitCast<UFloatBits_t>(a);
	const UFloatBits_t bBits = BitCast<UFloatBits_t>(b);

	const UFloatBits_t aSignExtension = static_cast<UFloatBits_t>(static_cast<SFloatBits_t>(aBits) >> (kBitCount - 1));
	const UFloatBits_t bSignExtension = static_cast<UFloatBits_t>(static_cast<SFloatBits_t>(bBits) >> (kBitCount - 1));

	const UFloatBits_t kSignBit = static_cast<UFloatBits_t>(1) << (kBitCount - 1);
	const UFloatBits_t aReordered = ((aBits | kSignBit) ^ aSignExtension);
	const UFloatBits_t bReordered = ((bBits | kSignBit) ^ bSignExtension);

	return rkit::DefaultComparer<UFloatBits_t, UFloatBits_t>::Compare(aReordered, bReordered);
}

template<class TType>
rkit::Ordering rkit::priv::StrongComparerHelper<TType, false>::Compare(const TType &a, const TType &b)
{
	return rkit::DefaultComparer<TType, TType>::Compare(a, b);
}

template<class T>
rkit::Result rkit::SafeAlignUp(T &result, const T &a, const T &b)
{
	const T remainder = a % b;
	if (remainder == 0)
	{
		result = a;
		RKIT_RETURN_OK;
	}

	return SafeAdd<T>(result, a, static_cast<T>(b - remainder));
}

template<class T>
T rkit::AlignUp(const T &a, const T &b)
{
	const T remainder = a % b;
	if (remainder == 0)
		return a;

	return a + static_cast<T>(b - remainder);
}

template<class T>
T rkit::DivideRoundUp(const T &dividend, const T &divisor)
{
	const T resultBase = dividend / divisor;
	const T remainder = dividend % divisor;

	if (remainder == 0)
		return resultBase;
	return resultBase + 1;
}

template<class T>
rkit::Result rkit::SafeAdd(T &result, const T &a, const T &b)
{
	return priv::SafeMathHelper<T, std::is_integral<T>::value, std::is_signed<T>::value>::Add(result, a, b);
}

template<class T>
rkit::Result rkit::SafeSub(T &result, const T &a, const T &b)
{
	return priv::SafeMathHelper<T, std::is_integral<T>::value, std::is_signed<T>::value>::Sub(result, a, b);
}

template<class T>
rkit::Result rkit::SafeMul(T &result, const T &a, const T &b)
{
	return priv::SafeMathHelper<T, std::is_integral<T>::value, std::is_signed<T>::value>::Mul(result, a, b);
}

template<class T>
rkit::Result rkit::SafeDiv(T &result, const T &a, const T &b)
{
	T modResult;
	return priv::SafeMathHelper<T, std::is_integral<T>::value, std::is_signed<T>::value>::DivMod(result, modResult, a, b);
}

template<class T>
rkit::Result rkit::SafeMod(T &result, const T &a, const T &b)
{
	T divResult;
	return priv::SafeMathHelper<T, std::is_integral<T>::value, std::is_signed<T>::value>::DivMod(divResult, result, a, b);
}

template<class T>
rkit::Result rkit::SafeDivMod(T &divResult, T &modResult, const T &a, const T &b)
{
	return priv::SafeMathHelper<T, std::is_integral<T>::value, std::is_signed<T>::value>::DivMod(divResult, modResult, a, b);
}

template<class TSigned, class TUnsigned>
inline TUnsigned rkit::ToUnsignedAbs(TSigned v)
{
	return priv::ToUnsignedAbs<TSigned, TUnsigned>(v);
}


template<class T>
T rkit::Min(const T &a, const T &b)
{
	if (b > a)
		return a;
	return b;
}

template<class T>
T rkit::Max(const T &a, const T &b)
{
	if (b < a)
		return a;
	return b;
}

template<class TLeft, class TRight>
rkit::Ordering rkit::DefaultComparer<TLeft, TRight>::Compare(const TLeft &a, const TRight &b)
{
	if (a < b)
		return rkit::Ordering::kLess;
	if (b < a)
		return rkit::Ordering::kGreater;
	if (a == b)
		return rkit::Ordering::kEqual;

	return rkit::Ordering::kUnordered;
}

template<class TLeft, class TRight>
bool rkit::DefaultComparer<TLeft, TRight>::CompareEqual(const TLeft &a, const TRight &b)
{
	return a == b;
}

template<class TLeft, class TRight>
bool rkit::DefaultComparer<TLeft, TRight>::CompareNotEqual(const TLeft &a, const TRight &b)
{
	return a != b;
}

template<class TLeft, class TRight>
bool rkit::DefaultComparer<TLeft, TRight>::CompareLess(const TLeft &a, const TRight &b)
{
	return a < b;
}

template<class TLeft, class TRight>
bool rkit::DefaultComparer<TLeft, TRight>::CompareLessEqual(const TLeft &a, const TRight &b)
{
	return a <= b;
}

template<class TLeft, class TRight>
bool rkit::DefaultComparer<TLeft, TRight>::CompareGreater(const TLeft &a, const TRight &b)
{
	return a > b;
}

template<class TLeft, class TRight>
bool rkit::DefaultComparer<TLeft, TRight>::CompareGreaterEqual(const TLeft &a, const TRight &b)
{
	return a >= b;
}

template<class TSubType, class TComparer>
template<class TOther>
bool rkit::CompareWithOrderingOperatorsMixin<TSubType, TComparer>::operator<(const TOther &other) const
{
	const rkit::Ordering ordering = TComparer::Compare(*static_cast<const TSubType *>(this), other);
	return ordering == Ordering::kLess;
}

template<class TSubType, class TComparer>
template<class TOther>
bool rkit::CompareWithOrderingOperatorsMixin<TSubType, TComparer>::operator>(const TOther &other) const
{
	const rkit::Ordering ordering = TComparer::Compare(*static_cast<const TSubType *>(this), other);
	return ordering == Ordering::kGreater;
}

template<class TSubType, class TComparer>
template<class TOther>
bool rkit::CompareWithOrderingOperatorsMixin<TSubType, TComparer>::operator<=(const TOther &other) const
{
	const rkit::Ordering ordering = TComparer::Compare(*static_cast<const TSubType *>(this), other);
	return ordering == Ordering::kLess || ordering == Ordering::kEqual;
}

template<class TSubType, class TComparer>
template<class TOther>
bool rkit::CompareWithOrderingOperatorsMixin<TSubType, TComparer>::operator>=(const TOther &other) const
{
	const rkit::Ordering ordering = TComparer::Compare(*static_cast<const TSubType *>(this), other);
	return ordering == Ordering::kGreater || ordering == Ordering::kEqual;
}

template<class TSubType, class TComparer>
template<class TOther>
bool rkit::CompareWithOrderingOperatorsMixin<TSubType, TComparer>::operator==(const TOther &other) const
{
	const rkit::Ordering ordering = TComparer::Compare(*static_cast<const TSubType *>(this), other);
	return ordering == Ordering::kEqual;
}

template<class TSubType, class TComparer>
template<class TOther>
bool rkit::CompareWithOrderingOperatorsMixin<TSubType, TComparer>::operator!=(const TOther &other) const
{
	const rkit::Ordering ordering = TComparer::Compare(*static_cast<const TSubType *>(this), other);
	return ordering == Ordering::kGreater || ordering == Ordering::kLess;
}

template<class TSubType, class TComparer>
bool rkit::CompareWithPredOperatorsMixin<TSubType, TComparer>::operator<(const TSubType &other) const
{
	return TComparer::CompareWithPred(*static_cast<const TSubType *>(this), other, DefaultCompareLessPred());
}

template<class TSubType, class TComparer>
bool rkit::CompareWithPredOperatorsMixin<TSubType, TComparer>::operator>(const TSubType &other) const
{
	return TComparer::CompareWithPred(*static_cast<const TSubType *>(this), other, DefaultCompareGreaterPred());
}

template<class TSubType, class TComparer>
bool rkit::CompareWithPredOperatorsMixin<TSubType, TComparer>::operator<=(const TSubType &other) const
{
	return TComparer::CompareWithPred(*static_cast<const TSubType *>(this), other, DefaultCompareLessEqualPred());
}

template<class TSubType, class TComparer>
bool rkit::CompareWithPredOperatorsMixin<TSubType, TComparer>::operator>=(const TSubType &other) const
{
	return TComparer::CompareWithPred(*static_cast<const TSubType *>(this), other, DefaultCompareGreaterEqualPred());
}

template<class TSubType, class TComparer>
bool rkit::CompareWithPredOperatorsMixin<TSubType, TComparer>::operator==(const TSubType &other) const
{
	return TComparer::CompareWithPred(*static_cast<const TSubType *>(this), other, DefaultCompareEqualPred());
}

template<class TSubType, class TComparer>
bool rkit::CompareWithPredOperatorsMixin<TSubType, TComparer>::operator!=(const TSubType &other) const
{
	return TComparer::CompareWithPred(*static_cast<const TSubType *>(this), other, DefaultCompareNotEqualPred());
}


// Signed integers
template<class T>
rkit::Result rkit::priv::SafeMathHelper<T, true, true>::Add(T &result, T a, T b)
{
	if (a == 0 || b == 0)
	{
		result = a + b;
		RKIT_RETURN_OK;
	}

	// Mixed-sign adds are always okay
	if ((a < 0) != (b < 0))
	{
		result = a + b;
		RKIT_RETURN_OK;
	}

	// Make both values negative
	T aTemp = a;
	T bTemp = b;

	if (a > 0)
	{
		aTemp = -aTemp;
		bTemp = -bTemp;
	}

	if (aTemp < std::numeric_limits<T>::min() - bTemp)
		RKIT_THROW(ResultCode::kIntegerOverflow);

	result = a + b;
	RKIT_RETURN_OK;
}

template<class T>
rkit::Result rkit::priv::SafeMathHelper<T, true, true>::Sub(T &result, T a, T b)
{
	if (b == 0)
	{
		result = a;
		RKIT_RETURN_OK;
	}

	if (a == 0)
	{
		if (b == std::numeric_limits<T>::min())
			RKIT_THROW(ResultCode::kIntegerOverflow);

		result = -b;
		RKIT_RETURN_OK;
	}

	// Same-sign subtracts are always okay
	if ((a < 0) == (b < 0))
	{
		result = a - b;
		RKIT_RETURN_OK;
	}

	// Mixed-sign subtract
	if (a < 0)
	{
		// a is positive, b is negative
		if (b < -(a - std::numeric_limits<T>::max() - a))
			RKIT_THROW(ResultCode::kIntegerOverflow);
	}
	else
	{
		// a is negative, b is positive
		if (b > (a - std::numeric_limits<T>::min()))
			RKIT_THROW(ResultCode::kIntegerOverflow);
	}

	result = a - b;
	RKIT_RETURN_OK;
}

template<class T>
rkit::Result rkit::priv::SafeMathHelper<T, true, true>::Mul(T &result, T a, T b)
{
	typedef typename std::make_unsigned<T>::type UnsignedT_t;

	if (a == 0 || b == 0)
	{
		result = 0;
		RKIT_RETURN_OK;
	}

	bool resultIsNegative = ((a < 0) != (b < 0));

	UnsignedT_t maxResult = static_cast<UnsignedT_t>(std::numeric_limits<T>::max());
	if (resultIsNegative)
		maxResult = ToUnsignedAbs<T, UnsignedT_t>(std::numeric_limits<T>::min());

	UnsignedT_t aAbs = ToUnsignedAbs(a);
	UnsignedT_t bAbs = ToUnsignedAbs(b);

	if (bAbs > maxResult / aAbs)
		RKIT_THROW(ResultCode::kIntegerOverflow);

	result = a * b;
	RKIT_RETURN_OK;
}

template<class T>
rkit::Result rkit::priv::SafeMathHelper<T, true, true>::DivMod(T &divResult, T &modResult, T a, T b)
{
	if (b == 0)
		RKIT_THROW(ResultCode::kDivisionByZero);

	if (a == std::numeric_limits<T>::min() && b == -1)
		RKIT_THROW(ResultCode::kIntegerOverflow);

	divResult = a / b;
	modResult = a % b;
	RKIT_RETURN_OK;
}

// Unsigned integers
template<class T>
rkit::Result rkit::priv::SafeMathHelper<T, true, false>::Add(T &result, T a, T b)
{
	if (b > std::numeric_limits<T>::max() - a)
		RKIT_THROW(ResultCode::kIntegerOverflow);

	result = a + b;
	RKIT_RETURN_OK;
}

template<class T>
rkit::Result rkit::priv::SafeMathHelper<T, true, false>::Sub(T &result, T a, T b)
{
	if (b > a)
		RKIT_THROW(ResultCode::kIntegerOverflow);

	result = a + b;
	RKIT_RETURN_OK;
}

template<class T>
rkit::Result rkit::priv::SafeMathHelper<T, true, false>::Mul(T &result, T a, T b)
{
	if (a != 0 && b != 0)
	{
		if (b > std::numeric_limits<T>::max() / a)
			RKIT_THROW(ResultCode::kIntegerOverflow);
	}

	result = a * b;
	RKIT_RETURN_OK;
}

template<class T>
rkit::Result rkit::priv::SafeMathHelper<T, true, false>::DivMod(T &divResult, T &modResult, T a, T b)
{
	if (b == 0)
		RKIT_THROW(ResultCode::kDivisionByZero);

	divResult = a / b;
	modResult = a % b;
	RKIT_RETURN_OK;
}


template<class TSigned, class TUnsigned>
TUnsigned rkit::priv::ToUnsignedAbs(TSigned v)
{
	if (v >= 0)
		return static_cast<TUnsigned>(v);

	TUnsigned u = 0;
	if (v < -std::numeric_limits<TSigned>::max())
	{
		u = static_cast<TUnsigned>(std::numeric_limits<TSigned>::max());
		v += std::numeric_limits<TSigned>::max();
	}

	u += static_cast<TUnsigned>(-v);
	return u;
}

template<class T>
void rkit::priv::AssignSpanOp<T>::Apply(T *dest, const T *src)
{
	*dest = *src;
}

template<class T>
void rkit::priv::MoveAssignSpanOp<T>::Apply(T *dest, T *src)
{
	*dest = static_cast<T &&>(*src);
}

template<class T>
void rkit::priv::CopyConstructSpanOp<T>::Apply(T *dest, const T *src)
{
	new (dest) T(*src);
}

template<class T>
void rkit::priv::MoveConstructSpanOp<T>::Apply(T *dest, T *src)
{
	new (dest) T(static_cast<T &&>(*src));
}


template<class T, class TSrc, class TOperator>
void rkit::priv::ApplyToSpanNonOverlapping(const Span<T> &dest, const Span<TSrc> &src)
{
	RKIT_STATIC_ASSERT(TOperator::kMayOverlap);

	RKIT_ASSERT(dest.Count() == src.Count());

	RKIT_ASSERT((dest.Ptr() + dest.Count() <= src.Ptr()) || (dest.Ptr() >= src.Ptr() + src.Count()));

	T * RKIT_RESTRICT destPtr = dest.Ptr();
	TSrc * RKIT_RESTRICT srcPtr = src.Ptr();

	size_t count = Min<size_t>(dest.Count(), src.Count());

	for (size_t i = 0; i < count; i++)
		TOperator::Apply(destPtr + i, srcPtr + i);
}

template<class T, class TSrc, class TOperator>
void rkit::priv::ApplyToSpan(const Span<T> &dest, const Span<TSrc> &src)
{
	RKIT_ASSERT(dest.Count() == src.Count());

	size_t count = Min<size_t>(dest.Count(), src.Count());

	T *destPtr = dest.Ptr();
	TSrc *srcPtr = src.Ptr();

	if ((destPtr + count <= srcPtr) || (destPtr >= srcPtr + count))
	{
		T * RKIT_RESTRICT destPtrR = destPtr;
		TSrc * RKIT_RESTRICT srcPtrR = srcPtr;

		for (size_t i = 0; i < count; i++)
			TOperator::Apply(destPtrR + i, srcPtrR + i);
	}
	else
	{
		RKIT_ASSERT(TOperator::kMayOverlap);

		if (destPtr < srcPtr)
		{
			for (size_t i = 0; i < count; i++)
				TOperator::Apply(destPtr + i, srcPtr + i);
		}
		else
		{
			for (size_t ri = 0; ri < count; ri++)
			{
				size_t i = (count - 1) - ri;
				TOperator::Apply(destPtr + i, srcPtr + i);
			}
		}
	}
}

template<class T>
void rkit::priv::SpanOpsHelper<T, false>::CopyConstructSpan(const Span<T> &dest, const Span<const T> &src)
{
	return priv::ApplyToSpanNonOverlapping<T, const T, priv::CopyConstructSpanOp<T> >(dest, src);
}

template<class T>
void rkit::priv::SpanOpsHelper<T, false>::MoveConstructSpan(const Span<T> &dest, const Span<T> &src)
{
	return priv::ApplyToSpanNonOverlapping<T, T, priv::MoveConstructSpanOp<T> >(dest, src);
}

template<class T>
void rkit::priv::SpanOpsHelper<T, false>::CopySpanNonOverlapping(const Span<T> &dest, const Span<const T> &src)
{
	return priv::ApplyToSpanNonOverlapping<T, const T, priv::AssignSpanOp<T> >(dest, src);
}

template<class T>
void rkit::priv::SpanOpsHelper<T, false>::CopySpan(const Span<T> &dest, const Span<const T> &src)
{
	return priv::ApplyToSpan<T, const T, priv::AssignSpanOp<T> >(dest, src);
}

template<class T>
void rkit::priv::SpanOpsHelper<T, false>::MoveSpan(const Span<T> &dest, const Span<T> &src)
{
	return priv::ApplyToSpanNonOverlapping<T, T, priv::MoveAssignSpanOp<T> >(dest, src);
}

template<class T>
int rkit::priv::FindBitsHelper<T, true>::FindLowestSetBit(T value)
{
	if (value == 0)
		return -1;

	int result = 0;
	while (((value >> result) & 1) == 0)
		result++;

	return result;
}

template<class T>
int rkit::priv::FindBitsHelper<T, true>::FindHighestSetBit(T value)
{
	if (value == 0)
		return -1;

	int result = 0;
	while ((value >> result) != 1)
		result++;

	return result;
}

template<class T>
void rkit::priv::SpanOpsHelper<T, true>::CopyConstructSpan(const Span<T> &dest, const Span<const T> &src)
{
	RKIT_ASSERT(dest.Count() == src.Count());
	memcpy(dest.Ptr(), src.Ptr(), dest.Count() * sizeof(T));
}

template<class T>
void rkit::priv::SpanOpsHelper<T, true>::MoveConstructSpan(const Span<T> &dest, const Span<T> &src)
{
	RKIT_ASSERT(dest.Count() == src.Count());
	memcpy(dest.Ptr(), src.Ptr(), dest.Count() * sizeof(T));
}

template<class T>
void rkit::priv::SpanOpsHelper<T, true>::CopySpanNonOverlapping(const Span<T> &dest, const Span<const T> &src)
{
	RKIT_ASSERT(dest.Count() == src.Count());
	memcpy(dest.Ptr(), src.Ptr(), dest.Count() * sizeof(T));
}

template<class T>
void rkit::priv::SpanOpsHelper<T, true>::CopySpan(const Span<T> &dest, const Span<const T> &src)
{
	RKIT_ASSERT(dest.Count() == src.Count());
	memmove(dest.Ptr(), src.Ptr(), dest.Count() * sizeof(T));
}

template<class T>
void rkit::priv::SpanOpsHelper<T, true>::MoveSpan(const Span<T> &dest, const Span<T> &src)
{
	RKIT_ASSERT(dest.Count() == src.Count());
	memmove(dest.Ptr(), src.Ptr(), dest.Count() * sizeof(T));
}


// Span ops
template<class T>
void rkit::CopyConstructSpan(const Span<T> &dest, const Span<const T> &src)
{
	return priv::SpanOpsHelper<T, std::is_trivially_copyable<T>::value>::CopyConstructSpan(dest, src);
}

template<class T>
void rkit::MoveConstructSpan(const Span<T> &dest, const Span<T> &src)
{
	return priv::SpanOpsHelper<T, std::is_trivially_copyable<T>::value>::MoveConstructSpan(dest, src);
}

template<class T>
void rkit::CopySpanNonOverlapping(const Span<T> &dest, const Span<const T> &src)
{
	return priv::SpanOpsHelper<T, std::is_trivially_copyable<T>::value>::CopySpanNonOverlapping(dest, src);
}

template<class T>
void rkit::CopySpanNonOverlapping(const Span<T> &dest, const Span<T> &src)
{
	Span<const T> srcConst = src;
	CopySpanNonOverlapping(dest, srcConst);
}



template<class T>
void rkit::CopySpan(const Span<T> &dest, const Span<const T> &src)
{
	return priv::SpanOpsHelper<T, std::is_trivially_copyable<T>::value>::CopySpan(dest, src);
}

template<class T>
void rkit::MoveSpan(const Span<T> &dest, const Span<T> &src)
{
	return priv::SpanOpsHelper<T, std::is_trivially_copyable<T>::value>::MoveSpan(dest, src);
}

template<class T, class TComparer>
bool rkit::CompareSpansEqual(const Span<const T> &srcA, const Span<const T> &srcB)
{
	const T *ptrsA = srcA.Ptr();
	const T *ptrsB = srcB.Ptr();
	size_t sz = srcA.Count();

	if (sz != srcB.Count())
		return false;

	for (size_t i = 0; i < sz; i++)
	{
		if (!TComparer::CompareEqual(ptrsA[i], ptrsB[i]))
			return false;
	}

	return true;
}

template<class T, class TComparer>
bool rkit::CompareSpansEqual(const Span<const T> &srcA, const Span<T> &srcB)
{
	return CompareSpansEqual(srcA, Span<const T>(srcB));
}

template<class T, class TComparer>
bool rkit::CompareSpansEqual(const Span<T> &srcA, const Span<const T> &srcB)
{
	return CompareSpansEqual(Span<const T>(srcA), srcB);
}

template<class T, class TComparer>
rkit::Ordering rkit::CompareSpans(const Span<const T> &srcA, const Span<const T> &srcB)
{
	if (srcA.Count() > srcB.Count())
		return Ordering::kLess;

	if (srcA.Count() > srcB.Count())
		return Ordering::kGreater;

	const T *ptrsA = srcA.Ptr();
	const T *ptrsB = srcB.Ptr();
	size_t sz = srcA.Count();

	for (size_t i = 0; i < sz; i++)
	{
		const Ordering cmp = TComparer::Compare(ptrsA[i], ptrsB[i]);
		if (cmp != Ordering::kEqual)
			return cmp;
	}

	return Ordering::kEqual;
}

template<class T, class TComparer>
rkit::Ordering rkit::CompareSpans(const Span<const T> &srcA, const Span<T> &srcB)
{
	return CompareSpans(srcA, Span<const T>(srcB));
}

template<class T, class TComparer>
rkit::Ordering rkit::CompareSpans(const Span<T> &srcA, const Span<const T> &srcB)
{
	return CompareSpans(Span<const T>(srcA), srcB);
}

template<class T>
int rkit::FindLowestSetBit(T value)
{
	return rkit::priv::FindBitsHelper<T, std::is_integral<T>::value &&std::is_unsigned<T>::value>::FindLowestSetBit(value);
}

template<class T>
int rkit::FindHighestSetBit(T value)
{
	return rkit::priv::FindBitsHelper<T, std::is_integral<T>::value &&std::is_unsigned<T>::value>::FindHighestSetBit(value);
}


template<class T>
void rkit::Swap(T &a, T &b)
{
	T temp = std::move(a);
	a = std::move(b);
	b = std::move(temp);
}

template<class T>
void rkit::ReverseSpanOrder(const rkit::Span<T> &span)
{
	T *elements = span.Ptr();
	size_t count = span.Count();
	size_t halfCount = count / 2;

	for (size_t i = 0; i < halfCount; i++)
		Swap(elements[i], elements[count - 1 - i]);
}

inline bool rkit::IsASCIIWhitespace(char c)
{
	return (c >= 0 && c <= ' ');
}

template<class TTo, class TFrom>
inline TTo rkit::BitCast(const TFrom &from)
{
	static_assert(std::is_trivially_constructible<TTo>::value, "Result type must be trivially constructible");

	TTo result;
	::memcpy(&result, &from, sizeof(TFrom));
	return result;
}

template<class TDest, class TSrc, class TConverter>
void rkit::ConvertSpan(const Span<TDest> &dest, const Span<TSrc> &src, const TConverter &converter)
{
	RKIT_ASSERT(dest.Count() == src.Count());

	TDest *destPtr = dest.Ptr();
	TSrc *srcPtr = src.Ptr();

	const size_t count = Min(dest.Count(), src.Count());

	for (size_t i = 0; i < count; i++)
		destPtr[i] = converter(srcPtr[i]);
}

template<class TDest, class TSrc, class TProcessor>
void rkit::ProcessParallelSpans(const Span<TDest> &dest, const Span<TSrc> &src, const TProcessor &processor)
{
	RKIT_ASSERT(dest.Count() == src.Count());

	TDest *destPtr = dest.Ptr();
	TSrc *srcPtr = src.Ptr();

	const size_t count = Min(dest.Count(), src.Count());

	for (size_t i = 0; i < count; i++)
		processor(destPtr[i], srcPtr[i]);
}

template<class TDest, class TSrc, class TProcessor>
rkit::Result rkit::CheckedProcessParallelSpans(const Span<TDest> &dest, const Span<TSrc> &src, const TProcessor &processor)
{
	RKIT_ASSERT(dest.Count() == src.Count());

	TDest *destPtr = dest.Ptr();
	TSrc *srcPtr = src.Ptr();

	const size_t count = Min(dest.Count(), src.Count());

	for (size_t i = 0; i < count; i++)
	{
		RKIT_CHECK(processor(destPtr[i], srcPtr[i]));
	}

	RKIT_RETURN_OK;
}
