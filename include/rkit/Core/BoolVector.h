#pragma once

#include "Vector.h"

#include <cstddef>
#include <cstdint>

namespace rkit
{
	class BoolVectorConstIterator;

	template<class T>
	class Span;

	class BoolVector
	{
	public:
		friend class BoolVectorConstIterator;

		typedef uint64_t Chunk_t;
		static const size_t kBitsPerChunk = sizeof(Chunk_t) * 8;

		BoolVector();
		BoolVector(BoolVector &&other);

		Result Resize(size_t newSize);
		Result Append(bool value);

		bool operator[](size_t index) const;
		void Set(size_t index, bool value);

		Result Duplicate(const BoolVector &other);

		BoolVector &operator=(BoolVector &&other);

		BoolVectorConstIterator begin() const;
		BoolVectorConstIterator end() const;

		Span<Chunk_t> GetChunks();
		Span<const Chunk_t> GetChunks() const;

		size_t Count() const;
		void Reset();

	private:
		Vector<Chunk_t> m_chunks;
		size_t m_size;
	};

	class BoolVectorConstIterator
	{
	public:
		friend class BoolVector;

		BoolVectorConstIterator(const BoolVector::Chunk_t *chunk, int bit);
		BoolVectorConstIterator(const BoolVectorConstIterator &other);

		BoolVectorConstIterator &operator++();
		BoolVectorConstIterator operator++(int);

		template<class T>
		BoolVectorConstIterator &operator+=(T value);
		template<class T>
		BoolVectorConstIterator &operator-=(T value);

		BoolVectorConstIterator &operator=(const BoolVectorConstIterator &other);

		bool operator==(const BoolVectorConstIterator &other) const;
		bool operator!=(const BoolVectorConstIterator &other) const;

	private:
		void Increase(size_t sz);
		void Increase(ptrdiff_t diff);

		void Decrease(size_t sz);
		void Decrease(ptrdiff_t diff);

		const BoolVector::Chunk_t *m_chunk;
		int m_bit;
	};
}


#include "ImplicitNumberCast.h"

#include <utility>

namespace rkit
{
	inline BoolVector::BoolVector()
	{
	}

	inline BoolVector::BoolVector(BoolVector &&other)
		: m_chunks(std::move(other.m_chunks))
		, m_size(other.m_size)
	{
		other.m_size = 0;
	}

	inline Result BoolVector::Resize(size_t newSize)
	{
		size_t chunksRequired = newSize / kBitsPerChunk;
		if (newSize % kBitsPerChunk != 0)
			chunksRequired++;

		size_t oldSize = m_size;
		size_t oldChunkCount = m_chunks.Count();
		if (m_chunks.Count() != chunksRequired)
		{
			RKIT_CHECK(m_chunks.Resize(chunksRequired));
		}

		if (chunksRequired > oldChunkCount)
		{
			Chunk_t *chunks = m_chunks.GetBuffer();
			for (size_t i = oldChunkCount; i < chunksRequired; i++)
				chunks[i] = 0;
		}

		if (newSize < oldSize)
		{
			const size_t lastChunkResidual = newSize % kBitsPerChunk;

			if (lastChunkResidual != 0)
			{
				const size_t trailingBits = kBitsPerChunk - lastChunkResidual;

				const Chunk_t bitMask = static_cast<Chunk_t>(((static_cast<Chunk_t>(1) << trailingBits) - 1u) << lastChunkResidual);

				Chunk_t &lastChunk = m_chunks[chunksRequired - 1];

				lastChunk &= ~bitMask;
			}
		}

		m_size = newSize;

		return ResultCode::kOK;
	}

	inline Result BoolVector::Append(bool value)
	{
		const size_t oldSize = m_size;

		if (oldSize % kBitsPerChunk == 0)
		{
			RKIT_CHECK(m_chunks.Append(0));
		}

		++m_size;

		if (value)
		{
			Chunk_t &lastChunk = m_chunks[oldSize / kBitsPerChunk];
			size_t bitPos = oldSize % kBitsPerChunk;

			const Chunk_t bitMask = static_cast<Chunk_t>(static_cast<Chunk_t>(1) << bitPos);
			lastChunk |= bitMask;
		}

		return ResultCode::kOK;
	}

	inline bool BoolVector::operator[](size_t index) const
	{
		return ((m_chunks[index / kBitsPerChunk] >> (index % kBitsPerChunk)) & 1u) != 0;
	}

	inline void BoolVector::Set(size_t index, bool value)
	{
		const Chunk_t bitMask = static_cast<Chunk_t>(static_cast<Chunk_t>(1) << (index % kBitsPerChunk));

		Chunk_t &chunk = m_chunks[index / kBitsPerChunk];

		if (value)
			chunk |= bitMask;
		else
			chunk &= ~bitMask;
	}

	inline Result BoolVector::Duplicate(const BoolVector &other)
	{
		if (this != &other)
		{
			size_t newChunkCount = other.m_chunks.Count();

			if (newChunkCount == 0)
				m_chunks.Reset();
			else
			{
				RKIT_CHECK(m_chunks.Resize(other.m_chunks.Count()));

				const Chunk_t *otherChunks = other.m_chunks.GetBuffer();
				Chunk_t *thisChunks = m_chunks.GetBuffer();

				for (size_t i = 0; i < newChunkCount; i++)
					thisChunks[i] = otherChunks[i];
			}

			m_size = other.m_size;
		}

		return ResultCode::kOK;
	}

	inline BoolVector &BoolVector::operator=(BoolVector &&other)
	{
		m_chunks = std::move(other.m_chunks);
		m_size = other.m_size;

		return *this;
	}

	inline BoolVectorConstIterator BoolVector::begin() const
	{
		return BoolVectorConstIterator(m_chunks.GetBuffer(), 0);
	}

	inline BoolVectorConstIterator BoolVector::end() const
	{
		if (m_size == 0)
			return BoolVectorConstIterator(nullptr, 0);

		return BoolVectorConstIterator(m_chunks.GetBuffer() + (m_size / kBitsPerChunk), static_cast<int>(m_size % kBitsPerChunk));
	}

	inline Span<BoolVector::Chunk_t> BoolVector::GetChunks()
	{
		return m_chunks.ToSpan();
	}

	inline Span<const BoolVector::Chunk_t> BoolVector::GetChunks() const
	{
		return m_chunks.ToSpan();
	}

	inline size_t BoolVector::Count() const
	{
		return m_size;
	}

	inline void BoolVector::Reset()
	{
		m_chunks.Reset();
		m_size = 0;
	}

	inline BoolVectorConstIterator::BoolVectorConstIterator(const BoolVector::Chunk_t *chunk, int bit)
		: m_chunk(chunk)
		, m_bit(bit)
	{
	}

	inline BoolVectorConstIterator::BoolVectorConstIterator(const BoolVectorConstIterator &other)
		: m_chunk(other.m_chunk)
		, m_bit(other.m_bit)
	{
	}

	inline BoolVectorConstIterator &BoolVectorConstIterator::operator++()
	{
		(*this) += 1;
		return *this;
	}

	inline BoolVectorConstIterator BoolVectorConstIterator::operator++(int)
	{
		BoolVectorConstIterator old(*this);
		++(*this);
		return old;
	}

	template<class T>
	BoolVectorConstIterator &BoolVectorConstIterator::operator+=(T value)
	{
		Increase(ImplicitIntCast<T, ptrdiff_t, size_t>(value));
		return *this;
	}

	template<class T>
	BoolVectorConstIterator &BoolVectorConstIterator::operator-=(T value)
	{
		Decrease(ImplicitIntCast<T, ptrdiff_t, size_t>(value));
		return *this;
	}

	inline void BoolVectorConstIterator::Increase(size_t sz)
	{
		size_t completeChunks = sz / BoolVector::kBitsPerChunk;
		size_t bits = sz % BoolVector::kBitsPerChunk;

		m_chunk += completeChunks;

		m_bit += static_cast<int>(bits);
		if (m_bit >= static_cast<int>(BoolVector::kBitsPerChunk))
		{
			m_bit -= static_cast<int>(BoolVector::kBitsPerChunk);
			++m_chunk;
		}
	}

	inline void BoolVectorConstIterator::Decrease(size_t sz)
	{
		size_t completeChunks = sz / BoolVector::kBitsPerChunk;
		size_t bits = sz % BoolVector::kBitsPerChunk;

		m_chunk -= completeChunks;

		m_bit -= static_cast<int>(bits);
		if (m_bit < 0)
		{
			m_bit += static_cast<int>(BoolVector::kBitsPerChunk);
			--m_chunk;
		}
	}

	inline void BoolVectorConstIterator::Increase(ptrdiff_t diff)
	{
		if (diff > 0)
			Increase(static_cast<size_t>(diff));
		else if (diff < 0)
		{
			static_assert(sizeof(size_t) == sizeof(ptrdiff_t));

			const size_t adjusted = static_cast<size_t>(0) - static_cast<size_t>(diff);

			Decrease(static_cast<size_t>(adjusted));
		}
	}

	inline void BoolVectorConstIterator::Decrease(ptrdiff_t diff)
	{
		if (diff > 0)
			Decrease(static_cast<size_t>(diff));
		else if (diff < 0)
		{
			static_assert(sizeof(size_t) == sizeof(ptrdiff_t));

			const size_t adjusted = static_cast<size_t>(0) - static_cast<size_t>(diff);

			Increase(static_cast<size_t>(adjusted));
		}
	}

	inline BoolVectorConstIterator &BoolVectorConstIterator::operator=(const BoolVectorConstIterator &other)
	{
		m_chunk = other.m_chunk;
		m_bit = other.m_bit;

		return *this;
	}

	inline bool BoolVectorConstIterator::operator==(const BoolVectorConstIterator &other) const
	{
		return m_chunk == other.m_chunk && m_bit == other.m_bit;
	}

	inline bool BoolVectorConstIterator::operator!=(const BoolVectorConstIterator &other) const
	{
		return !((*this) == other);
	}
}
