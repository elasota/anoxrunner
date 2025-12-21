#pragma once

#include "QuatProto.h"
#include "Vec.h"

namespace rkit { namespace math {
	template<class TComponent, size_t TRows, size_t TCols>
	class Matrix;

	template<class TComponent>
	class Quaternion
	{
	public:
		Quaternion() = default;
		Quaternion(const Quaternion &) = default;
		Quaternion(TComponent x, TComponent y, TComponent z, TComponent w);

		Quaternion &operator=(const Quaternion &) = default;

		Quaternion operator*(const Quaternion &other);

		Matrix<TComponent, 3, 3> ToRotationMatrix() const;
		static Quaternion<TComponent> FromRotationMatrix(const Matrix<TComponent, 3, 3> &mat);

		static Quaternion<TComponent> FromAxisAnglePrecomputed(TComponent axisX, TComponent axisY, TComponent axisZ, TComponent sinHalfAngle, TComponent cosHalfAngle);
		static Quaternion<TComponent> FromAxisAnglePrecomputed(const Vec<TComponent, 3> &axis, TComponent sinHalfAngle, TComponent cosHalfAngle);

	private:
		Quaternion(const Vec<TComponent, 4> &vec);

		Vec<TComponent, 4> m_value;
	};
} }

#include "Mat.h"

namespace rkit { namespace math {
	template<class TComponent>
	Quaternion<TComponent>::Quaternion(TComponent x, TComponent y, TComponent z, TComponent w)
		: m_value(x, y, z, w)
	{
	}

	template<class TComponent>
	Quaternion<TComponent> Quaternion<TComponent>::operator*(const Quaternion &other)
	{
		/*
			x        y        z        w
			+a.w*b.x +a.w*b.y +a.w*b.z +a.w*b.w
			+a.x*b.w -a.x*b.z +a.x*b.y -a.x*b.x
			+a.y*b.z +a.y*b.w -a.y*b.x -a.y*b.y
			-a.z*b.y +a.z*b.x +a.z*b.w -a.z*b.z
		*/
		const Vec<TComponent, 4> a = m_value;
		const Vec<TComponent, 4> b = other.m_value;

		const Vec<TComponent, 4> result =
			a.template Broadcast<3>() * b +
			(a.template Broadcast<0>() * b.template Swizzle<3, 2, 1, 0>()).template NegateElements<false, true, false, true>() +
			(a.template Broadcast<1>() * b.template Swizzle<2, 3, 0, 1>()).template NegateElements<false, false, true, true>() +
			(a.template Broadcast<2>() * b.template Swizzle<1, 0, 3, 2>()).template NegateElements<true, false, false, true>();

		return Quaternion<TComponent>(result);
	}

	template<class TComponent>
	Quaternion<TComponent>::Quaternion(const Vec<TComponent, 4> &vec)
		: m_value(vec)
	{
	}

	template<class TComponent>
	Quaternion<TComponent> Quaternion<TComponent>::FromAxisAnglePrecomputed(TComponent axisX, TComponent axisY, TComponent axisZ, TComponent sinHalfAngle, TComponent cosHalfAngle)
	{
		return Quaternion<TComponent>(axisX * sinHalfAngle, axisY * sinHalfAngle, axisZ * sinHalfAngle, cosHalfAngle);
	}

	template<class TComponent>
	Quaternion<TComponent> Quaternion<TComponent>::FromAxisAnglePrecomputed(const Vec<TComponent, 3> &axis, TComponent sinHalfAngle, TComponent cosHalfAngle)
	{
		return Quaternion<TComponent>(axis.GetX() * sinHalfAngle, axis.GetY() * sinHalfAngle, axis.GetZ() * sinHalfAngle, cosHalfAngle);
	}

	template<class TComponent>
	Matrix<TComponent, 3, 3> Quaternion<TComponent>::ToRotationMatrix() const
	{
		//          (1 - 2*y*y - 2*z*z)     (2*x*y - 2*z*w)     (2*x*z + 2*y*w)
		// result =     (2*x*y + 2*z*w) (1 - 2*x*x - 2*z*z)     (2*y*z - 2*x*w)
		//              (2*x*z - 2*y*w)     (2*y*z + 2*x*w) (1 - 2*x*x - 2*y*y)

		// Reordered:
		//          (1                    (0                   (0
		//             - 2*y*y               + 2*y*x              + 2*y*w
		//             - 2*z*z)              - 2*z*w)             + 2*z*x)
		//          (0                    (1                   (0
		// result =    + 2*x*y               - 2*x*x              - 2*x*w
		//             + 2*z*w)              - 2*z*z)             + 2*z*y)
		//          (0                    (0                   (1
		//             + 2*x*z               + 2*x*w)             - 2*x*x)
		//             - 2*y*w)              + 2*y*z)             - 2*y*y)

		Vec<TComponent, 3> valueMul2 = (m_value + m_value).template Swizzle<0, 1, 2>();

		const Vec<TComponent, 3> row0 = Vec<TComponent, 3>(static_cast<TComponent>(1), static_cast<TComponent>(0), static_cast<TComponent>(0))
			+ (valueMul2.template Broadcast<1>() * m_value.template Swizzle<1, 0, 3>()).template NegateElements<true, false, false>()
			- (valueMul2.template Broadcast<2>() * m_value.template Swizzle<2, 3, 0>()).template NegateElements<false, false, true>();

		const Vec<TComponent, 3> row1 = Vec<TComponent, 3>(static_cast<TComponent>(0), static_cast<TComponent>(1), static_cast<TComponent>(0))
			- (valueMul2.template Broadcast<0>() * m_value.template Swizzle<1, 0, 3>()).template NegateElements<true, false, false>()
			+ (valueMul2.template Broadcast<2>() * m_value.template Swizzle<3, 2, 1>()).template NegateElements<false, true, false>();

		const Vec<TComponent, 3> row2 =
			  Vec<TComponent, 3>(static_cast<TComponent>(0), static_cast<TComponent>(0), static_cast<TComponent>(1))
			+ (valueMul2.template Broadcast<0>() * m_value.template Swizzle<2, 3, 0>()).template NegateElements<false, false, true>()
			- (valueMul2.template Broadcast<1>() * m_value.template Swizzle<3, 2, 1>()).template NegateElements<false, true, false>();

		return Matrix<TComponent, 3, 3>(row0, row1, row2);
	}

	/*
	static Quaternion<TComponent> FromRotationMatrix(const Matrix<3, 3, TComponent> &mat);

	static Quaternion<TComponent> FromAxisAnglePrecomputed(const Vec<TComponent, 3> &axis, TComponent sinHalfAngle, TComponent cosHalfAngle);
	*/


} }
