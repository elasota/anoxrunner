#pragma once

namespace rkit
{
	template<class TIter, class TPredicate>
	void QuickSort(const TIter &itBegin, const TIter &itEnd, const TPredicate &pred);

	template<class TIter>
	void QuickSort(const TIter &itBegin, const TIter &itEnd);
}

#include "Algorithm.h"
#include "SmoothSort.h"
#include "TypeTraits.h"

namespace rkit { namespace priv
{
	template<class TIter, class TPredicate>
	TIter QuickSortPartition(TIter itBegin, TIter itEnd, const TPredicate &pred)
	{
		TIter pivot = itEnd;
		--pivot;

		TIter newPivot = itBegin;

		for (TIter scan = itBegin; scan != pivot; ++scan)
		{
			if (!pred(*pivot, *scan))
			{
				Swap(*newPivot, *scan);
				++newPivot;
			}
		}

		if (newPivot != pivot)
			Swap(*newPivot, *pivot);

		return newPivot;
	}

	template<class TIter, class TPredicate>
	void QuickSortRecursive(const TIter &itBeginRef, const TIter &itEndRef, const TPredicate &pred, int depth)
	{
		if (itBeginRef == itEndRef)
			return;

		if (depth > 20)
			SmoothSortImpl<TIter, TPredicate>(itBeginRef, itEndRef, pred);

		const TIter itBegin = itBeginRef;
		const TIter itEnd = itEndRef;

		const TIter pivot = QuickSortPartition<TIter, TPredicate>(itBegin, itEnd, pred);
		TIter nextAfterPivot = pivot;
		++nextAfterPivot;

		QuickSortRecursive<TIter, TPredicate>(itBegin, pivot, pred, depth + 1);
		QuickSortRecursive<TIter, TPredicate>(nextAfterPivot, itEnd, pred, depth + 1);
	}
} } // rkit::priv

namespace rkit
{
	template<class TIter, class TPredicate>
	void QuickSort(const TIter &itBegin, const TIter &itEnd, const TPredicate &pred)
	{
		return priv::QuickSortRecursive<TIter, TPredicate>(itBegin, itEnd, pred, 0);
	}

	template<class TIter>
	void QuickSort(const TIter &itBegin, const TIter &itEnd)
	{
		QuickSort(itBegin, itEnd, DefaultCompareLessPred());
	}
}
