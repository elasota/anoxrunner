#pragma once

#include <cstdint>

namespace rkit
{
	namespace endian
	{
		template<class T>
		class BigEndianHelper;

		template<class T>
		class LittleEndianHelper;

		template<class THelper>
		struct SwappableNumber;

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
