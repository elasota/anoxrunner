#pragma once

#include <stddef.h>

namespace rkit { namespace math {
	template<class TComponent, size_t TRows, size_t TCols>
	class Matrix;

	template<class TComponent>
	class TranslateRotateMatrix34;

	using Mat33 = Matrix<float, 3, 3>;
	using Mat34 = Matrix<float, 3, 4>;
	using Mat44 = Matrix<float, 4, 4>;
} }
