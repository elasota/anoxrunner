#pragma once

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

	template<class TChar>
	class BaseStringSliceView
	{
	public:
		BaseStringSliceView();
		BaseStringSliceView(const TChar *chars, size_t length);
		BaseStringSliceView(const BaseStringSliceView &other);

		template<size_t TLength>
		BaseStringSliceView(const TChar(&charsArray)[TLength]);

		explicit BaseStringSliceView(const Span<const TChar> &span);
		explicit BaseStringSliceView(const Span<TChar> &span);

		const TChar &operator[](size_t index) const;

		BaseStringSliceView SubString(size_t start) const;
		BaseStringSliceView SubString(size_t start, size_t length) const;

		Span<const TChar> ToSpan() const;

		const TChar *GetChars() const;
		size_t Length() const;

		template<class TComparer>
		bool EndsWith(const BaseStringSliceView<TChar> &other, const TComparer &comparer) const;
		bool EndsWith(const BaseStringSliceView<TChar> &other) const;
		bool EndsWithNoCase(const BaseStringSliceView<TChar> &other) const;

		bool operator==(const BaseStringSliceView<TChar> &other) const;
		bool operator!=(const BaseStringSliceView<TChar> &other) const;

		bool operator<(const BaseStringSliceView<TChar> &other) const;
		bool operator<=(const BaseStringSliceView<TChar> &other) const;

		bool operator>(const BaseStringSliceView<TChar> &other) const;
		bool operator>=(const BaseStringSliceView<TChar> &other) const;

	private:
		Span<const TChar> m_span;
	};

	template<class TChar>
	class BaseStringView : public BaseStringSliceView<TChar>
	{
	public:
		BaseStringView();
		BaseStringView(const TChar *chars, size_t length);

		template<size_t TLength>
		BaseStringView(const TChar(&charsArray)[TLength]);

		static BaseStringView FromCString(const TChar *chars);
	};
}

#include "Hasher.h"
#include "StringUtil.h"

template<class TChar>
rkit::BaseStringSliceView<TChar>::BaseStringSliceView()
	: m_span(nullptr, 0)
{
}

template<class TChar>
rkit::BaseStringSliceView<TChar>::BaseStringSliceView(const TChar *chars, size_t length)
	: m_span(chars, length)
{
}

template<class TChar>
rkit::BaseStringSliceView<TChar>::BaseStringSliceView(const BaseStringSliceView<TChar> &other)
	: m_span(other.m_span)
{
}

template<class TChar>
template<size_t TLength>
rkit::BaseStringSliceView<TChar>::BaseStringSliceView(const TChar(&charsArray)[TLength])
	: m_span(charsArray, TLength - 1)
{
}

template<class TChar>
rkit::BaseStringSliceView<TChar>::BaseStringSliceView(const Span<const TChar> &span)
	: m_span(span)
{
}

template<class TChar>
rkit::BaseStringSliceView<TChar>::BaseStringSliceView(const Span<TChar> &span)
	: m_span(span)
{
}

template<class TChar>
const TChar &rkit::BaseStringSliceView<TChar>::operator[](size_t index) const
{
	return m_span[index];
}

template<class TChar>
rkit::BaseStringSliceView<TChar> rkit::BaseStringSliceView<TChar>::SubString(size_t start) const
{
	if (start > m_span.Count())
		start = m_span.Count();

	return BaseStringSliceView<TChar>(m_span.Ptr() + start, m_span.Count() - start);
}

template<class TChar>
rkit::BaseStringSliceView<TChar> rkit::BaseStringSliceView<TChar>::SubString(size_t start, size_t length) const
{
	if (length == 0)
		return BaseStringSliceView<TChar>(nullptr, 0);

	if (start > m_span.Count())
		start = m_span.Count();

	const size_t maxLength = m_span.Count() - start;
	if (length > maxLength)
		length = maxLength;

	return BaseStringSliceView<TChar>(m_span.Ptr() + start, length);
}

template<class TChar>
rkit::Span<const TChar> rkit::BaseStringSliceView<TChar>::ToSpan() const
{
	// If length 0, return the local default char to minimize potential leakage from
	// other DLL
	if (m_span.Count() == 0)
		return Span<const TChar>(&BaseStringPrivate::NullTerminatedStringHelper<TChar>::kDefaultNullTerminator, 0);

	return m_span;
}

template<class TChar>
const TChar *rkit::BaseStringSliceView<TChar>::GetChars() const
{
	return ToSpan().Ptr();
}

template<class TChar>
size_t rkit::BaseStringSliceView<TChar>::Length() const
{
	return m_span.Count();
}


template<class TChar>
template<class TComparer>
bool rkit::BaseStringSliceView<TChar>::EndsWith(const BaseStringSliceView<TChar> &other, const TComparer &comparer) const
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
bool rkit::BaseStringSliceView<TChar>::EndsWith(const BaseStringSliceView<TChar> &other) const
{
	return EndsWith(other, CharStrictComparer<TChar>());
}

template<class TChar>
bool rkit::BaseStringSliceView<TChar>::EndsWithNoCase(const BaseStringSliceView<TChar> &other) const
{
	return EndsWith(other, CharCaseInsensitiveComparer<TChar, InvariantCharCaseAdjuster<TChar>>());
}

template<class TChar>
bool rkit::BaseStringSliceView<TChar>::operator==(const BaseStringSliceView<TChar> &other) const
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
bool rkit::BaseStringSliceView<TChar>::operator!=(const BaseStringSliceView<TChar> &other) const
{
	return !((*this) == other);
}

template<class TChar>
bool rkit::BaseStringSliceView<TChar>::operator<(const BaseStringSliceView<TChar> &other) const
{
	if (m_span.Count() != other.m_span.Count())
		return m_span.Count() < other.m_span.Count();

	const size_t length = m_span.Count();
	const TChar *charsA = m_span.Ptr();
	const TChar *charsB = other.m_span.Ptr();

	for (size_t i = 0; i < length; i++)
	{
		if (charsA[i] != charsB[i])
			return charsA[i] < charsB[i];
	}

	return false;
}

template<class TChar>
bool rkit::BaseStringSliceView<TChar>::operator<=(const BaseStringSliceView<TChar> &other) const
{
	return !((*this) > other);
}

template<class TChar>
bool rkit::BaseStringSliceView<TChar>::operator>(const BaseStringSliceView<TChar> &other) const
{
	return other < (*this);
}

template<class TChar>
bool rkit::BaseStringSliceView<TChar>::operator>=(const BaseStringSliceView<TChar> &other) const
{
	return !((*this) < other);
}

template<class TChar>
const TChar rkit::BaseStringPrivate::NullTerminatedStringHelper<TChar>::kDefaultNullTerminator = static_cast<TChar>(0);


template<class TChar>
struct rkit::Hasher<rkit::BaseStringSliceView<TChar>>
{
	static HashValue_t ComputeHash(HashValue_t baseHash, const BaseStringSliceView<TChar> &stringView);
	static HashValue_t ComputeHash(HashValue_t baseHash, const Span<const BaseStringSliceView<TChar>> &span);
};

template<class TChar>
struct rkit::Hasher<rkit::BaseStringView<TChar>> : public rkit::Hasher<rkit::BaseStringSliceView<TChar>>
{
};

template<class TChar>
rkit::HashValue_t rkit::Hasher<rkit::BaseStringSliceView<TChar>>::ComputeHash(HashValue_t baseHash, const BaseStringSliceView<TChar> &stringView)
{
	return BinaryHasher<TChar>::ComputeHash(baseHash, stringView.ToSpan());
}

template<class TChar>
rkit::HashValue_t rkit::Hasher<rkit::BaseStringSliceView<TChar>>::ComputeHash(HashValue_t baseHash, const Span<const BaseStringSliceView<TChar>> &span)
{
	HashValue_t hash = baseHash;

	for (const BaseStringView<TChar> &strView : span)
		hash = ComputeHash(baseHash, strView);

	return hash;
}


template<class TChar>
rkit::BaseStringView<TChar>::BaseStringView()
{
}

template<class TChar>
rkit::BaseStringView<TChar>::BaseStringView(const TChar *chars, size_t length)
	: BaseStringSliceView<TChar>(chars, length)
{
	RKIT_ASSERT(chars[length] == static_cast<TChar>(0));
}

template<class TChar>
template<size_t TLength>
rkit::BaseStringView<TChar>::BaseStringView(const TChar(&charsArray)[TLength])
	: BaseStringSliceView<TChar>(charsArray)
{
	RKIT_ASSERT(charsArray[TLength - 1] == static_cast<TChar>(0));
}

template<class TChar>
rkit::BaseStringView<TChar> rkit::BaseStringView<TChar>::FromCString(const TChar *chars)
{
	if (chars == nullptr)
		return BaseStringView<TChar>();

	size_t length = 0;
	while (chars[length] != static_cast<TChar>(0))
		length++;

	return BaseStringView<TChar>(chars, length);
}
