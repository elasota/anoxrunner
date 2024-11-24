#pragma once

#include "Hasher.h"
#include "NoCopy.h"

#include <cstdint>
#include <limits>

namespace rkit
{
	template<class TKey, class TValue, class TSize>
	class HashTableBase;

	template<class TKey, class TValue, class TSize>
	class HashMap;

	template<class TKey, class TSize>
	class HashSet;

	template<class TKey, class TValue, class TSize>
	class HashMapIterator;

	template<class TKey, class TValue, class TSize>
	class HashMapConstIterator;

	template<class TKey, class TSize>
	class HashSetIterator;

	template<class T>
	class HashMapValueContainer
	{
	public:
		HashMapValueContainer();
		explicit HashMapValueContainer(void *ptr);
		HashMapValueContainer(const HashMapValueContainer<T> &) = default;

		static const size_t kValueSize = sizeof(T);
		static const size_t kValueAlignment = alignof(T);

		void DestructValueAt(size_t index);
		void RelocateValue(size_t thisIndex, HashMapValueContainer<T> &other, size_t otherIndex);
		T *GetValuePtrAt(size_t index);
		const T *GetValuePtrAt(size_t index) const;

	private:
		T *m_values;
	};

	template<>
	class HashMapValueContainer<void>
	{
	public:
		HashMapValueContainer();
		explicit HashMapValueContainer(void *ptr);
		HashMapValueContainer(const HashMapValueContainer<void> &) = default;

		static const size_t kValueSize = 0;
		static const size_t kValueAlignment = 1;

		void DestructValueAt(size_t index);
		void RelocateValue(size_t thisIndex, HashMapValueContainer<void> &other, size_t otherIndex);
	};

	template<class TTarget, class TOriginal>
	class DefaultElementConstructor
	{
	public:
		static Result Construct(void *memory, TOriginal &&original);
		static Result Assign(TTarget &target, TOriginal &&original);
	};

	template<class TKey, class TValue>
	class HashMapKeyValueView
	{
	public:
		HashMapKeyValueView(const TKey &key, TValue &value);

		const TKey &Key() const;
		TValue &Value() const;

	private:
		HashMapKeyValueView() = delete;

		const TKey &m_key;
		TValue &m_value;
	};

	template<class TKey, class TValue, class TSize>
	class HashMapIterator
	{
		friend class HashMap<TKey, TValue, TSize>;
		friend class HashMapConstIterator<TKey, TValue, TSize>;

	public:
		HashMapIterator<TKey, TValue, TSize> operator++();
		HashMapIterator<TKey, TValue, TSize>& operator++(int);

		bool operator==(const HashMapIterator<TKey, TValue, TSize> &other) const;
		bool operator!=(const HashMapIterator<TKey, TValue, TSize> &other) const;

		bool operator==(const HashMapConstIterator<TKey, TValue, TSize> &other) const;
		bool operator!=(const HashMapConstIterator<TKey, TValue, TSize> &other) const;

		HashMapKeyValueView<TKey, TValue> operator*() const;

		const TKey &Key() const;
		TValue &Value() const;

	private:
		HashMapIterator(HashMap<TKey, TValue, TSize> &hashMap, TSize offset);

		HashMapIterator() = delete;

		void Normalize();

		HashMap<TKey, TValue, TSize> &m_hashMap;
		TSize m_offset;
	};

	template<class TKey, class TValue, class TSize>
	class HashMapConstIterator
	{
		friend class HashMap<TKey, TValue, TSize>;
		friend class HashMapIterator<TKey, TValue, TSize>;

	public:
		HashMapConstIterator(const HashMapIterator<TKey, TValue, TSize> &other);

		HashMapConstIterator<TKey, TValue, TSize> operator++();
		HashMapConstIterator<TKey, TValue, TSize> &operator++(int);

		bool operator==(const HashMapConstIterator<TKey, TValue, TSize> &other) const;
		bool operator!=(const HashMapConstIterator<TKey, TValue, TSize> &other) const;

		HashMapKeyValueView<TKey, const TValue> operator*() const;

		const TKey &Key() const;
		const TValue &Value() const;

	private:
		HashMapConstIterator(const HashMap<TKey, TValue, TSize> &hashTable, TSize offset);

		HashMapConstIterator() = delete;

		void Normalize();

		const HashMap<TKey, TValue, TSize> &m_hashMap;
		TSize m_offset;
	};

	// This uses a scheme similar to Lua, where collisions are moved out of their position
	// into new positions.  Iterators ARE invalidated upon removing entries!
	template<class TKey, class TValue, class TSize>
	class HashTableBase : public NoCopy
	{
	public:
		HashTableBase();
		explicit HashTableBase(IMallocDriver *alloc);
		HashTableBase(HashTableBase<TKey, TValue, TSize> &&other);
		~HashTableBase();

		void Clear();

		HashTableBase &operator=(HashTableBase<TKey, TValue, TSize> &&other);

	protected:
		Result Resize(TSize newCapacity);
		Result Rehash(TSize minimumCapacity);
		Result CreatePositionForNewEntry(HashValue_t newKeyHash, TSize &outPosition);
		void CreatePositionForNewEntryNoResize(HashValue_t newKeyHash, TSize &outPosition);

		void ReserveMainPosition(HashValue_t newKeyHash, TSize position);
		void ReserveNonMainPosition(HashValue_t newKeyHash, TSize initialFreePosition, TSize &newFreePosition);
		void RemoveEntryNoDestruct(TSize position);

		template<class TCandidateKey>
		bool FindKeyPosition(HashValue_t keyHash, const TCandidateKey &key, TSize &outPosition) const;

	private:
		struct NextChainAndOccupancy
		{
			TSize m_used : 1;
			TSize m_next : sizeof(TSize) * 8 - 1;
		};

	protected:
		bool GetOccupancyAt(TSize index) const;

	private:
		void SetOccupancyAt(TSize index, bool occupied);
		TSize GetMainPosition(HashValue_t hash) const;

		IMallocDriver *m_alloc;
		void *m_memoryBlob;

		HashValue_t *m_hashValues;
		NextChainAndOccupancy *m_nextAndOccupancy;

	protected:
		HashMapValueContainer<TValue> m_values;
		TKey *m_keys;
		TSize m_capacity;

	private:
		TSize m_count;
		TSize m_freePosScan;
	};

	template<class TKey, class TSize = uint32_t>
	class HashSet final : public HashTableBase<TKey, void, TSize>
	{
	public:
		HashSet() = default;
		explicit HashSet(IMallocDriver *alloc);
		HashSet(HashSet<TKey, TSize> &&other) = default;
		~HashSet() = default;

		HashSet &operator=(HashSet<TKey, TSize> &&other) = default;

		template<class TCandidateKey, class TKeyHasher = Hasher<TCandidateKey>, class TKeyConstructor = DefaultElementConstructor<TKey, std::remove_reference_t<TCandidateKey>>>
		Result Add(TCandidateKey &&key);

		template<class TCandidateKey, class TKeyHasher = Hasher<TCandidateKey>>
		bool Contains(const TCandidateKey &key) const;
	};

	template<class TKey, class TValue, class TSize = uint32_t>
	class HashMap final : public HashTableBase<TKey, TValue, TSize>
	{
		friend class HashMapIterator<TKey, TValue, TSize>;
		friend class HashMapConstIterator<TKey, TValue, TSize>;

	public:
		typedef HashMapIterator<TKey, TValue, TSize> Iterator_t;
		typedef HashMapConstIterator<TKey, TValue, TSize> ConstIterator_t;

		HashMap() = default;
		explicit HashMap(IMallocDriver *alloc);
		HashMap(HashMap<TKey, TValue, TSize> &&other) = default;

		HashMap &operator=(HashMap<TKey, TValue, TSize> &&other) = default;

		template<class TCandidateKey, class TCandidateValue, class TKeyHasher = Hasher<TCandidateKey>, class TKeyConstructor = DefaultElementConstructor<TKey, TCandidateKey>, class TValueConstructor = DefaultElementConstructor<TValue, TCandidateValue>>
		Result Set(TCandidateKey &&key, TCandidateValue &&value);

		HashMapIterator<TKey, TValue, TSize> begin();
		HashMapIterator<TKey, TValue, TSize> end();

		HashMapConstIterator<TKey, TValue, TSize> begin() const;
		HashMapConstIterator<TKey, TValue, TSize> end() const;

		template<class TCandidateKey, class TKeyHasher = Hasher<TCandidateKey>>
		HashMapIterator<TKey, TValue, TSize> Find(const TCandidateKey &key);

		template<class TCandidateKey, class TKeyHasher = Hasher<TCandidateKey>>
		HashMapConstIterator<TKey, TValue, TSize> Find(const TCandidateKey &key) const;
	};
}

#include "Drivers.h"
#include "MallocDriver.h"
#include "UtilitiesDriver.h"

#include <new>
#include <utility>


template<class T>
rkit::HashMapValueContainer<T>::HashMapValueContainer()
	: m_values(nullptr)
{
}

template<class T>
rkit::HashMapValueContainer<T>::HashMapValueContainer(void *ptr)
	: m_values(static_cast<T *>(ptr))
{
}

template<class T>
void rkit::HashMapValueContainer<T>::DestructValueAt(size_t index)
{
	m_values[index].~T();
}

template<class T>
void rkit::HashMapValueContainer<T>::RelocateValue(size_t thisIndex, HashMapValueContainer<T> &other, size_t otherIndex)
{
	T &srcItem = other.m_values[otherIndex];

	new (m_values + thisIndex) T(std::move(srcItem));
	srcItem.~T();
}

template<class T>
T *rkit::HashMapValueContainer<T>::GetValuePtrAt(size_t index)
{
	return m_values + index;
}

template<class T>
const T *rkit::HashMapValueContainer<T>::GetValuePtrAt(size_t index) const
{
	return m_values + index;
}


inline rkit::HashMapValueContainer<void>::HashMapValueContainer()
{
}

inline rkit::HashMapValueContainer<void>::HashMapValueContainer(void *ptr)
{
}

inline void rkit::HashMapValueContainer<void>::DestructValueAt(size_t index)
{
}

inline void rkit::HashMapValueContainer<void>::RelocateValue(size_t thisIndex, HashMapValueContainer<void> &other, size_t otherIndex)
{
}

// HashMapIterator
template<class TKey, class TValue, class TSize>
rkit::HashMapIterator<TKey, TValue, TSize> rkit::HashMapIterator<TKey, TValue, TSize>::operator++()
{
	m_offset++;
	this->Normalize();
	return *this;
}

template<class TKey, class TValue, class TSize>
rkit::HashMapIterator<TKey, TValue, TSize> &rkit::HashMapIterator<TKey, TValue, TSize>::operator++(int)
{
	TSize oldOffset = m_offset;

	m_offset++;
	this->Normalize();

	return HashMapIterator<TKey, TValue, TSize>(m_hashMap, oldOffset);
}

template<class TKey, class TValue, class TSize>
bool rkit::HashMapIterator<TKey, TValue, TSize>::operator==(const HashMapIterator<TKey, TValue, TSize> &other) const
{
	return m_offset == other.m_offset && (&m_hashMap) == (&other.m_hashMap);
}

template<class TKey, class TValue, class TSize>
bool rkit::HashMapIterator<TKey, TValue, TSize>::operator!=(const HashMapIterator<TKey, TValue, TSize> &other) const
{
	return !((*this) == other);
}

template<class TKey, class TValue, class TSize>
bool rkit::HashMapIterator<TKey, TValue, TSize>::operator==(const HashMapConstIterator<TKey, TValue, TSize> &other) const
{
	return HashMapConstIterator<TKey, TValue, TSize>(*this) == other;
}

template<class TKey, class TValue, class TSize>
bool rkit::HashMapIterator<TKey, TValue, TSize>::operator!=(const HashMapConstIterator<TKey, TValue, TSize> &other) const
{
	return !((*this) == other);
}

template<class TKey, class TValue, class TSize>
rkit::HashMapKeyValueView<TKey, TValue> rkit::HashMapIterator<TKey, TValue, TSize>::operator*() const
{
	return HashMapKeyValueView<TKey, TValue>(this->Key(), this->Value());
}

template<class TKey, class TValue, class TSize>
const TKey &rkit::HashMapIterator<TKey, TValue, TSize>::Key() const
{
	RKIT_ASSERT(m_offset < m_hashMap.m_capacity && m_hashMap.GetOccupancyAt(m_offset));
	return m_hashMap.m_keys[m_offset];
}

template<class TKey, class TValue, class TSize>
TValue &rkit::HashMapIterator<TKey, TValue, TSize>::Value() const
{
	RKIT_ASSERT(m_offset < m_hashMap.m_capacity && m_hashMap.GetOccupancyAt(m_offset));
	return *m_hashMap.m_values.GetValuePtrAt(m_offset);
}

template<class TKey, class TValue, class TSize>
rkit::HashMapIterator<TKey, TValue, TSize>::HashMapIterator(HashMap<TKey, TValue, TSize> &hashMap, TSize offset)
	: m_hashMap(hashMap)
	, m_offset(offset)
{
}

template<class TKey, class TValue, class TSize>
void rkit::HashMapIterator<TKey, TValue, TSize>::Normalize()
{
	TSize capacity = m_hashMap.m_capacity;
	TSize offset = m_offset;

	while (offset < capacity)
	{
		if (m_hashMap.GetOccupancyAt(offset))
			break;
	}

	m_offset = offset;
}

// HashMapConstIterator
template<class TKey, class TValue, class TSize>
rkit::HashMapConstIterator<TKey, TValue, TSize>::HashMapConstIterator(const HashMapIterator<TKey, TValue, TSize> &other)
	: m_hashMap(other.m_hashMap)
	, m_offset(other.m_offset)
{
}

template<class TKey, class TValue, class TSize>
rkit::HashMapConstIterator<TKey, TValue, TSize> rkit::HashMapConstIterator<TKey, TValue, TSize>::operator++()
{
	m_offset++;
	this->Normalize();
	return *this;
}

template<class TKey, class TValue, class TSize>
 rkit::HashMapConstIterator<TKey, TValue, TSize> &rkit::HashMapConstIterator<TKey, TValue, TSize>::operator++(int)
{
	TSize oldOffset = m_offset;

	m_offset++;
	this->Normalize();

	return HashMapConstIterator<TKey, TValue, TSize>(m_hashMap, oldOffset);
}

template<class TKey, class TValue, class TSize>
bool rkit::HashMapConstIterator<TKey, TValue, TSize>::operator==(const HashMapConstIterator<TKey, TValue, TSize> &other) const
{
	return m_offset == other.m_offset && (&m_hashMap) == (&other.m_hashMap);
}

template<class TKey, class TValue, class TSize>
bool rkit::HashMapConstIterator<TKey, TValue, TSize>::operator!=(const HashMapConstIterator<TKey, TValue, TSize> &other) const
{
	return !((*this) == other);
}

template<class TKey, class TValue, class TSize>
rkit::HashMapKeyValueView<TKey, const TValue> rkit::HashMapConstIterator<TKey, TValue, TSize>::operator*() const
{
	return HashMapKeyValueView<TKey, const TValue>(this->Key(), this->Value());
}

template<class TKey, class TValue, class TSize>
const TKey &rkit::HashMapConstIterator<TKey, TValue, TSize>::Key() const
{
	RKIT_ASSERT(m_offset < m_hashMap.m_capacity && m_hashMap.GetOccupancyAt(m_offset));
	return m_hashMap.m_keys[m_offset];
}

template<class TKey, class TValue, class TSize>
const TValue &rkit::HashMapConstIterator<TKey, TValue, TSize>::Value() const
{
	RKIT_ASSERT(m_offset < m_hashMap.m_capacity && m_hashMap.GetOccupancyAt(m_offset));
	return *m_hashMap.m_values.GetValuePtrAt(m_offset);
}

template<class TKey, class TValue, class TSize>
rkit::HashMapConstIterator<TKey, TValue, TSize>::HashMapConstIterator(const HashMap<TKey, TValue, TSize> &hashMap, TSize offset)
	: m_hashMap(hashMap)
	, m_offset(offset)
{
}

template<class TKey, class TValue, class TSize>
void rkit::HashMapConstIterator<TKey, TValue, TSize>::Normalize()
{
	TSize capacity = m_hashMap.m_capacity;
	TSize offset = m_offset;

	while (offset < capacity)
	{
		if (m_hashMap.GetOccupancyAt(offset))
			break;
	}

	m_offset = offset;
}

// HashTableBase
template<class TKey, class TValue, class TSize>
rkit::HashTableBase<TKey, TValue, TSize>::HashTableBase()
	: m_alloc(rkit::GetDrivers().m_mallocDriver)
	, m_memoryBlob(nullptr)
	, m_nextAndOccupancy(nullptr)
	, m_keys(nullptr)
	, m_hashValues(nullptr)
	, m_capacity(0)
	, m_count(0)
	, m_freePosScan(0)
{
}

template<class TKey, class TValue, class TSize>
rkit::HashTableBase<TKey, TValue, TSize>::~HashTableBase()
{
	Clear();
}

template<class TKey, class TValue, class TSize>
rkit::HashTableBase<TKey, TValue, TSize>::HashTableBase(HashTableBase<TKey, TValue, TSize> &&other)
	: m_values(std::move(other.m_values))
	, m_memoryBlob(other.m_memoryBlob)
	, m_nextAndOccupancy(other.m_nextAndOccupancy)
	, m_keys(other.m_keys)
	, m_hashValues(other.m_hashValues)
	, m_capacity(other.m_capacity)
	, m_count(other.m_count)
	, m_freePosScan(other.m_freePosScan)
{
	other.m_memoryBlob = nullptr;
	other.m_nextAndOccupancy = nullptr;
	other.m_keys = nullptr;
	other.m_hashValues = nullptr;
	other.m_capacity = 0;
	other.m_count = 0;
	other.m_freePosScan = 0;
}

template<class TKey, class TValue, class TSize>
void rkit::HashTableBase<TKey, TValue, TSize>::Clear()
{
	const NextChainAndOccupancy *nextAndOccupancy = m_nextAndOccupancy;
	TKey *keys = m_keys;
	HashMapValueContainer<TValue> values(m_values);
	size_t capacity = m_capacity;

	for (TSize i = 0; i < capacity; i++)
	{
		if (nextAndOccupancy[i].m_used)
		{
			keys[i].~TKey();
			values.DestructValueAt(i);
		}
	}

	if (m_memoryBlob)
	{
		m_alloc->Free(m_memoryBlob);
		m_memoryBlob = nullptr;
	}

	m_nextAndOccupancy = nullptr;
	m_keys = nullptr;
	m_hashValues = nullptr;
	m_capacity = 0;
	m_count = 0;
	m_freePosScan = 0;
}

template<class TKey, class TValue, class TSize>
rkit::HashTableBase<TKey, TValue, TSize> &rkit::HashTableBase<TKey, TValue, TSize>::operator=(HashTableBase<TKey, TValue, TSize> &&other)
{
	Clear();

	m_values = std::move(other.m_values);
	m_memoryBlob = other.m_memoryBlob;
	m_nextAndOccupancy = other.m_nextAndOccupancy;
	m_keys = other.m_keys;
	m_hashValues = other.m_hashValues;
	m_capacity = other.m_capacity;
	m_count = other.m_count;
	m_freePosScan = other.m_freePosScan;

	other.m_memoryBlob = nullptr;
	other.m_nextAndOccupancy = nullptr;
	other.m_keys = nullptr;
	other.m_hashValues = nullptr;
	other.m_capacity = 0;
	other.m_count = 0;
	other.m_freePosScan = 0;

	return *this;
}

template<class TKey, class TValue, class TSize>
rkit::Result rkit::HashTableBase<TKey, TValue, TSize>::Resize(TSize newCapacity)
{
	struct ResizePlanChunk
	{
		size_t m_size;
		size_t m_alignment;
		size_t m_count;
		size_t m_blobOffset;
	};

	constexpr size_t maxBlobSize = std::numeric_limits<size_t>::max();

	if (newCapacity == 0)
	{
		Clear();
		return ResultCode::kOK;
	}

	if (newCapacity > std::numeric_limits<TSize>::max() / 2)
		return ResultCode::kOutOfMemory;

	if (newCapacity < m_count)
		return ResultCode::kInvalidParameter;

	if ((newCapacity & (newCapacity - 1)) != 0)
		return ResultCode::kInvalidParameter;

	// Keys, Next/Occupancy, Values, HashValues
	ResizePlanChunk sizeAlignChunks[] =
	{
		{sizeof(TKey), alignof(TKey), newCapacity, 0},
		{sizeof(NextChainAndOccupancy), alignof(NextChainAndOccupancy), newCapacity, 0},
		{HashMapValueContainer<TValue>::kValueSize, HashMapValueContainer<TValue>::kValueAlignment, newCapacity, 0},
		{sizeof(HashValue_t), alignof(HashValue_t), newCapacity, 0},
	};

	size_t totalSize = 0;
	for (ResizePlanChunk &chunk : sizeAlignChunks)
	{
		size_t trailingAlignment = (totalSize % chunk.m_alignment);
		size_t padding = 0;
		if (trailingAlignment != 0)
			padding = (chunk.m_alignment - trailingAlignment);

		if (maxBlobSize - totalSize < padding)
			return ResultCode::kOutOfMemory;

		totalSize += padding;
		chunk.m_blobOffset = totalSize;

		if (chunk.m_size > 0 && maxBlobSize / chunk.m_size < chunk.m_count)
			return ResultCode::kOutOfMemory;

		size_t chunkSize = chunk.m_count * chunk.m_size;
		if (maxBlobSize - totalSize < chunkSize)
			return ResultCode::kOutOfMemory;

		totalSize += chunkSize;
	}

	void *newBlob = m_alloc->Alloc(totalSize);
	if (!newBlob)
		return ResultCode::kOutOfMemory;

	TKey *oldKeys = m_keys;
	const HashValue_t *oldHashValues = m_hashValues;
	const NextChainAndOccupancy *oldNextAndOccupancy = m_nextAndOccupancy;
	HashMapValueContainer<TValue> oldValues = m_values;
	void *oldMemory = m_memoryBlob;
	size_t oldCapacity = m_capacity;

	char *newBlobBytes = static_cast<char *>(newBlob);

	m_memoryBlob = newBlob;
	m_keys = reinterpret_cast<TKey *>(newBlobBytes + sizeAlignChunks[0].m_blobOffset);
	m_nextAndOccupancy = reinterpret_cast<NextChainAndOccupancy *>(newBlobBytes + sizeAlignChunks[1].m_blobOffset);
	m_values = HashMapValueContainer<TValue>(reinterpret_cast<TValue *>(newBlobBytes + sizeAlignChunks[2].m_blobOffset));
	m_hashValues = reinterpret_cast<HashValue_t *>(newBlobBytes + sizeAlignChunks[3].m_blobOffset);

	m_capacity = newCapacity;
	m_count = 0;
	m_freePosScan = 0;

	NextChainAndOccupancy *newNextAndOccupancy = m_nextAndOccupancy;
	for (TSize i = 0; i < newCapacity; i++)
	{
		newNextAndOccupancy[i].m_next = i;
		newNextAndOccupancy[i].m_used = 0;
	}

	if (oldMemory)
	{
		for (TSize i = 0; i < oldCapacity; i++)
		{
			if (oldNextAndOccupancy[i].m_used)
			{
				TSize newPosition = 0;
				this->CreatePositionForNewEntryNoResize(oldHashValues[i], newPosition);

				new (m_keys + newPosition) TKey(std::move(oldKeys[i]));
				oldKeys[i].~TKey();

				m_values.RelocateValue(newPosition, oldValues, i);
			}
		}

		m_alloc->Free(oldMemory);
	}

	return ResultCode::kOK;
}

template<class TKey, class TValue, class TSize>
rkit::Result rkit::HashTableBase<TKey, TValue, TSize>::Rehash(TSize minimumCapacity)
{
	TSize newSize = 0;
	for (int i = 4; i < (sizeof(TSize) * 8); i++)
	{
		newSize = static_cast<TSize>(static_cast<TSize>(1) << i);
		if (newSize >= minimumCapacity)
			return Resize(newSize);
	}

	return ResultCode::kOutOfMemory;
}

template<class TKey, class TValue, class TSize>
rkit::Result rkit::HashTableBase<TKey, TValue, TSize>::CreatePositionForNewEntry(HashValue_t newKeyHash, TSize &outPosition)
{
	if (m_count == std::numeric_limits<TSize>::max())
		return ResultCode::kOutOfMemory;

	if (m_capacity > 0)
	{
		TSize mainPosition = GetMainPosition(newKeyHash);

		if (!GetOccupancyAt(mainPosition))
		{
			ReserveMainPosition(newKeyHash, mainPosition);
			outPosition = mainPosition;
			return ResultCode::kOK;
		}
	}

	TSize insertPosition = m_freePosScan;
	while (insertPosition < m_capacity)
	{
		if (!GetOccupancyAt(insertPosition))
			break;

		insertPosition++;
	}

	if (insertPosition == m_capacity)
	{
		RKIT_CHECK(Rehash(m_count + 1));
		this->CreatePositionForNewEntryNoResize(newKeyHash, outPosition);
		return ResultCode::kOK;
	}

	m_freePosScan = insertPosition + 1;

	ReserveNonMainPosition(newKeyHash, insertPosition, outPosition);

	return ResultCode::kOK;
}

template<class TKey, class TValue, class TSize>
void rkit::HashTableBase<TKey, TValue, TSize>::CreatePositionForNewEntryNoResize(HashValue_t newKeyHash, TSize &outPosition)
{
	TSize mainPosition = GetMainPosition(newKeyHash);

	if (!GetOccupancyAt(mainPosition))
	{
		ReserveMainPosition(newKeyHash, mainPosition);
		outPosition = mainPosition;
		return;
	}

	TSize insertPosition = mainPosition + 1;
	for (;;)
	{
		if (insertPosition == m_capacity)
			insertPosition = 0;

		RKIT_ASSERT(insertPosition != mainPosition);

		if (!GetOccupancyAt(insertPosition))
			break;

		insertPosition++;
	}

	ReserveNonMainPosition(newKeyHash, insertPosition, outPosition);
}

template<class TKey, class TValue, class TSize>
void rkit::HashTableBase<TKey, TValue, TSize>::ReserveMainPosition(HashValue_t newKeyHash, TSize position)
{
	RKIT_ASSERT(!GetOccupancyAt(position));
	SetOccupancyAt(position, true);
	m_count++;
}

template<class TKey, class TValue, class TSize>
void rkit::HashTableBase<TKey, TValue, TSize>::ReserveNonMainPosition(HashValue_t newKeyHash, TSize initialFreePosition, TSize &outNewFreePosition)
{
	TSize mainPosition = GetMainPosition(newKeyHash);

	RKIT_ASSERT(!GetOccupancyAt(initialFreePosition));
	RKIT_ASSERT(GetOccupancyAt(mainPosition));

	TSize newFreePosition = initialFreePosition;
	if (GetMainPosition(m_hashValues[mainPosition]) == mainPosition)
	{
		// The colliding node is in its main position,
		// new node goes in the free position
		m_hashValues[initialFreePosition] = newKeyHash;
	}
	else
	{
		// The colliding node is not in its main position, move it to the free position
		// and take the main position
		this->m_values.RelocateValue(initialFreePosition, this->m_values, mainPosition);
		new (m_keys + initialFreePosition) TKey(std::move(m_keys[mainPosition]));
		m_keys[mainPosition].~TKey();

		m_hashValues[initialFreePosition] = m_hashValues[mainPosition];
		m_hashValues[mainPosition] = newKeyHash;

		newFreePosition = mainPosition;
	}

	m_nextAndOccupancy[initialFreePosition].m_next = m_nextAndOccupancy[mainPosition].m_next;
	m_nextAndOccupancy[mainPosition].m_next = initialFreePosition;

	SetOccupancyAt(initialFreePosition, true);

	outNewFreePosition = newFreePosition;

	m_count++;
}

template<class TKey, class TValue, class TSize>
void rkit::HashTableBase<TKey, TValue, TSize>::RemoveEntryNoDestruct(TSize position)
{
	TSize prev = position;

	while (m_nextAndOccupancy[prev].m_next != position)
		prev = m_nextAndOccupancy[prev].m_next;

	m_nextAndOccupancy[prev].m_next = m_nextAndOccupancy[position].m_next;
	m_nextAndOccupancy[position].m_next = position;

	SetOccupancyAt(position, false);
	m_count--;
}


template<class TKey, class TValue, class TSize>
template<class TCandidateKey>
bool rkit::HashTableBase<TKey, TValue, TSize>::FindKeyPosition(HashValue_t keyHash, const TCandidateKey &key, TSize &outPosition) const
{
	if (m_capacity == 0)
		return false;

	TSize pos = GetMainPosition(keyHash);
	TSize initialPos = pos;

	do
	{
		if (GetOccupancyAt(pos) && m_keys[pos] == key)
		{
			outPosition = pos;
			return true;
		}

		pos = m_nextAndOccupancy[pos].m_next;
	} while (pos != initialPos);

	return false;
}

template<class TKey, class TValue, class TSize>
bool rkit::HashTableBase<TKey, TValue, TSize>::GetOccupancyAt(TSize index) const
{
	RKIT_ASSERT(index < m_capacity);

	return m_nextAndOccupancy[index].m_used != 0;
}

template<class TKey, class TValue, class TSize>
void rkit::HashTableBase<TKey, TValue, TSize>::SetOccupancyAt(TSize index, bool occupied)
{
	RKIT_ASSERT(index < m_capacity);

	m_nextAndOccupancy[index].m_used = (occupied ? 1 : 0);
}

template<class TKey, class TValue, class TSize>
TSize rkit::HashTableBase<TKey, TValue, TSize>::GetMainPosition(HashValue_t hash) const
{
	return static_cast<TSize>(hash & (m_capacity - 1));
}

template<class TKey, class TSize>
rkit::HashSet<TKey, TSize>::HashSet(IMallocDriver *alloc)
	: HashTableBase<TKey, void, TSize>(alloc)
{
}

template<class TKey, class TSize>
template<class TCandidateKey, class THasher, class TKeyConstructor>
rkit::Result rkit::HashSet<TKey, TSize>::Add(TCandidateKey &&key)
{
	HashValue_t hash = THasher::ComputeHash(0, key);

	TSize position = 0;
	if (this->FindKeyPosition<TCandidateKey>(hash, key, position))
		return ResultCode::kOK;

	RKIT_CHECK(this->CreatePositionForNewEntry(hash, position));

	Result constructResult = TKeyConstructor::Construct(this->m_keys + position, std::forward<TKey>(key));
	if (!constructResult.IsOK())
	{
		this->m_keys[position].~TKey();
		this->RemoveEntryNoDestruct(position);
		return constructResult;
	}

	return ResultCode::kOK;
}

template<class TKey, class TSize>
template<class TCandidateKey, class THasher>
bool rkit::HashSet<TKey, TSize>::Contains(const TCandidateKey &key) const
{
	HashValue_t hash = THasher::ComputeHash(0, key);

	TSize position = 0;
	return this->FindKeyPosition<TCandidateKey>(hash, key, position);
}


template<class TKey, class TValue, class TSize>
rkit::HashMap<TKey, TValue, TSize>::HashMap(IMallocDriver *alloc)
	: HashTableBase<TKey, TValue, TSize>(alloc)
{
}

template<class TKey, class TValue, class TSize>
template<class TCandidateKey, class TCandidateValue, class TKeyHasher, class TKeyConstructor, class TValueConstructor>
rkit::Result rkit::HashMap<TKey, TValue, TSize>::Set(TCandidateKey &&key, TCandidateValue &&value)
{
	HashValue_t hash = TKeyHasher::ComputeHash(0, key);

	TSize position = 0;
	if (this->FindKeyPosition<TCandidateKey>(hash, key, position))
	{
		TValue *valuePtr = this->m_values.GetValuePtrAt(position);
		return TValueConstructor::Assign(*valuePtr, std::forward<TCandidateValue>(value));
	}

	RKIT_CHECK(this->CreatePositionForNewEntry(hash, position));

	{
		Result keyConstructResult = TKeyConstructor::Construct(this->m_keys + position, std::forward<TCandidateKey>(key));
		if (!keyConstructResult.IsOK())
		{
			this->RemoveEntryNoDestruct(position);
			return keyConstructResult;
		}
	}

	{
		Result valueConstructResult = TValueConstructor::Construct(this->m_values.GetValuePtrAt(position), std::forward<TCandidateValue>(value));
		if (!valueConstructResult.IsOK())
		{
			this->m_keys[position].~TKey();

			this->RemoveEntryNoDestruct(position);
			return valueConstructResult;
		}
	}

	return ResultCode::kOK;
}


template<class TKey, class TValue, class TSize>
rkit::HashMapIterator<TKey, TValue, TSize> rkit::HashMap<TKey, TValue, TSize>::begin()
{
	HashMapIterator<TKey, TValue, TSize> result(*this, 0);
	result.Normalize();

	return result;
}

template<class TKey, class TValue, class TSize>
rkit::HashMapIterator<TKey, TValue, TSize> rkit::HashMap<TKey, TValue, TSize>::end()
{
	return HashMapIterator<TKey, TValue, TSize>(*this, this->m_capacity);
}

template<class TKey, class TValue, class TSize>
rkit::HashMapConstIterator<TKey, TValue, TSize> rkit::HashMap<TKey, TValue, TSize>::begin() const
{
	HashMapConstIterator<TKey, TValue, TSize> result(*this, 0);
	result.Normalize();

	return result;
}

template<class TKey, class TValue, class TSize>
rkit::HashMapConstIterator<TKey, TValue, TSize> rkit::HashMap<TKey, TValue, TSize>::end() const
{
	return HashMapConstIterator<TKey, TValue, TSize>(*this, this->m_capacity);
}

template<class TKey, class TValue, class TSize>
template<class TCandidateKey, class TKeyHasher>
rkit::HashMapIterator<TKey, TValue, TSize> rkit::HashMap<TKey, TValue, TSize>::Find(const TCandidateKey &key)
{
	HashValue_t keyHash = TKeyHasher::ComputeHash(0, key);

	TSize position = 0;
	if (this->FindKeyPosition(keyHash, key, position))
		return rkit::HashMapIterator<TKey, TValue, TSize>(*this, position);

	return end();
}

template<class TKey, class TValue, class TSize>
template<class TCandidateKey, class TKeyHasher>
rkit::HashMapConstIterator<TKey, TValue, TSize> rkit::HashMap<TKey, TValue, TSize>::Find(const TCandidateKey &key) const
{
	HashValue_t keyHash = TKeyHasher::ComputeHash(0, key);

	TSize position = 0;
	if (this->FindKeyPosition(keyHash, key, position))
		return rkit::HashMapConstIterator<TKey, TValue, TSize>(*this, position);

	return end();
}


template<class TTarget, class TOriginal>
rkit::Result rkit::DefaultElementConstructor<TTarget, TOriginal>::Construct(void *memory, TOriginal &&original)
{
	new (memory) TTarget(std::forward<TOriginal>(original));
	return rkit::ResultCode::kOK;
}

template<class TTarget, class TOriginal>
rkit::Result rkit::DefaultElementConstructor<TTarget, TOriginal>::Assign(TTarget &target, TOriginal &&original)
{
	target = std::forward<TOriginal>(original);
	return rkit::ResultCode::kOK;
}

template<class TKey, class TValue>
rkit::HashMapKeyValueView<TKey, TValue>::HashMapKeyValueView(const TKey &key, TValue &value)
	: m_key(key)
	, m_value(value)
{
}

template<class TKey, class TValue>
const TKey &rkit::HashMapKeyValueView<TKey, TValue>::Key() const
{
	return m_key;
}

template<class TKey, class TValue>
TValue &rkit::HashMapKeyValueView<TKey, TValue>::Value() const
{
	return m_value;
}
