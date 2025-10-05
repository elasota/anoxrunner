#pragma once

#include <stdint.h>

#include <xmmintrin.h>
#include <pmmintrin.h>

#include "FPRounding.h"

namespace rkit { namespace priv {
	typedef uint32_t FloatingPointStateInternal;

	namespace fpcontrol
	{
		inline FloatingPointStateInternal GetCurrent()
		{
			return _mm_getcsr();
		}

		inline FloatingPointStateInternal GetDefault()
		{
			return 0;
		}

		inline void SetIEEEStrict(FloatingPointStateInternal &fpState)
		{
			const uint32_t maskBits = _MM_DENORMALS_ZERO_MASK | _MM_FLUSH_ZERO_MASK | _MM_ROUND_MASK;
			fpState &= ~maskBits;
		}

		inline void SetFast(FloatingPointStateInternal &fpState)
		{
			fpState |= _MM_DENORMALS_ZERO_MASK | _MM_FLUSH_ZERO_MASK;
		}

		inline void SetRoundingMode(FloatingPointStateInternal &fpState, FloatingPointRoundingMode roundingMode)
		{
			const uint32_t noModeCSR = fpState;
			uint32_t roundFlag = 0;

			switch (roundingMode)
			{
			case FloatingPointRoundingMode::RoundNearestTiesToEven:
				roundFlag = _MM_ROUND_NEAREST;
				break;
			case FloatingPointRoundingMode::RoundDown:
				roundFlag = _MM_ROUND_DOWN;
				break;
			case FloatingPointRoundingMode::RoundUp:
				roundFlag = _MM_ROUND_UP;
				break;
			case FloatingPointRoundingMode::RoundTowardZero:
				roundFlag = _MM_ROUND_TOWARD_ZERO;
				break;
			default:
				return;
			}

			fpState = (noModeCSR | roundFlag);
		}

		void Commit(FloatingPointStateInternal fpState)
		{
			_mm_setcsr(fpState);
		}
	};
} } // rkit::priv
