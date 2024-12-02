#pragma once

#include "Vector.h"
#include "HashTable.h"
#include "StringProto.h"
#include "UniquePtr.h"

#include <cstddef>

namespace rkit
{
	struct Result;

	template<class TChar>
	class BaseStringPoolBuilder
	{
	public:
		BaseStringPoolBuilder();

		Result IndexString(const BaseStringView<TChar> &str, size_t &outIndex);
		const BaseStringView<TChar> GetStringByIndex(size_t index) const;
		size_t NumStrings() const;

	private:
		typedef BaseString<TChar, 16> StringType_t;

		Vector<UniquePtr<StringType_t>> m_strings;
		HashMap<BaseStringView<TChar>, size_t> m_stringToIndex;
	};
}

#include "NewDelete.h"
#include "Result.h"
#include "String.h"

template<class TChar>
inline rkit::BaseStringPoolBuilder<TChar>::BaseStringPoolBuilder()
{
}

template<class TChar>
inline rkit::Result rkit::BaseStringPoolBuilder<TChar>::IndexString(const BaseStringView<TChar> &str, size_t &outIndex)
{
	typedef HashMap<BaseStringView<TChar>, size_t>::ConstIterator_t MapConstIterator_t;

	MapConstIterator_t it = m_stringToIndex.Find(str);
	if (it == m_stringToIndex.end())
	{
		StringType_t strInstance;
		RKIT_CHECK(strInstance.Set(str));

		UniquePtr<StringType_t> strPtr;
		RKIT_CHECK(New<StringType_t>(strPtr, std::move(strInstance)));

		BaseStringView<TChar> strPtrView = *strPtr.Get();

		size_t index = m_strings.Count();
		RKIT_CHECK(m_strings.Append(std::move(strPtr)));

		Result hashMapInsertResult = m_stringToIndex.Set(GetStringByIndex(index), index);
		if (!hashMapInsertResult.IsOK())
		{
			m_strings.RemoveRange(index, 1);
			return hashMapInsertResult;
		}

		outIndex = index;
	}
	else
		outIndex = it.Value();

	return ResultCode::kOK;
}

template<class TChar>
const rkit::BaseStringView<TChar> rkit::BaseStringPoolBuilder<TChar>::GetStringByIndex(size_t index) const
{
	return *m_strings[index];
}

template<class TChar>
size_t rkit::BaseStringPoolBuilder<TChar>::NumStrings() const
{
	return m_strings.Count();
}
