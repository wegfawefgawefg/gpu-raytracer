#pragma once

#include <cmath>

struct Float3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct alignas(16) Float4
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

inline Float3 operator+(const Float3& a, const Float3& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Float3 operator-(const Float3& a, const Float3& b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Float3 operator*(const Float3& v, float s)
{
    return {v.x * s, v.y * s, v.z * s};
}

inline Float3 operator*(float s, const Float3& v)
{
    return v * s;
}

inline Float3 operator/(const Float3& v, float s)
{
    return {v.x / s, v.y / s, v.z / s};
}

inline Float3& operator+=(Float3& a, const Float3& b)
{
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}

inline float Dot(const Float3& a, const Float3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Float3 Cross(const Float3& a, const Float3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

inline float Length(const Float3& v)
{
    return std::sqrt(Dot(v, v));
}

inline Float3 Normalize(const Float3& v)
{
    const float length = Length(v);
    if (length <= 0.0f)
    {
        return {0.0f, 0.0f, 0.0f};
    }

    return v / length;
}

inline Float4 ToFloat4(const Float3& v, float w = 0.0f)
{
    return {v.x, v.y, v.z, w};
}
