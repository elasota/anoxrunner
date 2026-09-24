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

	class DataFormatWriter
	{
	public:
		static void WriteOne(Span<uint8_t> &span, uint8_t value);
		static void WriteOne(Span<uint8_t> &span, uint16_t value);
		static void WriteOne(Span<uint8_t> &span, uint32_t value);
		static void WriteOne(Span<uint8_t> &span, uint64_t value);

		static void WriteOne(Span<uint8_t> &span, int8_t value);
		static void WriteOne(Span<uint8_t> &span, int16_t value);
		static void WriteOne(Span<uint8_t> &span, int32_t value);
		static void WriteOne(Span<uint8_t> &span, int64_t value);

		static void WriteOne(Span<uint8_t> &span, float value);
		static void WriteOne(Span<uint8_t> &span, double value);

		static void WriteOne(Span<uint8_t> &span, ContentID value);

		template<class TKey, class TSize>
		static uint32_t Deduplicate(rkit::HashMap<TKey, uint32_t, TSize> &hashMap, const TKey &item);

		template<class TKey, class TSize>
		static uint32_t Deduplicate(rkit::HashMap<TKey, uint32_t, TSize> &hashMap, TKey &item);

		template<class TKey, class TSize>
		static bool CollectInstance(rkit::HashMap<const TKey *, uint32_t, TSize> &hashMap, const TKey *ptr);

		template<class TKey, class TSize>
		static bool CollectInstance(rkit::HashMap<const TKey *, uint32_t, TSize> &hashMap, TKey *ptr);

		template<size_t TSize>
		static void WriteBlobs(IWriteStream &outStream, Span<ByteBlob<TSize>> byteBlobs);

		template<size_t TSize>
		static void WriteBlobs(IWriteStream &outStream, Span<const ByteBlob<TSize>> byteBlobs);

		template<size_t TSize>
		static void WriteDeduplicated(IWriteStream &outStream, const HashMap<ByteBlob<TSize>, uint32_t> &hashMap);

		static size_t Clusterize(::rkit::Vector<::rkit::data::ByteBlob<8>> &outItems, ::rkit::Vector<uint32_t> &clusterList);

	private:
		template<class T>
		static void WriteEndian(Span<uint8_t> &span, T value);

		static void WriteBytes(Span<uint8_t> &span, Span<const uint8_t> values);
	};
}

#include "rkit/Core/Algorithm.h"
#include "rkit/Core/QuickSort.h"

namespace rkit::data
{
	inline void DataFormatWriter::WriteOne(Span<uint8_t> &span, uint8_t value)
	{
		WriteEndian(span, value);
	}

	inline void DataFormatWriter::WriteOne(Span<uint8_t> &span, uint16_t value)
	{
		WriteEndian(span, value);
	}

	inline void DataFormatWriter::DataFormatWriter::WriteOne(Span<uint8_t> &span, uint32_t value)
	{
		WriteEndian(span, value);
	}

	inline void DataFormatWriter::WriteOne(Span<uint8_t> &span, uint64_t value)
	{
		WriteEndian(span, value);
	}

	inline void DataFormatWriter::WriteOne(Span<uint8_t> &span, int8_t value)
	{
		WriteEndian(span, value);
	}

	inline void DataFormatWriter::WriteOne(Span<uint8_t> &span, int16_t value)
	{
		WriteEndian(span, value);
	}

	inline void DataFormatWriter::WriteOne(Span<uint8_t> &span, int32_t value)
	{
		WriteEndian(span, value);
	}

	inline void DataFormatWriter::WriteOne(Span<uint8_t> &span, int64_t value)
	{
		WriteEndian(span, value);
	}

	inline void DataFormatWriter::WriteOne(Span<uint8_t> &span, float value)
	{
		WriteEndian(span, value);
	}

	inline void DataFormatWriter::WriteOne(Span<uint8_t> &span, double value)
	{
		WriteEndian(span, value);
	}

	inline void DataFormatWriter::WriteOne(Span<uint8_t> &span, ContentID value)
	{
		WriteBytes(span, rkit::Span<const ContentID>(&value, 1).ReinterpretCast<const uint8_t>());
	}

	template<class T>
	inline void DataFormatWriter::WriteEndian(Span<uint8_t> &span, T value)
	{
		rkit::endian::SwappableNumber<rkit::endian::LittleEndianHelper<T>> endianNumber(value);

		WriteBytes(span, endianNumber.GetBytes().ToSpan());
	}

	inline void DataFormatWriter::WriteBytes(Span<uint8_t> &span, Span<const uint8_t> values)
	{
		rkit::CopySpanNonOverlapping(span.SubSpan(0, values.Count()), values);
		span = span.SubSpan(values.Count());
	}

	template<class TKey, class TSize>
	uint32_t DataFormatWriter::Deduplicate(rkit::HashMap<TKey, uint32_t, TSize> &hashMap, const TKey &item)
	{
		const rkit::HashValue_t hash = rkit::Hasher<TKey>::ComputeHash(0, item);

		rkit::HashMapConstIterator<TKey, uint32_t, TSize> it = hashMap.FindPrehashed(hash, item);

		if (it == hashMap.end())
		{
			if (hashMap.Count() >= std::numeric_limits<uint32_t>::max())
				RKIT_THROW(rkit::ResultCode::kIntegerOverflow);

			const uint32_t newID = static_cast<uint32_t>(hashMap.Count());

			hashMap.SetPrehashed(hash, item, newID);

			return newID;
		}
		else
			return it.Value();
	}

	template<class TKey, class TSize>
	uint32_t DataFormatWriter::Deduplicate(rkit::HashMap<TKey, uint32_t, TSize> &hashMap, TKey &item)
	{
		const TKey &constItem = item;
		return Deduplicate(hashMap, constItem);
	}

	template<class TKey, class TSize>
	bool DataFormatWriter::CollectInstance(rkit::HashMap<const TKey *, uint32_t, TSize> &hashMap, const TKey *ptr)
	{
		const rkit::HashValue_t hash = rkit::Hasher<const TKey *>::ComputeHash(0, ptr);

		if (hashMap.FindPrehashed(hash, ptr) == hashMap.end())
		{
			if (hashMap.Count() >= std::numeric_limits<uint32_t>::max())
				RKIT_THROW(rkit::ResultCode::kIntegerOverflow);

			const uint32_t newID = static_cast<uint32_t>(hashMap.Count());

			hashMap.SetPrehashed(hash, ptr, newID);

			return true;
		}
		else
			return false;
	}


	template<size_t TSize>
	void DataFormatWriter::WriteBlobs(IWriteStream &outStream, Span<ByteBlob<TSize>> byteBlobs)
	{
		Span<const ByteBlob<TSize>> constSpan = byteBlobs;
		WriteBlobs(outStream, constSpan);
	}

	template<size_t TSize>
	void DataFormatWriter::WriteBlobs(IWriteStream &outStream, Span<const ByteBlob<TSize>> byteBlobs)
	{
		if constexpr (sizeof(ByteBlob<TSize>) == TSize)
			outStream.WriteAllSpan(byteBlobs);
		else
		{
			for (const ByteBlob<TSize> &blob : byteBlobs)
				outStream.WriteAllSpan(blob.GetStaticArray().ToSpan());
		}
	}

	template<size_t TSize>
	void DataFormatWriter::WriteDeduplicated(IWriteStream &outStream, const HashMap<ByteBlob<TSize>, uint32_t> &hashMap)
	{
		Vector<ByteBlob<TSize>> contentBlobs;

		contentBlobs.Resize(hashMap.Count());
		for (const HashMapKeyValueView<ByteBlob<TSize>, const uint32_t> &kvp : hashMap)
			contentBlobs[kvp.Value()] = kvp.Key();

		WriteBlobs(outStream, contentBlobs.ToSpan());
	}

	template<class TKey, class TSize>
	bool DataFormatWriter::CollectInstance(rkit::HashMap<const TKey *, uint32_t, TSize> &hashMap, TKey *ptr)
	{
		const TKey *constPtr = ptr;
		return CollectInstance(hashMap, constPtr);
	}

	inline size_t DataFormatWriter::Clusterize(::rkit::Vector<::rkit::data::ByteBlob<8>> &outItems, ::rkit::Vector<uint32_t> &clusterList)
	{
		QuickSort(clusterList.begin(), clusterList.end());

		uint32_t currentCluster = 0;
		uint32_t currentClusterContents = 0;

		const size_t initialCount = outItems.Count();

		auto flushClusterDWords = [&outItems](uint32_t value1, uint32_t value2)
			{
				rkit::data::ByteBlob<8> blob;
				rkit::Span<uint8_t> blobSpan = blob.ModifyStaticArray().ToSpan();
				WriteOne(blobSpan, value1);
				WriteOne(blobSpan, value2);

				outItems.Append(blob);
			};

		for (uint32_t clusterItem : clusterList)
		{
			const uint32_t cluster = clusterItem / 32u;
			const int clusterSubIndex = static_cast<int>(clusterItem % 32u);

			if (cluster != currentCluster)
			{
				if (currentClusterContents != 0)
					flushClusterDWords(currentCluster, currentClusterContents);

				currentCluster = cluster;
				currentClusterContents = 0;
			}

			currentClusterContents |= (static_cast<uint32_t>(1) << clusterSubIndex);
		}

		if (currentClusterContents != 0)
			flushClusterDWords(currentCluster, currentClusterContents);

		return outItems.Count() - initialCount;
	}
}
