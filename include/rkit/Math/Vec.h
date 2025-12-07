#pragma once

#include "rkit/Core/Platform.h"

#include "VecProto.h"

#include <type_traits>

namespace rkit
{
	template<size_t TSize>
	class StaticBoolArray;
}

namespace rkit { namespace math { namespace priv {
	template<class TComponent, size_t TSize, size_t TIndex>
	class VecStorageSwizzleReader
	{
	public:
		static const TComponent &Get(const TComponent(&array)[TSize]);
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

	enum class VecStorageExplicitParamListTag
	{
	};

	template<class TComponent, size_t TSize>
	class VecStorage
	{
	public:
		typedef TComponent Storage_t[TSize];

		const Storage_t &GetStorage() const;
		Storage_t &ModifyStorage();

		VecStorage FromArray(const TComponent(&array)[TSize]);

	private:
		template<class TOtherComponent, size_t TOtherSize>
		friend class VecStorage;

		template<class TPred>
		bool CompareWithPred(const VecStorage<TComponent, TSize> &other, const TPred &pred) const;

		template<class TPred>
		StaticBoolArray<TSize> MultiCompareWithPred(const VecStorage<TComponent, TSize> &other, const TPred &pred) const;

		explicit VecStorage(const Storage_t &storage);

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

		template<size_t TIndexCount, size_t... TIndexes>
		VecStorage<TComponent, TIndexCount> InternalSwizzle() const;

		Storage_t m_v[TSize];
	};

#if RKIT_PLATFORM_ARCH_HAVE_SSE
	template<size_t TOutputCount, size_t... TIndexes>
	struct M128StaticIndexSwizzler
	{
		static VecStorage<float, TOutputCount> Read(__m128 src);
	};

	template<size_t T0, size_t T1, size_t T2, size_t T3>
	struct M128StaticIndexSwizzler<4, T0, T1, T2, T3>
	{
		static VecStorage<float, 4> Read(__m128 src);
	};

	template<>
	class VecStorage<float, 4>
	{
	public:
		typedef __m128 Storage_t;

		const __m128 &GetStorage() const;
		__m128 &ModifyStorage();

		VecStorage FromArray(const float(&array)[4]);

	private:
		template<class TOtherComponent, size_t TOtherSize>
		friend class VecStorage;

		template<size_t TOutputCount, size_t... TIndexes>
		friend struct M128StaticIndexSwizzler;

		static StaticBoolArray<4> OpResultToBits(__m128 opResult);
		static bool MaskedOpAll(__m128 opResult);

		explicit VecStorage(__m128 storage);

	protected:
		VecStorage();
		VecStorage(const VecStorage<float, 4> &other) = default;
		VecStorage(VecStorageExplicitParamListTag, float x, float y, float z, float w);

		VecStorage &operator=(const VecStorage<float, 4> &other) = default;

		const float &InternalGetAt(size_t index) const;
		float &InternalModifyAt(size_t index);
		void InternalSetAt(size_t index, float value);

		bool InternalEqual(const VecStorage<float, 4> &other) const;
		bool InternalNotEqual(const VecStorage<float, 4> &other) const;

		StaticBoolArray<4> InternalMultiEqual(const VecStorage<float, 4> &other) const;
		StaticBoolArray<4> InternalMultiNotEqual(const VecStorage<float, 4> &other) const;
		StaticBoolArray<4> InternalMultiGreater(const VecStorage<float, 4> &other) const;
		StaticBoolArray<4> InternalMultiLess(const VecStorage<float, 4> &other) const;
		StaticBoolArray<4> InternalMultiGreaterOrEqual(const VecStorage<float, 4> &other) const;
		StaticBoolArray<4> InternalMultiLessOrEqual(const VecStorage<float, 4> &other) const;

		template<size_t TIndexCount, size_t... TIndexes>
		VecStorage<float, TIndexCount> InternalSwizzle() const;

		__m128 m_v;
	};
#endif
} } }

namespace rkit { namespace math {
	template<class TComponent, size_t TSize>
	class VecBase : public priv::VecStorage<TComponent, TSize>
	{
	public:
		VecBase() = default;
		VecBase(const VecBase &other) = default;

		VecBase &operator=(const VecBase &other) = default;

		TComponent &operator[](size_t index);
		const TComponent &operator[](size_t index) const;

		template<size_t TIndex>
		const TComponent &GetAt() const
		{
			static_assert(TIndex < TSize, "Index out of range");
			return this->InternalGetAt(TIndex);
		}

		template<size_t TIndex>
		TComponent &ModifyAt()
		{
			static_assert(TIndex < TSize, "Index out of range");
			return this->InternalModifyAt(TIndex);
		}

		// X
		template<typename = typename std::enable_if<(TSize >= 1)>::type>
		const TComponent& GetX() const { return this->InternalGetAt(0); }

		template<typename = typename std::enable_if<(TSize >= 1)>::type>
		TComponent& ModifyX() { return this->InternalModifyAt(0); }

		template<typename = typename std::enable_if<(TSize >= 1)>::type>
		void SetX(const TComponent &value) { return this->InternalSetAt(0, value); }

		// Y
		template<typename = typename std::enable_if<(TSize >= 2)>::type>
		const TComponent &GetY() const { return this->InternalGetAt(1); }

		template<typename = typename std::enable_if<(TSize >= 2)>::type>
		TComponent &ModifyY() { return this->InternalModifyAt(1); }

		template<typename = typename std::enable_if<(TSize >= 2)>::type>
		void SetY(const TComponent &value) { return this->InternalSetAt(1, value); }

		// Z
		template<typename = typename std::enable_if<(TSize >= 3)>::type>
		const TComponent &GetZ() const { return this->InternalGetAt(2); }

		template<typename = typename std::enable_if<(TSize >= 3)>::type>
		TComponent &ModifyZ() { return this->InternalModifyAt(2); }

		template<typename = typename std::enable_if<(TSize >= 3)>::type>
		void SetZ(const TComponent &value) { return this->InternalSetAt(2, value); }

		// W
		template<typename = typename std::enable_if<(TSize >= 4)>::type>
		const TComponent &GetW() const { return this->InternalGetAt(3); }

		template<typename = typename std::enable_if<(TSize >= 4)>::type>
		TComponent &ModifyW() { return this->InternalModifyAt(3); }

		template<typename = typename std::enable_if<(TSize >= 4)>::type>
		void SetW(const TComponent &value) { return this->InternalSetAt(3, value); }

		bool operator==(const VecBase<TComponent, TSize> &other) const;
		bool operator!=(const VecBase<TComponent, TSize> &other) const;

		StaticBoolArray<TSize> MultiEqual(const VecBase<TComponent, TSize> &other) const;
		StaticBoolArray<TSize> MultiNotEqual(const VecBase<TComponent, TSize> &other) const;
		StaticBoolArray<TSize> MultiGreater(const VecBase<TComponent, TSize> &other) const;
		StaticBoolArray<TSize> MultiLess(const VecBase<TComponent, TSize> &other) const;
		StaticBoolArray<TSize> MultiGreaterOrEqual(const VecBase<TComponent, TSize> &other) const;
		StaticBoolArray<TSize> MultiLessOrEqual(const VecBase<TComponent, TSize> &other) const;

	protected:
		template<class... TParam>
		explicit VecBase(priv::VecStorageExplicitParamListTag, const TParam&... params);

		template<size_t TIndexCount, size_t... TIndexes>
		VecBase<TComponent, TIndexCount> BaseSwizzle() const;

	private:
		explicit VecBase(const priv::VecStorage<TComponent, TSize> &other);

		template<size_t TIndex>
		static bool StaticValidateIndex2()
		{
			static_assert(TIndex < TSize, "Index out of range");
			return true;
		}

		template<class... TParam>
		static void StaticValidateIndex1(TParam...)
		{
		}
	};

	template<class TComponent, size_t TSize>
	class Vec final : public VecBase<TComponent, TSize>
	{
	public:
		Vec() = default;
		Vec(const Vec<TComponent, TSize> &other) = default;

		template<class... TParam>
		explicit Vec(const TParam&... params);

		template<class TOtherComponent>
		explicit Vec(const Vec<TOtherComponent, TSize> &other);

		template<size_t... TIndexes>
		Vec<TComponent, priv::IndexCounter<TIndexes...>::kCount> Swizzle() const;

	private:
		explicit Vec(const VecBase<TComponent, TSize> &base);
	};
} }

#include "rkit/Core/RKitAssert.h"
#include "rkit/Core/StaticBoolArray.h"
#include "rkit/Core/Algorithm.h"
#include "rkit/Core/TypeList.h"

namespace rkit { namespace math {
	template<class TComponent, size_t TSize>
	TComponent &VecBase<TComponent, TSize>::operator[](size_t index)
	{
		RKIT_ASSERT(index < TSize);
		return this->InternalModifyAt(index);
	}

	template<class TComponent, size_t TSize>
	const TComponent &VecBase<TComponent, TSize>::operator[](size_t index) const
	{
		RKIT_ASSERT(index < TSize);
		return this->InternalGetAt(index);
	}


	template<class TComponent, size_t TSize>
	bool VecBase<TComponent, TSize>::operator==(const VecBase<TComponent, TSize> &other) const
	{
		return this->InternalEqual(other);
	}

	template<class TComponent, size_t TSize>
	bool VecBase<TComponent, TSize>::operator!=(const VecBase<TComponent, TSize> &other) const
	{
		return this->InternalNotEqual(other);
	}

	template<class TComponent, size_t TSize>
	StaticBoolArray<TSize> VecBase<TComponent, TSize>::MultiEqual(const VecBase<TComponent, TSize> &other) const
	{
		return this->InternalMultiEqual(other);
	}

	template<class TComponent, size_t TSize>
	StaticBoolArray<TSize> VecBase<TComponent, TSize>::MultiNotEqual(const VecBase<TComponent, TSize> &other) const
	{
		return this->InternalMultiNotEqual(other);
	}

	template<class TComponent, size_t TSize>
	StaticBoolArray<TSize> VecBase<TComponent, TSize>::MultiGreater(const VecBase<TComponent, TSize> &other) const
	{
		return this->InternalMultiGreater(other);
	}

	template<class TComponent, size_t TSize>
	StaticBoolArray<TSize> VecBase<TComponent, TSize>::MultiLess(const VecBase<TComponent, TSize> &other) const
	{
		return this->InternalMultiLess(other);
	}

	template<class TComponent, size_t TSize>
	StaticBoolArray<TSize> VecBase<TComponent, TSize>::MultiGreaterOrEqual(const VecBase<TComponent, TSize> &other) const
	{
		return this->InternalMultiGreaterOrEqual(other);
	}

	template<class TComponent, size_t TSize>
	StaticBoolArray<TSize> VecBase<TComponent, TSize>::MultiLessOrEqual(const VecBase<TComponent, TSize> &other) const
	{
		return this->InternalMultiLessOrEqual(other);
	}

	template<class TComponent, size_t TSize>
	template<class... TParam>
	VecBase<TComponent, TSize>::VecBase(priv::VecStorageExplicitParamListTag, const TParam&... params)
		: priv::VecStorage<TComponent, TSize>(priv::VecStorageExplicitParamListTag(), params...)
	{
	}

	template<class TComponent, size_t TSize>
	VecBase<TComponent, TSize>::VecBase(const priv::VecStorage<TComponent, TSize> &other)
		: priv::VecStorage<TComponent, TSize>(other)
	{
	}

	template<class TComponent, size_t TSize>
	template<size_t TIndexCount, size_t... TIndexes>
	VecBase<TComponent, TIndexCount> VecBase<TComponent, TSize>::BaseSwizzle() const
	{
		StaticValidateIndex1(StaticValidateIndex2<TIndexes>()...);

		return VecBase<TComponent, TIndexCount>(this->InternalSwizzle<TIndexCount, TIndexes...>());
	}

	template<class TComponent, size_t TSize>
	template<class... TParam>
	inline Vec<TComponent, TSize>::Vec(const TParam&... params)
		: VecBase<TComponent, TSize>(priv::VecStorageExplicitParamListTag(), ImplicitCast<TComponent>(params)...)
	{
	}

	template<class TComponent, size_t TSize>
	template<class TOtherComponent>
	inline Vec<TComponent, TSize>::Vec(const Vec<TOtherComponent, TSize> &other)
		: VecBase(other)
	{
	}

	template<class TComponent, size_t TSize>
	inline Vec<TComponent, TSize>::Vec(const VecBase<TComponent, TSize> &base)
		: VecBase<TComponent, TSize>(base)
	{
	}

	template<class TComponent, size_t TSize>
	template<size_t... TIndexes>
	inline Vec<TComponent, priv::IndexCounter<TIndexes...>::kCount> Vec<TComponent, TSize>::Swizzle() const
	{
		typedef std::integral_constant<size_t, priv::IndexCounter<TIndexes...>::kCount> IndexCountType_t;
		return Vec<TComponent, IndexCountType_t::value>(this->BaseSwizzle<IndexCountType_t::value, TIndexes...>());
	}
} }

namespace rkit { namespace math { namespace priv {

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
	template<size_t TOutputCount, size_t... TIndexes>
	inline VecStorage<float, TOutputCount> M128StaticIndexSwizzler<TOutputCount, TIndexes...>::Read(__m128 src)
	{
		const float values[] =
		{
			reinterpret_cast<const float *>(&src)[TIndexes]...
		};
		return VecStorage<float, TOutputCount>::FromArray(values);
	}

	template<size_t T0, size_t T1, size_t T2, size_t T3>
	inline VecStorage<float, 4> M128StaticIndexSwizzler<4, T0, T1, T2, T3>::Read(__m128 src)
	{
		typedef std::integral_constant<unsigned int, static_cast<unsigned int>(T0 | (T1 << 2) | (T2 << 4) | (T3 << 6))> Imm_t;
		return VecStorage<float, 4>(_mm_shuffle_ps(src, src, Imm_t::value));
	};


	inline const __m128 &VecStorage<float, 4>::GetStorage() const
	{
		return m_v;
	}

	inline __m128 &VecStorage<float, 4>::ModifyStorage()
	{
		return m_v;
	}

	inline StaticBoolArray<4> VecStorage<float, 4>::OpResultToBits(__m128 opResult)
	{
		const uint8_t bits = static_cast<uint8_t>(_mm_movemask_ps(opResult));
		return StaticBoolArray<4>::FromBitsUnchecked(&bits);
	}

	inline bool VecStorage<float, 4>::MaskedOpAll(__m128 opResult)
	{
		return _mm_movemask_ps(opResult) == 0xf;
	}

	inline VecStorage<float, 4>::VecStorage(__m128 storage)
		: m_v(storage)
	{
	}

	inline VecStorage<float, 4>::VecStorage()
		: m_v(_mm_setzero_ps())
	{
	}

	inline VecStorage<float, 4>::VecStorage(VecStorageExplicitParamListTag, float x, float y, float z, float w)
		: m_v(_mm_set_ps(w, z, y, x))
	{
	}

	inline const float &VecStorage<float, 4>::InternalGetAt(size_t index) const
	{
		return reinterpret_cast<const float *>(&m_v)[index];
	}

	inline float &VecStorage<float, 4>::InternalModifyAt(size_t index)
	{
		return reinterpret_cast<float *>(&m_v)[index];
	}

	inline void VecStorage<float, 4>::InternalSetAt(size_t index, float value)
	{
		reinterpret_cast<float *>(&m_v)[index] = value;
	}

	inline bool VecStorage<float, 4>::InternalEqual(const VecStorage<float, 4> &other) const
	{
		return MaskedOpAll(_mm_cmpeq_ps(m_v, other.m_v));
	}

	inline bool VecStorage<float, 4>::InternalNotEqual(const VecStorage<float, 4> &other) const
	{
		return MaskedOpAll(_mm_cmpneq_ps(m_v, other.m_v));
	}

	inline StaticBoolArray<4> VecStorage<float, 4>::InternalMultiEqual(const VecStorage<float, 4> &other) const
	{
		return OpResultToBits(_mm_cmpeq_ps(m_v, other.m_v));
	}

	inline StaticBoolArray<4> VecStorage<float, 4>::InternalMultiNotEqual(const VecStorage<float, 4> &other) const
	{
		return OpResultToBits(_mm_cmpneq_ps(m_v, other.m_v));
	}

	inline StaticBoolArray<4> VecStorage<float, 4>::InternalMultiGreater(const VecStorage<float, 4> &other) const
	{
		return OpResultToBits(_mm_cmpgt_ps(m_v, other.m_v));
	}

	inline StaticBoolArray<4> VecStorage<float, 4>::InternalMultiLess(const VecStorage<float, 4> &other) const
	{
		return OpResultToBits(_mm_cmplt_ps(m_v, other.m_v));
	}

	inline StaticBoolArray<4> VecStorage<float, 4>::InternalMultiGreaterOrEqual(const VecStorage<float, 4> &other) const
	{
		return OpResultToBits(_mm_cmpge_ps(m_v, other.m_v));
	}

	inline StaticBoolArray<4> VecStorage<float, 4>::InternalMultiLessOrEqual(const VecStorage<float, 4> &other) const
	{
		return OpResultToBits(_mm_cmple_ps(m_v, other.m_v));
	}

	template<size_t TIndexCount, size_t... TIndexes>
	inline VecStorage<float, TIndexCount> VecStorage<float, 4>::InternalSwizzle() const
	{
		typedef M128StaticIndexSwizzler<TIndexCount, TIndexes...> Swizzler_t;
		return Swizzler_t::Read(m_v);
	}
#endif
} } }
