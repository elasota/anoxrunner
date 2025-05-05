#pragma once

namespace rkit
{
	template<class... TTypes>
	class Tuple;

	template<class TFirst, class TSecond>
	using Pair = Tuple<TFirst, TSecond>;
}
