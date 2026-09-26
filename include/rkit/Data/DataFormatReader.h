#pragma once

#include "rkit/Core/Endian.h"
#include "rkit/Core/SpanProtos.h"
#include "rkit/Core/HashTable.h"

#include "rkit/Data/ByteBlob.h"

namespace rkit
{
	template<class TKey, class TValue, class TSize>
	class HashMap;
}

namespace rkit::data
{
	struct ContentID;
	struct ClusterRef;

	class DataFormatReader
	{
	public:
		static void ReadSpan(Span<bool> values, Span<const uint8_t> &span);

		static void ReadSpan(Span<uint8_t> values, Span<const uint8_t> &span);
		static void ReadSpan(Span<uint16_t> values, Span<const uint8_t> &span);
		static void ReadSpan(Span<uint32_t> values, Span<const uint8_t> &span);
		static void ReadSpan(Span<uint64_t> values, Span<const uint8_t> &span);

		static void ReadSpan(Span<int8_t> values, Span<const uint8_t> &span);
		static void ReadSpan(Span<int16_t> values, Span<const uint8_t> &span);
		static void ReadSpan(Span<int32_t> values, Span<const uint8_t> &span);
		static void ReadSpan(Span<int64_t> values, Span<const uint8_t> &span);

		static void ReadSpan(Span<float> values, Span<const uint8_t> &span);
		static void ReadSpan(Span<double> values, Span<const uint8_t> &span);

		static void ReadSpan(Span<ContentID> values, Span<const uint8_t> &span);

		static void ReadSpan(Span<ClusterRef> values, Span<const uint8_t> &span);

		static bool ValidateClusterRefs(Span<const ClusterRef> inlineRefs, size_t numInstances);

	private:
		template<class T>
		static void ReadEndian(Span<T> value, Span<const uint8_t> &span);

		static void ReadBytes(Span<uint8_t> values, Span<const uint8_t> &span);
	};
}

#include "rkit/Core/Endian.h"
#include "rkit/Data/ClusterRefVector.h"

namespace rkit::data
{
	inline void DataFormatReader::ReadSpan(Span<bool> values, Span<const uint8_t> &span)
	{
		 bool *RKIT_RESTRICT outPtr = values.Ptr();
		 const uint8_t *RKIT_RESTRICT inPtr = span.Ptr();

		 const size_t count = values.Count();
		 span = span.SubSpan(count);

		 for (size_t i = 0; i < count; i++)
			 outPtr[i] = (inPtr[i] != 0);
	}

	inline void DataFormatReader::ReadSpan(Span<uint8_t> values, Span<const uint8_t> &span)
	{
		ReadEndian(values, span);
	}

	inline void DataFormatReader::ReadSpan(Span<uint16_t> values, Span<const uint8_t> &span)
	{
		ReadEndian(values, span);
	}

	inline void DataFormatReader::ReadSpan(Span<uint32_t> values, Span<const uint8_t> &span)
	{
		ReadEndian(values, span);
	}

	inline void DataFormatReader::ReadSpan(Span<uint64_t> values, Span<const uint8_t> &span)
	{
		ReadEndian(values, span);
	}

	inline void DataFormatReader::ReadSpan(Span<int8_t> values, Span<const uint8_t> &span)
	{
		ReadEndian(values, span);
	}

	inline void DataFormatReader::ReadSpan(Span<int16_t> values, Span<const uint8_t> &span)
	{
		ReadEndian(values, span);
	}

	inline void DataFormatReader::ReadSpan(Span<int32_t> values, Span<const uint8_t> &span)
	{
		ReadEndian(values, span);
	}

	inline void DataFormatReader::ReadSpan(Span<int64_t> values, Span<const uint8_t> &span)
	{
		ReadEndian(values, span);
	}

	inline void DataFormatReader::ReadSpan(Span<float> values, Span<const uint8_t> &span)
	{
		ReadEndian(values, span);
	}

	inline void DataFormatReader::ReadSpan(Span<double> values, Span<const uint8_t> &span)
	{
		ReadEndian(values, span);
	}


	void DataFormatReader::ReadSpan(Span<ContentID> values, Span<const uint8_t> &span)
	{
		ReadBytes(values.ReinterpretCast<uint8_t>(), span);
	}

	void DataFormatReader::ReadSpan(Span<ClusterRef> values, Span<const uint8_t> &span)
	{
		ReadBytes(values.ReinterpretCast<uint8_t>(), span);

		for (ClusterRef &clusterRef : values)
		{
			endian::LittleUInt32_t::StaticConvertToHostOrderInPlace(clusterRef.m_baseIndex);
			endian::LittleUInt32_t::StaticConvertToHostOrderInPlace(clusterRef.m_clusters);
		}
	}

	inline bool DataFormatReader::ValidateClusterRefs(Span<const ClusterRef> inlineRefs, size_t numInstances)
	{
		const size_t numCompleteClusters = numInstances / 32;
		const uint8_t numTrailingInstances = (numInstances % 32u);

		const uint32_t lastClusterCutoff = static_cast<uint32_t>((static_cast<uint32_t>(1) << numTrailingInstances) - 1u);

		for (const ClusterRef &clusterRef : inlineRefs)
		{
			if (clusterRef.m_clusters == 0)
				return false;

			// Not in the last complete cluster
			if (clusterRef.m_baseIndex < numCompleteClusters)
				continue;

			// Has to be in the last cluster
			if (clusterRef.m_baseIndex != numCompleteClusters || clusterRef.m_clusters > lastClusterCutoff)
				return false;
		}

		return true;
	}

	template<class T>
	inline void DataFormatReader::ReadEndian(Span<T> values, Span<const uint8_t> &span)
	{
		ReadBytes(values.ReinterpretCast<uint8_t>(), span);

		for (T &value : values)
			rkit::endian::SwappableNumber<rkit::endian::LittleEndianHelper<T>>::StaticConvertToHostOrderInPlace(value);
	}

	inline void DataFormatReader::ReadBytes(Span<uint8_t> values, Span<const uint8_t> &span)
	{
		rkit::CopySpanNonOverlapping(values, span.SubSpan(0, values.Count()));
		span = span.SubSpan(values.Count());
	}
}
