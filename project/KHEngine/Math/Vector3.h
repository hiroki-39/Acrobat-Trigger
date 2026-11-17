#pragma once
#include <cmath>

class Vector3
{
public:
    float x;
    float y;
    float z;

    // --- コンストラクタ ---
    Vector3() : x(0), y(0), z(0) {}
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

    // --- 演算子 ---
    Vector3 operator+(const Vector3& v) const { return { x + v.x, y + v.y, z + v.z }; }
    Vector3 operator-(const Vector3& v) const { return { x - v.x, y - v.y, z - v.z }; }
    Vector3 operator*(float s) const { return { x * s, y * s, z * s }; }
    Vector3 operator/(float s) const { return { x / s, y / s, z / s }; }

    Vector3& operator+=(const Vector3& v)
    {
        x += v.x;
        y += v.y;
        z += v.z;
        return *this;
	}

    Vector3& operator-=(const Vector3& v)
    {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        return *this;
	}

    Vector3& operator*=(float s)
    {
        x *= s;
        y *= s;
        z *= s;
        return *this;
    }

    Vector3& operator/=(float s)
    {
        x /= s;
        y /= s;
        z /= s;
        return *this;
	}

    // --- ベクトル演算 ---
	
    // ベクトルの長さを計算
    float Length() const { return std::sqrt(x * x + y * y + z * z); }
    
	// 正規化
    Vector3 Normalize() const
    {
        float len = Length();
        return len > 0 ? Vector3(x / len, y / len, z / len) : Vector3(0, 0, 0);
    }

	// 内積
    static float Dot(const Vector3& a, const Vector3& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

	// 外積
    static Vector3 Cross(const Vector3& a, const Vector3& b)
    {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }
};

// スカラー * Vector3 の二項演算子サポート
inline Vector3 operator*(float s, const Vector3& v)
{
    return v * s;
}
