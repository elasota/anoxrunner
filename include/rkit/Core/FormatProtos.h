#pragma once


namespace rkit
{
	template<class T>
	class Span;
}

namespace rkit {
	template<class TChar>
	struct FormatParameter;

	template<class TChar>
	using FormatParameterList = Span<const FormatParameter<TChar>>;
}
