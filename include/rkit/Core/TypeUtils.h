#pragma once

#include <cstdint>

namespace rkit::typeutils
{
	namespace priv
	{
		template<bool TUnsigned, bool TSize8, bool TSize16, bool TSize32, bool TSize64, bool TSizeGreaterThan64>
		struct IntOfSizeHelper
		{
		};

		template<>
		struct IntOfSizeHelper<true, true, false, false, false, false>
		{
			typedef uint8_t Type_t;
		};

		template<>
		struct IntOfSizeHelper<true, true, true, false, false, false>
		{
			typedef uint16_t Type_t;
		};

		template<>
		struct IntOfSizeHelper<true, true, true, true, false, false>
		{
			typedef uint32_t Type_t;
		};

		template<>
		struct IntOfSizeHelper<true, true, true, true, true, false>
		{
			typedef uint64_t Type_t;
		};

		template<>
		struct IntOfSizeHelper<false, true, false, false, false, false>
		{
			typedef int8_t Type_t;
		};

		template<>
		struct IntOfSizeHelper<false, true, true, false, false, false>
		{
			typedef int16_t Type_t;
		};

		template<>
		struct IntOfSizeHelper<false, true, true, true, false, false>
		{
			typedef int32_t Type_t;
		};

		template<>
		struct IntOfSizeHelper<false, true, true, true, true, false>
		{
			typedef int64_t Type_t;
		};
	}

	typedef<bool TUnsigned, int TBits>
	struct IntOfAtLeastBits : public ::rkit::typeutils::priv::IntOfSizeHelper<TUnsigned, true, (TBits > 8), (TBits > 16), (TBits > 32), (TBits > 64)>::Type_t
	{
	};

	typedef<int TBits>
	struct SIntOfAtLeastBits : public IntOfAtLeastBits<false, TBits>
	{
	};

	typedef<int TBits>
	struct UIntOfAtLeastBits : public IntOfAtLeastBits<true, TBits>
	{
	};
}
