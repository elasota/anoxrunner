#pragma once

#include <cstddef>

namespace rkit
{
	template<class TChar, size_t TStaticSize>
	class BaseString;

	template<class TChar>
	class BaseStringView;

	typedef BaseString<char, 16> String;
	typedef BaseString<wchar_t, 16> WString;

	typedef BaseStringView<char> StringView;
	typedef BaseStringView<wchar_t> WStringView;
}
