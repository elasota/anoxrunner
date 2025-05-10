#pragma once

#include "HashValue.h"

#include <type_traits>

namespace rkit
{
	template<class T>
	class Span;

	template<class T>
	struct BinaryHasher
	{
		static HashValue_t ComputeHash(HashValue_t baseHash, const T &value);
		static HashValue_t ComputeHash(HashValue_t baseHash, const Span<const T> &values);
	};

	template<class T>
	struct DefaultSpanHasher
	{
		static HashValue_t ComputeHash(HashValue_t baseHash, const Span<const T> &values);

		template<class TOther>
		static HashValue_t ComputeHashFromOtherType(HashValue_t baseHash, const Span<const TOther> &values);

		template<class TOther>
		static HashValue_t ComputeHashFromOtherTypeWithStaticCast(HashValue_t baseHash, const Span<const TOther> &values);
	};

	template<class T, bool TIsIntegral, bool TIsUnsigned, bool TIsFloat, bool TIsEnum, bool TIsPointer>
	struct DefaultHasher
	{
	};

	// These are all promoted to uint64 to ensure proper hashing of values that get promoted

	template<>
	struct DefaultHasher<int64_t, true, false, false, false, false> : public BinaryHasher<int64_t>
	{
	};

	// Signed integers
	template<class T>
	struct DefaultHasher<T, true, false, false, false, false>
	{
		static HashValue_t ComputeHash(HashValue_t baseHash, const T &value)
		{
			return BinaryHasher<int64_t>::ComputeHash(baseHash, value);
		}

		static HashValue_t ComputeHash(HashValue_t baseHash, const Span<const T> &values)
		{
			return DefaultSpanHasher<int64_t>::ComputeHashFromOtherType<T>(baseHash, values);
		}
	};

	// Unsigned integers
	template<class T>
	struct DefaultHasher<T, true, true, false, false, false>
	{
		static HashValue_t ComputeHash(HashValue_t baseHash, const T &value)
		{
			return BinaryHasher<uint64_t>::ComputeHash(baseHash, value);
		}

		static HashValue_t ComputeHash(HashValue_t baseHash, const Span<const T> &values)
		{
			return DefaultSpanHasher<uint64_t>::ComputeHashFromOtherType<T>(baseHash, values);
		}
	};

	// Floating point
	template<class T>
	struct DefaultHasher<T, false, false, true, false, false>
	{
		static HashValue_t ComputeHash(HashValue_t baseHash, const T &value)
		{
			return BinaryHasher<double>::ComputeHash(baseHash, value);
		}

		static HashValue_t ComputeHash(HashValue_t baseHash, const Span<const T> &values)
		{
			return DefaultSpanHasher<double>::ComputeHashFromOtherType<T>(baseHash, values);
		}
	};

	// Enums
	template<class T, bool TIsUnsigned>
	struct DefaultHasher<T, false, TIsUnsigned, false, true, false> : public BinaryHasher<T>
	{
	};

	// Pointers
	template<class T>
	struct DefaultHasher<T, false, false, false, false, true> : public BinaryHasher<T>
	{
	};

	template<class T>
	struct Hasher : public DefaultHasher<T, std::is_integral<T>::value, std::is_unsigned<T>::value, std::is_floating_point<T>::value, std::is_enum<T>::value, std::is_pointer<T>::value>
	{
	};

	template<class T>
	struct Hasher<const T> : public Hasher<T>
	{
	};

	template<class T>
	struct Hasher<T&> : public Hasher<T>
	{
	};

#define RKIT_DECLARE_BINARY_HASHER(type) \
	template<> struct ::rkit::Hasher<type> : public ::rkit::BinaryHasher<type> {}
}


#include "Drivers.h"
#include "Span.h"
#include "UtilitiesDriver.h"

template<class T>
rkit::HashValue_t rkit::BinaryHasher<T>::ComputeHash(HashValue_t baseHash, const T &value)
{
	return GetDrivers().m_utilitiesDriver->ComputeHash(baseHash, &value, sizeof(value));
}

template<class T>
rkit::HashValue_t rkit::BinaryHasher<T>::ComputeHash(HashValue_t baseHash, const Span<const T> &values)
{
	return GetDrivers().m_utilitiesDriver->ComputeHash(baseHash, values.Ptr(), values.Count() * sizeof(T));
}

template<class T>
rkit::HashValue_t rkit::DefaultSpanHasher<T>::ComputeHash(HashValue_t baseHash, const Span<const T> &values)
{
	return ComputeHashFromOtherType<T>(baseHash, values);
}


template<class T>
template<class TOther>
rkit::HashValue_t rkit::DefaultSpanHasher<T>::ComputeHashFromOtherType(HashValue_t baseHash, const Span<const TOther> &values)
{
	rkit::HashValue_t hash = baseHash;
	for (const TOther &value : values)
		hash = Hasher<T>::ComputeHash(hash, value);

	return hash;
}

template<class T>
template<class TOther>
rkit::HashValue_t rkit::DefaultSpanHasher<T>::ComputeHashFromOtherTypeWithStaticCast(HashValue_t baseHash, const Span<const TOther> &values)
{
	rkit::HashValue_t hash = baseHash;
	for (const TOther &value : values)
		hash = Hasher<T>::ComputeHash(hash, static_cast<T>(value));

	return hash;
}
