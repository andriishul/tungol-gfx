#pragma once
#include "types.hpp"

namespace tungol::math
{
    inline mat4 translate(const vec3& translation)
    {
        math::mat4 m = math::mat4::identity();
        for (uint32_t i = 0; i < 3; ++i)
            m[3][i] = translation[i];
        return m;
    }

    // Rodrigues rotation formula
    // Note: n must be a unit vector (normalized axis of rotation)
    inline mat4 rotate(const vec3& n, float_t theta)
    {
        mat4 m{};
        const float_t s = std::sin(theta);
        const float_t c = std::cos(theta);
        const float_t omc = 1.0f - c;

        m[0][0] = (n[0] * n[0]) * omc + c;
        m[0][1] = (n[1] * n[0]) * omc + (n[2] * s); 
        m[0][2] = (n[2] * n[0]) * omc - (n[1] * s);

        m[1][0] = (n[0] * n[1]) * omc - (n[2] * s); 
        m[1][1] = (n[1] * n[1]) * omc + c;
        m[1][2] = (n[2] * n[1]) * omc + (n[0] * s);

        m[2][0] = (n[0] * n[2]) * omc + (n[1] * s);
        m[2][1] = (n[1] * n[2]) * omc - (n[0] * s);
        m[2][2] = (n[2] * n[2]) * omc + c;

        m[3][3] = float_t(1);

        return m;
    }

    inline mat4 scale(const vec3& k)
    {
        mat4 m{};
        for (uint32_t i = 0; i < 3; ++i)
            m[i][i] = k[i];
        m[3][3] = float_t(1);
        return m;
    }

    inline void decompose(const mat4& m, vec3& t, quat& r, vec3& s)
    {
        t = vec3(m[3].x, m[3].y, m[3].z);

        vec3 x(m[0].x, m[0].y, m[0].z);
        vec3 y(m[1].x, m[1].y, m[1].z);
        vec3 z(m[2].x, m[2].y, m[2].z);


        s[0] = length(x);
        s[1] = length(y);
        s[2] = length(z);

        if (s[0] > 0) x = x / s[0];
        if (s[1] > 0) y = y / s[1];
        if (s[2] > 0) z = z / s[2];

        mat3 rm(x[0], y[0], z[0],
                x[1], y[1], z[1],
                x[2], y[2], z[2]);


        r = mat_to_quat(rm);
    }
}
