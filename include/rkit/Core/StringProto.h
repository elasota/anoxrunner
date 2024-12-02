#pragma once

#include <cstddef>

namespace rkit
{
	template<class TChar, size_t TStaticSize>
	class BaseString;

	template<class TChar>
	class BaseStringSliceView;

	template<class TChar>
	class BaseStringView;

	template<class TChar>
	class BaseStringPoolBuilder;

	typedef BaseString<char, 16> String;
	typedef BaseString<wchar_t, 16> WString;

	typedef BaseStringView<char> StringView;
	typedef BaseStringView<wchar_t> WStringView;

	typedef BaseStringSliceView<char> StringSliceView;
	typedef BaseStringSliceView<wchar_t> WStringSliceView;

	typedef BaseStringPoolBuilder<char> StringPoolBuilder;
	typedef BaseStringPoolBuilder<wchar_t> WStringPoolBuilder;
}
