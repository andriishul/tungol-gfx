#pragma once
#include "types.hpp"
#include "trigonometry.hpp"

namespace tungol::math {

	inline mat4 lookAt(vec3 eye, vec3 center, vec3 up)
	{
		vec3 f = normalize(center - eye);

		vec3 r = normalize(cross(f, up));
		vec3 u = cross(r, f);

		mat4 m(
			mat3(
				r[0], u[0], -f[0],
				r[1], u[1], -f[1],
				r[2], u[2], -f[2]
			));
		m = transpose(m);
		m[3][0] = -dot(r, eye);
		m[3][1] = -dot(u, eye);
		m[3][2] = dot(f, eye);
		m[3][3] = 1.0f;

		return m;
	}

	inline mat4 perspective(float fov, float aspect, float near, float far)
	{
		mat4 m{};

		m[0][0] = 1.0f / (aspect * std::tan(fov / 2.0f));
		m[1][1] = -(1.0f / std::tan(fov / 2.0f));
		m[2][2] = (far + near) / (near - far);
		m[2][3] = -1.0f;
		m[3][2] = (2.0f * far * near) / (near - far);

		return m;
	}

	inline mat4 ortho(float l, float r, float b, float t, float n, float f)
	{
		math::mat4 m = math::mat4::identity();

		m[0][0] = 2.0f / (r - l);
		m[1][1] = 2.0f / (t - b);
		m[2][2] = 1.0f / (n - f);

		m[3][0] = -(r + l) / (r - l);
		m[3][1] = -(t + b) / (t - b);
		m[3][2] = n / (n - f);

		return m;
	}
}
