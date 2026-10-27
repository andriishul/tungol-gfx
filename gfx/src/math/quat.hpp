#pragma once
#include "vec.hpp"
#include "trigonometry.hpp"

namespace tungol::math
{
	template<typename T>
	using qua = vec<T, 4>;

	template<typename T>
	constexpr inline qua<T> conjugate(const qua<T>& q)
	{
		return qua<T>(-q.x, -q.y, -q.z, q.w);
	}

	template<typename T>
	constexpr inline qua<T> inverse(const qua<T>& q)
	{
		return conjugate(q) / dot(q, q);
	}

	// Hamilton product, quaternion multiplication (x, y, z, w)
	template<typename T>
	constexpr inline qua<T> qmul(const qua<T>& q1, const qua<T>& q2)
	{
		return qua<T>(
			q1.w * q2.x + q1.x * q2[3] + q1[1] * q2[2] - q1[2] * q2[1], //x
			q1.w * q2.y + q1.y * q2[3] + q1[2] * q2[0] - q1[0] * q2[2], //y
			q1.w * q2.z + q1.z * q2.w + q1.x * q2.y - q1.y * q2.x, //z
			q1.w * q2.w - (q1.x * q2.x + q1.y * q2.y + q1.z * q2.z) //w
		);
	}

	// n should be normalized unit axis vector
	template<typename T>
	constexpr inline qua<T> rotate(T angle, const vec<T, 3>& n) 
	{
		T halfAngle = angle * T(0.5);
		T s = sin(halfAngle);
		T c = cos(halfAngle);
		vec<T, 3> r = n * s;
		return qua<T>(r.x, r.y, r.z, c);
	}


	template<typename T>
	constexpr inline qua<T> lookAt(const vec<T, 3>& dest, const vec<T, 3>& src) 
	{
		vec<T, 3> forward{ 0, 0, 1 };
		vec<T, 3> forwardVec = normalize(dest - src);

		T d = dot(forward, forwardVec);

		if (d < T(-0.999999))
		{
			return qua<T>(0, 1, 0, 0); 
		}

		vec<T, 3> half = normalize(forward + forwardVec);
		vec<T, 3> xyz = cross(forward, half);
		T w = dot(forward, half);

		return qua<T>(xyz[0], xyz[1], xyz[2], w);
	}
}

