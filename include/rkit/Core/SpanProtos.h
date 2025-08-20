#pragma once

namespace rkit
{
	template<class T>
	struct ISpan;

	template<class T>
	class Span;

	template<class T>
	class SpanIterator;

	template<class T>
	using ConstSpan = Span<const T>;

	template<class T>
	using ConstSpanIterator = SpanIterator<const T>;

	template<class T>
	using IConstSpan = ISpan<const T>;
}
