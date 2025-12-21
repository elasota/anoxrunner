#include <stddef.h>

namespace rkit { namespace math {
	template<class TComponent, size_t TSize>
	class Vec;

	typedef Vec<float, 2> Vec2;
	typedef Vec<float, 3> Vec3;
	typedef Vec<float, 4> Vec4;

	typedef Vec<double, 2> DVec2;
	typedef Vec<double, 3> DVec3;
	typedef Vec<double, 4> DVec4;
} }
