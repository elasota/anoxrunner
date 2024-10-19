#pragma once

#include "HashValue.h"

namespace rkit
{
	template<class T>
	class Span;

	template<class T>
	struct Hasher
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
	struct BinaryHasher
	{
		static HashValue_t ComputeHash(HashValue_t baseHash, const T &value);
		static HashValue_t ComputeHash(HashValue_t baseHash, const Span<const T> &values);
	};

#define RKIT_DECLARE_BINARY_HASHER(type) \
	template<> struct ::rkit::Hasher<type> : public ::rkit::BinaryHasher<type> {}
}

RKIT_DECLARE_BINARY_HASHER(char);
RKIT_DECLARE_BINARY_HASHER(unsigned char);
RKIT_DECLARE_BINARY_HASHER(signed char);
RKIT_DECLARE_BINARY_HASHER(int);
RKIT_DECLARE_BINARY_HASHER(unsigned int);
RKIT_DECLARE_BINARY_HASHER(float);
RKIT_DECLARE_BINARY_HASHER(double);
RKIT_DECLARE_BINARY_HASHER(long);
RKIT_DECLARE_BINARY_HASHER(unsigned long);
RKIT_DECLARE_BINARY_HASHER(long long);
RKIT_DECLARE_BINARY_HASHER(unsigned long long);

#include "Drivers.h"
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
