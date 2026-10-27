#pragma once

namespace tungol::math
{
	template<typename T>
	constexpr const T PI = static_cast<T>(3.14159);

    template<typename T>
    constexpr inline T to_radians(T degrees) {
        return degrees * (PI<T> / static_cast<T>(180));
    }

    template<typename T>
    constexpr inline T to_degrees(T radians) {
        return radians * (static_cast<T>(180) / PI<T>);
    }
}
