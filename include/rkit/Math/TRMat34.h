#pragma once

#include "Mat.h"
#include "Vec.h"

namespace rkit { namespace math {

	// This represents a rotation and translation, as T*R
	template<class TComponent>
	class TranslateRotateMatrix34
	{
	public:
		TranslateRotateMatrix34() = default;
		TranslateRotateMatrix34(const TranslateRotateMatrix34 &other) = default;
		explicit TranslateRotateMatrix34(const Vec<TComponent, 3> &translate, const Matrix<TComponent, 3, 3> &rotate);

		const Matrix<TComponent> &GetRotation() const;
		const Vec<TComponent, 3> &GetTranslation() const;

		Vec<TComponent, 3> Transform(const Vec<TComponent, 3> &pos) const;
		Vec<TComponent, 3> InverseTransform(const Vec<TComponent, 3> &pos) const;

		TranslateRotateMatrix34<TComponent> operator*(const TranslateRotateMatrix34<TComponent> &other) const;
		TranslateRotateMatrix34<TComponent> Rotate(const Matrix<TComponent, 3, 3> &other) const;
		TranslateRotateMatrix34<TComponent> Translate(const Vec<TComponent, 3> &other) const;

		TranslateRotateMatrix34<TComponent> Inverse() const;

	private:
		Matrix<TComponent, 3, 4> m_mat;
	};
} }

namespace rkit { namespace math {
} }
