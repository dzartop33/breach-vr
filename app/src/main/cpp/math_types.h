#pragma once
// Breach VR - 3D Math Types & Transformations
// Optimized for VR stereoscopic rendering and CQB ballistics.

#include <cmath>
#include <cstring>
#include <algorithm>

namespace breachvr {

constexpr float PI = 3.14159265358979323846f;
constexpr float DEG2RAD = PI / 180.0f;
constexpr float RAD2DEG = 180.0f / PI;

struct Vec2 {
    float x = 0.0f, y = 0.0f;
    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}
    float Length() const { return std::sqrt(x * x + y * y); }
};

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator/(float s) const { float inv = 1.0f / s; return Vec3(x * inv, y * inv, z * inv); }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }

    float Dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 Cross(const Vec3& o) const {
        return Vec3(y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x);
    }
    float LengthSq() const { return x * x + y * y + z * z; }
    float Length() const { return std::sqrt(LengthSq()); }
    Vec3 Normalized() const {
        float l = Length();
        return (l > 1e-6f) ? (*this * (1.0f / l)) : Vec3(0, 0, 0);
    }
    static float Distance(const Vec3& a, const Vec3& b) { return (a - b).Length(); }
    static Vec3 Lerp(const Vec3& a, const Vec3& b, float t) {
        return a + (b - a) * std::clamp(t, 0.0f, 1.0f);
    }
};

inline Vec3 operator*(float s, const Vec3& v) { return v * s; }

struct Vec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
    Vec4() = default;
    Vec4(float x_, float y_, float z_, float w_ = 1.0f) : x(x_), y(y_), z(z_), w(w_) {}
    Vec4(const Vec3& v, float w_ = 1.0f) : x(v.x), y(v.y), z(v.z), w(w_) {}
};

struct Quat {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
    Quat() = default;
    Quat(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}

    static Quat Identity() { return Quat(0, 0, 0, 1); }

    static Quat FromAxisAngle(const Vec3& axis, float angleRad) {
        float half = angleRad * 0.5f;
        float s = std::sin(half);
        Vec3 normAxis = axis.Normalized();
        return Quat(normAxis.x * s, normAxis.y * s, normAxis.z * s, std::cos(half));
    }

    Quat operator*(const Quat& o) const {
        return Quat(
            w * o.x + x * o.w + y * o.z - z * o.y,
            w * o.y - x * o.z + y * o.w + z * o.x,
            w * o.z + x * o.y - y * o.x + z * o.w,
            w * o.w - x * o.x - y * o.y - z * o.z
        );
    }

    Vec3 Rotate(const Vec3& v) const {
        Vec3 qv(x, y, z);
        Vec3 uv = qv.Cross(v);
        Vec3 uuv = qv.Cross(uv);
        return v + ((uv * w) + uuv) * 2.0f;
    }

    static Quat Slerp(const Quat& a, const Quat& b, float t) {
        float cosHalfTheta = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
        Quat target = b;
        if (cosHalfTheta < 0.0f) {
            target = Quat(-b.x, -b.y, -b.z, -b.w);
            cosHalfTheta = -cosHalfTheta;
        }
        if (cosHalfTheta >= 0.999f) {
            // Linear interpolate
            return Quat(
                a.x + t * (target.x - a.x),
                a.y + t * (target.y - a.y),
                a.z + t * (target.z - a.z),
                a.w + t * (target.w - a.w)
            );
        }
        float halfTheta = std::acos(cosHalfTheta);
        float sinHalfTheta = std::sqrt(1.0f - cosHalfTheta * cosHalfTheta);
        float ratioA = std::sin((1.0f - t) * halfTheta) / sinHalfTheta;
        float ratioB = std::sin(t * halfTheta) / sinHalfTheta;
        return Quat(
            a.x * ratioA + target.x * ratioB,
            a.y * ratioA + target.y * ratioB,
            a.z * ratioA + target.z * ratioB,
            a.w * ratioA + target.w * ratioB
        );
    }
};

// 4x4 Column-Major Matrix for OpenGL ES
struct Mat4 {
    float m[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };

    Mat4() = default;

    static Mat4 Identity() {
        return Mat4();
    }

    static Mat4 Translation(const Vec3& t) {
        Mat4 res;
        res.m[12] = t.x;
        res.m[13] = t.y;
        res.m[14] = t.z;
        return res;
    }

    static Mat4 Scaling(const Vec3& s) {
        Mat4 res;
        res.m[0]  = s.x;
        res.m[5]  = s.y;
        res.m[10] = s.z;
        return res;
    }

    static Mat4 FromRotation(const Quat& q) {
        Mat4 res;
        float x2 = q.x + q.x, y2 = q.y + q.y, z2 = q.z + q.z;
        float xx = q.x * x2,  xy = q.x * y2,  xz = q.x * z2;
        float yy = q.y * y2,  yz = q.y * z2,  zz = q.z * z2;
        float wx = q.w * x2,  wy = q.w * y2,  wz = q.w * z2;

        res.m[0]  = 1.0f - (yy + zz);
        res.m[1]  = xy + wz;
        res.m[2]  = xz - wy;
        res.m[3]  = 0.0f;

        res.m[4]  = xy - wz;
        res.m[5]  = 1.0f - (xx + zz);
        res.m[6]  = yz + wx;
        res.m[7]  = 0.0f;

        res.m[8]  = xz + wy;
        res.m[9]  = yz - wx;
        res.m[10] = 1.0f - (xx + yy);
        res.m[11] = 0.0f;

        res.m[12] = 0.0f;
        res.m[13] = 0.0f;
        res.m[14] = 0.0f;
        res.m[15] = 1.0f;
        return res;
    }

    static Mat4 TRS(const Vec3& t, const Quat& r, const Vec3& s) {
        Mat4 rot = FromRotation(r);
        Mat4 res;
        res.m[0]  = rot.m[0] * s.x;
        res.m[1]  = rot.m[1] * s.x;
        res.m[2]  = rot.m[2] * s.x;
        res.m[3]  = 0.0f;

        res.m[4]  = rot.m[4] * s.y;
        res.m[5]  = rot.m[5] * s.y;
        res.m[6]  = rot.m[6] * s.y;
        res.m[7]  = 0.0f;

        res.m[8]  = rot.m[8] * s.z;
        res.m[9]  = rot.m[9] * s.z;
        res.m[10] = rot.m[10] * s.z;
        res.m[11] = 0.0f;

        res.m[12] = t.x;
        res.m[13] = t.y;
        res.m[14] = t.z;
        res.m[15] = 1.0f;
        return res;
    }

    Mat4 operator*(const Mat4& b) const {
        Mat4 res;
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                res.m[c * 4 + r] =
                    m[0 * 4 + r] * b.m[c * 4 + 0] +
                    m[1 * 4 + r] * b.m[c * 4 + 1] +
                    m[2 * 4 + r] * b.m[c * 4 + 2] +
                    m[3 * 4 + r] * b.m[c * 4 + 3];
            }
        }
        return res;
    }

    Vec3 TransformPoint(const Vec3& p) const {
        float x = m[0] * p.x + m[4] * p.y + m[8]  * p.z + m[12];
        float y = m[1] * p.x + m[5] * p.y + m[9]  * p.z + m[13];
        float z = m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14];
        float w = m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15];
        if (std::abs(w) > 1e-6f) {
            float inv = 1.0f / w;
            return Vec3(x * inv, y * inv, z * inv);
        }
        return Vec3(x, y, z);
    }

    Vec3 TransformVector(const Vec3& v) const {
        return Vec3(
            m[0] * v.x + m[4] * v.y + m[8]  * v.z,
            m[1] * v.x + m[5] * v.y + m[9]  * v.z,
            m[2] * v.x + m[6] * v.y + m[10] * v.z
        );
    }

    // Invert rigid transform (Rotation + Translation)
    static Mat4 InvertRigid(const Mat4& in) {
        Mat4 res;
        // Transpose 3x3 rotation
        res.m[0] = in.m[0]; res.m[1] = in.m[4]; res.m[2] = in.m[8];
        res.m[4] = in.m[1]; res.m[5] = in.m[5]; res.m[6] = in.m[9];
        res.m[8] = in.m[2]; res.m[9] = in.m[6]; res.m[10] = in.m[10];

        // Translation = - R^T * T
        Vec3 t(in.m[12], in.m[13], in.m[14]);
        res.m[12] = -(res.m[0] * t.x + res.m[4] * t.y + res.m[8]  * t.z);
        res.m[13] = -(res.m[1] * t.x + res.m[5] * t.y + res.m[9]  * t.z);
        res.m[14] = -(res.m[2] * t.x + res.m[6] * t.y + res.m[10] * t.z);
        res.m[15] = 1.0f;
        return res;
    }

    // OpenXR stereoscopic projection matrix (from tangents)
    static Mat4 CreateProjection(float angleLeft, float angleRight,
                                float angleUp,   float angleDown,
                                float nearZ,     float farZ) {
        float tanLeft  = std::tan(angleLeft);
        float tanRight = std::tan(angleRight);
        float tanDown  = std::tan(angleDown);
        float tanUp    = std::tan(angleUp);

        float tanWidth  = tanRight - tanLeft;
        float tanHeight = tanUp - tanDown;

        Mat4 res;
        std::memset(res.m, 0, sizeof(res.m));

        if (tanWidth <= 0 || tanHeight <= 0) return res;

        res.m[0]  = 2.0f / tanWidth;
        res.m[5]  = 2.0f / tanHeight;
        res.m[8]  = (tanRight + tanLeft) / tanWidth;
        res.m[9]  = (tanUp + tanDown) / tanHeight;
        res.m[10] = -(farZ + nearZ) / (farZ - nearZ);
        res.m[11] = -1.0f;
        res.m[14] = -(2.0f * farZ * nearZ) / (farZ - nearZ);
        return res;
    }
};

struct Ray {
    Vec3 origin;
    Vec3 direction;
    Ray(const Vec3& o, const Vec3& d) : origin(o), direction(d.Normalized()) {}
    Vec3 PointAt(float t) const { return origin + direction * t; }
};

struct AABB {
    Vec3 minBounds;
    Vec3 maxBounds;

    AABB() : minBounds(1e9f, 1e9f, 1e9f), maxBounds(-1e9f, -1e9f, -1e9f) {}
    AABB(const Vec3& minB, const Vec3& maxB) : minBounds(minB), maxBounds(maxB) {}

    bool IntersectRay(const Ray& ray, float& tMinOut, float maxDist = 100.0f) const {
        float tmin = 0.0f;
        float tmax = maxDist;

        for (int i = 0; i < 3; ++i) {
            float o = (&ray.origin.x)[i];
            float d = (&ray.direction.x)[i];
            float minB = (&minBounds.x)[i];
            float maxB = (&maxBounds.x)[i];

            if (std::abs(d) < 1e-6f) {
                if (o < minB || o > maxB) return false;
            } else {
                float invD = 1.0f / d;
                float t1 = (minB - o) * invD;
                float t2 = (maxB - o) * invD;
                if (t1 > t2) std::swap(t1, t2);
                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);
                if (tmin > tmax) return false;
            }
        }
        tMinOut = tmin;
        return true;
    }
};

} // namespace breachvr
