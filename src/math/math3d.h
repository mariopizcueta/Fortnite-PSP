// Matematicas 3D minimas: vectores, matrices 4x4 y frustum.
//
// Convenciones (identicas a OpenGL y a libpspgum):
//  - Sistema dextrogiro, +Y arriba. La camara mira hacia -Z en espacio de vista.
//  - Matrices column-major: el elemento (fila r, columna c) esta en m[c * 4 + r],
//    la traslacion en m[12..14]. Se transforma como v' = M * v (vector columna).
//  - Mismo layout que ScePspFMatrix4, asi que se suben tal cual con sceGuSetMatrix.
#pragma once

#include <math.h>

#include <psptypes.h>

namespace math3d {

constexpr float kPi = 3.14159265358979f;

inline float toRadians(float degrees) { return degrees * (kPi / 180.0f); }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;

    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
};

inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(const Vec3& v) { return sqrtf(dot(v, v)); }
inline Vec3 normalize(const Vec3& v) {
    const float len = length(v);
    return len > 1e-6f ? v * (1.0f / len) : Vec3{0.0f, 0.0f, 0.0f};
}
inline Vec3 lerp(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }

struct alignas(16) Mat4 {
    float m[16];

    static Mat4 identity();
    static Mat4 translation(const Vec3& t);
    static Mat4 scale(const Vec3& s);
    static Mat4 rotationX(float radians);
    static Mat4 rotationY(float radians);
    static Mat4 rotationZ(float radians);
    // Proyeccion en perspectiva (igual que gluPerspective / sceGumPerspective).
    static Mat4 perspective(float fovyRadians, float aspect, float zNear, float zFar);
    // Matriz de vista (igual que gluLookAt / sceGumLookAt).
    static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up);

    Mat4 operator*(const Mat4& o) const;
    Vec3 transformPoint(const Vec3& p) const;

    // Copia alineada en el tipo que espera sceGuSetMatrix.
    ScePspFMatrix4 toPsp() const;
};

// Plano n . p + d = 0, con la normal apuntando hacia el interior del frustum.
struct Plane {
    Vec3 n;
    float d = 0.0f;
};

struct Frustum {
    Plane planes[6];

    // Extrae los planos de proyeccion * vista (metodo de Gribb-Hartmann).
    static Frustum fromMatrix(const Mat4& viewProjection);
    bool sphereVisible(const Vec3& center, float radius) const;
};

}  // namespace math3d
