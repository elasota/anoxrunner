#pragma once

#include "CoreDefs.h"

namespace rkit
{
	struct Result;
	template<class T>
	class Span;

	namespace Private
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
	}


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
	void CopySpan(const Span<T> &dest, const Span<const T> &src);

	template<class T>
	void MoveSpan(const Span<T> &dest, const Span<T> &src);
}

#include "CoreDefs.h"
#include "Result.h"
#include "Span.h"

#include <limits>
#include <new>
#include <type_traits>

template<class T>
rkit::Result rkit::SafeAdd(T &result, const T &a, const T &b)
{
	return Private::SafeMathHelper<T, std::is_integral<T>::value, std::is_signed<T>::value>::Add(result, a, b);
}

template<class T>
rkit::Result rkit::SafeSub(T &result, const T &a, const T &b)
{
	return Private::SafeMathHelper<T, std::is_integral<T>::value, std::is_signed<T>::value>::Sub(result, a, b);
}

template<class T>
rkit::Result rkit::SafeMul(T &result, const T &a, const T &b)
{
	return Private::SafeMathHelper<T, std::is_integral<T>::value, std::is_signed<T>::value>::Mul(result, a, b);
}

template<class T>
rkit::Result rkit::SafeDiv(T &result, const T &a, const T &b)
{
	T modResult;
	return Private::SafeMathHelper<T, std::is_integral<T>::value, std::is_signed<T>::value>::Div(result, modResult, a, b);
}

template<class T>
rkit::Result rkit::SafeMod(T &result, const T &a, const T &b)
{
	T divResult;
	return Private::SafeMathHelper<T, std::is_integral<T>::value, std::is_signed<T>::value>::Div(divResult, result, a, b);
}

template<class T>
rkit::Result rkit::SafeDivMod(T &divResult, T &modResult, const T &a, const T &b)
{
	return Private::SafeMathHelper<T, std::is_integral<T>::value, std::is_signed<T>::value>::Div(divResult, modResult, a, b);
}


template<class TSigned, class TUnsigned>
inline TUnsigned rkit::ToUnsignedAbs(TSigned v)
{
	return Private::ToUnsignedAbs<TSigned, TUnsigned>(v);
}


template<class T>
T rkit::Min(const T &a, const T &b)
{
	if (b > a)
		return b;
	return a;
}

template<class T>
T rkit::Max(const T &a, const T &b)
{
	if (b < a)
		return b;
	return a;
}

// Signed integers
template<class T>
rkit::Result rkit::Private::SafeMathHelper<T, true, true>::Add(T &result, T a, T b)
{
	if (a == 0 || b == 0)
	{
		result = a + b;
		return ResultCode::kOK;
	}

	// Mixed-sign adds are always okay
	if ((a < 0) != (b < 0))
	{
		result = a + b;
		return ResultCode::kOK;
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
		return ResultCode::kIntegerOverflow;

	result = a + b;
	return ResultCode::kOK;
}

template<class T>
rkit::Result rkit::Private::SafeMathHelper<T, true, true>::Sub(T &result, T a, T b)
{
	if (b == 0)
	{
		result = a;
		return ResultCode::kOK;
	}

	if (a == 0)
	{
		if (b == std::numeric_limits<T>::min())
			return ResultCode::kIntegerOverflow;

		result = -b;
		return ResultCode::kOK;
	}

	// Same-sign subtracts are always okay
	if ((a < 0) == (b < 0))
	{
		result = a - b;
		return ResultCode::kOK;
	}

	// Mixed-sign subtract
	if (a < 0)
	{
		// a is positive, b is negative
		if (b < -(a - std::numeric_limits<T>::max() - a))
			return ResultCode::kIntegerOverflow;
	}
	else
	{
		// a is negative, b is positive
		if (b > (a - std::numeric_limits<T>::min()))
			return ResultCode::kIntegerOverflow;
	}

	result = a - b;
	return ResultCode::kOK;
}

template<class T>
rkit::Result rkit::Private::SafeMathHelper<T, true, true>::Mul(T &result, T a, T b)
{
	typedef typename std::make_unsigned<T>::type UnsignedT_t;

	if (a == 0 || b == 0)
	{
		result = 0;
		return ResultCode::kOK;
	}

	bool resultIsNegative = ((a < 0) != (b < 0));

	UnsignedT_t maxResult = static_cast<UnsignedT_t>(std::numeric_limits<T>::max());
	if (resultIsNegative)
		maxResult = ToUnsignedAbs<T, UnsignedT_t>(std::numeric_limits<T>::min());

	UnsignedT_t aAbs = ToUnsignedAbs(a);
	UnsignedT_t bAbs = ToUnsignedAbs(b);

	if (bAbs > maxResult / aAbs)
		return ResultCode::kIntegerOverflow;

	result = a * b;
	return ResultCode::kOK;
}

template<class T>
rkit::Result rkit::Private::SafeMathHelper<T, true, true>::DivMod(T &divResult, T &modResult, T a, T b)
{
	if (b == 0)
		return ResultCode::kDivisionByZero;

	if (a == std::numeric_limits<T>::min() && b == -1)
		return ResultCode::kIntegerOverflow;

	divResult = a / b;
	modResult = a % b;
	return ResultCode::kOK;
}

// Unsigned integers
template<class T>
rkit::Result rkit::Private::SafeMathHelper<T, true, false>::Add(T &result, T a, T b)
{
	if (b > std::numeric_limits<T>::max() - a)
		return ResultCode::kIntegerOverflow;

	result = a + b;
	return ResultCode::kOK;
}

template<class T>
rkit::Result rkit::Private::SafeMathHelper<T, true, false>::Sub(T &result, T a, T b)
{
	if (b > a)
		return ResultCode::kIntegerOverflow;

	result = a + b;
	return ResultCode::kOK;
}

template<class T>
rkit::Result rkit::Private::SafeMathHelper<T, true, false>::Mul(T &result, T a, T b)
{
	if (a != 0 && b != 0)
	{
		if (b > std::numeric_limits<T>::max() / a)
			return ResultCode::kIntegerOverflow;
	}

	result = a * b;
	return ResultCode::kOK;
}

template<class T>
rkit::Result rkit::Private::SafeMathHelper<T, true, false>::DivMod(T &divResult, T &modResult, T a, T b)
{
	if (b == 0)
		return ResultCode::kDivisionByZero;

	divResult = a / b;
	modResult = a % b;
	return ResultCode::kOK;
}


template<class TSigned, class TUnsigned>
TUnsigned rkit::Private::ToUnsignedAbs(TSigned v)
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
void rkit::Private::AssignSpanOp<T>::Apply(T *dest, const T *src)
{
	*dest = *src;
}

template<class T>
void rkit::Private::MoveAssignSpanOp<T>::Apply(T *dest, T *src)
{
	*dest = static_cast<T &&>(*src);
}

template<class T>
void rkit::Private::CopyConstructSpanOp<T>::Apply(T *dest, const T *src)
{
	new (dest) T(*src);
}

template<class T>
void rkit::Private::MoveConstructSpanOp<T>::Apply(T *dest, T *src)
{
	new (dest) T(static_cast<T &&>(*src));
}


template<class T, class TSrc, class TOperator>
void rkit::Private::ApplyToSpanNonOverlapping(const Span<T> &dest, const Span<TSrc> &src)
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
void rkit::Private::ApplyToSpan(const Span<T> &dest, const Span<TSrc> &src)
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
void rkit::Private::SpanOpsHelper<T, false>::CopyConstructSpan(const Span<T> &dest, const Span<const T> &src)
{
	return Private::ApplyToSpanNonOverlapping<T, const T, Private::CopyConstructSpanOp<T> >(dest, src);
}

template<class T>
void rkit::Private::SpanOpsHelper<T, false>::MoveConstructSpan(const Span<T> &dest, const Span<T> &src)
{
	return Private::ApplyToSpanNonOverlapping<T, T, Private::MoveConstructSpanOp<T> >(dest, src);
}

template<class T>
void rkit::Private::SpanOpsHelper<T, false>::CopySpanNonOverlapping(const Span<T> &dest, const Span<const T> &src)
{
	return Private::ApplyToSpanNonOverlapping<T, const T, Private::AssignSpanOp<T> >(dest, src);
}

template<class T>
void rkit::Private::SpanOpsHelper<T, false>::CopySpan(const Span<T> &dest, const Span<const T> &src)
{
	return Private::ApplyToSpan<T, const T, Private::AssignSpanOp<T> >(dest, src);
}

template<class T>
void rkit::Private::SpanOpsHelper<T, false>::MoveSpan(const Span<T> &dest, const Span<T> &src)
{
	return Private::ApplyToSpanNonOverlapping<T, T, Private::MoveAssignSpanOp<T> >(dest, src);
}


template<class T>
void rkit::Private::SpanOpsHelper<T, true>::CopyConstructSpan(const Span<T> &dest, const Span<const T> &src)
{
	RKIT_ASSERT(dest.Count() == src.Count());
	memcpy(dest.Ptr(), src.Ptr(), dest.Count() * sizeof(T));
}

template<class T>
void rkit::Private::SpanOpsHelper<T, true>::MoveConstructSpan(const Span<T> &dest, const Span<T> &src)
{
	RKIT_ASSERT(dest.Count() == src.Count());
	memcpy(dest.Ptr(), src.Ptr(), dest.Count() * sizeof(T));
}

template<class T>
void rkit::Private::SpanOpsHelper<T, true>::CopySpanNonOverlapping(const Span<T> &dest, const Span<const T> &src)
{
	RKIT_ASSERT(dest.Count() == src.Count());
	memcpy(dest.Ptr(), src.Ptr(), dest.Count() * sizeof(T));
}

template<class T>
void rkit::Private::SpanOpsHelper<T, true>::CopySpan(const Span<T> &dest, const Span<const T> &src)
{
	RKIT_ASSERT(dest.Count() == src.Count());
	memmove(dest.Ptr(), src.Ptr(), dest.Count() * sizeof(T));
}

template<class T>
void rkit::Private::SpanOpsHelper<T, true>::MoveSpan(const Span<T> &dest, const Span<T> &src)
{
	RKIT_ASSERT(dest.Count() == src.Count());
	memmove(dest.Ptr(), src.Ptr(), dest.Count() * sizeof(T));
}


// Span ops
template<class T>
void rkit::CopyConstructSpan(const Span<T> &dest, const Span<const T> &src)
{
	return Private::SpanOpsHelper<T, std::is_trivially_copyable<T>::value>::CopyConstructSpan(dest, src);
}

template<class T>
void rkit::MoveConstructSpan(const Span<T> &dest, const Span<T> &src)
{
	return Private::SpanOpsHelper<T, std::is_trivially_copyable<T>::value>::MoveConstructSpan(dest, src);
}

template<class T>
void rkit::CopySpanNonOverlapping(const Span<T> &dest, const Span<const T> &src)
{
	return Private::SpanOpsHelper<T, std::is_trivially_copyable<T>::value>::CopySpanNonOverlapping(dest, src);
}

template<class T>
void rkit::CopySpan(const Span<T> &dest, const Span<const T> &src)
{
	return Private::SpanOpsHelper<T, std::is_trivially_copyable<T>::value>::CopySpan(dest, src);
}

template<class T>
void rkit::MoveSpan(const Span<T> &dest, const Span<T> &src)
{
	return Private::SpanOpsHelper<T, std::is_trivially_copyable<T>::value>::MoveSpan(dest, src);
}
