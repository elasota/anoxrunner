#pragma once

#include "Vector.h"
#include "HashTable.h"
#include "StringProto.h"
#include "UniquePtr.h"

#include <cstddef>

namespace rkit
{
	template<class TChar, CharacterEncoding TEncoding>
	class BaseStringPoolBuilder
	{
	public:
		BaseStringPoolBuilder();

		Result IndexString(const BaseStringView<TChar, TEncoding> &str, size_t &outIndex);
		const BaseStringView<TChar, TEncoding> GetStringByIndex(size_t index) const;
		size_t NumStrings() const;

	private:
		typedef BaseString<TChar, TEncoding, 16> StringType_t;

		Vector<UniquePtr<StringType_t>> m_strings;
		HashMap<BaseStringView<TChar, TEncoding>, size_t> m_stringToIndex;
	};
}

#include "NewDelete.h"
#include "Result.h"
#include "String.h"

template<class TChar, rkit::CharacterEncoding TEncoding>
inline rkit::BaseStringPoolBuilder<TChar, TEncoding>::BaseStringPoolBuilder()
{
}

template<class TChar, rkit::CharacterEncoding TEncoding>
inline rkit::Result rkit::BaseStringPoolBuilder<TChar, TEncoding>::IndexString(const BaseStringView<TChar, TEncoding> &str, size_t &outIndex)
{
	typedef HashMap<BaseStringView<TChar, TEncoding>, size_t>::ConstIterator_t MapConstIterator_t;

	MapConstIterator_t it = m_stringToIndex.Find(str);
	if (it == m_stringToIndex.end())
	{
		StringType_t strInstance;
		RKIT_CHECK(strInstance.Set(str));

		UniquePtr<StringType_t> strPtr;
		RKIT_CHECK(New<StringType_t>(strPtr, std::move(strInstance)));

		BaseStringView<TChar, TEncoding> strPtrView = *strPtr.Get();

		size_t index = m_strings.Count();
		RKIT_CHECK(m_strings.Append(std::move(strPtr)));

		Result hashMapInsertResult = m_stringToIndex.Set(GetStringByIndex(index), index);
		if (!utils::ResultIsOK(hashMapInsertResult))
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

template<class TChar, rkit::CharacterEncoding TEncoding>
const rkit::BaseStringView<TChar, TEncoding> rkit::BaseStringPoolBuilder<TChar, TEncoding>::GetStringByIndex(size_t index) const
{
	return *m_strings[index];
}

template<class TChar, rkit::CharacterEncoding TEncoding>
size_t rkit::BaseStringPoolBuilder<TChar, TEncoding>::NumStrings() const
{
	return m_strings.Count();
}
