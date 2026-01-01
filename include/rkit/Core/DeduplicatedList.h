#pragma once

#include "Vector.h"
#include "HashTable.h"

namespace rkit
{
	template<class T>
	class Span;

	template<class TItem>
	class DeduplicatedList
	{
	public:
		Result AddAndGetIndex(size_t &outIndex, const TItem &item);
		Result Add(const TItem &item);

		Span<const TItem> GetItems() const;

	private:
		HashMap<TItem, size_t> m_lookup;
		Vector<TItem> m_items;
	};
}

#include "HashValue.h"

#include <utility>

namespace rkit
{
	template<class TItem>
	Result DeduplicatedList<TItem>::AddAndGetIndex(size_t &outIndex, const TItem &item)
	{
		const HashValue_t hashValue = Hasher<TItem>::ComputeHash(0, item);

		const typename HashMap<TItem, size_t>::ConstIterator_t it = m_lookup.FindPrehashed(hashValue, item);
		if (it != m_lookup.end())
			outIndex = it.Value();
		else
		{
			const size_t newIndex = m_items.Count();
			RKIT_CHECK(m_items.Append(item));
			const Result insertResult = m_lookup.SetPrehashed(hashValue, item, newIndex);
			if (!utils::ResultIsOK(insertResult))
			{
				m_items.ShrinkToSize(newIndex);
				return insertResult;
			}

			outIndex = newIndex;
		}

		return ResultCode::kOK;
	}

	template<class TItem>
	Result DeduplicatedList<TItem>::Add(const TItem &item)
	{
		size_t index = 0;
		return AddAndGetIndex(index, item);
	}

	template<class TItem>
	Span<const TItem> DeduplicatedList<TItem>::GetItems() const
	{
		return m_items.ToSpan();
	}
}
