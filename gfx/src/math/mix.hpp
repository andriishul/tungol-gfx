#pragma once
#include "quat.hpp"

namespace tungol::math::mix
{
	
	template<typename T, uint32_t S>
	inline vec<T, S> lerp(const vec<T, S>& a, const vec<T, S>& b, T t)
	{
		return a * (T(1) - t) + b * t;
	}
	
	template<typename T>
	inline qua<T> slerp(const qua<T>& q0, const qua<T>& q1, T t)
	{
		qua<T> temp_q0(q0), temp_q1(q1);
		T cos_w = dot(temp_q0, temp_q1);


		if (cos_w < T(0))
		{
			temp_q1 = -temp_q1;
			cos_w = -cos_w;
		}

		T k0, k1;

		if (cos_w > T(0.9995))
		{
			k0 = T(1) - t;
			k1 = t;
		}
		else
		{
			T sin_w = sqrt(1 - cos_w * cos_w);

			T w = atan2(sin_w, cos_w);

			k0 = sin((T(1) - t) * w) / sin_w;
			k1 = sin(t * w) / sin_w;
		}

		temp_q0 = temp_q0 * k0;
		temp_q1 = temp_q1 * k1;

		return temp_q0 + temp_q1;
	}
}
