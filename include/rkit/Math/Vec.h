#pragma once

#include "rkit/Core/Platform.h"
#include "rkit/Core/IntList.h"

#include "VecProto.h"

#include <type_traits>

#if RKIT_PLATFORM_ARCH_HAVE_SSE
#include <xmmintrin.h>
#endif

namespace rkit
{
	template<size_t TSize>
	class StaticBoolArray;

	template<class T>
	class Span;
}

namespace rkit { namespace math { namespace priv {
	template<class TComponent, size_t TSize>
	struct VecStorageResolver;

	template<class TComponent, size_t TRows, size_t TCols>
	struct MatrixTransposer;

	template<class TComponent, size_t TSize>
	class VecStorage;

	template<class TComponent, size_t TSize, size_t TOtherSize>
	struct DotProductCollater
	{
		static Vec<TComponent, TSize> Collate(const Vec<TComponent, TOtherSize> *values);
	};

	template<class TComponent, size_t TSize, size_t TIndex>
	class VecStorageSwizzleReader
	{
	public:
		static const TComponent &Get(const TComponent(&array)[TSize]);
	};

	template<class TCandidate>
	struct VecUnroller
	{
		static constexpr size_t kSize = 1;
		static TCandidate Unroll(const TCandidate &candidate, size_t index);
	};

	template<class TVecComponent, size_t TSize>
	struct VecUnroller<Vec<TVecComponent, TSize>>
	{
		static constexpr size_t kSize = TSize;
		static TVecComponent Unroll(const Vec<TVecComponent, TSize> &candidate, size_t index);
	};

	template<class TCandidate>
	struct VecUnroller<const TCandidate>
		: public VecUnroller<TCandidate>
	{
	};

	template<class TCandidate>
	struct VecUnroller<TCandidate &>
		: public VecUnroller<TCandidate>
	{
	};

	template<size_t... TIndexes>
	struct IndexCounter
	{
	};

	template<size_t TIndex>
	struct IndexCounter<TIndex>
	{
		static constexpr size_t kCount = 1;
	};

	template<size_t TIndex, size_t... TMore>
	struct IndexCounter<TIndex, TMore...>
	{
		static constexpr size_t kCount = IndexCounter<TMore...>::kCount + 1u;
	};

	template<size_t... TIndexes>
	struct IndexChecker
	{
		template<size_t TSize>
		static void Check();
	};

	template<size_t TFirstIndex, size_t... TMoreIndexes>
	struct IndexChecker<TFirstIndex, TMoreIndexes...>
	{
		template<size_t TSize>
		static void Check();
	};

	enum class VecStorageExplicitParamListTag
	{
	};

	template<class TComponent, size_t TSize, class TIndexList>
	struct VecStorageLoader
	{
	};

	template<class TComponent, size_t TSize, size_t... TIndexes>
	struct VecStorageLoader<TComponent, TSize, IntList<size_t, TIndexes...>>
	{
		static VecStorage<TComponent, TSize> Load();
	};

	template<class TComponent, size_t TSize>
	class VecStorage
	{
	public:
		typedef TComponent Storage_t[TSize];

		const Storage_t &GetStorage() const;
		Storage_t &ModifyStorage();

		static VecStorage FromArray(const TComponent(&array)[TSize]);
		static VecStorage FromPtr(const TComponent *ptr);

	private:
		template<class TOtherComponent, size_t TOtherSize>
		friend class VecStorage;

		template<class TComponent, size_t TSize, class TIndexList>
		friend struct VecStorageLoader;

		template<class TPred>
		bool CompareWithPred(const VecStorage<TComponent, TSize> &other, const TPred &pred) const;

		template<class TPred>
		StaticBoolArray<TSize> MultiCompareWithPred(const VecStorage<TComponent, TSize> &other, const TPred &pred) const;

		explicit VecStorage(const Storage_t &storage);

		template<bool... TParams>
		VecStorage InternalNegateElementsImpl() const;

	protected:
		VecStorage();
		VecStorage(const VecStorage<TComponent, TSize> &other) = default;

		template<class... TParam>
		explicit VecStorage(VecStorageExplicitParamListTag, const TParam&... params);

		VecStorage &operator=(const VecStorage<TComponent, TSize> &other) = default;

		const TComponent &InternalGetAt(size_t index) const;
		TComponent &InternalModifyAt(size_t index);
		void InternalSetAt(size_t index, const TComponent &value);

		bool InternalEqual(const VecStorage<TComponent, TSize> &other) const;
		bool InternalNotEqual(const VecStorage<TComponent, TSize> &other) const;

		StaticBoolArray<TSize> InternalMultiEqual(const VecStorage<TComponent, TSize> &other) const;
		StaticBoolArray<TSize> InternalMultiNotEqual(const VecStorage<TComponent, TSize> &other) const;
		StaticBoolArray<TSize> InternalMultiGreater(const VecStorage<TComponent, TSize> &other) const;
		StaticBoolArray<TSize> InternalMultiLess(const VecStorage<TComponent, TSize> &other) const;
		StaticBoolArray<TSize> InternalMultiGreaterOrEqual(const VecStorage<TComponent, TSize> &other) const;
		StaticBoolArray<TSize> InternalMultiLessOrEqual(const VecStorage<TComponent, TSize> &other) const;

		VecStorage InternalDotProduct(const VecStorage<TComponent, TSize> &other) const;

		template<size_t TIndexCount, size_t... TIndexes>
		typename VecStorageResolver<TComponent, TIndexCount>::Type_t InternalSwizzle() const;

		template<bool... TParam>
		VecStorage InternalNegateElements() const
		{
			static_assert(IntListSize<bool, TParam...>::kValue == TSize, "Wrong number of bool parameters");
			return this->InternalNegateElementsImpl<TParam...>();
		}

		template<size_t TIndex>
		VecStorage InternalBroadcast() const;

		Storage_t m_v[TSize];
	};

	template<class TComponent, size_t TSize>
	struct VecStorageResolver
	{
		typedef VecStorage<TComponent, TSize> Type_t;
	};

	template<class TComponent, size_t TSize, class TIndexList>
	struct VecIndexListSwizzler
	{
	};

	template<class TComponent, size_t TSize, size_t... TIndexes>
	struct VecIndexListSwizzler<TComponent, TSize, IntList<size_t, TIndexes...>>
	{
		static Vec<TComponent, IntListSize<IntList<size_t, TIndexes...>>::kValue> Swizzle(const Vec<TComponent, TSize> &vec);
	};

#if RKIT_PLATFORM_ARCH_HAVE_SSE
	template<size_t TComponentCount>
	class VecSSEFloatStorage;

	template<bool TUse, size_t TSize, size_t TOtherSize>
	struct M128DotProductCollater
	{
	};

	template<size_t TSize, size_t TOtherSize>
	struct M128DotProductCollater<true, TSize, TOtherSize>
	{
		static Vec<float, TSize> Collate(const Vec<float, TOtherSize> *values);
	};

	template<size_t TSize, size_t TOtherSize>
	struct M128DotProductCollater<false, TSize, TOtherSize>
		: public DotProductCollater<float, TSize, TOtherSize>
	{
	};

	template<bool TIsSequential, size_t TOutputCount, size_t... TIndexes>
	struct M128StaticIndexSwizzler
	{
		static VecStorage<float, TOutputCount> Read(__m128 src);
	};

	template<size_t TOutputCount, size_t... TIndexes>
	struct M128StaticIndexSwizzler<true, TOutputCount, TIndexes...>
	{
		static VecSSEFloatStorage<TOutputCount> Read(__m128 src);
	};

	template<size_t T0, size_t T1>
	struct M128StaticIndexSwizzler<false, 2, T0, T1>
	{
		static VecSSEFloatStorage<2> Read(__m128 src);
	};

	template<size_t T0, size_t T1, size_t T2>
	struct M128StaticIndexSwizzler<false, 3, T0, T1, T2>
	{
		static VecSSEFloatStorage<3> Read(__m128 src);
	};

	template<size_t T0, size_t T1, size_t T2, size_t T3>
	struct M128StaticIndexSwizzler<false, 4, T0, T1, T2, T3>
	{
		static VecSSEFloatStorage<4> Read(__m128 src);
	};

	template<size_t TSize>
	struct M128DotProductCalculator
	{
	};

	template<>
	struct M128DotProductCalculator<2>
	{
		static float Compute(__m128 a, __m128 b);
	};

	template<>
	struct M128DotProductCalculator<3>
	{
		static float Compute(__m128 a, __m128 b);
	};

	template<>
	struct M128DotProductCalculator<4>
	{
		static float Compute(__m128 a, __m128 b);
	};

	template<size_t TSize>
	struct M128FloatLoader
	{
	};

	template<>
	struct M128FloatLoader<2>
	{
		static __m128 Load(const float *f);
	};

	template<>
	struct M128FloatLoader<3>
	{
		static __m128 Load(const float *f);
	};

	template<>
	struct M128FloatLoader<4>
	{
		static __m128 Load(const float *f);
	};

	template<uint32_t T0, uint32_t T1, uint32_t T2, uint32_t T3>
	struct M128FloatBitsConstant
	{
		static __m128 Create();
	};

	template<size_t TComponentCount>
	class VecSSEFloatStorage
	{
	public:
		typedef __m128 Storage_t;

		const __m128 &GetStorage() const;
		__m128 &ModifyStorage();

		static VecSSEFloatStorage<TComponentCount> FromArray(const float(&array)[TComponentCount]);
		static VecSSEFloatStorage<TComponentCount> FromPtr(const float *ptr);

	private:
		template<class TOtherComponent, size_t TOtherSize>
		friend class VecStorage;

		template<bool TIsSequential, size_t TOutputCount, size_t... TIndexes>
		friend struct M128StaticIndexSwizzler;

		template<class TOtherComponent, size_t TRows, size_t TCols>
		friend struct MatrixTransposer;

		static StaticBoolArray<TComponentCount> OpResultToBits(__m128 opResult);
		static bool MaskedOpAll(__m128 opResult);

		explicit VecSSEFloatStorage(__m128 storage);

		template<bool T0, bool T1, bool T2, bool T3>
		VecSSEFloatStorage<TComponentCount> InternalNegateElements4() const;

	protected:
		VecSSEFloatStorage();
		VecSSEFloatStorage(const VecSSEFloatStorage<TComponentCount> &other) = default;

		VecSSEFloatStorage(VecStorageExplicitParamListTag, float x, float y);
		VecSSEFloatStorage(VecStorageExplicitParamListTag, float x, float y, float z);
		VecSSEFloatStorage(VecStorageExplicitParamListTag, float x, float y, float z, float w);

		VecSSEFloatStorage<TComponentCount> &operator=(const VecSSEFloatStorage<TComponentCount> &other) = default;

		const float &InternalGetAt(size_t index) const;
		float &InternalModifyAt(size_t index);
		void InternalSetAt(size_t index, float value);

		bool InternalEqual(const VecSSEFloatStorage<TComponentCount> &other) const;
		bool InternalNotEqual(const VecSSEFloatStorage<TComponentCount> &other) const;

		VecSSEFloatStorage<TComponentCount> InternalAddScalar(float value) const;
		VecSSEFloatStorage<TComponentCount> InternalSubScalar(float value) const;
		VecSSEFloatStorage<TComponentCount> InternalMulScalar(float value) const;
		VecSSEFloatStorage<TComponentCount> InternalDivScalar(float value) const;

		VecSSEFloatStorage<TComponentCount> InternalAddVector(const VecSSEFloatStorage<TComponentCount> &other) const;
		VecSSEFloatStorage<TComponentCount> InternalSubVector(const VecSSEFloatStorage<TComponentCount> &other) const;
		VecSSEFloatStorage<TComponentCount> InternalMulVector(const VecSSEFloatStorage<TComponentCount> &other) const;
		VecSSEFloatStorage<TComponentCount> InternalDivVector(const VecSSEFloatStorage<TComponentCount> &other) const;

		StaticBoolArray<TComponentCount> InternalMultiEqual(const VecSSEFloatStorage<TComponentCount> &other) const;
		StaticBoolArray<TComponentCount> InternalMultiNotEqual(const VecSSEFloatStorage<TComponentCount> &other) const;
		StaticBoolArray<TComponentCount> InternalMultiGreater(const VecSSEFloatStorage<TComponentCount> &other) const;
		StaticBoolArray<TComponentCount> InternalMultiLess(const VecSSEFloatStorage<TComponentCount> &other) const;
		StaticBoolArray<TComponentCount> InternalMultiGreaterOrEqual(const VecSSEFloatStorage<TComponentCount> &other) const;
		StaticBoolArray<TComponentCount> InternalMultiLessOrEqual(const VecSSEFloatStorage<TComponentCount> &other) const;

		float InternalDotProduct(const VecSSEFloatStorage<TComponentCount> &other) const;

		template<size_t TIndexCount, size_t... TIndexes>
		typename VecStorageResolver<float, TIndexCount>::Type_t InternalSwizzle() const;

		template<bool T0>
		VecSSEFloatStorage<TComponentCount> InternalNegateElements() const
		{
			static_assert(TComponentCount == 1, "Wrong number of bool parameters");
			return this->InternalNegateElements4<T0, false, false, false>();
		}

		template<bool T0, bool T1>
		VecSSEFloatStorage<TComponentCount> InternalNegateElements() const
		{
			static_assert(TComponentCount == 2, "Wrong number of bool parameters");
			return this->InternalNegateElements4<T0, T1, false, false>();
		}

		template<bool T0, bool T1, bool T2>
		VecSSEFloatStorage<TComponentCount> InternalNegateElements() const
		{
			static_assert(TComponentCount == 3, "Wrong number of bool parameters");
			return this->InternalNegateElements4<T0, T1, T2, false>();
		}

		template<bool T0, bool T1, bool T2, bool T3>
		VecSSEFloatStorage<TComponentCount> InternalNegateElements() const
		{
			static_assert(TComponentCount == 4, "Wrong number of bool parameters");
			return this->InternalNegateElements4<T0, T1, T2, T3>();
		}

		template<size_t TIndex>
		VecSSEFloatStorage<TComponentCount> InternalBroadcast() const;

		__m128 m_v;
	};

	template<>
	struct VecStorageResolver<float, 4>
	{
		typedef VecSSEFloatStorage<4> Type_t;
	};

	template<>
	struct VecStorageResolver<float, 3>
	{
		typedef VecSSEFloatStorage<3> Type_t;
	};

	template<>
	struct VecStorageResolver<float, 2>
	{
		typedef VecSSEFloatStorage<2> Type_t;
	};
#endif
} } }

namespace rkit { namespace math {
	template<class TComponent, size_t TSize>
	class Vec final : public priv::VecStorageResolver<TComponent, TSize>::Type_t
	{
	public:
		template<class TOtherComponent, size_t TOtherSize>
		friend class Vec;

		template<class TOtherComponent, size_t TOtherRows, size_t TOtherCols>
		friend class Matrix;

		template<class TOtherComponent, size_t TOtherRows, size_t TOtherCols>
		friend struct priv::MatrixTransposer;

		typedef typename priv::VecStorageResolver<TComponent, TSize>::Type_t StorageType_t;

		Vec() = default;
		Vec(const Vec<TComponent, TSize> &other) = default;

		template<class... TParam>
		explicit Vec(const TParam&... params);

		template<class TOtherComponent>
		explicit Vec(const Vec<TOtherComponent, TSize> &other);

		template<size_t... TIndexes>
		Vec<TComponent, priv::IndexCounter<TIndexes...>::kCount> Swizzle() const;

		template<bool... TNegate>
		Vec<TComponent, TSize> NegateElements() const;

		template<size_t TIndex>
		Vec<TComponent, TSize> Broadcast() const;

		template<size_t TIndex, size_t TOutputSize>
		Vec<TComponent, TOutputSize> BroadcastToSize() const;

		template<size_t TIndex>
		const TComponent &GetAt() const;

		template<size_t TIndex>
		TComponent &ModifyAt();

		template<size_t TIndex>
		void SetAt(const TComponent &value);

		const TComponent &GetX() const { return this->GetAt<0>(); }
		TComponent &ModifyX() { return this->ModifyAt<0>(); }
		void SetX(const TComponent &value) { this->SetAt<0>(value); }

		const TComponent &GetY() const { return this->GetAt<1>(); }
		TComponent &ModifyY() { return this->ModifyAt<1>(); }
		void SetY(const TComponent &value) { this->SetAt<1>(value); }

		const TComponent &GetZ() const { return this->GetAt<2>(); }
		TComponent &ModifyZ() { return this->ModifyAt<2>(); }
		void SetZ(const TComponent &value) { this->SetAt<2>(value); }

		const TComponent &GetW() const { return this->GetAt<3>(); }
		TComponent &ModifyW() { return this->ModifyAt<3>(); }
		void SetW(const TComponent &value) { this->SetAt<3>(value); }

		TComponent &operator[](size_t index);
		const TComponent &operator[](size_t index) const;

		Vec<TComponent, TSize> operator+(TComponent value) const
		{
			return Vec<TComponent, TSize>(this->InternalAddScalar(value));
		}

		Vec<TComponent, TSize> operator+(const Vec<TComponent, TSize> &other) const
		{
			return Vec<TComponent, TSize>(this->InternalAddVector(other));
		}

		Vec<TComponent, TSize> operator-(TComponent value) const
		{
			return Vec<TComponent, TSize>(this->InternalSubScalar(value));
		}

		Vec<TComponent, TSize> operator-(const Vec<TComponent, TSize> &other) const
		{
			return Vec<TComponent, TSize>(this->InternalSubVector(other));
		}

		Vec<TComponent, TSize> operator*(TComponent value) const
		{
			return Vec<TComponent, TSize>(this->InternalMulScalar(value));
		}

		Vec<TComponent, TSize> operator*(const Vec<TComponent, TSize> &other) const
		{
			return Vec<TComponent, TSize>(this->InternalMulVector(other));
		}

		Vec<TComponent, TSize> operator/(TComponent value) const
		{
			return Vec<TComponent, TSize>(this->InternalDivScalar(value));
		}

		Vec<TComponent, TSize> operator/(const Vec<TComponent, TSize> &other) const
		{
			return Vec<TComponent, TSize>(this->InternalDivVector(other));
		}

		TComponent DotProduct(const Vec<TComponent, TSize> &other) const
		{
			return this->InternalDotProduct(other);
		}

		bool operator==(const Vec<TComponent, TSize> &other) const
		{
			return this->InternalEqual(other);
		}

		bool operator!=(const Vec<TComponent, TSize> &other) const
		{
			return this->InternalNotEqual(other);
		}

		StaticBoolArray<TSize> MultiEqual(const Vec<TComponent, TSize> &other) const
		{
			return this->InternalMultiEqual(other);
		}

		StaticBoolArray<TSize> MultiNotEqual(const Vec<TComponent, TSize> &other) const
		{
			return this->InternalMultiNotEqual(other);
		}

		StaticBoolArray<TSize> MultiGreater(const Vec<TComponent, TSize> &other) const
		{
			return this->InternalMultiGreater(other);
		}

		StaticBoolArray<TSize> MultiLess(const Vec<TComponent, TSize> &other) const
		{
			return this->InternalMultiLess(other);
		}

		StaticBoolArray<TSize> MultiGreaterOrEqual(const Vec<TComponent, TSize> &other) const
		{
			return this->InternalMultiGreaterOrEqual(other);
		}

		StaticBoolArray<TSize> MultiLessOrEqual(const Vec<TComponent, TSize> &other) const
		{
			return this->InternalMultiLessOrEqual(other);
		}

		TComponent GetLength() const;

		Vec<TComponent, TSize> GetNormalized() const
		{
			return (*this) / (this->GetLength());
		}

		Vec<TComponent, TSize> GetNormalizedFast() const;
		Vec<TComponent, TSize> GetNormalizedFastNonDeterministic() const;

		static Vec<TComponent, TSize> FromSpan(const Span<const TComponent> &values);
		static Vec<TComponent, TSize> FromArray(const TComponent (&values)[TSize]);
		static Vec<TComponent, TSize> FromPtr(const TComponent *values);

		template<size_t TOtherSize>
		static Vec<TComponent, TSize> FromMultiDotProductSpan(const Span<const Vec<TComponent, TOtherSize>> &values);

		template<size_t TOtherSize>
		static Vec<TComponent, TSize> FromMultiDotProductArray(const Vec<TComponent, TOtherSize>(&otherArray)[TSize]);

		template<size_t TOtherSize>
		static Vec<TComponent, TSize> FromMultiDotProductPtr(const Vec<TComponent, TOtherSize> *values);

	private:
		explicit Vec(const StorageType_t &storage);
	};
} }

#include "rkit/Core/RKitAssert.h"
#include "rkit/Core/StaticBoolArray.h"
#include "rkit/Core/Algorithm.h"
#include "rkit/Core/TypeList.h"
#include "rkit/Math/Functions.h"

#include <string.h>

namespace rkit { namespace math {
	template<class TComponent, size_t TSize>
	template<class... TParam>
	inline Vec<TComponent, TSize>::Vec(const TParam&... params)
		: StorageType_t(priv::VecStorageExplicitParamListTag(), ImplicitCast<TComponent>(params)...)
	{
	}

	template<class TComponent, size_t TSize>
	template<class TOtherComponent>
	inline Vec<TComponent, TSize>::Vec(const Vec<TOtherComponent, TSize> &other)
		: StorageType_t(StorageType_t::FromOtherStorage(ImplicitCast<typename priv::VecStorageResolver<TOtherComponent, TSize>::Type_t>(other)))
	{
	}

	template<class TComponent, size_t TSize>
	TComponent Vec<TComponent, TSize>::GetLength() const
	{
		return Sqrtf(this->DotProduct(*this));
	}

	template<class TComponent, size_t TSize>
	Vec<TComponent, TSize> Vec<TComponent, TSize>::GetNormalizedFast() const
	{
		return (*this) * Vec<TComponent, TSize>(FastRsqrtDeterministic(this->DotProduct(*this)));
	}

	template<class TComponent, size_t TSize>
	Vec<TComponent, TSize> Vec<TComponent, TSize>::GetNormalizedFastNonDeterministic() const
	{
		return (*this) * Vec<TComponent, TSize>(FastRsqrtNonDeterministic(this->DotProduct(*this)));
	}

	template<class TComponent, size_t TSize>
	Vec<TComponent, TSize> Vec<TComponent, TSize>::FromSpan(const Span<const TComponent> &values)
	{
		RKIT_ASSERT(values.Count() == TSize);
		return Vec<TComponent, TSize>::FromPtr(values.Ptr());
	}

	template<class TComponent, size_t TSize>
	Vec<TComponent, TSize> Vec<TComponent, TSize>::FromArray(const TComponent(&values)[TSize])
	{
		return Vec<TComponent, TSize>(StorageType_t::FromArray(values));
	}

	template<class TComponent, size_t TSize>
	Vec<TComponent, TSize> Vec<TComponent, TSize>::FromPtr(const TComponent *values)
	{
		return Vec<TComponent, TSize>(StorageType_t::FromPtr(values));
	}

	template<class TComponent, size_t TSize>
	template<size_t TOtherSize>
	static Vec<TComponent, TSize> Vec<TComponent, TSize>::FromMultiDotProductSpan(const Span<const Vec<TComponent, TOtherSize>> &values);

	template<class TComponent, size_t TSize>
	template<size_t TOtherSize>
	static Vec<TComponent, TSize> Vec<TComponent, TSize>::FromMultiDotProductArray(const Vec<TComponent, TOtherSize>(&otherArray)[TSize]);

	template<class TComponent, size_t TSize>
	template<size_t TOtherSize>
	static Vec<TComponent, TSize> Vec<TComponent, TSize>::FromMultiDotProductPtr(const Vec<TComponent, TOtherSize> *values);

	template<class TComponent, size_t TSize>
	inline Vec<TComponent, TSize>::Vec(const StorageType_t &base)
		: StorageType_t(base)
	{
	}

	template<class TComponent, size_t TSize>
	template<size_t... TIndexes>
	inline Vec<TComponent, priv::IndexCounter<TIndexes...>::kCount> Vec<TComponent, TSize>::Swizzle() const
	{
		typedef priv::IndexChecker<TIndexes...> Checker_t;
		Checker_t::template Check<TSize>();

		typedef std::integral_constant<size_t, priv::IndexCounter<TIndexes...>::kCount> IndexCountType_t;
		typedef Vec<TComponent, IndexCountType_t::value> ResultType_t;

		return ResultType_t(this->template InternalSwizzle<IndexCountType_t::value, TIndexes...>());
	}


	template<class TComponent, size_t TSize>
	template<bool... TNegate>
	Vec<TComponent, TSize> Vec<TComponent, TSize>::NegateElements() const
	{
		return Vec<TComponent, TSize>(this->template InternalNegateElements<TNegate...>());
	}

	template<class TComponent, size_t TSize>
	template<size_t TIndex>
	Vec<TComponent, TSize> Vec<TComponent, TSize>::Broadcast() const
	{
		static_assert(TIndex < TSize, "Invalid index");
		return Vec<TComponent, TSize>(this->template InternalBroadcast<TIndex>());
	}

	template<class TComponent, size_t TSize>
	template<size_t TIndex, size_t TOutputSize>
	Vec<TComponent, TOutputSize> Vec<TComponent, TSize>::BroadcastToSize() const
	{
		return priv::VecIndexListSwizzler<TComponent, TSize, typename IntListCreateSequence<size_t, TOutputSize, TIndex, 0>::Type_t>::Swizzle(*this);
	}

	template<class TComponent, size_t TSize>
	template<size_t TIndex>
	const TComponent &Vec<TComponent, TSize>::GetAt() const
	{
		static_assert(TIndex < TSize, "Invalid index");
		return this->InternalGetAt(TIndex);
	}

	template<class TComponent, size_t TSize>
	template<size_t TIndex>
	TComponent &Vec<TComponent, TSize>::ModifyAt()
	{
		static_assert(TIndex < TSize, "Invalid index");
		return this->InternalModifyAt(TIndex);
	}

	template<class TComponent, size_t TSize>
	template<size_t TIndex>
	void Vec<TComponent, TSize>::SetAt(const TComponent &value)
	{
		static_assert(TIndex < TSize, "Invalid index");
		this->InternalSetAt(TIndex, value);
	}

	template<class TComponent, size_t TSize>
	TComponent &Vec<TComponent, TSize>::operator[](size_t index)
	{
		RKIT_ASSERT(index < TSize);
		return this->InternalModifyAt(index);
	}

	template<class TComponent, size_t TSize>
	const TComponent &Vec<TComponent, TSize>::operator[](size_t index) const
	{
		RKIT_ASSERT(index < TSize);
		return this->InternalGetAt(index);
	}
} }

namespace rkit { namespace math { namespace priv {
	template<size_t... TIndexes>
	template<size_t TSize>
	void IndexChecker<TIndexes...>::Check()
	{
	}

	template<size_t TFirstIndex, size_t... TMoreIndexes>
	template<size_t TSize>
	void IndexChecker<TFirstIndex, TMoreIndexes...>::Check()
	{
		static_assert(TFirstIndex < TSize, "Invalid index");
		IndexChecker<TMoreIndexes...>::template Check<TSize>();
	}

	template<class TComponent, size_t TSize>
	const typename VecStorage<TComponent, TSize>::Storage_t &VecStorage<TComponent, TSize>::GetStorage() const
	{
		return m_v;
	}

	template<class TComponent, size_t TSize>
	typename VecStorage<TComponent, TSize>::Storage_t &VecStorage<TComponent, TSize>::ModifyStorage()
	{
		return m_v;
	}

	template<class TComponent, size_t TSize>
	template<class TPred>
	bool VecStorage<TComponent, TSize>::CompareWithPred(const VecStorage<TComponent, TSize> &other, const TPred &pred) const
	{
		bool result = true;

		// This loops over all elements without early-out so the compiler can assume
		// that all are accessible
		for (size_t index = 0; index < TSize; index++)
			result = pred(m_v[index], other.m_v[index]) && result;

		return result;
	}

	template<class TComponent, size_t TSize>
	template<class TPred>
	StaticBoolArray<TSize> VecStorage<TComponent, TSize>::MultiCompareWithPred(const VecStorage<TComponent, TSize> &other, const TPred &pred) const
	{
		StaticBoolArray<TSize> result;
		for (size_t index = 0; index < TSize; index++)
			result[index] = pred(m_v[index], other.m_v[index]);

		return result;
	}

	template<class TComponent, size_t TSize>
	const TComponent &VecStorage<TComponent, TSize>::InternalGetAt(size_t index) const
	{
		return this->m_v[index];
	}

	template<class TComponent, size_t TSize>
	TComponent &VecStorage<TComponent, TSize>::InternalModifyAt(size_t index)
	{
		return this->m_v[index];
	}

	template<class TComponent, size_t TSize>
	void VecStorage<TComponent, TSize>::InternalSetAt(size_t index, const TComponent &value)
	{
		this->m_v[index] = value;
	}

	template<class TComponent, size_t TSize>
	bool VecStorage<TComponent, TSize>::InternalEqual(const VecStorage<TComponent, TSize> &other) const
	{
		return this->CompareWithPred(other, rkit::DefaultCompareEqualPred());
	}

	template<class TComponent, size_t TSize>
	bool VecStorage<TComponent, TSize>::InternalNotEqual(const VecStorage<TComponent, TSize> &other) const
	{
		return this->CompareWithPred(other, rkit::DefaultCompareNotEqualPred());
	}

	template<class TComponent, size_t TSize>
	StaticBoolArray<TSize> VecStorage<TComponent, TSize>::InternalMultiEqual(const VecStorage<TComponent, TSize> &other) const
	{
		return this->MultiCompareWithPred(other, DefaultCompareEqualPred());
	}

	template<class TComponent, size_t TSize>
	StaticBoolArray<TSize> VecStorage<TComponent, TSize>::InternalMultiNotEqual(const VecStorage<TComponent, TSize> &other) const
	{
		return this->MultiCompareWithPred(other, DefaultCompareNotEqualPred());
	}

	template<class TComponent, size_t TSize>
	StaticBoolArray<TSize> VecStorage<TComponent, TSize>::InternalMultiGreater(const VecStorage<TComponent, TSize> &other) const
	{
		return this->MultiCompareWithPred(other, DefaultCompareGreaterPred());
	}

	template<class TComponent, size_t TSize>
	StaticBoolArray<TSize> VecStorage<TComponent, TSize>::InternalMultiLess(const VecStorage<TComponent, TSize> &other) const
	{
		return this->MultiCompareWithPred(other, DefaultCompareLessPred());
	}

	template<class TComponent, size_t TSize>
	StaticBoolArray<TSize> VecStorage<TComponent, TSize>::InternalMultiGreaterOrEqual(const VecStorage<TComponent, TSize> &other) const
	{
		return this->MultiCompareWithPred(other, DefaultCompareGreaterEqualPred());
	}

	template<class TComponent, size_t TSize>
	StaticBoolArray<TSize> VecStorage<TComponent, TSize>::InternalMultiLessOrEqual(const VecStorage<TComponent, TSize> &other) const
	{
		return this->MultiCompareWithPred(other, DefaultCompareLessEqualPred());
	}

#if RKIT_PLATFORM_ARCH_HAVE_SSE
	template<bool TIsSequential, size_t TOutputCount, size_t... TIndexes>
	VecStorage<float, TOutputCount> M128StaticIndexSwizzler<TIsSequential, TOutputCount, TIndexes...>::Read(__m128 src)
	{
		const float *fv = reinterpret_cast<const float *>(&src);

		const float floats[TOutputCount] =
		{
			fv[TIndexes]...
		};

		return VecStorage<float, TOutputCount>::FromArray(floats);
	}

	template<size_t T0, size_t T1>
	VecSSEFloatStorage<2> M128StaticIndexSwizzler<false, 2, T0, T1>::Read(__m128 src)
	{
		typedef std::integral_constant<unsigned int, ((T0 << 0) | (T1 << 2))> IntConst_t;
		return VecSSEFloatStorage<2>(_mm_shuffle_ps(src, _mm_setzero_ps(), IntConst_t::value));
	}

	template<size_t TOutputCount, size_t... TIndexes>
	VecSSEFloatStorage<TOutputCount> M128StaticIndexSwizzler<true, TOutputCount, TIndexes...>::Read(__m128 src)
	{
		return VecSSEFloatStorage<TOutputCount>(src);
	}

	template<size_t T0, size_t T1, size_t T2>
	VecSSEFloatStorage<3> M128StaticIndexSwizzler<false, 3, T0, T1, T2>::Read(__m128 src)
	{
		typedef std::integral_constant<unsigned int, ((T0 << 0) | (T1 << 2) | (T2 << 4))> IntConst_t;

		return VecSSEFloatStorage<3>(_mm_shuffle_ps(src, src, IntConst_t::value));
	}

	template<size_t T0, size_t T1, size_t T2, size_t T3>
	VecSSEFloatStorage<4> M128StaticIndexSwizzler<false, 4, T0, T1, T2, T3>::Read(__m128 src)
	{
		typedef std::integral_constant<unsigned int, ((T0 << 0) | (T1 << 2) | (T2 << 4) | (T3 << 6))> IntConst_t;
		return VecSSEFloatStorage<4>(_mm_shuffle_ps(src, src, IntConst_t::value));
	}

	inline float M128DotProductCalculator<2>::Compute(__m128 a, __m128 b)
	{
		__m128 v = _mm_mul_ps(a, b);

		__m128 element1 = _mm_shuffle_ps(v, v, 1);

		return _mm_cvtss_f32(_mm_add_ss(v, element1));
	}

	inline float M128DotProductCalculator<3>::Compute(__m128 a, __m128 b)
	{
		__m128 v = _mm_mul_ps(a, b);

		__m128 element1 = _mm_shuffle_ps(v, v, 1);
		__m128 element2 = _mm_shuffle_ps(v, v, 2);

		return _mm_cvtss_f32(_mm_add_ss(_mm_add_ss(v, element1), element2));
	}

	inline float M128DotProductCalculator<4>::Compute(__m128 a, __m128 b)
	{
		const __m128 v = _mm_mul_ps(a, b);

		const __m128 element23 = _mm_shuffle_ps(v, v, (2 | (3 << 2)));

		const __m128 step1 = _mm_add_ps(v, element23);

		const __m128 step1_element1 = _mm_shuffle_ps(step1, step1, 1);

		return _mm_cvtss_f32(_mm_add_ss(step1, step1_element1));
	}

	inline __m128 M128FloatLoader<2>::Load(const float *f)
	{
		return _mm_set_ps(0.f, 0.f, f[1], f[0]);
	}

	inline __m128 M128FloatLoader<3>::Load(const float *f)
	{
		return _mm_set_ps(0.f, f[2], f[1], f[0]);
	}

	inline __m128 M128FloatLoader<4>::Load(const float *f)
	{
		return _mm_loadu_ps(f);
	}

	template<uint32_t T0, uint32_t T1, uint32_t T2, uint32_t T3>
	__m128 M128FloatBitsConstant<T0, T1, T2, T3>::Create()
	{
		uint32_t dwords[4] = { T0, T1, T2, T3 };
		__m128 result;
		memcpy(&result, dwords, 16);
		return result;
	}

	template<size_t TSize>
	const __m128 &VecSSEFloatStorage<TSize>::GetStorage() const
	{
		return this->m_v;
	}

	template<size_t TSize>
	__m128 &VecSSEFloatStorage<TSize>::ModifyStorage()
	{
		return this->m_v;
	}

	template<size_t TSize>
	VecSSEFloatStorage<TSize> VecSSEFloatStorage<TSize>::FromArray(const float(&array)[TSize])
	{
		return VecSSEFloatStorage<TSize>(M128FloatLoader<TSize>::Load(array));
	}

	template<size_t TSize>
	VecSSEFloatStorage<TSize> VecSSEFloatStorage<TSize>::FromPtr(const float *array)
	{
		return VecSSEFloatStorage<TSize>(M128FloatLoader<TSize>::Load(array));
	}

	template<size_t TSize>
	StaticBoolArray<TSize> VecSSEFloatStorage<TSize>::OpResultToBits(__m128 opResult)
	{
		constexpr int bitMask = (1 << TSize) - 1;
		const uint8_t bits = static_cast<uint8_t>(_mm_movemask_ps(opResult) & bitMask);

		return StaticBoolArray<TSize>::FromBitsUnchecked(&bits);
	}

	template<size_t TSize>
	bool VecSSEFloatStorage<TSize>::MaskedOpAll(__m128 opResult)
	{
		constexpr int bitMask = (1 << TSize) - 1;

		return _mm_movemask_ps(opResult) >= bitMask;
	}

	template<size_t TSize>
	VecSSEFloatStorage<TSize>::VecSSEFloatStorage()
		: m_v(_mm_setzero_ps())
	{
	}

	template<size_t TSize>
	VecSSEFloatStorage<TSize>::VecSSEFloatStorage(__m128 storage)
		: m_v(storage)
	{
	}

	template<size_t TSize>
	VecSSEFloatStorage<TSize>::VecSSEFloatStorage(VecStorageExplicitParamListTag, float x, float y)
		: m_v(_mm_set_ps(0.f, 0.f, y, x))
	{
		static_assert(TSize == 2, "Wrong constructor for this size");
	}

	template<size_t TSize>
	VecSSEFloatStorage<TSize>::VecSSEFloatStorage(VecStorageExplicitParamListTag, float x, float y, float z)
		: m_v(_mm_set_ps(0.f, z, y, x))
	{
		static_assert(TSize == 3, "Wrong constructor for this size");
	}

	template<size_t TSize>
	VecSSEFloatStorage<TSize>::VecSSEFloatStorage(VecStorageExplicitParamListTag, float x, float y, float z, float w)
		: m_v(_mm_set_ps(w, z, y, x))
	{
		static_assert(TSize == 4, "Wrong constructor for this size");
	}

	template<size_t TSize>
	const float &VecSSEFloatStorage<TSize>::InternalGetAt(size_t index) const
	{
		return reinterpret_cast<const float *>(&m_v)[index];
	}

	template<size_t TSize>
	float &VecSSEFloatStorage<TSize>::InternalModifyAt(size_t index)
	{
		return reinterpret_cast<float *>(&m_v)[index];
	}

	template<size_t TSize>
	void VecSSEFloatStorage<TSize>::InternalSetAt(size_t index, float value)
	{
		reinterpret_cast<float *>(&m_v)[index] = value;
	}

	template<size_t TSize>
	bool VecSSEFloatStorage<TSize>::InternalEqual(const VecSSEFloatStorage<TSize> &other) const
	{
		return this->MaskedOpAll(_mm_cmpeq_ps(m_v, other.m_v));
	}

	template<size_t TSize>
	bool VecSSEFloatStorage<TSize>::InternalNotEqual(const VecSSEFloatStorage<TSize> &other) const
	{
		return this->MaskedOpAll(_mm_cmpneq_ps(m_v, other.m_v));
	}

	template<size_t TSize>
	VecSSEFloatStorage<TSize> VecSSEFloatStorage<TSize>::InternalAddScalar(float value) const
	{
		return VecSSEFloatStorage<TSize>(_mm_add_ps(m_v, _mm_set_ps1(value)));
	}

	template<size_t TSize>
	VecSSEFloatStorage<TSize> VecSSEFloatStorage<TSize>::InternalSubScalar(float value) const
	{
		return VecSSEFloatStorage<TSize>(_mm_sub_ps(m_v, _mm_set_ps1(value)));
	}

	template<size_t TSize>
	VecSSEFloatStorage<TSize> VecSSEFloatStorage<TSize>::InternalMulScalar(float value) const
	{
		return VecSSEFloatStorage<TSize>(_mm_mul_ps(m_v, _mm_set_ps1(value)));
	}

	template<size_t TSize>
	VecSSEFloatStorage<TSize> VecSSEFloatStorage<TSize>::InternalDivScalar(float value) const
	{
		return VecSSEFloatStorage<TSize>(_mm_div_ps(m_v, _mm_set_ps1(value)));
	}


	template<size_t TSize>
	VecSSEFloatStorage<TSize> VecSSEFloatStorage<TSize>::InternalAddVector(const VecSSEFloatStorage<TSize> &other) const
	{
		return VecSSEFloatStorage<TSize>(_mm_add_ps(m_v, other.m_v));
	}

	template<size_t TSize>
	VecSSEFloatStorage<TSize> VecSSEFloatStorage<TSize>::InternalSubVector(const VecSSEFloatStorage<TSize> &other) const
	{
		return VecSSEFloatStorage<TSize>(_mm_sub_ps(m_v, other.m_v));
	}

	template<size_t TSize>
	VecSSEFloatStorage<TSize> VecSSEFloatStorage<TSize>::InternalMulVector(const VecSSEFloatStorage<TSize> &other) const
	{
		return VecSSEFloatStorage<TSize>(_mm_mul_ps(m_v, other.m_v));
	}

	template<size_t TSize>
	VecSSEFloatStorage<TSize> VecSSEFloatStorage<TSize>::InternalDivVector(const VecSSEFloatStorage<TSize> &other) const
	{
		return VecSSEFloatStorage<TSize>(_mm_div_ps(m_v, other.m_v));
	}

	template<size_t TSize>
	StaticBoolArray<TSize> VecSSEFloatStorage<TSize>::InternalMultiEqual(const VecSSEFloatStorage<TSize> &other) const
	{
		return this->OpResultToBits(_mm_cmpeq_ps(m_v, other.m_v));
	}

	template<size_t TSize>
	StaticBoolArray<TSize> VecSSEFloatStorage<TSize>::InternalMultiNotEqual(const VecSSEFloatStorage<TSize> &other) const
	{
		return this->OpResultToBits(_mm_cmpneq_ps(m_v, other.m_v));
	}

	template<size_t TSize>
	StaticBoolArray<TSize> VecSSEFloatStorage<TSize>::InternalMultiGreater(const VecSSEFloatStorage<TSize> &other) const
	{
		return this->OpResultToBits(_mm_cmpgt_ps(m_v, other.m_v));
	}

	template<size_t TSize>
	StaticBoolArray<TSize> VecSSEFloatStorage<TSize>::InternalMultiLess(const VecSSEFloatStorage<TSize> &other) const
	{
		return this->OpResultToBits(_mm_cmplt_ps(m_v, other.m_v));
	}

	template<size_t TSize>
	StaticBoolArray<TSize> VecSSEFloatStorage<TSize>::InternalMultiGreaterOrEqual(const VecSSEFloatStorage<TSize> &other) const
	{
		return this->OpResultToBits(_mm_cmpge_ps(m_v, other.m_v));
	}

	template<size_t TSize>
	StaticBoolArray<TSize> VecSSEFloatStorage<TSize>::InternalMultiLessOrEqual(const VecSSEFloatStorage<TSize> &other) const
	{
		return this->OpResultToBits(_mm_cmple_ps(m_v, other.m_v));
	}

	template<size_t TSize>
	float VecSSEFloatStorage<TSize>::InternalDotProduct(const VecSSEFloatStorage<TSize> &other) const
	{
		return M128DotProductCalculator<TSize>::Compute(m_v, other.m_v);
	}

	template<size_t TSize>
	template<size_t TIndexCount, size_t... TIndexes>
	typename VecStorageResolver<float, TIndexCount>::Type_t VecSSEFloatStorage<TSize>::InternalSwizzle() const
	{
		typedef IntList<size_t, TIndexes...> IndexList_t;
		constexpr bool isSequential = IntListIsSequential<IndexList_t>::kValue;
		constexpr bool isFirstZero = (IntListElement<0, IndexList_t>::kValue == 0);
		constexpr bool isSmallEnough = (TIndexCount <= 4);
		return M128StaticIndexSwizzler<(isSequential && isFirstZero && isSmallEnough), TIndexCount, TIndexes...>::Read(m_v);
	}

	template<size_t TSize>
	template<size_t TIndex>
	VecSSEFloatStorage<TSize> VecSSEFloatStorage<TSize>::InternalBroadcast() const
	{
		return VecSSEFloatStorage<TSize>(_mm_shuffle_ps(m_v, m_v, TIndex * 0x55));
	}


	template<size_t TSize>
	template<bool T0, bool T1, bool T2, bool T3>
	VecSSEFloatStorage<TSize> VecSSEFloatStorage<TSize>::InternalNegateElements4() const
	{
		const __m128 xorMask = M128FloatBitsConstant<(T0 ? 0x80000000u : 0u), (T1 ? 0x80000000u : 0u), (T2 ? 0x80000000u : 0u), (T3 ? 0x80000000u : 0u)>::Create();
		return VecSSEFloatStorage<TSize>(_mm_xor_ps(xorMask, m_v));
	}
#endif
} } }
