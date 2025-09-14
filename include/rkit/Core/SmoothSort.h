#pragma once

#include <limits>

#include "rkit/Core/Algorithm.h"
#include "rkit/Core/Optional.h"
#include "rkit/Math/BitOps.h"

/*
 * Based on musl
 *
 * Copyright (C) 2011 by Valentin Ochs
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

namespace rkit { namespace priv {
	template<size_t... TNumbers>
	struct SumCheck
	{
		static const size_t kValue = 0;
		static const bool kOverflows = false;
	};

	template<size_t TNumber>
	struct SumCheck<TNumber>
	{
		static const size_t kValue = TNumber;
		static const bool kOverflows = false;
	};

	template<size_t TFirstNumber, size_t... TMoreNumbers>
	struct SumCheck<TFirstNumber, TMoreNumbers...>
	{
	private:
		typedef SumCheck<TMoreNumbers...> Prev_t;

	public:
		static const size_t kValue = TFirstNumber + Prev_t::kValue;
		static const bool kOverflows = Prev_t::kOverflows || ((std::numeric_limits<size_t>::max() - Prev_t::kValue) < TFirstNumber);
	};

	template<bool TIsTerminal, size_t TNumberCount, size_t TTrailingLast, size_t TTrailingSecondLast, size_t... TNumbers>
	struct LeonardoSequence
	{
	};

	template<size_t TTrailingLast, size_t TNumberCount, size_t TTrailingSecondLast, size_t... TNumbers>
	struct LeonardoSequence<true, TNumberCount, TTrailingLast, TTrailingSecondLast, TNumbers...>
	{
		static const size_t kNumberCount = TNumberCount;
		static const size_t kNumbers[TNumberCount];
	};

	template<size_t TTrailingLast, size_t TNumberCount, size_t TTrailingSecondLast, size_t... TNumbers>
	const size_t LeonardoSequence<true, TNumberCount, TTrailingLast, TTrailingSecondLast, TNumbers...>::kNumbers[TNumberCount] =
	{
		TNumbers...
	};

	template<size_t TNumberCount, size_t TTrailingLast, size_t TTrailingSecondLast, size_t... TNumbers>
	struct LeonardoSequence<false, TNumberCount, TTrailingLast, TTrailingSecondLast, TNumbers...>
		: public LeonardoSequence<
			SumCheck<TTrailingLast, TTrailingLast, TTrailingSecondLast, 2>::kOverflows,
			TNumberCount + 1,
			(TTrailingLast + TTrailingSecondLast + 1),
			TTrailingLast, TNumbers..., (TTrailingLast + TTrailingSecondLast + 1)>
	{
	};

	typedef LeonardoSequence<false, 2, 1, 1, 1, 1> LeonardoSequence_t;

	struct SmoothSortSizePair
	{
		size_t m_low = 0;
		size_t m_high = 0;
	};

	static inline int SmoothSortPairNTZ(SmoothSortSizePair &p)
	{
		int r = bitops::FindLowestSet(p.m_low - 1);
		if (r != 0)
			return r;

		r = bitops::FindLowestSet(p.m_high);

		if (r != 0)
			return r + 8 * sizeof(size_t);

		return 0;
	}

	/* shl() and shr() need n > 0 */
	static inline void SmoothSortPairSHL(SmoothSortSizePair &p, int n)
	{
		if (n >= 8 * sizeof(size_t))
		{
			n -= 8 * sizeof(size_t);
			p.m_high = p.m_low;
			p.m_low = 0;
		}
		p.m_high <<= n;
		p.m_high |= p.m_low >> (sizeof(size_t) * 8 - n);
		p.m_low <<= n;
	}

	static inline void SmoothSortPairSHR(SmoothSortSizePair &p, int n)
	{
		if (n >= 8 * sizeof(size_t))
		{
			n -= 8 * sizeof(size_t);
			p.m_low = p.m_high;
			p.m_high = 0;
		}
		p.m_low >>= n;
		p.m_low |= p.m_high << (sizeof(size_t) * 8 - n);
		p.m_high >>= n;
	}

	template<class TIter, class TPred>
	static void SmoothSortSift(const TIter &headRef, const TPred &cmp, int pshift)
	{
		int i = 1;

		TIter head = headRef;

		while (pshift > 1)
		{
			TIter headPrev = head;
			--headPrev;

			const TIter rt = headPrev;
			const TIter lf = headPrev - rkit::priv::LeonardoSequence_t::kNumbers[pshift - 2];

			if (!cmp(*head, *lf) && !cmp(*head, *rt))
				break;

			if (!cmp(*lf, *rt))
			{
				Swap(*lf, *head);
				head = lf;
				pshift -= 1;
			}
			else
			{
				Swap(*rt, *head);
				head = rt;
				pshift -= 2;
			}
		}
	}

	template<class TIter, class TPred>
	static void SmoothSortTrinkle(TIter head, const TPred &cmp, const SmoothSortSizePair &pp, int pshift, bool trusty)
	{
		int i = 1;

		SmoothSortSizePair p = pp;

		while (p.m_low != 1 || p.m_high != 0)
		{
			const TIter stepson = head - rkit::priv::LeonardoSequence_t::kNumbers[pshift];
			if (!cmp(*head, *stepson))
				break;

			if (!trusty && pshift > 1)
			{
				TIter headPrev = head;
				--headPrev;

				const TIter rt = headPrev;
				const TIter lf = headPrev - rkit::priv::LeonardoSequence_t::kNumbers[pshift - 2];
				if (!cmp(*rt, *stepson) || !cmp(*lf, *stepson))
					break;
			}

			Swap(*head, *stepson);
			head = stepson;
			int trail = SmoothSortPairNTZ(p);
			SmoothSortPairSHR(p, trail);
			pshift += trail;
			trusty = false;
		}
		if (!trusty)
			SmoothSortSift(head, cmp, pshift);
	}

	template<class TIter, class TPred>
	void SmoothSortImpl(const TIter &beginRef, const TIter &endRef, const TPred &cmp)
	{
		if (beginRef == endRef)
			return;

		SmoothSortSizePair p = { 1, 0 };
		int pshift = 1;

		TIter head = beginRef;
		TIter high = endRef;
		--high;

		/* Precompute Leonardo numbers, scaled by element width */
		while (head != high)
		{
			if ((p.m_low & 3) == 3)
			{
				SmoothSortSift(head, cmp, pshift);
				SmoothSortPairSHR(p, 2);
				pshift += 2;
			}
			else
			{
				RKIT_ASSERT(pshift > 0);

				if (rkit::priv::LeonardoSequence_t::kNumbers[pshift - 1] >= static_cast<size_t>(high - head))
					SmoothSortTrinkle(head, cmp, p, pshift, false);
				else
					SmoothSortSift(head, cmp, pshift);

				if (pshift == 1)
				{
					SmoothSortPairSHL(p, 1);
					pshift = 0;
				}
				else
				{
					SmoothSortPairSHL(p, pshift - 1);
					pshift = 1;
				}
			}

			p.m_low |= 1;
			++head;
		}

		SmoothSortTrinkle(head, cmp, p, pshift, false);

		while (pshift != 1 || p.m_low != 1 || p.m_high != 0)
		{
			--head;
			if (pshift <= 1)
			{
				int trail = SmoothSortPairNTZ(p);
				SmoothSortPairSHR(p, trail);
				pshift += trail;
			}
			else
			{
				SmoothSortPairSHL(p, 2);
				pshift -= 2;
				p.m_low ^= 7;
				SmoothSortPairSHR(p, 1);
				SmoothSortTrinkle(head - rkit::priv::LeonardoSequence_t::kNumbers[pshift], cmp, p, pshift + 1, true);
				SmoothSortPairSHL(p, 1);
				p.m_low |= 1;
				SmoothSortTrinkle(head, cmp, p, pshift, true);
			}
		}
	}

} }

#include "Algorithm.h"

namespace rkit
{
	template<class TIter, class TPredicate>
	void SmoothSort(const TIter &itBegin, const TIter &itEnd, const TPredicate &pred)
	{
		priv::SmoothSortImpl(itBegin, itEnd, pred);
	}

	template<class TIter>
	void SmoothSort(const TIter &itBegin, const TIter &itEnd)
	{
		SmoothSort(itBegin, itEnd, DefaultCompareLessPred());
	}
}
