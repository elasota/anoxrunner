#pragma once

#include "Mat.h"
#include "Vec.h"

#include "TRMat34Proto.h"

namespace rkit { namespace math {

	// This represents a rotation and translation, as T*R
	template<class TComponent>
	class TranslateRotateMatrix34
	{
	public:
		TranslateRotateMatrix34() = default;
		TranslateRotateMatrix34(const TranslateRotateMatrix34 &other) = default;
		explicit TranslateRotateMatrix34(const Vec<TComponent, 3> &translate, const Matrix<TComponent, 3, 3> &rotate);

		Matrix<TComponent, 3, 3> GetRotation() const;
		Vec<TComponent, 3> GetTranslation() const;

		Vec<TComponent, 3> Transform(const Vec<TComponent, 3> &pos) const;
		Vec<TComponent, 3> InverseTransform(const Vec<TComponent, 3> &pos) const;

		TranslateRotateMatrix34<TComponent> operator*(const TranslateRotateMatrix34<TComponent> &other) const;
		TranslateRotateMatrix34<TComponent> Rotate(const Matrix<TComponent, 3, 3> &other) const;
		TranslateRotateMatrix34<TComponent> Translate(const Vec<TComponent, 3> &other) const;

		TranslateRotateMatrix34<TComponent> Inverse() const;

		Vec<TComponent, 4> &operator[](size_t row);
		const Vec<TComponent, 4> &operator[](size_t row) const;

	private:
		Matrix<TComponent, 3, 4> m_mat;
	};
} }

namespace rkit { namespace math {
	template<class TComponent>
	TranslateRotateMatrix34<TComponent>::TranslateRotateMatrix34(const Vec<TComponent, 3> &translate, const Matrix<TComponent, 3, 3> &rotate)
		: m_mat(
			Vec<TComponent, 4>(rotate[0][0], rotate[0][1], rotate[0][2], translate[0]),
			Vec<TComponent, 4>(rotate[1][0], rotate[1][1], rotate[1][2], translate[1]),
			Vec<TComponent, 4>(rotate[2][0], rotate[2][1], rotate[2][2], translate[2])
		)
	{
	}

	template<class TComponent>
	Matrix<TComponent, 3, 3> TranslateRotateMatrix34<TComponent>::GetRotation() const
	{
		return Matrix<TComponent, 3, 3>(
			m_mat[0].template Swizzle<0, 1, 2>(),
			m_mat[1].template Swizzle<0, 1, 2>(),
			m_mat[2].template Swizzle<0, 1, 2>()
		);
	}

	template<class TComponent>
	Vec<TComponent, 3> TranslateRotateMatrix34<TComponent>::GetTranslation() const
	{
		return Vec<TComponent, 3>(m_mat[0][0], m_mat[0][1], m_mat[0][2]);
	}

	template<class TComponent>
	Vec<TComponent, 4> &TranslateRotateMatrix34<TComponent>::operator[](size_t row)
	{
		return m_mat[row];
	}

	template<class TComponent>
	const Vec<TComponent, 4> &TranslateRotateMatrix34<TComponent>::operator[](size_t row) const
	{
		return m_mat[row];
	}
} }
