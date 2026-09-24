#pragma once

#include "rkit/Core/StaticArray.h"
#include "rkit/Core/Hasher.h"
#include "rkit/Core/Span.h"

namespace rkit::data
{
	template<size_t TSize>
	class ByteBlob
	{
	public:
		bool operator==(const ByteBlob<TSize> &other);

		rkit::StaticArray<uint8_t, TSize> &ModifyStaticArray();
		const rkit::StaticArray<uint8_t, TSize> &GetStaticArray() const;

	private:
		rkit::StaticArray<uint8_t, TSize> m_bytes;
	};
}

#include "rkit/Core/Algorithm.h"

namespace rkit::data
{
	template<size_t TSize>
	bool ByteBlob<TSize>::operator==(const ByteBlob<TSize> &other)
	{
		return rkit::CompareSpansEqual(m_bytes.ToSpan(), other.m_bytes.ToSpan());
	}

	template<size_t TSize>
	rkit::StaticArray<uint8_t, TSize> &ByteBlob<TSize>::ModifyStaticArray()
	{
		return m_bytes;
	}

	template<size_t TSize>
	const rkit::StaticArray<uint8_t, TSize> &ByteBlob<TSize>::GetStaticArray() const
	{
		return m_bytes;
	}
}

namespace rkit
{
	template<size_t TSize>
	struct Hasher<data::ByteBlob<TSize>>
	{
		static HashValue_t ComputeHash(HashValue_t baseHash, const data::ByteBlob<TSize> &value);
		static HashValue_t ComputeHash(HashValue_t baseHash, const Span<const data::ByteBlob<TSize>> &values);
	};
}

namespace rkit
{
	template<size_t TSize>
	HashValue_t Hasher<data::ByteBlob<TSize>>::ComputeHash(HashValue_t baseHash, const data::ByteBlob<TSize> &value)
	{
		return rkit::BinaryHasher<uint8_t>::ComputeHash(baseHash, value.GetStaticArray().ToSpan());
	}

	template<size_t TSize>
	HashValue_t Hasher<data::ByteBlob<TSize>>::ComputeHash(HashValue_t baseHash, const Span<const data::ByteBlob<TSize>> &values)
	{
		HashValue_t result = baseHash;
		for (const data::ByteBlob<TSize> &blob : values)
			result = rkit::BinaryHasher<uint8_t>::ComputeHash(result, blob.GetStaticArray().ToSpan());

		return result;
	}
}

