#pragma once

#include <cstdint>

#include "StaticArray.h"

namespace rkit
{
	namespace endian
	{
		template<int TSize>
		struct SizedIntHelper
		{
		};

		template<>
		struct SizedIntHelper<2>
		{
			typedef uint16_t Type_t;
		};

		template<>
		struct SizedIntHelper<4>
		{
			typedef uint32_t Type_t;
		};

		template<>
		struct SizedIntHelper<8>
		{
			typedef uint64_t Type_t;
		};

		template<class T>
		class BigEndianHelper
		{
		public:
			typedef typename SizedIntHelper<sizeof(T)>::Type_t IntType_t;
			typedef T Number_t;

			static void SetValue(StaticArray<uint8_t, sizeof(T)> &bytes, T value);
			static T GetValue(const StaticArray<uint8_t, sizeof(T)> &bytes);

			static void SetBits(StaticArray<uint8_t, sizeof(T)> &bytes, IntType_t value);
			static IntType_t GetBits(const StaticArray<uint8_t, sizeof(T)> &bytes);
		};

		template<class T>
		class LittleEndianHelper
		{
		public:
			typedef typename SizedIntHelper<sizeof(T)>::Type_t IntType_t;
			typedef T Number_t;

			static void SetValue(StaticArray<uint8_t, sizeof(T)> &bytes, T value);
			static T GetValue(const StaticArray<uint8_t, sizeof(T)> &bytes);

			static void SetBits(StaticArray<uint8_t, sizeof(T)> &bytes, IntType_t value);
			static IntType_t GetBits(const StaticArray<uint8_t, sizeof(T)> &bytes);
		};

		template<class THelper>
		struct SwappableNumber
		{
			typedef typename THelper::Number_t Number_t;
			typedef typename THelper::IntType_t Bits_t;

			SwappableNumber();
			explicit SwappableNumber(Number_t value);
			SwappableNumber(const SwappableNumber<THelper> &other) = default;
			SwappableNumber(SwappableNumber<THelper> &&other) = default;

			SwappableNumber<THelper> &operator =(const SwappableNumber<THelper> &other) = default;
			SwappableNumber<THelper> &operator =(SwappableNumber<THelper> &&other) = default;

			SwappableNumber<THelper> &operator =(Number_t number);

			Number_t Get() const;

			static SwappableNumber<THelper> FromBits(Bits_t bits);
			Bits_t GetBits() const;

			StaticArray<uint8_t, sizeof(Number_t)> &GetBytes();
			const StaticArray<uint8_t, sizeof(Number_t)> &GetBytes() const;

		private:
			enum class BitsSignal
			{
			};

			SwappableNumber(Bits_t bits, BitsSignal bitsSignal);

			StaticArray<uint8_t, sizeof(Number_t)> m_bytes;
		};

		typedef SwappableNumber<BigEndianHelper<uint64_t> > BigUInt64_t;
		typedef SwappableNumber<BigEndianHelper<int64_t> > BigInt64_t;
		typedef SwappableNumber<BigEndianHelper<uint32_t> > BigUInt32_t;
		typedef SwappableNumber<BigEndianHelper<int32_t> > BigInt32_t;
		typedef SwappableNumber<BigEndianHelper<uint16_t> > BigUInt16_t;
		typedef SwappableNumber<BigEndianHelper<int16_t> > BigInt16_t;
		typedef SwappableNumber<BigEndianHelper<double> > BigFloat64_t;
		typedef SwappableNumber<BigEndianHelper<float> > BigFloat32_t;

		typedef SwappableNumber<LittleEndianHelper<uint64_t> > LittleUInt64_t;
		typedef SwappableNumber<LittleEndianHelper<int64_t> > LittleInt64_t;
		typedef SwappableNumber<LittleEndianHelper<uint32_t> > LittleUInt32_t;
		typedef SwappableNumber<LittleEndianHelper<int32_t> > LittleInt32_t;
		typedef SwappableNumber<LittleEndianHelper<uint16_t> > LittleUInt16_t;
		typedef SwappableNumber<LittleEndianHelper<int16_t> > LittleInt16_t;
		typedef SwappableNumber<LittleEndianHelper<double> > LittleFloat64_t;
		typedef SwappableNumber<LittleEndianHelper<float> > LittleFloat32_t;
	}
}

#include <new>
#include <string.h>

namespace rkit { namespace endian
{
	template<class T>
	void BigEndianHelper<T>::SetValue(StaticArray<uint8_t, sizeof(T)> &bytes, T value)
	{
		IntType_t intValue = 0;
		memcpy(&intValue, &value, sizeof(T));

		SetBits(bytes, intValue);
	}

	template<class T>
	void BigEndianHelper<T>::SetBits(StaticArray<uint8_t, sizeof(T)> &bytes, IntType_t value)
	{
		for (int i = 0; i < sizeof(T); i++)
			bytes[sizeof(T) - 1 - i] = static_cast<uint8_t>((value >> (i * 8)) & 0xff);
	}

	template<class T>
	T BigEndianHelper<T>::GetValue(const StaticArray<uint8_t, sizeof(T)> &bytes)
	{
		IntType_t intValue = GetBits(bytes);

		T result = 0;
		memcpy(&result, &intValue, sizeof(T));

		return result;
	}

	template<class T>
	typename BigEndianHelper<T>::IntType_t BigEndianHelper<T>::GetBits(const StaticArray<uint8_t, sizeof(T)> &bytes)
	{
		IntType_t intValue = 0;
		for (int i = 0; i < sizeof(T); i++)
			intValue |= static_cast<IntType_t>(static_cast<IntType_t>(bytes[sizeof(T) - 1 - i]) << (i * 8));

		return intValue;
	}

	template<class T>
	void LittleEndianHelper<T>::SetValue(StaticArray<uint8_t, sizeof(T)> &bytes, T value)
	{
		IntType_t intValue = 0;
		memcpy(&intValue, &value, sizeof(T));

		SetBits(bytes, intValue);
	}

	template<class T>
	void LittleEndianHelper<T>::SetBits(StaticArray<uint8_t, sizeof(T)> &bytes, IntType_t value)
	{
		for (int i = 0; i < sizeof(T); i++)
			bytes[i] = static_cast<uint8_t>((value >> (i * 8)) & 0xff);
	}

	template<class T>
	T LittleEndianHelper<T>::GetValue(const StaticArray<uint8_t, sizeof(T)> &bytes)
	{
		IntType_t intValue = GetBits(bytes);

		T result = 0;
		memcpy(&result, &intValue, sizeof(T));

		return result;
	}

	template<class T>
	typename LittleEndianHelper<T>::IntType_t LittleEndianHelper<T>::GetBits(const StaticArray<uint8_t, sizeof(T)> &bytes)
	{
		IntType_t intValue = 0;
		for (int i = 0; i < sizeof(T); i++)
			intValue |= static_cast<IntType_t>(static_cast<IntType_t>(bytes[i]) << (i * 8));

		return intValue;
	}

	template<class THelper>
	SwappableNumber<THelper>::SwappableNumber()
	{
		THelper::SetValue(m_bytes, 0);
	}

	template<class THelper>
	SwappableNumber<THelper>::SwappableNumber(Number_t value)
	{
		THelper::SetValue(m_bytes, value);
	}

	template<class THelper>
	SwappableNumber<THelper>::SwappableNumber(Bits_t bits, BitsSignal bitsSignal)
	{
		THelper::SetBits(m_bytes, bits);
	}

	template<class THelper>
	SwappableNumber<THelper> &SwappableNumber<THelper>::operator=(typename SwappableNumber<THelper>::Number_t number)
	{
		THelper::SetValue(m_bytes, number);
		return *this;
	}

	template<class THelper>
	typename SwappableNumber<THelper>::Number_t SwappableNumber<THelper>::Get() const
	{
		return THelper::GetValue(m_bytes);
	}

	template<class THelper>
	SwappableNumber<THelper> SwappableNumber<THelper>::FromBits(Bits_t bits)
	{
		return SwappableNumber<THelper>(bits, BitsSignal());
	}

	template<class THelper>
	typename SwappableNumber<THelper>::Bits_t SwappableNumber<THelper>::GetBits() const
	{
		return THelper::GetBits(m_bytes);
	}

	template<class THelper>
	StaticArray<uint8_t, sizeof(typename SwappableNumber<THelper>::Number_t)> &SwappableNumber<THelper>::GetBytes()
	{
		return m_bytes;
	}

	template<class THelper>
	const StaticArray<uint8_t, sizeof(typename SwappableNumber<THelper>::Number_t)> &SwappableNumber<THelper>::GetBytes() const
	{
		return m_bytes;
	}
} } // rkit::endian
