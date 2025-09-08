#pragma once

#include "Ordering.h"

namespace rkit
{
	template<class TChar>
	class InvariantCharCaseAdjuster
	{
	public:
		static TChar ToUpper(TChar c);
		static TChar ToLower(TChar c);
	};

	template<class TChar>
	class CharStrictComparer
	{
	public:
		static Ordering Compare(TChar a, TChar b);
		static bool CompareEqual(TChar a, TChar b);
	};

	template<class TChar, class TCaseAdjuster>
	class CharCaseInsensitiveComparer
	{
	public:
		static Ordering Compare(TChar a, TChar b);
		static bool CompareEqual(TChar a, TChar b);
	};
}


template<class TChar>
TChar rkit::InvariantCharCaseAdjuster<TChar>::ToUpper(TChar c)
{
	if (c >= 'a' && c <= 'z')
		return static_cast<TChar>(c - 'a' + 'A');

	return c;
}

template<class TChar>
TChar rkit::InvariantCharCaseAdjuster<TChar>::ToLower(TChar c)
{
	if (c >= 'A' && c <= 'Z')
		return static_cast<TChar>(c - 'A' + 'a');

	return c;
}

template<class TChar>
rkit::Ordering rkit::CharStrictComparer<TChar>::Compare(TChar a, TChar b)
{
	if (a < b)
		return Ordering::kLess;
		
	if (a > b)
		return Ordering::kGreater;

	return Ordering::kEqual;
}

template<class TChar>
bool rkit::CharStrictComparer<TChar>::CompareEqual(TChar a, TChar b)
{
	return a == b;
}

template<class TChar, class TCaseAdjuster>
rkit::Ordering rkit::CharCaseInsensitiveComparer<TChar, TCaseAdjuster>::Compare(TChar a, TChar b)
{
	a = TCaseAdjuster::ToLower(a);
	b = TCaseAdjuster::ToLower(b);

	return CharStrictComparer<TChar>::Compare(a, b);
}

template<class TChar, class TCaseAdjuster>
bool rkit::CharCaseInsensitiveComparer<TChar, TCaseAdjuster>::CompareEqual(TChar a, TChar b)
{
	a = TCaseAdjuster::ToLower(a);
	b = TCaseAdjuster::ToLower(b);

	return a == b;
}
