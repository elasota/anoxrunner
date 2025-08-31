#pragma once

#include "rkit/Core/Algorithm.h"
#include "rkit/Core/StaticArray.h"

#include <stdint.h>

namespace rkit { namespace render {
	class HeapKey final : public DefaultOperatorsMixin<HeapKey>
	{
	public:
		HeapKey();
		explicit HeapKey(uint64_t key);
		HeapKey(const HeapKey &other) = default;

		bool operator==(const HeapKey &other) const;
		bool operator<(const HeapKey &other) const;

	private:
		StaticArray<uint64_t, 1> m_keyBits;
	};
} }

#include "rkit/Core/Hasher.h"

namespace rkit
{
	template<>
	struct Hasher<render::HeapKey> : public BinaryHasher<render::HeapKey>
	{
	};
}

namespace rkit { namespace render {
	inline HeapKey::HeapKey()
	{
	}

	inline HeapKey::HeapKey(uint64_t key)
	{
		m_keyBits[0] = key;
	}

	inline bool HeapKey::operator==(const HeapKey &other) const
	{
		for (size_t i = 0; i < m_keyBits.Count(); i++)
		{
			if (m_keyBits[i] != other.m_keyBits[i])
				return false;
		}

		return true;
	}

	inline bool HeapKey::operator<(const HeapKey &other) const
	{
		for (size_t i = 0; i < m_keyBits.Count(); i++)
		{
			if (m_keyBits[i] != other.m_keyBits[i])
				return m_keyBits[i] < other.m_keyBits[i];
		}

		return true;
	}
} }
