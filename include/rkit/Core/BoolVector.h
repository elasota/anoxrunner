#pragma once

#include "Vector.h"

#include <cstddef>
#include <cstdint>

namespace rkit
{
	class BoolVectorConstIterator;
	class ConstBoolSpan;
	class BoolSpan;

	template<class T>
	class Span;

	class BoolSpanLValue;

	class BoolVector
	{
	public:
		friend class BoolVectorConstIterator;

		typedef uint64_t Chunk_t;
		static const size_t kBitsPerChunk = sizeof(Chunk_t) * 8;
		static const size_t kLog2BitsPerChunk = 6;

		static const Chunk_t kFullChunkBits = ((((static_cast<Chunk_t>(1) << (kBitsPerChunk - 1)) - 1u) << 1u) | 1u);

		BoolVector();
		BoolVector(BoolVector &&other) noexcept;

		Result Resize(size_t newSize);
		Result Append(bool value);

		BoolSpanLValue operator[](size_t index);
		bool operator[](size_t index) const;
		void Set(size_t index, bool value);

		Result Duplicate(const BoolVector &other);

		BoolVector &operator=(BoolVector &&other) noexcept;

		BoolVectorConstIterator begin() const;
		BoolVectorConstIterator end() const;

		Span<Chunk_t> GetChunks();
		Span<const Chunk_t> GetChunks() const;

		BoolSpan ToSpan();
		ConstBoolSpan ToSpan() const;

		size_t Count() const;
		void Reset();

	private:
		Vector<Chunk_t> m_chunks;
		size_t m_size;
	};

	class BoolSpanLValue
	{
	public:
		BoolSpanLValue(BoolVector::Chunk_t &chunk, uint8_t bit);
		BoolSpanLValue(BoolSpanLValue &&) = default;

		bool operator=(bool value) const;
		operator bool() const;

	private:
		BoolSpanLValue() = delete;
		BoolSpanLValue(const BoolSpanLValue &) = delete;

		BoolSpanLValue &operator=(const BoolSpanLValue &) = delete;

		BoolVector::Chunk_t &m_chunk;
		BoolVector::Chunk_t m_bitMask;
	};

	class ConstBoolSpan
	{
	public:
		friend class BoolVector;

		typedef BoolVector::Chunk_t Chunk_t;

		ConstBoolSpan();
		ConstBoolSpan(const ConstBoolSpan &) = default;

		ConstBoolSpan SubSpan(size_t offset) const;
		ConstBoolSpan SubSpan(size_t offset, size_t size) const;

		BoolSpanLValue operator[](size_t index);
		bool operator[](size_t index) const;

		BoolVectorConstIterator begin() const;
		BoolVectorConstIterator end() const;

	protected:
		explicit ConstBoolSpan(Chunk_t *firstChunk, uint8_t firstBit, size_t count);

		void GetPtrTo(size_t offset, Chunk_t *&outChunk, uint8_t &outBit) const;

		static const size_t kBitsPerChunk = sizeof(Chunk_t) * 8;

		Chunk_t *m_firstChunk;
		size_t m_count;
		uint8_t m_firstBit;
	};

	class BoolSpan : public ConstBoolSpan
	{
	public:
		friend class BoolVector;

		BoolSpan() = default;
		BoolSpan(const BoolSpan &) = default;

		BoolSpan SubSpan(size_t offset) const;
		BoolSpan SubSpan(size_t offset, size_t size) const;

		BoolSpanLValue operator[](size_t index);
		bool operator[](size_t index) const;

		void SetAt(size_t index, bool value) const;

	private:
		explicit BoolSpan(const ConstBoolSpan &span);
	};

	class BoolVectorConstIterator
	{
	public:
		friend class BoolVector;
		friend class BoolSpan;

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

		bool operator*() const;

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
		: m_size(0)
	{
		static_assert((1 << kLog2BitsPerChunk) == kBitsPerChunk);
	}

	inline BoolVector::BoolVector(BoolVector &&other) noexcept
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
		else if (newSize < oldSize)
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

		RKIT_RETURN_OK;
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

		RKIT_RETURN_OK;
	}

	inline BoolSpanLValue BoolVector::operator[](size_t index)
	{
		return ToSpan()[index];
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

		RKIT_RETURN_OK;
	}

	inline BoolVector &BoolVector::operator=(BoolVector &&other) noexcept
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


	inline BoolSpan BoolVector::ToSpan()
	{
		const BoolVector *constThis = this;
		return BoolSpan(constThis->ToSpan());
	}

	inline ConstBoolSpan BoolVector::ToSpan() const
	{
		return ConstBoolSpan(const_cast<Chunk_t *>(m_chunks.GetBuffer()), 0, m_size);
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

	inline bool BoolVectorConstIterator::operator*() const
	{
		return (((*m_chunk) >> m_bit) & 1) != 0;
	}

	inline BoolSpanLValue::BoolSpanLValue(BoolVector::Chunk_t &chunk, uint8_t bit)
		: m_chunk(chunk)
		, m_bitMask(static_cast<BoolVector::Chunk_t>(1) << bit)
	{
	}

	inline bool BoolSpanLValue::operator=(bool value) const
	{
		if (value)
			m_chunk |= m_bitMask;
		else
			m_chunk &= ~m_bitMask;

		return value;
	}

	inline BoolSpanLValue::operator bool() const
	{
		return (m_chunk & m_bitMask) != 0;
	}

	inline ConstBoolSpan::ConstBoolSpan()
		: m_firstChunk(nullptr)
		, m_count(0)
		, m_firstBit(0)
	{
	}

	inline void ConstBoolSpan::GetPtrTo(size_t offset, Chunk_t *&outChunk, uint8_t &outBit) const
	{
		if (m_count == 0)
		{
			outChunk = nullptr;
			outBit = 0;
		}
		else
		{
			const uint8_t adjustedFirstBit = m_firstBit + static_cast<uint8_t>(offset % kBitsPerChunk);

			outChunk = m_firstChunk + (adjustedFirstBit / kBitsPerChunk) + offset / kBitsPerChunk;
			outBit = static_cast<uint8_t>(adjustedFirstBit % kBitsPerChunk);
		}
	}

	inline ConstBoolSpan ConstBoolSpan::SubSpan(size_t offset) const
	{
		RKIT_ASSERT(offset < m_count);

		return SubSpan(offset, m_count - offset);
	}

	inline ConstBoolSpan ConstBoolSpan::SubSpan(size_t offset, size_t size) const
	{
		RKIT_ASSERT(offset < m_count);
		RKIT_ASSERT(m_count - offset >= size);

		Chunk_t *firstChunk = nullptr;
		uint8_t firstBit = 0;
		GetPtrTo(offset, firstChunk, firstBit);

		return ConstBoolSpan(firstChunk, firstBit, size);
	}


	inline bool ConstBoolSpan::operator[](size_t index) const
	{
		const ConstBoolSpan singleBitSpan = SubSpan(index, 1);
		return ((singleBitSpan.m_firstChunk[0] >> singleBitSpan.m_firstBit) & 1) != 0;
	}

	inline BoolVectorConstIterator ConstBoolSpan::begin() const
	{
		return BoolVectorConstIterator(m_firstChunk, m_firstBit);
	}

	inline BoolVectorConstIterator ConstBoolSpan::end() const
	{
		Chunk_t *chunk = nullptr;
		uint8_t bit = 0;
		GetPtrTo(m_count, chunk, bit);

		return BoolVectorConstIterator(chunk, bit);
	}

	inline ConstBoolSpan::ConstBoolSpan(Chunk_t *firstChunk, uint8_t firstBit, size_t count)
		: m_firstChunk(firstChunk)
		, m_count(count)
		, m_firstBit(firstBit)
	{
	}

	inline BoolSpan BoolSpan::SubSpan(size_t offset) const
	{
		return BoolSpan(static_cast<const ConstBoolSpan *>(this)->SubSpan(offset));
	}

	inline BoolSpan BoolSpan::SubSpan(size_t offset, size_t size) const
	{
		return BoolSpan(static_cast<const ConstBoolSpan *>(this)->SubSpan(offset, size));
	}


	inline BoolSpanLValue BoolSpan::operator[](size_t index)
	{
		Chunk_t *chunk = nullptr;
		uint8_t bit = 0;

		GetPtrTo(index, chunk, bit);
		return BoolSpanLValue(*chunk, bit);
	}

	inline bool BoolSpan::operator[](size_t index) const
	{
		const ConstBoolSpan &constSpan = *this;

		return constSpan[index];
	}

	inline void BoolSpan::SetAt(size_t index, bool value) const
	{
		RKIT_ASSERT(index < m_count);

		Chunk_t *chunkPtr = nullptr;
		uint8_t bit = 0;
		GetPtrTo(index, chunkPtr, bit);

		const Chunk_t offsetBit = static_cast<Chunk_t>(static_cast<Chunk_t>(1) << bit);

		if (value)
			(*chunkPtr) |= offsetBit;
		else
			(*chunkPtr) &= ~offsetBit;
	}

	inline BoolSpan::BoolSpan(const ConstBoolSpan &span)
		: ConstBoolSpan(span)
	{
	}
}
