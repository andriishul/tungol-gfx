#pragma once
#include <cmath>
#include "vec.hpp"
#include <array>

namespace tungol::math {


    /*
    Column-major matrix storage.

    Constructed from row-major input for readability.

    Indexing:
      raw[column][row]
      column = basis vector
      row = component
    */
    template<typename T, uint32_t C, uint32_t R>
    struct mat
    {
    public:
        explicit mat(T def)
        {
            for (uint32_t c = 0; c < C; ++c)
                raw[c] = vec<T, R>(def);
        }

        mat() : mat(T(0)) 
        {}

        // Fills matrix in column-major order
        mat(const T* ptr) : mat(static_cast<T>(0))
        {
            if (!ptr) return;
            for (uint32_t c = 0; c < C; ++c) 
            {
                for (uint32_t r = 0; r < R; ++r) 
                    raw[c][r] = static_cast<T>(ptr[c * R + r]);
            }
        }

        // Fills matrix in column-major order
        mat(const std::array<T, C * R>& arr)
        {
            for (uint32_t c = 0; c < C; ++c)
            {
                for (uint32_t r = 0; r < R; ++r)
                    raw[c][r] = static_cast<T>(arr[c * R + r]);
            }
        }

        template<uint32_t mC, uint32_t mR>
        explicit mat(const mat<T, mC, mR>& m) : mat(static_cast<T>(0))
        {
            static_assert(mC <= C && mR <= R, "Submatrix too large");

            for (uint32_t c = 0; c < mC; ++c)
                for (uint32_t r = 0; r < mR; ++r)
                    raw[c][r] = m[c][r];
        }
        
        // Fills matrix in row-major order (left → right, top → bottom)
        template<typename ... Args>
        mat(T arg1, T arg2, Args... args) : mat(static_cast<T>(0))
        {
            static_assert(sizeof...(Args) + 2 == C * R, "Matrix requires exact number of elements");

            uint32_t c = 0, r = 0;
            auto assign = [this, &c, &r](T value)
                {
                    raw[c][r] = value;
                    if (++c >= C)
                    {
                        c = 0;
                        r++;
                    }
                   
                };


            assign(arg1);
            assign(arg2);
            (assign(static_cast<T>(args)), ...);
        }
        
        // Column access (matrix is column-major).
        // row_id selects basis vector; result is a column (vec of rows).
        vec<T, R>& operator[](int row_id) { return raw[row_id]; }
        const vec<T, R>& operator[](int row_id) const { return raw[row_id]; }

        static constexpr inline mat<T, C, R> identity()
        {
            mat<T, C, R> m{};
            const uint32_t diagonalSize = (C < R) ? C : R;
            for (uint32_t i = 0; i < diagonalSize; ++i)
                m[i][i] = static_cast<T>(1);
            return m;
        }

        const T* ptr() const { return raw[0].ptr(); }

    private:
        vec<T, R> raw[C];
    };


    template<typename T, uint32_t C, uint32_t R>
    constexpr inline mat<T, C, R> operator*(const mat<T, C, R>& m1, const mat<T, C, R>& m2)
    {
        mat<T, C, R> m{};
        for (uint32_t c = 0; c < C; ++c)
            for (uint32_t r = 0; r < R; ++r)
                for (uint32_t k = 0; k < C; ++k)
                    m[c][r] += m1[k][r] * m2[c][k];
        return m;
    }

    template<typename T, uint32_t C, uint32_t R>
    constexpr inline vec<T, R> operator*(const mat<T, C, R>& m1, const vec<T, C>& v2)
    {
        vec<T, R> v1{};
        for (uint32_t c = 0; c < C; ++c)
            for (uint32_t r = 0; r < R; ++r)
                v1[r] += m1[c][r] * v2[c];
        return v1;
    }

    template<typename T, uint32_t C, uint32_t R>
    constexpr inline mat<T, C, R> operator*(const mat<T, C, R>& m1, T scalar)
    {
        mat<T, C, R> m{};
        for (uint32_t c = 0; c < C; ++c)
            for (uint32_t r = 0; r < R; ++r)
                m[c][r] = m1[c][r] * scalar;
        return m;
    }

    template<typename T, uint32_t C, uint32_t R>
    constexpr inline mat<T, C, R> operator+(const mat<T, C, R>& m1, const mat<T, C, R>& m2)
    {
        mat<T, C, R> m{};
        for (uint32_t c = 0; c < C; ++c)
            for (uint32_t r = 0; r < R; ++r)
                m[c][r] = m1[c][r] + m2[c][r];
        return m;
    }

    template<typename T, uint32_t C, uint32_t R>
    constexpr inline mat<T, R, C> transpose(const mat<T, C, R>& m)
    {
        mat<T, R, C> res{};

        for (uint32_t c = 0; c < C; ++c)
            for (uint32_t r = 0; r < R; ++r)
                res[r][c] = m[c][r];
        return res;
    }

    template<typename T>
    constexpr inline T det(const mat<T, 3, 3>& m)
    {
        return static_cast<T>(m[0][0] * (m[1][1] * m[2][2] - m[2][1] * m[1][2])
            - m[0][1] * (m[1][0] * m[2][2] - m[2][0] * m[1][2])
            + m[0][2] * (m[1][0] * m[2][1] - m[2][0] * m[1][1]));
    }

    template<typename T>
    constexpr inline T det(const mat<T, 4, 4>& m)
    {
        T subFactor00 = m[2][2] * m[3][3] - m[3][2] * m[2][3];
        T subFactor01 = m[2][1] * m[3][3] - m[3][1] * m[2][3];
        T subFactor02 = m[2][1] * m[3][2] - m[3][1] * m[2][2];
        T subFactor03 = m[2][0] * m[3][3] - m[3][0] * m[2][3];
        T subFactor04 = m[2][0] * m[3][2] - m[3][0] * m[2][2];
        T subFactor05 = m[2][0] * m[3][1] - m[3][0] * m[2][1];

        vec<T, 4> detCof(
            +(m[1][1] * subFactor00 - m[1][2] * subFactor01 + m[1][3] * subFactor02),
            -(m[1][0] * subFactor00 - m[1][2] * subFactor03 + m[1][3] * subFactor04),
            +(m[1][0] * subFactor01 - m[1][1] * subFactor03 + m[1][3] * subFactor05),
            -(m[1][0] * subFactor02 - m[1][1] * subFactor04 + m[1][2] * subFactor05)
        );
        return static_cast<T>((m[0][0] * detCof[0]) +
            (m[0][1] * detCof[1]) +
            (m[0][2] * detCof[2]) +
            (m[0][3] * detCof[3]));

    }

    template<typename T>
    constexpr inline mat<T, 4, 4> inverse(const mat<T, 4, 4>& m)
    {
        T d = det(m);
        if (d == 0) return mat<T, 4, 4>(static_cast<T>(0));

        mat<T, 4, 4> adj{};

        for (uint32_t c = 0; c < 4; ++c)
        {
            for (uint32_t r = 0; r < 4; ++r)
            {
                mat<T, 3, 3> minorMatrix{};
                uint32_t minorCol = 0;
                for (uint32_t i = 0; i < 4; ++i)
                {
                    if (i == c) continue;
                    uint32_t minorRow = 0;
                    for (uint32_t j = 0; j < 4; ++j)
                    {
                        if (j == r) continue;
                        minorMatrix[minorCol][minorRow] = m[i][j];
                        minorRow++;
                    }
                    minorCol++;
                }

                // Calculate cofactor: sign * det(minor)
                T sign = ((r + c) % 2 == 0) ? 1.f : -1.f;
                adj[c][r] = sign * det(minorMatrix);
            }
        }

        return transpose(adj) * (static_cast<T>(1) / d);
    }


    /*
        TODO: Implement inverse and determinant for 3x3, 2x2 matrices.
    */

    template<typename T>
    inline mat<T, 4, 4> quat_to_mat(const vec<T, 4>& quat) 
    {

        mat<T, 4, 4> m{};

        T x = quat[0], y = quat[1], z = quat[2], w = quat[3];

        // Page 282, 3D Math Primer for Graphics and Game Development Second Edition
        m[0][0] = T(1) - T(2) * (y * y + z * z);
        m[0][1] = T(2) * (x * y + w * z);
        m[0][2] = T(2) * (x * z - w * y);

        m[1][0] = T(2) * (x * y - w * z);
        m[1][1] = T(1) - T(2) * (x * x + z * z);
        m[1][2] = T(2) * (y * z + w * x);

        m[2][0] = T(2) * (x * z + w * y);
        m[2][1] = T(2) * (y * z - w * x);
        m[2][2] = T(1) - T(2) * (x * x + y * y);


        m[3][3] = T(1);

        return m;
    }

    template<typename T>
    vec<T, 4> mat_to_quat(const mat<T, 3, 3>& m) 
    {
        // Page 284, 3D Math Primer for Graphics and Game Development Second Edition
        T x, y, z, w;
        
        T diagX = m[0][0] - m[1][1] - m[2][2];
        T diagY = m[1][1] - m[0][0] - m[2][2];
        T diagZ = m[2][2] - m[0][0] - m[1][1];
        T diagW = m[0][0] + m[1][1] + m[2][2];
        uint32_t biggestIndex = 0;
        T biggestDiag = diagW;
        if(diagX > biggestDiag)
        {
            biggestDiag = diagX;
            biggestIndex = 1;
        }
        if(diagY > biggestDiag)
        {
            biggestDiag = diagY;
            biggestIndex = 2;
        }
        if(diagZ > biggestDiag) 
        {
            biggestDiag = diagZ;
            biggestIndex = 3;
        }

        T biggestVal = std::sqrt(biggestDiag + T(1.0)) * T(0.5);
        T mult = T(0.25) / biggestVal;
        
        switch (biggestIndex) {
        case 0:
            w = biggestVal;
            x = (m[2][1] - m[1][2]) * mult;
            y = (m[0][2] - m[2][0]) * mult;
            z = (m[1][0] - m[0][1]) * mult;
            break;
        case 1:
            x = biggestVal;
            w = (m[2][1] - m[1][2]) * mult;
            y = (m[1][0] + m[0][1]) * mult;
            z = (m[0][2] + m[2][0]) * mult;
            break;
        case 2:
            y = biggestVal;
            w = (m[0][2] - m[2][0]) * mult;
            x = (m[1][0] + m[0][1]) * mult;
            z = (m[2][1] + m[1][2]) * mult;
            break;
        case 3:
            z = biggestVal;
            w = (m[1][0] - m[0][1]) * mult;
            x = (m[2][0] + m[0][2]) * mult;
            y = (m[1][2] + m[2][1]) * mult;
            break;
        }

        return vec<T, 4>{x, y, z, w};
    }
}
