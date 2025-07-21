#pragma once

#include "CharacterEncoding.h"
#include "Ordering.h"
#include "Span.h"

#include <cstddef>
#include <type_traits>

namespace rkit
{
	namespace BaseStringPrivate
	{
		template<class TChar>
		struct NullTerminatedStringHelper
		{
			static const TChar kDefaultNullTerminator;
		};
	}

	template<class TChar, CharacterEncoding TEncoding>
	class BaseStringSliceView
	{
	public:
		BaseStringSliceView();
		BaseStringSliceView(const TChar *chars, size_t length);
		BaseStringSliceView(const BaseStringSliceView &other);

		template<size_t TLength>
		BaseStringSliceView(const TChar(&charsArray)[TLength]);

		explicit BaseStringSliceView(const Span<const TChar> &span);

		const TChar &operator[](size_t index) const;

		BaseStringSliceView SubString(size_t start) const;
		BaseStringSliceView SubString(size_t start, size_t length) const;

		Span<const TChar> ToSpan() const;

		SpanIterator<TChar> begin();
		SpanIterator<const TChar> begin() const;

		SpanIterator<TChar> end();
		SpanIterator<const TChar> end() const;

		const TChar *GetChars() const;
		size_t Length() const;

		template<class TComparer>
		bool StartsWith(const BaseStringSliceView<TChar, TEncoding> &other, const TComparer &comparer) const;
		bool StartsWith(const BaseStringSliceView<TChar, TEncoding> &other) const;
		bool StartsWithNoCase(const BaseStringSliceView<TChar, TEncoding> &other) const;

		template<class TComparer>
		bool EndsWith(const BaseStringSliceView<TChar, TEncoding> &other, const TComparer &comparer) const;
		bool EndsWith(const BaseStringSliceView<TChar, TEncoding> &other) const;
		bool EndsWithNoCase(const BaseStringSliceView<TChar, TEncoding> &other) const;

		template<class TComparer>
		bool Equals(const BaseStringSliceView<TChar, TEncoding> &other, const TComparer &comparer) const;
		bool Equals(const BaseStringSliceView<TChar, TEncoding> &other) const;
		bool EqualsNoCase(const BaseStringSliceView<TChar, TEncoding> &other) const;

		bool operator==(const BaseStringSliceView<TChar, TEncoding> &other) const;
		bool operator!=(const BaseStringSliceView<TChar, TEncoding> &other) const;

		bool operator<(const BaseStringSliceView<TChar, TEncoding> &other) const;
		bool operator<=(const BaseStringSliceView<TChar, TEncoding> &other) const;

		bool operator>(const BaseStringSliceView<TChar, TEncoding> &other) const;
		bool operator>=(const BaseStringSliceView<TChar, TEncoding> &other) const;

		Ordering Compare(const BaseStringSliceView<TChar, TEncoding> &other) const;

	private:
		Span<const TChar> m_span;
	};

	template<class TChar, CharacterEncoding TEncoding>
	class BaseStringView : public BaseStringSliceView<TChar, TEncoding>
	{
	public:
		BaseStringView();
		BaseStringView(const TChar *chars, size_t length);

		template<size_t TLength>
		BaseStringView(const TChar(&charsArray)[TLength]);

		static BaseStringView FromCString(const TChar *chars);
	};
}

#include "CharCompare.h"
#include "Hasher.h"
#include "StringUtil.h"

template<class TChar, rkit::CharacterEncoding TEncoding>
rkit::BaseStringSliceView<TChar, TEncoding>::BaseStringSliceView()
	: m_span(nullptr, 0)
{
}

template<class TChar, rkit::CharacterEncoding TEncoding>
rkit::BaseStringSliceView<TChar, TEncoding>::BaseStringSliceView(const TChar *chars, size_t length)
	: m_span(chars, length)
{
}

template<class TChar, rkit::CharacterEncoding TEncoding>
rkit::BaseStringSliceView<TChar, TEncoding>::BaseStringSliceView(const BaseStringSliceView<TChar, TEncoding> &other)
	: m_span(other.m_span)
{
}

template<class TChar, rkit::CharacterEncoding TEncoding>
template<size_t TLength>
rkit::BaseStringSliceView<TChar, TEncoding>::BaseStringSliceView(const TChar(&charsArray)[TLength])
	: m_span(charsArray, TLength - 1)
{
}

template<class TChar, rkit::CharacterEncoding TEncoding>
rkit::BaseStringSliceView<TChar, TEncoding>::BaseStringSliceView(const Span<const TChar> &span)
	: m_span(span)
{
}

template<class TChar, rkit::CharacterEncoding TEncoding>
const TChar &rkit::BaseStringSliceView<TChar, TEncoding>::operator[](size_t index) const
{
	return m_span[index];
}

template<class TChar, rkit::CharacterEncoding TEncoding>
rkit::BaseStringSliceView<TChar, TEncoding> rkit::BaseStringSliceView<TChar, TEncoding>::SubString(size_t start) const
{
	if (start > m_span.Count())
		start = m_span.Count();

	return BaseStringSliceView<TChar, TEncoding>(m_span.Ptr() + start, m_span.Count() - start);
}

template<class TChar, rkit::CharacterEncoding TEncoding>
rkit::BaseStringSliceView<TChar, TEncoding> rkit::BaseStringSliceView<TChar, TEncoding>::SubString(size_t start, size_t length) const
{
	if (length == 0)
		return BaseStringSliceView<TChar, TEncoding>(nullptr, 0);

	if (start > m_span.Count())
		start = m_span.Count();

	const size_t maxLength = m_span.Count() - start;
	if (length > maxLength)
		length = maxLength;

	return BaseStringSliceView<TChar, TEncoding>(m_span.Ptr() + start, length);
}

template<class TChar, rkit::CharacterEncoding TEncoding>
rkit::Span<const TChar> rkit::BaseStringSliceView<TChar, TEncoding>::ToSpan() const
{
	// If length 0, return the local default char to minimize potential leakage from
	// other DLL
	if (m_span.Count() == 0)
		return Span<const TChar>(&BaseStringPrivate::NullTerminatedStringHelper<TChar>::kDefaultNullTerminator, 0);

	return m_span;
}


template<class TChar, rkit::CharacterEncoding TEncoding>
rkit::SpanIterator<TChar> rkit::BaseStringSliceView<TChar, TEncoding>::begin()
{
	return ToSpan().begin();
}

template<class TChar, rkit::CharacterEncoding TEncoding>
rkit::SpanIterator<const TChar> rkit::BaseStringSliceView<TChar, TEncoding>::begin() const
{
	return ToSpan().begin();
}

template<class TChar, rkit::CharacterEncoding TEncoding>
rkit::SpanIterator<TChar> rkit::BaseStringSliceView<TChar, TEncoding>::end()
{
	return ToSpan().end();
}

template<class TChar, rkit::CharacterEncoding TEncoding>
rkit::SpanIterator<const TChar> rkit::BaseStringSliceView<TChar, TEncoding>::end() const
{
	return ToSpan().end();
}

template<class TChar, rkit::CharacterEncoding TEncoding>
const TChar *rkit::BaseStringSliceView<TChar, TEncoding>::GetChars() const
{
	return ToSpan().Ptr();
}

template<class TChar, rkit::CharacterEncoding TEncoding>
size_t rkit::BaseStringSliceView<TChar, TEncoding>::Length() const
{
	return m_span.Count();
}


template<class TChar, rkit::CharacterEncoding TEncoding>
template<class TComparer>
bool rkit::BaseStringSliceView<TChar, TEncoding>::StartsWith(const BaseStringSliceView<TChar, TEncoding> &other, const TComparer &comparer) const
{
	const size_t cmpLength = other.m_span.Count();
	if (m_span.Count() < cmpLength)
		return false;

	const TChar *thisChars = m_span.Ptr();
	const TChar *otherChars = other.m_span.Ptr();

	for (size_t i = 0; i < cmpLength; i++)
	{
		if (TComparer::Compare(thisChars[i], otherChars[i]) != 0)
			return false;
	}

	return true;
}

template<class TChar, rkit::CharacterEncoding TEncoding>
bool rkit::BaseStringSliceView<TChar, TEncoding>::StartsWith(const BaseStringSliceView<TChar, TEncoding> &other) const
{
	return EndsWith(other, CharStrictComparer<TChar>());
}

template<class TChar, rkit::CharacterEncoding TEncoding>
bool rkit::BaseStringSliceView<TChar, TEncoding>::StartsWithNoCase(const BaseStringSliceView<TChar, TEncoding> &other) const
{
	return EndsWith(other, CharCaseInsensitiveComparer<TChar, InvariantCharCaseAdjuster<TChar>>());
}

template<class TChar, rkit::CharacterEncoding TEncoding>
template<class TComparer>
bool rkit::BaseStringSliceView<TChar, TEncoding>::EndsWith(const BaseStringSliceView<TChar, TEncoding> &other, const TComparer &comparer) const
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

template<class TChar, rkit::CharacterEncoding TEncoding>
bool rkit::BaseStringSliceView<TChar, TEncoding>::EndsWith(const BaseStringSliceView<TChar, TEncoding> &other) const
{
	return EndsWith(other, CharStrictComparer<TChar>());
}

template<class TChar, rkit::CharacterEncoding TEncoding>
bool rkit::BaseStringSliceView<TChar, TEncoding>::EndsWithNoCase(const BaseStringSliceView<TChar, TEncoding> &other) const
{
	return EndsWith(other, CharCaseInsensitiveComparer<TChar, InvariantCharCaseAdjuster<TChar>>());
}

template<class TChar, rkit::CharacterEncoding TEncoding>
template<class TComparer>
bool rkit::BaseStringSliceView<TChar, TEncoding>::Equals(const BaseStringSliceView<TChar, TEncoding> &other, const TComparer &comparer) const
{
	const size_t cmpLength = other.m_span.Count();
	if (m_span.Count() != cmpLength)
		return false;

	const TChar *thisChars = m_span.Ptr();
	const TChar *otherChars = other.m_span.Ptr();

	for (size_t i = 0; i < cmpLength; i++)
	{
		if (TComparer::Compare(thisChars[i], otherChars[i]) != 0)
			return false;
	}

	return true;
}

template<class TChar, rkit::CharacterEncoding TEncoding>
bool rkit::BaseStringSliceView<TChar, TEncoding>::Equals(const BaseStringSliceView<TChar, TEncoding> &other) const
{
	return Equals(other, CharStrictComparer<TChar>());
}

template<class TChar, rkit::CharacterEncoding TEncoding>
bool rkit::BaseStringSliceView<TChar, TEncoding>::EqualsNoCase(const BaseStringSliceView<TChar, TEncoding> &other) const
{
	return EndsWith(other, CharCaseInsensitiveComparer<TChar, InvariantCharCaseAdjuster<TChar>>());
}

template<class TChar, rkit::CharacterEncoding TEncoding>
bool rkit::BaseStringSliceView<TChar, TEncoding>::operator==(const BaseStringSliceView<TChar, TEncoding> &other) const
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

template<class TChar, rkit::CharacterEncoding TEncoding>
bool rkit::BaseStringSliceView<TChar, TEncoding>::operator!=(const BaseStringSliceView<TChar, TEncoding> &other) const
{
	return !((*this) == other);
}

template<class TChar, rkit::CharacterEncoding TEncoding>
bool rkit::BaseStringSliceView<TChar, TEncoding>::operator<(const BaseStringSliceView<TChar, TEncoding> &other) const
{
	const size_t length = rkit::Min(m_span.Count(), other.m_span.Count());
	const TChar *charsA = m_span.Ptr();
	const TChar *charsB = other.m_span.Ptr();

	for (size_t i = 0; i < length; i++)
	{
		if (charsA[i] != charsB[i])
			return charsA[i] < charsB[i];
	}

	return m_span.Count() < other.m_span.Count();
}

template<class TChar, rkit::CharacterEncoding TEncoding>
bool rkit::BaseStringSliceView<TChar, TEncoding>::operator<=(const BaseStringSliceView<TChar, TEncoding> &other) const
{
	return !((*this) > other);
}

template<class TChar, rkit::CharacterEncoding TEncoding>
bool rkit::BaseStringSliceView<TChar, TEncoding>::operator>(const BaseStringSliceView<TChar, TEncoding> &other) const
{
	return other < (*this);
}

template<class TChar, rkit::CharacterEncoding TEncoding>
bool rkit::BaseStringSliceView<TChar, TEncoding>::operator>=(const BaseStringSliceView<TChar, TEncoding> &other) const
{
	return !((*this) < other);
}

template<class TChar, rkit::CharacterEncoding TEncoding>
rkit::Ordering rkit::BaseStringSliceView<TChar, TEncoding>::Compare(const BaseStringSliceView<TChar, TEncoding> &other) const
{
	return CharCompare<TChar, TEncoding>::Compare(m_span, other.m_span);
}

template<class TChar>
const TChar rkit::BaseStringPrivate::NullTerminatedStringHelper<TChar>::kDefaultNullTerminator = static_cast<TChar>(0);


template<class TChar, rkit::CharacterEncoding TEncoding>
struct rkit::Hasher<rkit::BaseStringSliceView<TChar, TEncoding>>
{
	static HashValue_t ComputeHash(HashValue_t baseHash, const BaseStringSliceView<TChar, TEncoding> &stringView);
	static HashValue_t ComputeHash(HashValue_t baseHash, const Span<const BaseStringSliceView<TChar, TEncoding>> &span);
};

template<class TChar, rkit::CharacterEncoding TEncoding>
struct rkit::Hasher<rkit::BaseStringView<TChar, TEncoding>> : public rkit::Hasher<rkit::BaseStringSliceView<TChar, TEncoding>>
{
};

template<class TChar, rkit::CharacterEncoding TEncoding>
rkit::HashValue_t rkit::Hasher<rkit::BaseStringSliceView<TChar, TEncoding>>::ComputeHash(HashValue_t baseHash, const BaseStringSliceView<TChar, TEncoding> &stringView)
{
	return BinaryHasher<TChar>::ComputeHash(baseHash, stringView.ToSpan());
}

template<class TChar, rkit::CharacterEncoding TEncoding>
rkit::HashValue_t rkit::Hasher<rkit::BaseStringSliceView<TChar, TEncoding>>::ComputeHash(HashValue_t baseHash, const Span<const BaseStringSliceView<TChar, TEncoding>> &span)
{
	HashValue_t hash = baseHash;

	for (const BaseStringView<TChar> &strView : span)
		hash = ComputeHash(baseHash, strView);

	return hash;
}


template<class TChar, rkit::CharacterEncoding TEncoding>
rkit::BaseStringView<TChar, TEncoding>::BaseStringView()
{
}

template<class TChar, rkit::CharacterEncoding TEncoding>
rkit::BaseStringView<TChar, TEncoding>::BaseStringView(const TChar *chars, size_t length)
	: BaseStringSliceView<TChar, TEncoding>(chars, length)
{
	RKIT_ASSERT(chars[length] == static_cast<TChar>(0));
}

template<class TChar, rkit::CharacterEncoding TEncoding>
template<size_t TLength>
rkit::BaseStringView<TChar, TEncoding>::BaseStringView(const TChar(&charsArray)[TLength])
	: BaseStringSliceView<TChar, TEncoding>(charsArray)
{
	RKIT_ASSERT(charsArray[TLength - 1] == static_cast<TChar>(0));
}

template<class TChar, rkit::CharacterEncoding TEncoding>
rkit::BaseStringView<TChar, TEncoding> rkit::BaseStringView<TChar, TEncoding>::FromCString(const TChar *chars)
{
	if (chars == nullptr)
		return BaseStringView<TChar, TEncoding>();

	size_t length = 0;
	while (chars[length] != static_cast<TChar>(0))
		length++;

	return BaseStringView<TChar, TEncoding>(chars, length);
}
