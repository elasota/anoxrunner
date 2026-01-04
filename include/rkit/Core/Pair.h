#pragma once

#include "Tuple.h"

namespace rkit
{
	template<class TFirst, class TSecond>
	class Pair final : public Tuple<TFirst, TSecond>
	{
	public:
		Pair() = default;
		Pair(const Pair<TFirst, TSecond> &) = default;
		Pair(Pair<TFirst, TSecond> &&) = default;

		Pair(const TFirst &first, const TSecond &second);
		Pair(const TFirst &first, TSecond &&second);
		Pair(TFirst &&first, const TSecond &second);
		Pair(TFirst &&first, TSecond &&second);

		Pair &operator=(const Pair &) = default;
		Pair &operator=(Pair &&) = default;

		TFirst &First();
		const TFirst &First() const;

		TSecond &Second();
		const TSecond &Second() const;
	};
}

namespace rkit
{
	template<class TFirst, class TSecond>
	Pair<TFirst, TSecond>::Pair(const TFirst &first, const TSecond &second)
		: Tuple<TFirst, TSecond>(first, second)
	{
	}

	template<class TFirst, class TSecond>
	Pair<TFirst, TSecond>::Pair(const TFirst &first, TSecond &&second)
		: Tuple<TFirst, TSecond>(first, std::move(second))
	{
	}

	template<class TFirst, class TSecond>
	Pair<TFirst, TSecond>::Pair(TFirst &&first, const TSecond &second)
		: Tuple<TFirst, TSecond>(std::move(first), second)
	{
	}

	template<class TFirst, class TSecond>
	Pair<TFirst, TSecond>::Pair(TFirst &&first, TSecond &&second)
		: Tuple<TFirst, TSecond>(std::move(first), std::move(second))
	{
	}

	template<class TFirst, class TSecond>
	TFirst &Pair<TFirst, TSecond>::First()
	{
		return this->template GetAt<0>();
	}

	template<class TFirst, class TSecond>
	const TFirst &Pair<TFirst, TSecond>::First() const
	{
		return this->template GetAt<0>();
	}

	template<class TFirst, class TSecond>
	TSecond &Pair<TFirst, TSecond>::Second()
	{
		return this->template GetAt<1>();
	}

	template<class TFirst, class TSecond>
	const TSecond &Pair<TFirst, TSecond>::Second() const
	{
		return this->template GetAt<1>();
	}
}
