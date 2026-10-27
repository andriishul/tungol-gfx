#pragma once
#include <cstdint>
#include <limits>

#include <iostream>
#include <stdexcept>

namespace tungol::math {

    template<typename T, uint32_t S>
    struct vec;

    template<typename T>
    struct vec<T, 2> 
    {
        union 
        {
            struct { T x, y; };
            T xy[2];
        };

        vec(T x = 0, T y = 0)
            : x(x), y(y)
        {
        }

        const T& operator[](uint32_t index) const
        {
            if (index >= sizeof(xy) / sizeof(T))
                throw std::out_of_range("accessing vec2 out of bounds");
            return xy[index];
        }

        T& operator[](uint32_t index)
        {
            if (index >= sizeof(xy) / sizeof(T)) 
                throw std::out_of_range("accessing vec2 out of bounds");
            return xy[index];
        }
    };

    template<typename T>
    struct vec<T, 3>
    {
        union
        {
            struct { T x, y, z; };
            T xyz[3];
        };


        vec(T x = 0, T y = 0, T z = 0)
            : x(x), y(y), z(z)
        {
        }

        const T& operator[](uint32_t index) const
        {
            if (index >= sizeof(xyz) / sizeof(T)) throw std::out_of_range("accessing vec3 out of bounds");
            return xyz[index];
        }

        T& operator[](uint32_t index)
        {
            if (index >= sizeof(xyz) / sizeof(T)) throw std::out_of_range("accessing vec3 out of bounds");
            return xyz[index];
        }
    };

    template<typename T>
    struct vec<T, 4>
    {
        union
        {
            struct { T x, y, z, w; };
            T xyzw[4];
        };

        vec(T x = 0, T y = 0, T z = 0, T w = 0) 
            : x(x), y(y), z(z), w(w)
        {}

        const T& operator[](uint32_t index) const
        {
            if (index >= sizeof(xyzw) / sizeof(T))
                throw std::out_of_range("accessing vec4 out of bounds");
            return xyzw[index];
        }

        T& operator[](uint32_t index)
        {
            if (index >= sizeof(xyzw) / sizeof(T))
                throw std::out_of_range("accessing vec4 out of bounds");
            return xyzw[index];
        }
    };


    template<typename T, uint32_t S, uint32_t R>
    constexpr inline vec<T, S> operator+(const vec<T, S>& v1, const vec<T, R>& v2) {
        vec<T, S> v = v1;
        constexpr uint32_t MIN = (S < R) ? S : R;
        for (uint32_t i = 0; i < MIN; ++i) v[i] = v1[i] + v2[i];
        return v;
    }

    template<typename T, uint32_t S, uint32_t R>
    constexpr inline vec<T, S> operator-(const vec<T, S>& v1, const vec<T, R>& v2) {
        vec<T, S> v = v1;
        constexpr uint32_t MIN = (S < R) ? S : R;
        for (uint32_t i = 0; i < MIN; ++i) v[i] = v1[i] - v2[i];
        return v;
    }

    template<typename T, uint32_t S>
    constexpr inline vec<T, S> operator-(const vec<T, S>& v) {
        return vec<T, S>(T(0)) - v;
    }

    template<typename T, uint32_t S, uint32_t R>
    constexpr inline vec<T, S> operator*(const vec<T, S>& v1, const vec<T, R>& v2) {
        vec<T, S> v = v1;
        constexpr uint32_t MIN = (S < R) ? S : R;
        for (uint32_t i = 0; i < MIN; ++i) v[i] = v1[i] * v2[i];
        return v;
    }

    template<typename T, uint32_t S>
    constexpr inline vec<T, S> operator*(const vec<T, S>& v1, T s) {
        vec<T, S> v{};
        for (uint32_t i = 0; i < S; ++i) v[i] = v1[i] * s;
        return v;
    }

    template<typename T, uint32_t S>
    constexpr inline vec<T, S> operator/(const vec<T, S>& v1, T s) {
        vec<T, S> v{};
        for (uint32_t i = 0; i < S; ++i) v[i] = v1[i] / s;
        return v;
    }

    template<typename T>
    constexpr inline vec<T, 3> cross(const vec<T, 3>& v1, const vec<T, 3>& v2) {
        return vec<T, 3>(
            v1.y * v2.z - v1.z * v2.y,
            v1.z * v2.x - v1.x * v2.z,
            v1.x * v2.y - v1.y * v2.x
        );
    }

    template<typename T, uint32_t S>
    constexpr inline T dot(const vec<T, S>& v1, const vec<T, S>& v2) {
        T result = v1[0] * v2[0];
        for (uint32_t i = 1; i < S; ++i) result += v1[i] * v2[i];
        return result;
    }

    template<typename T, uint32_t S>
    constexpr inline T length(const vec<T, S>& v) {
        return std::sqrt(dot(v, v));
    }

    template<typename T, uint32_t S>
    constexpr inline vec<T, S> normalize(const vec<T, S>& v) {
        T len = length(v);
        return (len > std::numeric_limits<T>::epsilon()) ? (v / len) : vec<T, S>{};
    }

    template<typename T, uint32_t S>
    std::ostream& operator<<(std::ostream& os, const vec<T, S>& v) 
    {
        os << "[ ";
        for (uint32_t i = 0; i < S; ++i) os << v[i] << " ";
        return os << "]";
    }
}
