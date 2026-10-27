#pragma once
#include "mat.hpp"
#include "quat.hpp"

namespace tungol::math
{
    using quat = qua<float>;
   
    using vec2 = vec<float, 2>;
    using vec3 = vec<float, 3>;
    using vec4 = vec<float, 4>;

    using ivec4 = vec<int32_t, 4>;

    using mat3 = mat<float, 3, 3>;
    using mat4 = mat<float, 4, 4>;
}
