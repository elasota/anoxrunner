#pragma once

#include "Span.h"

#include <cstddef>

namespace rkit
{
	template<class TChar>
	class BaseStringView
	{
	public:
		BaseStringView();
		BaseStringView(const TChar *chars, size_t length);

		template<size_t TLength>
		BaseStringView(const TChar(&charsArray)[TLength]);

		const TChar &operator[](size_t index) const;

		Span<const TChar> SubString(size_t start, size_t length) const;

		Span<const TChar> ToSpan() const;

		const TChar *GetChars() const;
		size_t Length() const;

		template<class TComparer>
		bool EndsWith(const BaseStringView<TChar> &other, const TComparer &comparer) const;

		bool EndsWith(const BaseStringView<TChar> &other) const;
		bool EndsWithNoCase(const BaseStringView<TChar> &other) const;

		bool operator==(const BaseStringView<TChar> &other) const;
		bool operator!=(const BaseStringView<TChar> &other) const;

	private:
		Span<const TChar> m_span;

		static const TChar kDefaultStrChar;
	};
}

#include "Hasher.h"
#include "StringUtil.h"

template<class TChar>
rkit::BaseStringView<TChar>::BaseStringView()
	: m_span(&kDefaultStrChar, 0)
{
}

template<class TChar>
rkit::BaseStringView<TChar>::BaseStringView(const TChar *chars, size_t length)
	: m_span(chars, length)
{
}

template<class TChar>
template<size_t TLength>
rkit::BaseStringView<TChar>::BaseStringView(const TChar(&charsArray)[TLength])
	: m_span(charsArray, TLength - 1)
{
	RKIT_ASSERT(charsArray[TLength - 1] == static_cast<TChar>(0));
}

template<class TChar>
const TChar &rkit::BaseStringView<TChar>::operator[](size_t index) const
{
	return m_span[index];
}


template<class TChar>
rkit::Span<const TChar> rkit::BaseStringView<TChar>::SubString(size_t start, size_t length) const
{
	if (start > m_span.Count())
		start = m_span.Count();

	const size_t maxLength = m_span.Count() - start;
	if (length > maxLength)
		length = maxLength;

	return Span<const TChar>(m_span.Ptr() + start, length);
}

template<class TChar>
rkit::Span<const TChar> rkit::BaseStringView<TChar>::ToSpan() const
{
	return m_span;
}

template<class TChar>
const TChar *rkit::BaseStringView<TChar>::GetChars() const
{
	return m_span.Ptr();
}

template<class TChar>
size_t rkit::BaseStringView<TChar>::Length() const
{
	return m_span.Count();
}


template<class TChar>
template<class TComparer>
bool rkit::BaseStringView<TChar>::EndsWith(const BaseStringView<TChar> &other, const TComparer &comparer) const
{
	const size_t cmpLength = other.m_span.Count();
	if (m_span.Count() < cmpLength)
		return false;

	const TChar *thisChars = m_span.Ptr() + m_span.Count() - cmpLength;
	const TChar *otherChars = other.m_span.Ptr();

	for (size_t i = 0; i < cmpLength; i++)
	{
		if (TComparer::Compare(thisChars[i], otherChars[i]) != 0)
			return false;
	}

	return true;
}

template<class TChar>
bool rkit::BaseStringView<TChar>::EndsWith(const BaseStringView<TChar> &other) const
{
	return EndsWith(other, CharStrictComparer<TChar>());
}

template<class TChar>
bool rkit::BaseStringView<TChar>::EndsWithNoCase(const BaseStringView<TChar> &other) const
{
	return EndsWith(other, CharCaseInsensitiveComparer<TChar, InvariantCharCaseAdjuster<TChar>>());
}

template<class TChar>
bool rkit::BaseStringView<TChar>::operator==(const BaseStringView<TChar> &other) const
{
	if (m_span.Count() != other.m_span.Count())
		return false;

	const size_t length = m_span.Count();
	const TChar *charsA = m_span.Ptr();
	const TChar *charsB = other.m_span.Ptr();

	for (size_t i = 0; i < length; i++)
	{
		if (charsA[i] != charsB[i])
			return false;
	}

	return true;
}

template<class TChar>
bool rkit::BaseStringView<TChar>::operator!=(const BaseStringView<TChar> &other) const
{
	return !((*this) == other);
}

template<class TChar>
const TChar rkit::BaseStringView<TChar>::kDefaultStrChar = static_cast<TChar>(0);


template<class TChar>
struct rkit::Hasher<rkit::BaseStringView<TChar>>
{
	static HashValue_t ComputeHash(HashValue_t baseHash, const BaseStringView<TChar> &stringView);
	static HashValue_t ComputeHash(HashValue_t baseHash, const Span<const BaseStringView<TChar>> &span);
};

template<class TChar>
rkit::HashValue_t rkit::Hasher<rkit::BaseStringView<TChar>>::ComputeHash(HashValue_t baseHash, const BaseStringView<TChar> &stringView)
{
	return BinaryHasher<TChar>::ComputeHash(baseHash, stringView.ToSpan());
}

template<class TChar>
rkit::HashValue_t rkit::Hasher<rkit::BaseStringView<TChar>>::ComputeHash(HashValue_t baseHash, const Span<const BaseStringView<TChar>> &span)
{
	HashValue_t hash = baseHash;

	for (const BaseStringView<TChar> &strView : span)
		hash = ComputeHash(baseHash, strView);

	return hash;
}
