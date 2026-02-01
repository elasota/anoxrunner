#include "rkit/Math/BitOps.h"
#include "rkit/Core/CoreLib.h"
#include "rkit/Core/Algorithm.h"
#include "rkit/Core/Platform.h"

#include <string.h>

#if RKIT_PLATFORM_ARCH_HAVE_SSE2 != 0
#include <emmintrin.h>
#endif

#if RKIT_PLATFORM_ARCH_HAVE_SSE41 != 0
#include <smmintrin.h>
#endif

namespace rkit { namespace text { namespace priv
{
	template<class TChar>
	struct CharConvertBatchCompareFlags;

	class CharConvertVectorOps;

#if RKIT_PLATFORM_ARCH_HAVE_SSE2 != 0
	template<class TChar>
	struct CharConvertBatchCompareFlags
	{
		__m128i m_values[sizeof(TChar)];
	};

	class CharConvertSSE2Helper
	{
	public:
		static void ShrinkingConvert16To8(__m128i (&results)[1], const uint16_t *src, __m128i (&validityFlags)[2]);
		static void ShrinkingConvert32To8(__m128i (&results)[1], const uint32_t *src, __m128i (&validityFlags)[4]);
		static void ShrinkingConvert32To16(__m128i (&results)[2], const uint32_t *src, __m128i (&validityFlags)[4]);

		static __m128i SSE2U_ConditionU8(__m128i reg);
		static __m128i SSE2U_ConditionU16(__m128i reg);
		static __m128i SSE2U_ConditionU32(__m128i reg);

		static __m128i SSE2U_PackUnsigned16To8(__m128i a, __m128i b);
		static __m128i SSE2U_PackUnsigned32To16(__m128i a, __m128i b);

		static __m128i Select(__m128i selector, __m128i a, __m128i b);
	};

	class CharConvertVectorOps
	{
	public:
		typedef __m128i VectorUnit_t;
		typedef __m128i CompareResults_t;

		static void EnlargingConvert(uint16_t *dest, const uint8_t *src);
		static void EnlargingConvert(uint32_t *dest, const uint8_t *src);
		static void EnlargingConvert(uint32_t *dest, const uint16_t *src);

		static void CheckShrinkingConvert(uint8_t *dest, const uint16_t *src, CharConvertBatchCompareFlags<uint16_t> &srcValidFlags);
		static void CheckShrinkingConvert(uint8_t *dest, const uint32_t *src, CharConvertBatchCompareFlags<uint32_t> &srcValidFlags);
		static void CheckShrinkingConvert(uint16_t *dest, const uint32_t *src, CharConvertBatchCompareFlags<uint32_t> &srcValidFlags);

		static void ReplacingShrinkingConvert(uint8_t *dest, const uint16_t *src, uint8_t replacement);
		static void ReplacingShrinkingConvert(uint8_t *dest, const uint32_t *src, uint8_t replacement);
		static void ReplacingShrinkingConvert(uint16_t *dest, const uint32_t *src, uint16_t replacement);

		static bool AllValid(const CharConvertBatchCompareFlags<uint8_t> &validChars);
		static bool AllValid(const CharConvertBatchCompareFlags<uint16_t> &validChars);
		static bool AllValid(const CharConvertBatchCompareFlags<uint32_t> &validChars);

		template<class TChar>
		static bool AllValidIfFast(const CharConvertBatchCompareFlags<TChar> &validChars);

		template<class TChar>
		static bool CheckCompareFlag(const CharConvertBatchCompareFlags<TChar> &flags, size_t index);

		static const size_t kBatchSize = 16;
	};

	void CharConvertVectorOps::EnlargingConvert(uint16_t *dest, const uint8_t *src)
	{
		__m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i *>(src));
		_mm_storeu_si128(reinterpret_cast<__m128i *>(dest) + 0, _mm_unpacklo_epi8(v, _mm_setzero_si128()));
		_mm_storeu_si128(reinterpret_cast<__m128i *>(dest) + 1, _mm_unpackhi_epi8(v, _mm_setzero_si128()));
	}

	void CharConvertVectorOps::EnlargingConvert(uint32_t *dest, const uint8_t *src)
	{
		__m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i *>(src));
		__m128i v0 = _mm_unpacklo_epi8(v, _mm_setzero_si128());
		__m128i v1 = _mm_unpacklo_epi8(v, _mm_setzero_si128());
		_mm_storeu_si128(reinterpret_cast<__m128i *>(dest) + 0, _mm_unpacklo_epi16(v0, _mm_setzero_si128()));
		_mm_storeu_si128(reinterpret_cast<__m128i *>(dest) + 1, _mm_unpackhi_epi16(v0, _mm_setzero_si128()));
		_mm_storeu_si128(reinterpret_cast<__m128i *>(dest) + 2, _mm_unpacklo_epi16(v1, _mm_setzero_si128()));
		_mm_storeu_si128(reinterpret_cast<__m128i *>(dest) + 3, _mm_unpackhi_epi16(v1, _mm_setzero_si128()));
	}

	void CharConvertVectorOps::EnlargingConvert(uint32_t *dest, const uint16_t *src)
	{
		__m128i v0 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(src) + 0);
		__m128i v1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(src) + 1);
		_mm_storeu_si128(reinterpret_cast<__m128i *>(dest) + 0, _mm_unpacklo_epi16(v0, _mm_setzero_si128()));
		_mm_storeu_si128(reinterpret_cast<__m128i *>(dest) + 1, _mm_unpackhi_epi16(v0, _mm_setzero_si128()));
		_mm_storeu_si128(reinterpret_cast<__m128i *>(dest) + 2, _mm_unpacklo_epi16(v1, _mm_setzero_si128()));
		_mm_storeu_si128(reinterpret_cast<__m128i *>(dest) + 3, _mm_unpackhi_epi16(v1, _mm_setzero_si128()));
	}

	void CharConvertSSE2Helper::ShrinkingConvert16To8(__m128i (&results)[1], const uint16_t *src, __m128i (&validityFlags)[2])
	{
		__m128i srcVectors[2] =
		{
			_mm_loadu_si128(reinterpret_cast<const __m128i *>(src) + 0),
			_mm_loadu_si128(reinterpret_cast<const __m128i *>(src) + 1)
		};

		for (int i = 0; i < 2; i++)
		{
			__m128i highBits = _mm_srli_epi16(srcVectors[i], 8);
			validityFlags[i] = _mm_cmpeq_epi16(highBits, _mm_setzero_si128());
		}

		results[0] = SSE2U_ConditionU8(SSE2U_PackUnsigned16To8(SSE2U_ConditionU16(srcVectors[0]), SSE2U_ConditionU16(srcVectors[1])));
	}

	void CharConvertSSE2Helper::ShrinkingConvert32To8(__m128i (&results)[1], const uint32_t *src, __m128i (&validityFlags)[4])
	{
		__m128i srcVectors[4] =
		{
			_mm_loadu_si128(reinterpret_cast<const __m128i *>(src) + 0),
			_mm_loadu_si128(reinterpret_cast<const __m128i *>(src) + 1),
			_mm_loadu_si128(reinterpret_cast<const __m128i *>(src) + 2),
			_mm_loadu_si128(reinterpret_cast<const __m128i *>(src) + 3)
		};

		for (int i = 0; i < 4; i++)
		{
			__m128i highBits = _mm_srli_epi16(srcVectors[i], 8);
			validityFlags[i] = _mm_cmpeq_epi32(highBits, _mm_setzero_si128());
		}

		__m128i packed0 = SSE2U_PackUnsigned32To16(SSE2U_ConditionU32(srcVectors[0]), SSE2U_ConditionU32(srcVectors[1]));
		__m128i packed1 = SSE2U_PackUnsigned32To16(SSE2U_ConditionU32(srcVectors[2]), SSE2U_ConditionU32(srcVectors[3]));

		results[0] = SSE2U_ConditionU8(SSE2U_PackUnsigned16To8(packed0, packed1));
	}

	void CharConvertSSE2Helper::ShrinkingConvert32To16(__m128i (&results)[2], const uint32_t *src, __m128i (&validityFlags)[4])
	{
		__m128i srcVectors[4] =
		{
			_mm_loadu_si128(reinterpret_cast<const __m128i *>(src) + 0),
			_mm_loadu_si128(reinterpret_cast<const __m128i *>(src) + 1),
			_mm_loadu_si128(reinterpret_cast<const __m128i *>(src) + 2),
			_mm_loadu_si128(reinterpret_cast<const __m128i *>(src) + 3)
		};

		for (int i = 0; i < 4; i++)
		{
			__m128i highBits = _mm_srli_epi16(srcVectors[i], 16);
			validityFlags[i] = _mm_cmpeq_epi32(highBits, _mm_setzero_si128());
		}

		results[0] = SSE2U_ConditionU16(SSE2U_PackUnsigned32To16(SSE2U_ConditionU32(srcVectors[0]), SSE2U_ConditionU32(srcVectors[1])));
		results[1] = SSE2U_ConditionU16(SSE2U_PackUnsigned32To16(SSE2U_ConditionU32(srcVectors[2]), SSE2U_ConditionU32(srcVectors[3])));
	}

	__m128i CharConvertSSE2Helper::SSE2U_ConditionU8(__m128i reg)
	{
#if RKIT_PLATFORM_ARCH_HAVE_SSE41 != 0
		return reg;
#else
		return _mm_xor_epi32(reg, _mm_set1_epi8(-0x80));
#endif
	}

	__m128i CharConvertSSE2Helper::SSE2U_ConditionU16(__m128i reg)
	{
#if RKIT_PLATFORM_ARCH_HAVE_SSE41 != 0
		return reg;
#else
		return _mm_xor_epi32(reg, _mm_set1_epi16(-0x8000));
#endif
	}

	__m128i CharConvertSSE2Helper::SSE2U_ConditionU32(__m128i reg)
	{
#if RKIT_PLATFORM_ARCH_HAVE_SSE41 != 0
		return reg;
#else
		return _mm_xor_epi32(reg, _mm_set1_epi32(-0x80000000));
#endif
	}

	__m128i CharConvertSSE2Helper::SSE2U_PackUnsigned16To8(__m128i a, __m128i b)
	{
#if RKIT_PLATFORM_ARCH_HAVE_SSE41 != 0
		return _mm_packus_epi16(a, b);
#else
		return _mm_packs_epi16(a, b);
#endif
	}

	__m128i CharConvertSSE2Helper::SSE2U_PackUnsigned32To16(__m128i a, __m128i b)
	{
#if RKIT_PLATFORM_ARCH_HAVE_SSE41 != 0
		return _mm_packus_epi32(a, b);
#else
		return _mm_packs_epi32(a, b);
#endif
	}

	__m128i CharConvertSSE2Helper::Select(__m128i selector, __m128i a, __m128i b)
	{
		const __m128i aSelected = _mm_and_si128(selector, a);
		const __m128i bSelected = _mm_andnot_si128(selector, a);
		return _mm_or_si128(aSelected, bSelected);
	}

	void CharConvertVectorOps::CheckShrinkingConvert(uint8_t *dest, const uint16_t *src, CharConvertBatchCompareFlags<uint16_t> &srcValidFlags)
	{
		__m128i results[1];

		CharConvertSSE2Helper::ShrinkingConvert16To8(results, src, srcValidFlags.m_values);
		_mm_storeu_si128(reinterpret_cast<__m128i *>(dest) + 0, results[0]);
	}

	void CharConvertVectorOps::CheckShrinkingConvert(uint8_t *dest, const uint32_t *src, CharConvertBatchCompareFlags<uint32_t> &srcValidFlags)
	{
		__m128i results[1];

		CharConvertSSE2Helper::ShrinkingConvert32To8(results, src, srcValidFlags.m_values);
		_mm_storeu_si128(reinterpret_cast<__m128i *>(dest) + 0, results[0]);
	}

	void CharConvertVectorOps::CheckShrinkingConvert(uint16_t *dest, const uint32_t *src, CharConvertBatchCompareFlags<uint32_t> &srcValidFlags)
	{
		__m128i results[2];

		CharConvertSSE2Helper::ShrinkingConvert32To16(results, src, srcValidFlags.m_values);
		_mm_storeu_si128(reinterpret_cast<__m128i *>(dest) + 0, results[0]);
		_mm_storeu_si128(reinterpret_cast<__m128i *>(dest) + 1, results[1]);
	}

	void CharConvertVectorOps::ReplacingShrinkingConvert(uint8_t *dest, const uint16_t *src, uint8_t replacement)
	{
		__m128i results[1];
		__m128i validMask[2];
		CharConvertSSE2Helper::ShrinkingConvert16To8(results, src, validMask);

		__m128i shrunkValueMask = _mm_packs_epi16(validMask[0], validMask[1]);
		__m128i fallback = _mm_set1_epi8(static_cast<char>(replacement));

		_mm_storeu_si128(reinterpret_cast<__m128i *>(dest), CharConvertSSE2Helper::Select(shrunkValueMask, results[0], fallback));
	}

	void CharConvertVectorOps::ReplacingShrinkingConvert(uint8_t *dest, const uint32_t *src, uint8_t replacement)
	{
		__m128i results[1];
		__m128i validMask[4];
		CharConvertSSE2Helper::ShrinkingConvert32To8(results, src, validMask);

		__m128i shrunkValueMask0 = _mm_packs_epi32(validMask[0], validMask[1]);
		__m128i shrunkValueMask1 = _mm_packs_epi32(validMask[2], validMask[3]);
		__m128i shrunkValueMask = _mm_packs_epi16(shrunkValueMask0, shrunkValueMask1);
		__m128i fallback = _mm_set1_epi8(static_cast<char>(replacement));

		_mm_storeu_si128(reinterpret_cast<__m128i *>(dest), CharConvertSSE2Helper::Select(shrunkValueMask, results[0], fallback));
	}

	void CharConvertVectorOps::ReplacingShrinkingConvert(uint16_t *dest, const uint32_t *src, uint16_t replacement)
	{
		__m128i results[2];
		__m128i validMask[4];
		CharConvertSSE2Helper::ShrinkingConvert32To16(results, src, validMask);

		__m128i shrunkValueMask0 = _mm_packs_epi32(validMask[0], validMask[1]);
		__m128i shrunkValueMask1 = _mm_packs_epi32(validMask[2], validMask[3]);
		__m128i fallback = _mm_set1_epi16(static_cast<short>(replacement));

		_mm_storeu_si128(reinterpret_cast<__m128i *>(dest) + 0, CharConvertSSE2Helper::Select(shrunkValueMask0, results[0], fallback));
		_mm_storeu_si128(reinterpret_cast<__m128i *>(dest) + 1, CharConvertSSE2Helper::Select(shrunkValueMask1, results[1], fallback));
	}

	bool CharConvertVectorOps::AllValid(const CharConvertBatchCompareFlags<uint8_t> &validChars)
	{
		return _mm_movemask_epi8(validChars.m_values[0]) == 0xffff;
	}

	bool CharConvertVectorOps::AllValid(const CharConvertBatchCompareFlags<uint16_t> &validChars)
	{
		__m128i combined = _mm_and_si128(validChars.m_values[0], validChars.m_values[1]);
		return _mm_movemask_epi8(combined) == 0xffff;
	}

	bool CharConvertVectorOps::AllValid(const CharConvertBatchCompareFlags<uint32_t> &validChars)
	{
		__m128i combined0 = _mm_and_si128(validChars.m_values[0], validChars.m_values[1]);
		__m128i combined1 = _mm_and_si128(validChars.m_values[2], validChars.m_values[3]);
		__m128i combined = _mm_and_si128(combined0, combined1);
		return _mm_movemask_epi8(combined) == 0xffff;
	}

	template<class TChar>
	bool CharConvertVectorOps::AllValidIfFast(const CharConvertBatchCompareFlags<TChar> &validChars)
	{
		return AllValid(validChars);
	}

	template<class TChar>
	bool CharConvertVectorOps::CheckCompareFlag(const CharConvertBatchCompareFlags<TChar> &flags, size_t index)
	{
		return reinterpret_cast<const TChar *>(&flags.m_values[0])[index] != 0;
	}

#else
	class CharVectorOps
	{
	public:
		static const size_t kBatchSize = 1;
	};
#endif

	// 22 so we can ignore missing zero bits in UTF-8's 4-byte representation and just
	// treat them as invalid UTF-32 instead
	static const int kUTF32PackingBits = 22;
	static const uint32_t kUTF32CodePointMask = (1 << kUTF32PackingBits) - 1;
	static const uint32_t kInvalidCodePoint = kUTF32CodePointMask;
	static const uint32_t kMaxValidUTFCodePoint = 0x10ffff;

	static const uint16_t kUnicodeAnySurrogateMask = 0xFFFFF800u;
	static const uint16_t kUnicodeAnySurrogateExpected = 0xD800u;

	static const uint32_t kUnicodeHighOrLowSurrogateMask = 0xFFFFFC00u;
	static const uint32_t kUnicodeHighSurrogateExpected = 0xD800u;
	static const uint32_t kUnicodeLowSurrogateExpected = 0xDC00u;

	static bool IsValidUTF32CodePoint(uint32_t cp)
	{
		if (cp > kMaxValidUTFCodePoint)
			return false;
		if ((cp & kUnicodeAnySurrogateMask) == kUnicodeAnySurrogateExpected)
			return false;
		return true;
	}

	static uint32_t PackUTF32DecodeResult(uint32_t codePoint, uint_fast16_t numExtraChars)
	{
		return (static_cast<uint32_t>(numExtraChars) << kUTF32PackingBits) | codePoint;
	}

	static uint32_t UnpackUTF32CodePoint(uint32_t packed)
	{
		return packed & kUTF32CodePointMask;
	}

	static uint_fast16_t UnpackUTF32ExtraChars(uint32_t packed)
	{
		return static_cast<uint_fast16_t>(packed >> kUTF32PackingBits);
	}

	// Decodes an input character to a "packed" code point with extra bytes consumed signaling
	typedef uint32_t (*DecodeToUTF32RangeCheckedFunc_t)(const void *inputCharsPtr, size_t numAvailableCodes);
	typedef uint32_t (*DecodeToUTF32Func_t)(const void *inputCharsPtr);

	// Encodes a code point and returns number of characters encoded, or 0 if invalid
	typedef uint_fast8_t (*EncodeFromUTF32Func_t)(uint32_t codePoint, void *outputChars);

	// Measures the storage required for a specific symbol, or 0 if it is invalid
	typedef uint_fast8_t (*MeasureUTF32EncodeFunc_t)(uint32_t codePoint);

	struct ASCIIHandler
	{
		typedef uint8_t Char_t;
		static const uint8_t kMaxCodeLength = 1;

		static uint32_t Decode(const Char_t *inputCharsPtr, size_t numAvailable);
		static uint_fast8_t Encode(uint32_t codePoint, Char_t *outputChars);
		static uint_fast8_t Measure(uint32_t codePoint);
	};

	struct UTF8Handler
	{
		typedef uint8_t Char_t;
		static const uint8_t kMaxCodeLength = 4;

		static uint32_t Decode(const Char_t *inputCharsPtr, size_t numAvailable);
		static uint_fast8_t Encode(uint32_t codePoint, Char_t *outputChars);
		static uint_fast8_t Measure(uint32_t codePoint);

	private:
		// This uses a 5:2:9 packing scheme, which takes advantage of the fact that we can store the minimum
		// code point as 9 bits added to 0x80, conveniently also shifted left 7 (= 5+2) bits.
		//
		// 5 bits for low bit mask
		// 2 bits for number of additional bytes
		// 9 bits for first code point
		static constexpr uint16_t kVLC_2 = (((1 << 5) - 1) | (1 << 5) | (0x80 - 0x80));
		static constexpr uint16_t kVLC_3 = (((1 << 4) - 1) | (2 << 5) | (0x800 - 0x80));

		// There are only 3 low bits but we intentionally code it as 4 so that invalid chars
		// get converted to out-of-range UTF-32 values.
		static constexpr uint16_t kVLC_4 = (((1 << 4) - 1) | (3 << 5) | (0x10000 - 0x80));

		// Coded as no low-bits and no extra bits, with a minimum code point that's too low
		// to ever be reached
		static constexpr uint32_t kVLC_Invalid = 0xff80;

		static uint32_t ms_vlc[8];
	};

	struct UTF16Handler
	{
		typedef uint16_t Char_t;
		static const uint8_t kMaxCodeLength = 2;

		static uint32_t Decode(const Char_t *inputCharsPtr, size_t numAvailable);
		static uint_fast8_t Encode(uint32_t codePoint, Char_t *outputChars);
		static uint_fast8_t Measure(uint32_t codePoint);
	};

	struct UTF32Handler
	{
		typedef uint32_t Char_t;
		static const uint8_t kMaxCodeLength = 1;

		static uint32_t Decode(const Char_t *inputCharsPtr, size_t numAvailable);
		static uint_fast8_t Encode(uint32_t codePoint, Char_t *outputChars);
		static uint_fast8_t Measure(uint32_t codePoint);
	};

	struct InvalidHandler
	{
		typedef uint8_t Char_t;
		static const uint8_t kMaxCodeLength = 0;

		static uint32_t Decode(const Char_t *inputCharsPtr, size_t numAvailable);
		static uint_fast8_t Encode(uint32_t codePoint, Char_t *outputChars);
		static uint_fast8_t Measure(uint32_t codePoint);
	};

	template<class THandler>
	struct AutoUnicodeTranscoder
	{
		static const uint8_t kMaxCodeLength = THandler::kMaxCodeLength;

		static void SelectDecoder(DecodeToUTF32Func_t &outDecode, DecodeToUTF32RangeCheckedFunc_t &outDecodeRangeChecked, uint_fast8_t &outMaxCodeLength);
		static void SelectEncoder(EncodeFromUTF32Func_t &outEncode, MeasureUTF32EncodeFunc_t &outMeasure, uint_fast8_t &outMaxCodeLength);

	private:
		static uint32_t Decode(const void *inputCharsPtr);
		static uint32_t DecodeRangeChecked(const void *inputCharsPtr, size_t numAvailable);
		static uint_fast8_t Encode(uint32_t codePoint, void *outputChars);
		static uint_fast8_t Measure(uint32_t codePoint);
	};

	template<class TTargetChar, class TSourceChar, bool TIsValid>
	struct ConvertHelper
	{
	};

	template<class TTargetChar, class TSourceChar>
	struct ConvertHelper<TTargetChar, TSourceChar, false>
	{
		static void ConvertToLargerRawEncoding(TTargetChar *outputChars, const TSourceChar *inputChars, size_t charCount)
		{
		}

		static size_t ConvertToSmallerRawEncoding(TTargetChar *outputChars, size_t outMaxChars, size_t &outCharsEmitted,
			const TSourceChar *inputChars, size_t inCharCount, UnknownCharBehavior unknownCharBehavior, TTargetChar unknownReplacementChar)
		{
			outCharsEmitted = 0;
			return 0;
		}
	};

	template<class TTargetChar, class TSourceChar>
	struct ConvertHelper<TTargetChar, TSourceChar, true>
	{
		static void ConvertToLargerRawEncoding(TTargetChar *outputChars, const TSourceChar *inputChars, size_t charCount);
		static size_t ConvertToSmallerRawEncoding(TTargetChar *outputChars, size_t outMaxChars, size_t &outCharsEmitted,
			const TSourceChar *inputChars, size_t inCharCount, UnknownCharBehavior unknownCharBehavior, TTargetChar unknownReplacementChar);
	};

	static size_t DecodeInvalid(uint32_t *outputChars, size_t outMaxChars, uint8_t *optCharConsumpionCounters, size_t &outCharsEmitted,
		const void *inputChars, CharacterEncoding inEncoding, size_t inCharCount)
	{
		outCharsEmitted = 0;
		return 0;
	}

	static size_t EncodeInvalid(void *outputChars, CharacterEncoding outEncoding, size_t outMaxChars, size_t &outCharsEmitted,
		const uint32_t *inputChars, size_t inCharCount, uint32_t unknownReplacementChar)
	{
		outCharsEmitted = 0;
		return 0;
	}

	template<class THandler>
	void AutoUnicodeTranscoder<THandler>::SelectDecoder(DecodeToUTF32Func_t &outDecode, DecodeToUTF32RangeCheckedFunc_t &outDecodeRangeChecked, uint_fast8_t &outMaxCodeLength)
	{
		outDecode = Decode;
		outDecodeRangeChecked = DecodeRangeChecked;
		outMaxCodeLength = kMaxCodeLength;
	}

	template<class THandler>
	void AutoUnicodeTranscoder<THandler>::SelectEncoder(EncodeFromUTF32Func_t &outEncode, MeasureUTF32EncodeFunc_t &outMeasure, uint_fast8_t &outMaxCodeLength)
	{
		outEncode = Encode;
		outMeasure = Measure;
		outMaxCodeLength = kMaxCodeLength;
	}


	template<class THandler>
	uint32_t AutoUnicodeTranscoder<THandler>::Decode(const void *inputCharsPtr)
	{
		return THandler::Decode(static_cast<const typename THandler::Char_t *>(inputCharsPtr), THandler::kMaxCodeLength);
	}

	template<class THandler>
	uint32_t AutoUnicodeTranscoder<THandler>::DecodeRangeChecked(const void *inputCharsPtr, size_t availableCharacters)
	{
		return THandler::Decode(static_cast<const typename THandler::Char_t *>(inputCharsPtr), availableCharacters);
	}

	template<class THandler>
	uint_fast8_t AutoUnicodeTranscoder<THandler>::Encode(uint32_t codePoint, void *outputChars)
	{
		return THandler::Encode(codePoint, static_cast<typename THandler::Char_t *>(outputChars));
	}

	template<class THandler>
	uint_fast8_t AutoUnicodeTranscoder<THandler>::Measure(uint32_t codePoint)
	{
		return THandler::Measure(codePoint);
	}

	// ===========================================================================================
	// Implementations

	uint32_t ASCIIHandler::Decode(const Char_t *inputCharsPtr, size_t numAvailable)
	{
		const Char_t ch = *inputCharsPtr;
		if (ch & 0x80)
			return kInvalidCodePoint;
		else
			return ch;
	}

	uint_fast8_t ASCIIHandler::Encode(uint32_t codePoint, Char_t *outputChars)
	{
		if (codePoint >= 0x80)
			return 0;

		*outputChars = static_cast<Char_t>(codePoint);
		return 1;
	}

	uint_fast8_t ASCIIHandler::Measure(uint32_t codePoint)
	{
		if (codePoint >= 0x80)
			return 0;

		return 1;
	}

	uint32_t UTF8Handler::Decode(const Char_t *inputCharsPtr, size_t numAvailable)
	{
		const Char_t firstCh = *inputCharsPtr;

		if ((firstCh & 0x80) == 0)
			return firstCh;

		const uint16_t vlcInfo = ms_vlc[(firstCh >> 4) & 7];

		const uint16_t lowBitMask = (vlcInfo & 0x1f);
		const uint8_t numExtraBytes = ((vlcInfo >> 5) & 3);
		const uint32_t minCodePoint = static_cast<uint32_t>(vlcInfo & 0xFF80) + 0x80;

		if (numAvailable < static_cast<size_t>(1) + numExtraBytes)
			return kInvalidCodePoint;

		uint32_t codePoint = (firstCh & lowBitMask);
		for (size_t i = 0; i < numExtraBytes; i++)
		{
			const Char_t extraCh = inputCharsPtr[i + 1];
			if ((extraCh & 0xc0) != 0x80)
				return kInvalidCodePoint;

			codePoint = ((codePoint << 6) | (extraCh & 0x3f));
		}

		if (codePoint < minCodePoint)
			return kInvalidCodePoint;

		return PackUTF32DecodeResult(codePoint, numExtraBytes);
	}

	uint_fast8_t UTF8Handler::Encode(uint32_t codePoint, Char_t *outputChars)
	{
		const uint_fast8_t length = Measure(codePoint);

		if (length < 2)
		{
			if (length == 1)
				*outputChars = static_cast<uint8_t>(codePoint);
		}
		else
		{
			const uint_fast8_t initialByteHighBits = ((0xf00 >> length) & 0xff);

			for (uint_fast8_t byteIndex = length - 1; byteIndex > 0; --byteIndex)
			{
				outputChars[byteIndex] = static_cast<Char_t>((codePoint & 0x3f) | 0x80);
				codePoint >>= 6;
			}

			outputChars[0] = static_cast<Char_t>((codePoint | (0xf00 >> length)) & 0xff);
		}

		return length;
	}

	uint_fast8_t UTF8Handler::Measure(uint32_t codePoint)
	{
		switch (rkit::bitops::FindHighestSet(codePoint) + 1)
		{
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
			return 1;
		case 8:
		case 9:
		case 10:
		case 11:
			return 2;
		case 12:
		case 13:
		case 14:
		case 15:
			return 3;
		case 16:
			if ((codePoint & kUnicodeAnySurrogateMask) == kUnicodeAnySurrogateExpected)
				return 0;
			return 3;
		case 17:
		case 18:
		case 19:
		case 20:
			return 4;
		case 21:
			if (codePoint > kMaxValidUTFCodePoint)
				return 0;
			return 4;
		default:
			return 0;
		}
	}

	uint32_t UTF8Handler::ms_vlc[8] =
	{
		kVLC_Invalid, // 1000xxxx
		kVLC_Invalid, // 1001xxxx
		kVLC_Invalid, // 1010xxxx
		kVLC_Invalid, // 1011xxxx

		kVLC_2, // 1100xxxx
		kVLC_2, // 1101xxxx
		kVLC_3, // 1110xxxx
		kVLC_4, // 1111xxxx
	};


	uint32_t UTF16Handler::Decode(const Char_t *RKIT_MAY_ALIAS inputCharsPtr, size_t numAvailable)
	{
		const Char_t firstCh = *inputCharsPtr;

		switch (firstCh & kUnicodeHighOrLowSurrogateMask)
		{
		case kUnicodeHighSurrogateExpected:
			{
				if (numAvailable < 2)
					return kInvalidCodePoint;

				const Char_t secondCh = inputCharsPtr[1];

				if ((secondCh & kUnicodeHighOrLowSurrogateMask) != kUnicodeLowSurrogateExpected)
					return kInvalidCodePoint;

				const uint32_t codePoint = ((static_cast<uint32_t>(firstCh & 0x3ff) << 10) | static_cast<uint32_t>(secondCh & 0x3ff)) + 0x10000u;
				return PackUTF32DecodeResult(codePoint, 1);
			}
			break;
		case kUnicodeLowSurrogateExpected:
			return kInvalidCodePoint;
		default:
			return firstCh;
		}
	}

	uint_fast8_t UTF16Handler::Encode(uint32_t codePoint, Char_t *RKIT_MAY_ALIAS outputChars)
	{
		const uint_fast8_t length = Measure(codePoint);
		if (length == 1)
			outputChars[0] = static_cast<Char_t>(codePoint);
		else if (length == 2)
		{
			const uint32_t surrogateOffset = codePoint - 0x10000;
			const uint32_t highBits = (surrogateOffset >> 10);
			const uint32_t lowBits = (surrogateOffset & 0x3ff);

			outputChars[0] = static_cast<Char_t>(kUnicodeHighSurrogateExpected | highBits);
			outputChars[1] = static_cast<Char_t>(kUnicodeLowSurrogateExpected | lowBits);
		}

		return length;
	}

	uint_fast8_t UTF16Handler::Measure(uint32_t codePoint)
	{
		if (codePoint > kMaxValidUTFCodePoint)
			return 0;

		if (codePoint > 0xffff)
			return 2;

		return 1;
	}

	uint32_t UTF32Handler::Decode(const Char_t *RKIT_MAY_ALIAS inputCharsPtr, size_t numAvailable)
	{
		return *inputCharsPtr;
	}

	uint_fast8_t UTF32Handler::Encode(uint32_t codePoint, Char_t *RKIT_MAY_ALIAS outputChars)
	{
		if (IsValidUTF32CodePoint(codePoint))
			return 0;

		*outputChars = codePoint;
		return 1;
	}

	uint_fast8_t UTF32Handler::Measure(uint32_t codePoint)
	{
		return IsValidUTF32CodePoint(codePoint) ? 1 : 0;
	}

	uint32_t InvalidHandler::Decode(const Char_t *inputCharsPtr, size_t numAvailable)
	{
		return kInvalidCodePoint;
	}

	uint_fast8_t InvalidHandler::Encode(uint32_t codePoint, Char_t *outputChars)
	{
		return 0;
	}

	uint_fast8_t InvalidHandler::Measure(uint32_t codePoint)
	{
		return 0;
	}

	template<class TTargetChar, class TSourceChar>
	void ConvertHelper<TTargetChar, TSourceChar, true>::ConvertToLargerRawEncoding(TTargetChar * RKIT_MAY_ALIAS RKIT_RESTRICT outputChars, const TSourceChar * RKIT_MAY_ALIAS RKIT_RESTRICT inputChars, size_t charCount)
	{
		while (charCount >= CharConvertVectorOps::kBatchSize)
		{
			CharConvertVectorOps::EnlargingConvert(outputChars, inputChars);

			outputChars += CharConvertVectorOps::kBatchSize;
			inputChars += CharConvertVectorOps::kBatchSize;
			charCount -= CharConvertVectorOps::kBatchSize;
		}

		for (size_t i = 0; i < charCount; i++)
			outputChars[i] = inputChars[i];
	}

	template<class TTargetChar, class TSourceChar>
	size_t ConvertHelper<TTargetChar, TSourceChar, true>::ConvertToSmallerRawEncoding(TTargetChar * RKIT_MAY_ALIAS RKIT_RESTRICT outputChars, size_t outMaxChars, size_t &outCharsEmitted,
		const TSourceChar * RKIT_MAY_ALIAS RKIT_RESTRICT inputChars, size_t inCharCount, UnknownCharBehavior unknownCharBehavior, TTargetChar unknownReplacementChar)
	{
		if (unknownCharBehavior == UnknownCharBehavior::kFail)
		{
			size_t charsToConvert = Min(outMaxChars, inCharCount);

			while (charsToConvert >= CharConvertVectorOps::kBatchSize)
			{
				CharConvertBatchCompareFlags<TSourceChar> validChars;
				CharConvertVectorOps::CheckShrinkingConvert(outputChars, inputChars, validChars);

				if (!CharConvertVectorOps::AllValid(validChars))
				{
					outCharsEmitted = 0;
					return 0;
				}

				outputChars += CharConvertVectorOps::kBatchSize;
				inputChars += CharConvertVectorOps::kBatchSize;
				charsToConvert -= CharConvertVectorOps::kBatchSize;
			}

			while (charsToConvert > 0)
			{
				const TSourceChar ch = *inputChars;
				if (ch > std::numeric_limits<TTargetChar>::max())
				{
					outCharsEmitted = 0;
					return 0;
				}

				*outputChars = static_cast<TTargetChar>(ch);

				outputChars++;
				inputChars++;
				charsToConvert--;
			}

			outCharsEmitted = charsToConvert;
			return outCharsEmitted;
		}

		if (unknownCharBehavior == UnknownCharBehavior::kSkipInvalid)
		{
			const TTargetChar *outputCharsStart = outputChars;
			const TSourceChar *inputCharsStart = inputChars;

			while (outMaxChars >= CharConvertVectorOps::kBatchSize && inCharCount >= CharConvertVectorOps::kBatchSize)
			{
				CharConvertBatchCompareFlags<TSourceChar> validChars;
				CharConvertVectorOps::CheckShrinkingConvert(outputChars, inputChars, validChars);

				size_t numSkipped = 0;
				if (!CharConvertVectorOps::AllValidIfFast(validChars))
				{
					size_t inOffset = 0;

					// Scan for skippable char
					while (inOffset < CharConvertVectorOps::kBatchSize)
					{
						if (!CharConvertVectorOps::CheckCompareFlag(validChars, inOffset++))
						{
							numSkipped++;
							break;
						}
					}

					// Move other chars
					while (inOffset < CharConvertVectorOps::kBatchSize)
					{
						if (CharConvertVectorOps::CheckCompareFlag(validChars, inOffset))
						{
							const TSourceChar ch = inputChars[inOffset];
							outputChars[inOffset - numSkipped] = static_cast<TTargetChar>(ch);
						}
						else
							numSkipped++;

						inOffset++;
					}
				}

				outputChars += CharConvertVectorOps::kBatchSize - numSkipped;
				outMaxChars -= CharConvertVectorOps::kBatchSize - numSkipped;

				inputChars += CharConvertVectorOps::kBatchSize;
				inCharCount -= CharConvertVectorOps::kBatchSize;
			}

			while (outMaxChars > 0 && inCharCount > 0)
			{
				const TSourceChar ch = *inputChars;
				if (ch <= std::numeric_limits<TTargetChar>::max())
				{
					*outputChars = static_cast<TTargetChar>(ch);

					outputChars++;
					outMaxChars--;
				}

				inputChars++;
				inCharCount--;
			}

			outCharsEmitted = static_cast<size_t>(outputChars - outputCharsStart);
			return static_cast<size_t>(inputChars - inputCharsStart);
		}

		if (unknownCharBehavior == UnknownCharBehavior::kReplaceInvalid)
		{
			size_t charsToConvert = Min(outMaxChars, inCharCount);

			while (charsToConvert >= CharConvertVectorOps::kBatchSize)
			{
				CharConvertVectorOps::ReplacingShrinkingConvert(outputChars, inputChars, unknownReplacementChar);

				outputChars += CharConvertVectorOps::kBatchSize;
				inputChars += CharConvertVectorOps::kBatchSize;
				charsToConvert -= CharConvertVectorOps::kBatchSize;
			}

			while (charsToConvert > 0)
			{
				const TSourceChar ch = *inputChars;
				if (ch > std::numeric_limits<TTargetChar>::max())
					*outputChars = unknownReplacementChar;
				else
					*outputChars = static_cast<TTargetChar>(ch);

				outputChars++;
				inputChars++;
				charsToConvert--;
			}

			outCharsEmitted = charsToConvert;
			return charsToConvert;
		}

		outCharsEmitted = 0;
		return 0;
	}

	template<class TTargetChar>
	void ConvertToLargerRawEncodingBySize(TTargetChar *outputChars, const void *inputChars, size_t inCharSize, size_t charCount)
	{
		switch (inCharSize)
		{
		case 1:
			ConvertHelper<TTargetChar, uint16_t, (sizeof(TTargetChar) > 1)>::ConvertToLargerRawEncoding(outputChars, static_cast<const uint16_t *>(inputChars), charCount);
			break;
		case 2:
			ConvertHelper<TTargetChar, uint16_t, (sizeof(TTargetChar) > 2)>::ConvertToLargerRawEncoding(outputChars, static_cast<const uint16_t *>(inputChars), charCount);
			break;
		default:
			break;
		}
	}

	template<class TTargetChar>
	size_t ConvertToSmallerRawEncodingBySize(TTargetChar *outputChars, size_t outMaxChars, size_t &outCharsEmitted,
		const void *inputChars, size_t inCharSize, size_t inCharCount, UnknownCharBehavior unknownCharBehavior, TTargetChar unknownReplacementChar)
	{
		switch (inCharSize)
		{
		case 2:
			return ConvertHelper<TTargetChar, uint16_t, (sizeof(TTargetChar) < 2)>::ConvertToSmallerRawEncoding(outputChars, outMaxChars, outCharsEmitted, static_cast<const uint16_t *>(inputChars), inCharCount, unknownCharBehavior, unknownReplacementChar);
		case 4:
			return ConvertHelper<TTargetChar, uint32_t, (sizeof(TTargetChar) < 4)>::ConvertToSmallerRawEncoding(outputChars, outMaxChars, outCharsEmitted, static_cast<const uint32_t *>(inputChars), inCharCount, unknownCharBehavior, unknownReplacementChar);
		default:
			outCharsEmitted = 0;
			return 0;
		}
	}

	template<class TTargetChar>
	size_t ConvertToRawEncoding(TTargetChar *outputChars, size_t outMaxChars, size_t &outCharsEmitted,
		const void *inputChars, CharacterEncoding inEncoding, size_t inCharCount, UnknownCharBehavior unknownCharBehavior, uint32_t unknownReplacementChar)
	{
		const size_t numCharsToCopy = Min(inCharCount, outMaxChars);
		const size_t inputCharSize = CharSizeForEncoding(inEncoding);

		if (sizeof(TTargetChar) == inputCharSize)
		{
			// Same-size copy
			if (outputChars)
				memcpy(outputChars, inputChars, sizeof(TTargetChar) * numCharsToCopy);

			return numCharsToCopy;
		}
		if (sizeof(TTargetChar) > inputCharSize)
		{
			ConvertToLargerRawEncodingBySize(outputChars, inputChars, inputCharSize, numCharsToCopy);
			return numCharsToCopy;
		}
		else //if (sizeof(TTargetChar) < inputCharSize)
		{
			return ConvertToSmallerRawEncodingBySize(outputChars, outMaxChars, outCharsEmitted,
				inputChars, inputCharSize, inCharCount, unknownCharBehavior, static_cast<TTargetChar>(unknownReplacementChar));
		}
	}

	namespace TranscodeFlags
	{
		static const uint32_t kHaveOutput = 1;
		static const uint32_t kInputIsVLC = 2;
	}

	struct TranscodeUnicodeParams
	{
		void *m_outputChars;
		size_t m_outMaxChars;
		size_t *m_outCharsEmitted;
		const void *m_inputChars;
		size_t m_inCharCount;
		UnknownCharBehavior m_unknownCharBehavior;
		uint32_t m_unknownReplacementChar;
		DecodeToUTF32Func_t m_decode;
		DecodeToUTF32RangeCheckedFunc_t m_decodeRangeChecked;
		uint_fast8_t m_inCharSize;
		uint_fast8_t m_inMaxCodeLength;
		EncodeFromUTF32Func_t m_encode;
		MeasureUTF32EncodeFunc_t m_measure;
		uint_fast8_t m_outCharSize;
		uint_fast8_t m_outMaxCodeLength;
	};

	template<class TOutChar, class TInChar, UnknownCharBehavior TUnknownCharBehavior, uint32_t TFlags>
	size_t TranscodeUnicodeImpl(const TranscodeUnicodeParams &params)
	{
		TOutChar *const outputChars = static_cast<TOutChar *>(params.m_outputChars);
		const size_t outMaxChars = params.m_outMaxChars;
		size_t &outCharsEmitted = *params.m_outCharsEmitted;
		const TInChar *const inputChars = static_cast<const TInChar *>(params.m_inputChars);
		const size_t inCharCount = params.m_inCharCount;

		const uint32_t unknownReplacementChar = params.m_unknownReplacementChar;
		const DecodeToUTF32Func_t decode = params.m_decode;
		const DecodeToUTF32RangeCheckedFunc_t decodeRangeChecked = params.m_decodeRangeChecked;
		const uint_fast8_t inMaxCodeLength = params.m_inMaxCodeLength;
		const EncodeFromUTF32Func_t encode = params.m_encode;
		const MeasureUTF32EncodeFunc_t measure = params.m_measure;
		const uint_fast8_t outMaxCodeLength = params.m_outMaxCodeLength;

		const size_t inMaxExtraChars = (TFlags & TranscodeFlags::kInputIsVLC) ? (inMaxCodeLength - 1) : 0;
		const size_t outMaxExtraChars = outMaxCodeLength - 1;

		const size_t inVlcCheckStart = Max(inMaxExtraChars, inCharCount) - inMaxExtraChars;
		const size_t outVlcCheckStart = Max(outMaxExtraChars, outMaxChars) - outMaxExtraChars;

		size_t numCharsEmitted = 0;
		size_t numCharsConsumed = 0;

		// VLC in and VLC out are both in range
		while (numCharsConsumed < inVlcCheckStart && numCharsEmitted < outVlcCheckStart)
		{
			const uint32_t packedDecodeResult = decode(inputChars + numCharsConsumed);
			uint32_t codePoint = (TFlags & TranscodeFlags::kInputIsVLC) ? UnpackUTF32CodePoint(packedDecodeResult) : packedDecodeResult;
			const uint32_t stepExtraCharsConsumed = (TFlags & TranscodeFlags::kInputIsVLC) ? UnpackUTF32ExtraChars(packedDecodeResult) : 0;
			uint_fast8_t stepCharsEmitted = 1;

			if (TFlags & TranscodeFlags::kHaveOutput)
				stepCharsEmitted = encode(codePoint, outputChars + numCharsEmitted);
			else
				stepCharsEmitted = measure(codePoint);

			if (stepCharsEmitted == 0)
			{
				if (TUnknownCharBehavior == UnknownCharBehavior::kReplaceInvalid)
				{
					stepCharsEmitted = 1;
					if (TFlags & TranscodeFlags::kHaveOutput)
						stepCharsEmitted = encode(unknownReplacementChar, outputChars + numCharsEmitted);
					else
						stepCharsEmitted = measure(unknownReplacementChar);
				}
				else if (TUnknownCharBehavior == UnknownCharBehavior::kFail)
				{
					outCharsEmitted = 0;
					return 0;
				}
			}

			numCharsConsumed += static_cast<size_t>(1) + stepExtraCharsConsumed;
			numCharsEmitted += stepCharsEmitted;
		}

		size_t charsConsumedLimit = 0;

		while (numCharsConsumed < inCharCount)
		{
			const uint32_t packedDecodeResult =
				(TFlags & TranscodeFlags::kInputIsVLC)
					? decodeRangeChecked(inputChars + numCharsConsumed, inCharCount - numCharsConsumed)
					: decode(inputChars + numCharsConsumed);

			uint32_t codePoint = (TFlags & TranscodeFlags::kInputIsVLC) ? UnpackUTF32CodePoint(packedDecodeResult) : packedDecodeResult;
			const uint32_t stepExtraCharsConsumed = (TFlags & TranscodeFlags::kInputIsVLC) ? UnpackUTF32ExtraChars(packedDecodeResult) : 0;

			const size_t availableOutputChars = outMaxChars - numCharsEmitted;
			uint_fast8_t wantToEmit = measure(codePoint);

			if (wantToEmit == 0)
			{
				// Invalid character
				if (TUnknownCharBehavior == UnknownCharBehavior::kReplaceInvalid)
				{
					codePoint = unknownReplacementChar;
					wantToEmit = measure(codePoint);
				}
				else if (TUnknownCharBehavior == UnknownCharBehavior::kFail)
				{
					outCharsEmitted = 0;
					return 0;
				}
			}

			if (wantToEmit > availableOutputChars)
				break;

			const uint_fast8_t stepCharsEmitted = (TFlags & TranscodeFlags::kHaveOutput)
				? encode(codePoint, outputChars + numCharsEmitted)
				: wantToEmit;

			numCharsConsumed += static_cast<size_t>(1) + stepExtraCharsConsumed;
			numCharsEmitted += stepCharsEmitted;
		}

		outCharsEmitted = numCharsEmitted;
		return numCharsConsumed;
	}

	template<class TOutChar, class TInChar, uint32_t TFlags>
	size_t TranscodeUnicode4(const TranscodeUnicodeParams &params)
	{
		switch (params.m_unknownCharBehavior)
		{
		case UnknownCharBehavior::kSkipInvalid:
			return TranscodeUnicodeImpl<TOutChar, TInChar, UnknownCharBehavior::kSkipInvalid, TFlags>(params);
		case UnknownCharBehavior::kReplaceInvalid:
			return TranscodeUnicodeImpl<TOutChar, TInChar, UnknownCharBehavior::kReplaceInvalid, TFlags>(params);
		case UnknownCharBehavior::kFail:
			return TranscodeUnicodeImpl<TOutChar, TInChar, UnknownCharBehavior::kFail, TFlags>(params);
		default:
			*params.m_outCharsEmitted = 0;
			return 0;
		}
	}

	template<class TOutChar, class TInChar, uint32_t TFlags>
	size_t TranscodeUnicode3(const TranscodeUnicodeParams &params)
	{
		if (params.m_inMaxCodeLength > 1)
			return TranscodeUnicode4<TOutChar, TInChar, (TFlags | TranscodeFlags::kInputIsVLC)>(params);
		else
			return TranscodeUnicode4<TOutChar, TInChar, TFlags>(params);
	}

	template<class TOutChar, uint32_t TFlags>
	size_t TranscodeUnicode1(const TranscodeUnicodeParams &params)
	{
		switch (params.m_inCharSize)
		{
		case 1:
			return TranscodeUnicode3<TOutChar, uint8_t, TFlags>(params);
		case 2:
			return TranscodeUnicode3<TOutChar, uint16_t, TFlags>(params);
		case 4:
			return TranscodeUnicode3<TOutChar, uint32_t, TFlags>(params);
		default:
			*params.m_outCharsEmitted = 0;
			return 0;
		}
	}

	static size_t TranscodeUnicode(const TranscodeUnicodeParams &params)
	{
		if (params.m_outputChars == nullptr)
		{
			// Don't need output size permutations
			return TranscodeUnicode1<uint8_t, 0>(params);
		}
		else
		{
			switch (params.m_outCharSize)
			{
			case 1:
				return TranscodeUnicode1<uint8_t, TranscodeFlags::kHaveOutput>(params);
			case 2:
				return TranscodeUnicode1<uint16_t, TranscodeFlags::kHaveOutput>(params);
			case 4:
				return TranscodeUnicode1<uint32_t, TranscodeFlags::kHaveOutput>(params);
			default:
				*params.m_outCharsEmitted = 0;
				return 0;
			}
		}
	}

	void SelectUnicodeDecoder(CharacterEncoding inEncoding, DecodeToUTF32Func_t &outDecode, DecodeToUTF32RangeCheckedFunc_t &outDecodeRangeChecked, uint_fast8_t &outMaxInCodeLength)
	{
		switch (inEncoding)
		{
		case CharacterEncoding::kASCII:
			AutoUnicodeTranscoder<ASCIIHandler>::SelectDecoder(outDecode, outDecodeRangeChecked, outMaxInCodeLength);
			break;
		case CharacterEncoding::kUTF8:
			AutoUnicodeTranscoder<UTF8Handler>::SelectDecoder(outDecode, outDecodeRangeChecked, outMaxInCodeLength);
			break;
		case CharacterEncoding::kUTF16:
			AutoUnicodeTranscoder<UTF16Handler>::SelectDecoder(outDecode, outDecodeRangeChecked, outMaxInCodeLength);
			break;
		case CharacterEncoding::kUTF32:
			AutoUnicodeTranscoder<UTF32Handler>::SelectDecoder(outDecode, outDecodeRangeChecked, outMaxInCodeLength);
			break;
		default:
			AutoUnicodeTranscoder<InvalidHandler>::SelectDecoder(outDecode, outDecodeRangeChecked, outMaxInCodeLength);
			break;
		}
	}

	void SelectUnicodeEncoder(CharacterEncoding outEncoding, EncodeFromUTF32Func_t &outEncode, MeasureUTF32EncodeFunc_t &outMeasure, uint_fast8_t &outMaxOutCodeLength)
	{
		switch (outEncoding)
		{
		case CharacterEncoding::kASCII:
			AutoUnicodeTranscoder<ASCIIHandler>::SelectEncoder(outEncode, outMeasure, outMaxOutCodeLength);
			break;
		case CharacterEncoding::kUTF8:
			AutoUnicodeTranscoder<UTF8Handler>::SelectEncoder(outEncode, outMeasure, outMaxOutCodeLength);
			break;
		case CharacterEncoding::kUTF16:
			AutoUnicodeTranscoder<UTF16Handler>::SelectEncoder(outEncode, outMeasure, outMaxOutCodeLength);
			break;
		case CharacterEncoding::kUTF32:
			AutoUnicodeTranscoder<UTF32Handler>::SelectEncoder(outEncode, outMeasure, outMaxOutCodeLength);
			break;
		default:
			AutoUnicodeTranscoder<InvalidHandler>::SelectEncoder(outEncode, outMeasure, outMaxOutCodeLength);
			break;
		}
	}

	size_t ConvertUnicode(void *outputChars, CharacterEncoding outEncoding, size_t outMaxChars, size_t &outCharsEmitted,
		const void *inputChars, CharacterEncoding inEncoding, size_t inCharCount, UnknownCharBehavior unknownCharBehavior, uint32_t unknownReplacementChar)
	{
		DecodeToUTF32Func_t decode = nullptr;
		DecodeToUTF32RangeCheckedFunc_t decodeRangeChecked = nullptr;
		uint_fast8_t maxInCodeLength = 0;

		EncodeFromUTF32Func_t encode = nullptr;
		MeasureUTF32EncodeFunc_t measure = nullptr;
		uint_fast8_t maxOutCodeLength = 0;

		SelectUnicodeDecoder(inEncoding, decode, decodeRangeChecked, maxInCodeLength);
		SelectUnicodeEncoder(outEncoding, encode, measure, maxOutCodeLength);

		TranscodeUnicodeParams params =
		{
			outputChars,
			outMaxChars,
			&outCharsEmitted,
			inputChars,
			inCharCount,
			unknownCharBehavior,
			unknownReplacementChar,
			decode,
			decodeRangeChecked,
			CharSizeForEncoding(inEncoding),
			maxInCodeLength,
			encode,
			measure,
			CharSizeForEncoding(outEncoding),
			maxOutCodeLength
		};

		return TranscodeUnicode(params);
	}

} } } // rkit::text::priv

namespace rkit { namespace text {
	uint32_t RKIT_CORELIB_API GetDefaultUnknownCharForEncoding(CharacterEncoding outEncoding)
	{
		switch (outEncoding)
		{
		case CharacterEncoding::kUTF8:
		case CharacterEncoding::kUTF16:
		case CharacterEncoding::kUTF32:
			return 0xfffd;
		case CharacterEncoding::kASCII:
		case CharacterEncoding::kByte:
		default:
			return '?';
		}
	}

	size_t RKIT_CORELIB_API ConvertText(void *outputChars, CharacterEncoding outEncoding, size_t outMaxChars, size_t &outCharsEmitted,
		const void *inputChars, CharacterEncoding inEncoding, size_t inCharCount, UnknownCharBehavior unknownCharBehavior, uint32_t unknownReplacementChar)
	{
		// Simple copy
		if (IsCharacterEncodingCompatible(inEncoding, outEncoding))
		{
			const size_t charsToCopy = Min(inCharCount, outMaxChars);
			memcpy(outputChars, inputChars, CharSizeForEncoding(inEncoding) * charsToCopy);
			outCharsEmitted = charsToCopy;
			return charsToCopy;
		}

		if (outEncoding == CharacterEncoding::kByte)
			return priv::ConvertToRawEncoding<uint8_t>(static_cast<uint8_t *>(outputChars), outMaxChars, outCharsEmitted, inputChars, inEncoding, inCharCount, unknownCharBehavior, unknownReplacementChar);

		if (inEncoding == CharacterEncoding::kByte)
			return 0;

		return priv::ConvertUnicode(outputChars, outEncoding, outMaxChars, outCharsEmitted, inputChars, inEncoding, inCharCount, unknownCharBehavior, unknownReplacementChar);
	}
} }
