#pragma once

#include "MatProto.h"
#include "Vec.h"

#include <type_traits>

namespace rkit
{
	template<class T>
	class Span;

	template<typename T, T... TValues>
	struct IntList;
}

namespace rkit { namespace math { namespace priv {
	template<class TComponent, size_t TRows, size_t TCols>
	struct MatrixBase
	{
	};

	template<class TComponent>
	struct MatrixBase<TComponent, 3, 3>
	{
		static Matrix<TComponent, 3, 3> FromRotation(const Vec<TComponent, 3> &axis, TComponent angleRadians);
		static Matrix<TComponent, 3, 3> FromRotationPrecomputed(const Vec<TComponent, 3> &axis, TComponent sinAngle, TComponent cosAngle);
	};

	template<class TComponent, size_t TRows, size_t TCols>
	struct MatrixTransposer
	{
		static Matrix<TComponent, TCols, TRows> Transpose(const Vec<TComponent, TCols>(& RKIT_RESTRICT in)[TRows]);
	};

	enum class MatExplicitParamListTag
	{
	};

	template<class TComponent, size_t TRows, size_t TCols, class TIndexList>
	struct MatrixArrayInitReader
	{
	};

	template<class TComponent, size_t TRows, size_t TCols, size_t... TIndexes>
	struct MatrixArrayInitReader<TComponent, TRows, TCols, IntList<size_t, TIndexes...>>
	{
		static Matrix<TComponent, TRows, TCols> Create(const Vec<TComponent, TCols> *array);
	};

	template<class TComponent, size_t TIndexCount, class TIndexList>
	struct MatrixMultiplyComputer
	{
	};

	template<class TComponent, size_t TCommonDimensionIndex, size_t TCommonDimension, size_t TRightCols>
	struct VecMatMultiplyComputer
	{
		static Vec<TComponent, TCommonDimension> Compute(const Vec<TComponent, TCommonDimension> &left, const Vec<TComponent, TRightCols>(&rightRows)[TCommonDimension]);
	};

	template<class TComponent, size_t TCommonDimension, size_t TRightCols>
	struct VecMatMultiplyComputer<TComponent, 0, TCommonDimension, TRightCols>
	{
		static Vec<TComponent, TCommonDimension> Compute(const Vec<TComponent, TCommonDimension> &left, const Vec<TComponent, TRightCols>(&rightRows)[TCommonDimension]);
	};

	template<class TComponent, size_t TIndexCount, size_t... TIndexes>
	struct MatrixMultiplyComputer<TComponent, TIndexCount, IntList<size_t, TIndexes...>>
	{
	public:
		// In this case, index count is the output row count (left-side rows)
		template<size_t TCommonDimension, size_t TRightCols>
		static Matrix<TComponent, TIndexCount, TRightCols> ComputeMatMat(const Vec<TComponent, TCommonDimension>(&leftRows)[TIndexCount], const Vec<TComponent, TRightCols>(&rightRows)[TCommonDimension]);
	};

#if RKIT_PLATFORM_ARCH_HAVE_SSE
	template<>
	struct MatrixTransposer<float, 3, 3>
	{
		static Matrix<float, 3, 3> Transpose(const Vec<float, 3>(&RKIT_RESTRICT in)[3]);
	};

	template<>
	struct MatrixTransposer<float, 4, 3>
	{
		static Matrix<float, 3, 4> Transpose(const Vec<float, 3>(&RKIT_RESTRICT in)[4]);
	};

	template<>
	struct MatrixTransposer<float, 3, 4>
	{
		static Matrix<float, 4, 3> Transpose(const Vec<float, 4>(&RKIT_RESTRICT in)[3]);
	};

	template<>
	struct MatrixTransposer<float, 4, 4>
	{
		static Matrix<float, 4, 4> Transpose(const Vec<float, 4>(&RKIT_RESTRICT in)[4]);
	};
#endif
} } }

namespace rkit { namespace math {
	template<class TComponent, size_t TRows, size_t TCols>
	class Matrix : public priv::MatrixBase<TComponent, TRows, TCols>
	{
	public:
		Matrix() = default;
		Matrix(const Matrix &) = default;

		template<class... TParam>
		explicit Matrix(const TParam&... params);

		Matrix &operator=(const Matrix &) = default;

		bool operator==(const Matrix &other) const;
		bool operator!=(const Matrix &other) const;

		template<size_t TOtherCols>
		Matrix<TComponent, TRows, TOtherCols> operator*(const Matrix<TComponent, TCols, TOtherCols> &other) const;

		Matrix<TComponent, TRows, TCols> MemberwiseMultiply(const Matrix<TComponent, TRows, TCols> &other) const;
		Matrix<TComponent, TRows, TCols> Transpose() const;

		static Matrix<TComponent, TRows, TCols> FromRowsArray(const Vec<TComponent, TCols> (&rows)[TRows]);
		static Matrix<TComponent, TRows, TCols> FromRowsPtr(const Vec<TComponent, TCols> *rows);
		static Matrix<TComponent, TRows, TCols> FromRowsSpan(const Span<const Vec<TComponent, TCols>> &rows);

		Vec<TComponent, TCols> &operator[](size_t index);
		const Vec<TComponent, TCols> &operator[](size_t index) const;

	private:
		Vec<TComponent, TCols> m_rows[TRows];
	};
} }

#include "rkit/Core/RKitAssert.h"
#include "rkit/Core/IntList.h"
#include "rkit/Core/TypeList.h"

namespace rkit { namespace math {
	template<class TComponent, size_t TRows, size_t TCols>
	template<class... TParam>
	Matrix<TComponent, TRows, TCols>::Matrix(const TParam&... params)
		: m_rows{ params... }
	{
		static_assert(TypeListSize<TypeList<TParam...>>::kSize == TRows);
	}

	template<class TComponent, size_t TRows, size_t TCols>
	template<size_t TOtherCols>
	Matrix<TComponent, TRows, TOtherCols> Matrix<TComponent, TRows, TCols>::operator*(const Matrix<TComponent, TCols, TOtherCols> &other) const
	{
		typedef IntListCreateSequence<size_t, TRows>::Type_t RowSequence_t;
		typedef priv::MatrixMultiplyComputer<TComponent, TRows, RowSequence_t> MatrixComputer_t;

		return MatrixComputer_t::template ComputeMatMat<TCols, TOtherCols>(m_rows, other.m_rows);
	}

	template<class TComponent, size_t TRows, size_t TCols>
	Matrix<TComponent, TRows, TCols> Matrix<TComponent, TRows, TCols>::MemberwiseMultiply(const Matrix<TComponent, TRows, TCols> &other) const
	{
		Vec<TComponent, TCols> results[TRows];

		for (size_t row = 0; row < TRows; row++)
			results[row] = m_rows[row] * other.m_rows[row];

		return Matrix<TComponent, TRows, TCols>::FromRowsArray(results);
	}

	template<class TComponent, size_t TRows, size_t TCols>
	Matrix<TComponent, TRows, TCols> Matrix<TComponent, TRows, TCols>::Transpose() const
	{
		return priv::MatrixTransposer<TComponent, TRows, TCols>::Transpose(m_rows);
	}


	template<class TComponent, size_t TRows, size_t TCols>
	Matrix<TComponent, TRows, TCols> Matrix<TComponent, TRows, TCols>::FromRowsArray(const Vec<TComponent, TCols>(&rows)[TRows])
	{
		return Matrix<TComponent, TRows, TCols>::FromRowsPtr(rows);
	}

	template<class TComponent, size_t TRows, size_t TCols>
	Matrix<TComponent, TRows, TCols> Matrix<TComponent, TRows, TCols>::FromRowsSpan(const Span<const Vec<TComponent, TCols>> &rows)
	{
		RKIT_ASSERT(rows.Count() == TRows);
		return FromRowsPtr(rows.Ptr());
	}

	template<class TComponent, size_t TRows, size_t TCols>
	Matrix<TComponent, TRows, TCols> Matrix<TComponent, TRows, TCols>::FromRowsPtr(const Vec<TComponent, TCols> *rows)
	{
		return priv::MatrixArrayInitReader<TComponent, TRows, TCols, typename IntListCreateSequence<size_t, TRows>::Type_t>::Create(rows);
	}

	template<class TComponent, size_t TRows, size_t TCols>
	Vec<TComponent, TCols> &Matrix<TComponent, TRows, TCols>::operator[](size_t index)
	{
		RKIT_ASSERT(index < TRows);
		return m_rows[index];
	}

	template<class TComponent, size_t TRows, size_t TCols>
	const Vec<TComponent, TCols> &Matrix<TComponent, TRows, TCols>::operator[](size_t index) const
	{
		RKIT_ASSERT(index < TRows);
		return m_rows[index];
	}
} }


namespace rkit { namespace math { namespace priv {
	template<class TComponent, size_t TRows, size_t TCols>
	Matrix<TComponent, TCols, TRows> MatrixTransposer<TComponent, TRows, TCols>::Transpose(const Vec<TComponent, TCols>(&in)[TRows])
	{
		Vec<TComponent, TRows> transposedMat[TCols] = {};
		for (size_t col = 0; col < TCols; col++)
		{
			TComponent transposedCol[TRows] = {};
			for (size_t row = 0; row < TRows; row++)
				transposedCol[row] = in[row][col];

			transposedMat[col] = Vec<TComponent, TRows>::FromArray(transposedCol);
		}

		return Matrix<TComponent, TCols, TRows>::FromRowsArray(transposedMat);
	}

	template<class TComponent, size_t TRows, size_t TCols, size_t... TIndexes>
	Matrix<TComponent, TRows, TCols> MatrixArrayInitReader<TComponent, TRows, TCols, IntList<size_t, TIndexes...>>::Create(const Vec<TComponent, TCols> *array)
	{
		return Matrix<TComponent, TRows, TCols>(array[TIndexes]...);
	}

	template<class TComponent, size_t TCommonDimensionIndex, size_t TCommonDimension, size_t TRightCols>
	Vec<TComponent, TCommonDimension> VecMatMultiplyComputer<TComponent, TCommonDimensionIndex, TCommonDimension, TRightCols>::Compute(const Vec<TComponent, TCommonDimension> &left, const Vec<TComponent, TRightCols>(&rightRows)[TCommonDimension])
	{
		static_assert(TCommonDimensionIndex != 0, "This shouldn't be compiled for 0");
		static_assert(TCommonDimensionIndex < TCommonDimension, "Invalid index");

		return
			VecMatMultiplyComputer<TComponent, TCommonDimensionIndex - 1, TCommonDimension, TRightCols>::Compute(left, rightRows)
			+ (left.template Broadcast<TCommonDimensionIndex>()) * rightRows[TCommonDimensionIndex];
	}

	template<class TComponent, size_t TCommonDimension, size_t TRightCols>
	Vec<TComponent, TCommonDimension> VecMatMultiplyComputer<TComponent, 0, TCommonDimension, TRightCols>::Compute(const Vec<TComponent, TCommonDimension> &left, const Vec<TComponent, TRightCols>(&rightRows)[TCommonDimension])
	{
		static_assert(TCommonDimension != 0, "Invalid index");

		return (left.template Broadcast<0>()) * rightRows[0];
	}

	template<class TComponent, size_t TIndexCount, size_t... TOutRowIndex>
	template<size_t TCommonDimension, size_t TRightCols>
	Matrix<TComponent, TIndexCount, TRightCols> MatrixMultiplyComputer<TComponent, TIndexCount, IntList<size_t, TOutRowIndex...>>::ComputeMatMat(const Vec<TComponent, TCommonDimension>(&leftRows)[TIndexCount], const Vec<TComponent, TRightCols>(&rightRows)[TCommonDimension])
	{
		typedef IntListCreateSequence<size_t, TCommonDimension>::Type_t CommonIndexList_t;
		typedef MatrixMultiplyComputer<TComponent, TCommonDimension, CommonIndexList_t> CommonIndexComputer_t;

		return Matrix<TComponent, TIndexCount, TRightCols>(
			VecMatMultiplyComputer<TComponent, TCommonDimension - 1, TCommonDimension, TRightCols>::Compute(leftRows[TOutRowIndex], rightRows)...
		);
	}

	template<class TComponent>
	Matrix<TComponent, 3, 3> MatrixBase<TComponent, 3, 3>::FromRotationPrecomputed(const Vec<TComponent, 3> &axis, TComponent sinAngle, TComponent cosAngle)
	{
		const TComponent oneMinusC = static_cast<TComponent>(1) - cosAngle;

		const Vec<TComponent, 4> axisXYZ_one = Vec<TComponent, 4>(axis[0], axis[1], axis[2], static_cast<TComponent>(1));
		const Vec<TComponent, 3> axisMulOneMinusC = axis * Vec<TComponent, 3>(oneMinusC, oneMinusC, oneMinusC);

		const Vec<TComponent, 3> posS_negS_c = Vec<TComponent, 3>(sinAngle, sinAngle, cosAngle).template NegateElements<false, true, false>();

		/*
		result.m_rows[0][0] = x * x * oneMinusC + c;
		result.m_rows[0][1] = x * y * oneMinusC - z * s;
		result.m_rows[0][2] = x * z * oneMinusC + y * s;

		result.m_rows[1][0] = y * x * oneMinusC + z * s;
		result.m_rows[1][1] = y * y * oneMinusC + c;
		result.m_rows[1][2] = y * z * oneMinusC - x * s;

		result.m_rows[2][0] = z * x * oneMinusC - y * s;
		result.m_rows[2][1] = z * y * oneMinusC + x * s;
		result.m_rows[2][2] = z * z * oneMinusC + c;
		*/

		const Vec<TComponent, 3> row0 =
			axisXYZ_one.template Swizzle<0, 0, 0>() * axisMulOneMinusC
			+ (posS_negS_c.template Swizzle<2, 1, 0>() * axisXYZ_one.template Swizzle<3, 2, 1>());

		const Vec<TComponent, 3> row1 =
			axisXYZ_one.template Swizzle<1, 1, 1>() * axisMulOneMinusC
			+ (posS_negS_c.template Swizzle<0, 2, 1>() * axisXYZ_one.template Swizzle<2, 3, 0>());

		const Vec<TComponent, 3> row2 =
			axisXYZ_one.template Swizzle<2, 2, 2>() * axisMulOneMinusC
			+ (posS_negS_c.template Swizzle<1, 0, 2>() * axisXYZ_one.template Swizzle<1, 0, 3>());

		return Matrix<TComponent, 3, 3>(row0, row1, row2);
	}

#if RKIT_PLATFORM_ARCH_HAVE_SSE
	inline Matrix<float, 3, 3> MatrixTransposer<float, 3, 3>::Transpose(const Vec<float, 3>(&in)[3])
	{
		const __m128 in0 = in[0].m_v;
		const __m128 in1 = in[1].m_v;
		const __m128 in2 = in[2].m_v;

		const __m128 m_00_01_10_11 = _mm_shuffle_ps(in0, in1, ((0 << 0) | (1 << 2) | (0 << 4) | (1 << 6)));
		const __m128 m_02_03_12_13 = _mm_shuffle_ps(in0, in1, ((2 << 0) | (3 << 2) | (2 << 4) | (3 << 6)));

		const __m128 m_00_10_20_xx = _mm_shuffle_ps(m_00_01_10_11, in2, ((0 << 0) | (2 << 2) | (0 << 4)));
		const __m128 m_01_11_21_xx = _mm_shuffle_ps(m_00_01_10_11, in2, ((1 << 0) | (3 << 2) | (1 << 4)));
		const __m128 m_02_12_22_xx = _mm_shuffle_ps(m_02_03_12_13, in2, ((0 << 0) | (2 << 2) | (2 << 4)));

		return Matrix<float, 3, 3>(
			Vec<float, 3>(VecSSEFloatStorage<3>(m_00_10_20_xx)),
			Vec<float, 3>(VecSSEFloatStorage<3>(m_01_11_21_xx)),
			Vec<float, 3>(VecSSEFloatStorage<3>(m_02_12_22_xx))
		);
	}

	inline Matrix<float, 4, 3> MatrixTransposer<float, 3, 4>::Transpose(const Vec<float, 4>(&in)[3])
	{
		const __m128 in0 = in[0].m_v;
		const __m128 in1 = in[1].m_v;
		const __m128 in2 = in[2].m_v;

		const __m128 m_00_01_10_11 = _mm_shuffle_ps(in0, in1, ((0 << 0) | (1 << 2) | (0 << 4) | (1 << 6)));
		const __m128 m_02_03_12_13 = _mm_shuffle_ps(in0, in1, ((2 << 0) | (3 << 2) | (2 << 4) | (3 << 6)));

		const __m128 m_00_10_20_xx = _mm_shuffle_ps(m_00_01_10_11, in2, ((0 << 0) | (2 << 2) | (0 << 4)));
		const __m128 m_01_11_21_xx = _mm_shuffle_ps(m_00_01_10_11, in2, ((1 << 0) | (3 << 2) | (1 << 4)));
		const __m128 m_02_12_22_xx = _mm_shuffle_ps(m_02_03_12_13, in2, ((0 << 0) | (2 << 2) | (2 << 4)));
		const __m128 m_03_13_23_xx = _mm_shuffle_ps(m_02_03_12_13, in2, ((1 << 0) | (3 << 2) | (3 << 4)));

		return Matrix<float, 4, 3>(
			Vec<float, 3>(VecSSEFloatStorage<3>(m_00_10_20_xx)),
			Vec<float, 3>(VecSSEFloatStorage<3>(m_01_11_21_xx)),
			Vec<float, 3>(VecSSEFloatStorage<3>(m_02_12_22_xx)),
			Vec<float, 3>(VecSSEFloatStorage<3>(m_03_13_23_xx))
		);
	}

	inline Matrix<float, 3, 4> MatrixTransposer<float, 4, 3>::Transpose(const Vec<float, 3>(&in)[4])
	{
		const __m128 in0 = in[0].m_v;
		const __m128 in1 = in[1].m_v;
		const __m128 in2 = in[2].m_v;
		const __m128 in3 = in[3].m_v;

		const __m128 m_00_01_10_11 = _mm_shuffle_ps(in0, in1, ((0 << 0) | (1 << 2) | (0 << 4) | (1 << 6)));
		const __m128 m_20_21_30_31 = _mm_shuffle_ps(in2, in3, ((0 << 0) | (1 << 2) | (0 << 4) | (1 << 6)));


		const __m128 m_02_xx_12_xx = _mm_shuffle_ps(in0, in1, ((2 << 0) | (3 << 2) | (2 << 4) | (3 << 6)));
		const __m128 m_22_xx_32_xx = _mm_shuffle_ps(in2, in3, ((2 << 0) | (3 << 2) | (2 << 4) | (3 << 6)));

		const __m128 m_00_10_20_30 = _mm_shuffle_ps(m_00_01_10_11, m_20_21_30_31, ((0 << 0) | (2 << 2) | (0 << 4) | (2 << 6)));
		const __m128 m_01_11_21_31 = _mm_shuffle_ps(m_00_01_10_11, m_20_21_30_31, ((1 << 0) | (3 << 2) | (1 << 4) | (3 << 6)));
		const __m128 m_02_12_22_32 = _mm_shuffle_ps(m_02_xx_12_xx, m_22_xx_32_xx, ((0 << 0) | (2 << 2) | (0 << 4) | (2 << 6)));

		return Matrix<float, 3, 4>(
			Vec<float, 4>(VecSSEFloatStorage<4>(m_00_10_20_30)),
			Vec<float, 4>(VecSSEFloatStorage<4>(m_01_11_21_31)),
			Vec<float, 4>(VecSSEFloatStorage<4>(m_02_12_22_32))
		);
	}

	inline Matrix<float, 4, 4> MatrixTransposer<float, 4, 4>::Transpose(const Vec<float, 4>(&in)[4])
	{
		const __m128 in0 = in[0].m_v;
		const __m128 in1 = in[1].m_v;
		const __m128 in2 = in[2].m_v;
		const __m128 in3 = in[3].m_v;

		const __m128 m_00_01_10_11 = _mm_shuffle_ps(in0, in1, ((0 << 0) | (1 << 2) | (0 << 4) | (1 << 6)));
		const __m128 m_02_03_12_13 = _mm_shuffle_ps(in0, in1, ((2 << 0) | (3 << 2) | (2 << 4) | (3 << 6)));
		const __m128 m_20_21_30_31 = _mm_shuffle_ps(in2, in3, ((0 << 0) | (1 << 2) | (0 << 4) | (1 << 6)));
		const __m128 m_22_23_32_33 = _mm_shuffle_ps(in2, in3, ((2 << 0) | (3 << 2) | (2 << 4) | (3 << 6)));

		const __m128 m_00_10_20_30 = _mm_shuffle_ps(m_00_01_10_11, m_20_21_30_31, ((0 << 0) | (2 << 2) | (0 << 4) | (2 << 6)));
		const __m128 m_01_11_21_31 = _mm_shuffle_ps(m_00_01_10_11, m_20_21_30_31, ((1 << 0) | (3 << 2) | (1 << 4) | (3 << 6)));
		const __m128 m_02_12_22_32 = _mm_shuffle_ps(m_02_03_12_13, m_22_23_32_33, ((0 << 0) | (2 << 2) | (0 << 4) | (2 << 6)));
		const __m128 m_03_13_23_33 = _mm_shuffle_ps(m_02_03_12_13, m_22_23_32_33, ((1 << 0) | (3 << 2) | (1 << 4) | (3 << 6)));

		return Matrix<float, 4, 4>(
			Vec<float, 4>(VecSSEFloatStorage<4>(m_00_10_20_30)),
			Vec<float, 4>(VecSSEFloatStorage<4>(m_01_11_21_31)),
			Vec<float, 4>(VecSSEFloatStorage<4>(m_02_12_22_32)),
			Vec<float, 4>(VecSSEFloatStorage<4>(m_03_13_23_33))
		);
	}
#endif
} } }
