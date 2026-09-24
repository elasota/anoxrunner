#pragma once

#include "CoreDefs.h"

#include <utility>

namespace rkit
{
	template<class TIter>
	RKIT_NODISCARD size_t DeduplicateSortedList(const TIter &itBegin, const TIter &itEnd)
	{
		TIter inPosition = itBegin;
		TIter outPosition = inPosition;

		++inPosition;

		size_t numDuplicates = 0;
		while (inPosition != itEnd)
		{
			if ((*inPosition) == (*outPosition))
				numDuplicates++;
			else
			{
				++outPosition;

				if (numDuplicates > 0)
					(*outPosition) = std::move(*inPosition);
			}

			++inPosition;
		}

		return numDuplicates;
	}
}
