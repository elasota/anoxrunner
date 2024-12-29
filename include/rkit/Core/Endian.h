#include <cstdint>

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

			static void SetValue(uint8_t(&bytes)[sizeof(T)], T value);
			static T GetValue(const uint8_t(&bytes)[sizeof(T)]);
		};

		template<class T>
		class LittleEndianHelper
		{
		public:
			typedef typename SizedIntHelper<sizeof(T)>::Type_t IntType_t;
			typedef T Number_t;

			static void SetValue(uint8_t(&bytes)[sizeof(T)], T value);
			static T GetValue(const uint8_t(&bytes)[sizeof(T)]);
		};

		template<class THelper>
		struct SwappableNumber
		{
			typedef typename THelper::Number_t Number_t;

			SwappableNumber();
			explicit SwappableNumber(Number_t value);
			SwappableNumber(const SwappableNumber<THelper> &other) = default;
			SwappableNumber(SwappableNumber<THelper> &&other) = default;

			SwappableNumber<THelper> &operator =(const SwappableNumber<THelper> &other) = default;
			SwappableNumber<THelper> &operator =(SwappableNumber<THelper> &&other) = default;

			Number_t Get() const;

		private:
			uint8_t m_bytes[sizeof(Number_t)];
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


template<class T>
void rkit::endian::BigEndianHelper<T>::SetValue(uint8_t(&bytes)[sizeof(T)], T value)
{
	IntType_t intValue = 0;
	memcpy(&intValue, &value, sizeof(T));

	for (int i = 0; i < sizeof(T); i++)
		bytes[sizeof(T) - 1 - i] = static_cast<uint8_t>((intValue >> (i * 8)) & 0xff);
}

template<class T>
T rkit::endian::BigEndianHelper<T>::GetValue(const uint8_t(&bytes)[sizeof(T)])
{
	IntType_t intValue = 0;
	for (int i = 0; i < sizeof(T); i++)
		intValue |= static_cast<IntType_t>(static_cast<IntType_t>(bytes[sizeof(T) - 1 - i]) << (i * 8));

	T result = 0;
	memcpy(&result, &intValue, sizeof(T));

	return result;
}

template<class T>
void rkit::endian::LittleEndianHelper<T>::SetValue(uint8_t(&bytes)[sizeof(T)], T value)
{
	IntType_t intValue = 0;
	memcpy(&intValue, &value, sizeof(T));

	for (int i = 0; i < sizeof(T); i++)
		bytes[i] = static_cast<uint8_t>((intValue >> (i * 8)) & 0xff);
}

template<class T>
T rkit::endian::LittleEndianHelper<T>::GetValue(const uint8_t(&bytes)[sizeof(T)])
{
	IntType_t intValue = 0;
	for (int i = 0; i < sizeof(T); i++)
		intValue |= static_cast<IntType_t>(static_cast<IntType_t>(bytes[i]) << (i * 8));

	T result = 0;
	memcpy(&result, &intValue, sizeof(T));

	return result;
}

template<class THelper>
rkit::endian::SwappableNumber<THelper>::SwappableNumber()
{
	memset(m_bytes, 0, sizeof(Number_t));
}

template<class THelper>
rkit::endian::SwappableNumber<THelper>::SwappableNumber(Number_t value)
{
	THelper::SetValue(m_bytes, value);
}

template<class THelper>
typename rkit::endian::SwappableNumber<THelper>::Number_t rkit::endian::SwappableNumber<THelper>::Get() const
{
	return THelper::GetValue(m_bytes);
}
