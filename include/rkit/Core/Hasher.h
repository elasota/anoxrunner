#pragma once

#include "HashValue.h"

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

	template<class T, bool TIsPOD>
	struct DefaultHasher
	{
	};

	template<class T>
	struct DefaultHasher<T, true> : public BinaryHasher<T>
	{
	};

	template<class T>
	struct Hasher : public DefaultHasher<T, std::is_arithmetic<T>::value || std::is_enum<T>::value || std::is_pointer<T>::value>
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

	template<class T>
	struct DefaultSpanHasher
	{
		static HashValue_t ComputeHash(HashValue_t baseHash, const Span<const T> &values);
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
	rkit::HashValue_t hash = baseHash;
	for (const T &value : values)
		hash = Hasher<T>::ComputeHash(hash, value);

	return hash;
}
