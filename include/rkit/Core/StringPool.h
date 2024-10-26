#pragma once

#include "Vector.h"
#include "HashTable.h"
#include "StringView.h"
#include "UniquePtr.h"

#include <cstddef>

namespace rkit
{
	struct Result;

	class StringPoolBuilder
	{
	public:
		StringPoolBuilder();

		Result IndexString(const StringView &str, size_t &outIndex);
		const StringView GetStringByIndex(size_t index);

	private:
		Vector<UniquePtr<String>> m_strings;
		HashMap<StringView, size_t> m_stringToIndex;
	};
}

#include "NewDelete.h"
#include "Result.h"
#include "String.h"

inline rkit::StringPoolBuilder::StringPoolBuilder()
{
}

inline rkit::Result rkit::StringPoolBuilder::IndexString(const StringView &str, size_t &outIndex)
{
	HashMap<StringView, size_t>::ConstIterator_t it = m_stringToIndex.Find(str);
	if (it == m_stringToIndex.end())
	{
		String strInstance;
		RKIT_CHECK(strInstance.Set(str));

		UniquePtr<String> strPtr;
		New<String>(strPtr, std::move(strInstance));

		size_t index = m_strings.Count();
		RKIT_CHECK(m_strings.Append(std::move(strPtr)));

		StringView strPtrView = *strPtr.Get();

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

const rkit::StringView rkit::StringPoolBuilder::GetStringByIndex(size_t index)
{
	return *m_strings[index];
}
