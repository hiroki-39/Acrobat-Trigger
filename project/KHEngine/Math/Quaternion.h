#pragma once
#include "Vector3.h"
#include "Matrix4x4.h"
#include <cmath>
#include <algorithm>

/// <summary>
/// クォータニオン（四元数）クラス
/// 左手座標系・行ベクトル規約（Matrix4x4::RotateX/Y/Z と完全互換）
/// </summary>
class Quaternion
{
public:
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float w = 1.0f;

	Quaternion() = default;
	Quaternion(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

	static Quaternion Identity()
	{
		return { 0.0f, 0.0f, 0.0f, 1.0f };
	}

	float Length() const
	{
		return std::sqrt(x * x + y * y + z * z + w * w);
	}

	Quaternion Normalized() const
	{
		float len = Length();
		if (len > 0.00001f)
		{
			float inv = 1.0f / len;
			return { x * inv, y * inv, z * inv, w * inv };
		}
		return Identity();
	}

	/// <summary>
	/// 任意軸と角度（ラジアン）からクォータニオンを生成
	/// </summary>
	static Quaternion FromAxisAngle(const Vector3& axis, float angle)
	{
		Vector3 n = axis;
		float len = n.Length();
		if (len > 0.00001f)
		{
			n = n / len;
		}
		else
		{
			return Identity();
		}
		float half = angle * 0.5f;
		float s = std::sinf(half);
		float c = std::cosf(half);
		return { n.x * s, n.y * s, n.z * s, c };
	}

	/// <summary>
	/// オイラー角（X -> Y -> Z 順）からクォータニオンを生成
	/// </summary>
	static Quaternion FromEuler(const Vector3& euler)
	{
		Quaternion qx = FromAxisAngle({ 1.0f, 0.0f, 0.0f }, euler.x);
		Quaternion qy = FromAxisAngle({ 0.0f, 1.0f, 0.0f }, euler.y);
		Quaternion qz = FromAxisAngle({ 0.0f, 0.0f, 1.0f }, euler.z);
		// 行ベクトル規約 (Rx * Ry * Rz) に対応: qz * qy * qx
		return Multiply(qz, Multiply(qy, qx)).Normalized();
	}

	/// <summary>
	/// クォータニオンの乗算 (lhs * rhs : 先に rhs の回転を適用し、次に lhs の回転を適用)
	/// </summary>
	static Quaternion Multiply(const Quaternion& lhs, const Quaternion& rhs)
	{
		return {
			lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
			lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
			lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
			lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z
		};
	}

	Quaternion operator*(const Quaternion& rhs) const
	{
		return Multiply(*this, rhs);
	}

	/// <summary>
	/// 球面線形補間 (Slerp)
	/// </summary>
	static Quaternion Slerp(const Quaternion& q0, const Quaternion& q1, float t)
	{
		Quaternion a = q0.Normalized();
		Quaternion b = q1.Normalized();
		float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
		if (dot < 0.0f)
		{
			b = { -b.x, -b.y, -b.z, -b.w };
			dot = -dot;
		}
		if (dot > 0.9995f)
		{
			Quaternion res = {
				a.x + (b.x - a.x) * t,
				a.y + (b.y - a.y) * t,
				a.z + (b.z - a.z) * t,
				a.w + (b.w - a.w) * t
			};
			return res.Normalized();
		}
		float theta = std::acosf(std::clamp(dot, -1.0f, 1.0f));
		float sinTheta = std::sinf(theta);
		float w0 = std::sinf((1.0f - t) * theta) / sinTheta;
		float w1 = std::sinf(t * theta) / sinTheta;
		return {
			a.x * w0 + b.x * w1,
			a.y * w0 + b.y * w1,
			a.z * w0 + b.z * w1,
			a.w * w0 + b.w * w1
		};
	}

	/// <summary>
	/// 前方ベクトルと上方向ベクトルから回転クォータニオンを生成
	/// </summary>
	static Quaternion LookRotation(const Vector3& forward, const Vector3& up = { 0.0f, 1.0f, 0.0f })
	{
		Vector3 f = forward;
		float fLen = f.Length();
		if (fLen < 0.00001f) return Identity();
		f = f / fLen;

		Vector3 uRef = up;
		Vector3 r = Vector3::Cross(uRef, f);
		float rLen = r.Length();
		if (rLen < 0.00001f)
		{
			// forward と up が平行な場合のフォールバック
			uRef = { 0.0f, 0.0f, -1.0f };
			r = Vector3::Cross(uRef, f);
			rLen = r.Length();
			if (rLen < 0.00001f) return Identity();
		}
		r = r / rLen;
		Vector3 u = Vector3::Cross(f, r);

		// 行ベクトル回転行列 [r; u; f] からクォータニオンへ変換
		float trace = r.x + u.y + f.z;
		Quaternion q;
		if (trace > 0.0f)
		{
			float s = std::sqrtf(trace + 1.0f) * 2.0f;
			q.w = 0.25f * s;
			q.x = (u.z - f.y) / s;
			q.y = (f.x - r.z) / s;
			q.z = (r.y - u.x) / s;
		}
		else if (r.x > u.y && r.x > f.z)
		{
			float s = std::sqrtf(1.0f + r.x - u.y - f.z) * 2.0f;
			q.w = (u.z - f.y) / s;
			q.x = 0.25f * s;
			q.y = (u.x + r.y) / s;
			q.z = (f.x + r.z) / s;
		}
		else if (u.y > f.z)
		{
			float s = std::sqrtf(1.0f + u.y - r.x - f.z) * 2.0f;
			q.w = (f.x - r.z) / s;
			q.x = (u.x + r.y) / s;
			q.y = 0.25f * s;
			q.z = (f.y + u.z) / s;
		}
		else
		{
			float s = std::sqrtf(1.0f + f.z - r.x - u.y) * 2.0f;
			q.w = (r.y - u.x) / s;
			q.x = (f.x + r.z) / s;
			q.y = (f.y + u.z) / s;
			q.z = 0.25f * s;
		}
		return q.Normalized();
	}

	/// <summary>
	/// 回転行列（行ベクトル規約）へ変換
	/// </summary>
	Matrix4x4 ToMatrix() const
	{
		Quaternion q = Normalized();
		float xx = q.x * q.x;
		float yy = q.y * q.y;
		float zz = q.z * q.z;
		float xy = q.x * q.y;
		float xz = q.x * q.z;
		float yz = q.y * q.z;
		float wx = q.w * q.x;
		float wy = q.w * q.y;
		float wz = q.w * q.z;

		Matrix4x4 m = Matrix4x4::Identity();
		m.m[0][0] = 1.0f - 2.0f * (yy + zz);
		m.m[0][1] = 2.0f * (xy + wz);
		m.m[0][2] = 2.0f * (xz - wy);

		m.m[1][0] = 2.0f * (xy - wz);
		m.m[1][1] = 1.0f - 2.0f * (xx + zz);
		m.m[1][2] = 2.0f * (yz + wx);

		m.m[2][0] = 2.0f * (xz + wy);
		m.m[2][1] = 2.0f * (yz - wx);
		m.m[2][2] = 1.0f - 2.0f * (xx + yy);
		return m;
	}
};
