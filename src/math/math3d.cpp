#include "math/math3d.h"

#include <string.h>

namespace math3d {

Mat4 Mat4::identity() {
    Mat4 r;
    memset(r.m, 0, sizeof(r.m));
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

Mat4 Mat4::translation(const Vec3& t) {
    Mat4 r = identity();
    r.m[12] = t.x;
    r.m[13] = t.y;
    r.m[14] = t.z;
    return r;
}

Mat4 Mat4::scale(const Vec3& s) {
    Mat4 r = identity();
    r.m[0] = s.x;
    r.m[5] = s.y;
    r.m[10] = s.z;
    return r;
}

Mat4 Mat4::rotationX(float radians) {
    const float c = cosf(radians), s = sinf(radians);
    Mat4 r = identity();
    r.m[5] = c;
    r.m[6] = s;
    r.m[9] = -s;
    r.m[10] = c;
    return r;
}

Mat4 Mat4::rotationY(float radians) {
    const float c = cosf(radians), s = sinf(radians);
    Mat4 r = identity();
    r.m[0] = c;
    r.m[2] = -s;
    r.m[8] = s;
    r.m[10] = c;
    return r;
}

Mat4 Mat4::rotationZ(float radians) {
    const float c = cosf(radians), s = sinf(radians);
    Mat4 r = identity();
    r.m[0] = c;
    r.m[1] = s;
    r.m[4] = -s;
    r.m[5] = c;
    return r;
}

Mat4 Mat4::perspective(float fovyRadians, float aspect, float zNear, float zFar) {
    const float f = 1.0f / tanf(fovyRadians * 0.5f);
    Mat4 r;
    memset(r.m, 0, sizeof(r.m));
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (zFar + zNear) / (zNear - zFar);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * zFar * zNear) / (zNear - zFar);
    return r;
}

Mat4 Mat4::lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    const Vec3 f = normalize(center - eye);
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);
    Mat4 r = identity();
    r.m[0] = s.x;  r.m[4] = s.y;  r.m[8] = s.z;
    r.m[1] = u.x;  r.m[5] = u.y;  r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -dot(s, eye);
    r.m[13] = -dot(u, eye);
    r.m[14] = dot(f, eye);
    return r;
}

Mat4 Mat4::operator*(const Mat4& o) const {
    Mat4 r;
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            r.m[c * 4 + row] = m[0 * 4 + row] * o.m[c * 4 + 0] + m[1 * 4 + row] * o.m[c * 4 + 1] +
                               m[2 * 4 + row] * o.m[c * 4 + 2] + m[3 * 4 + row] * o.m[c * 4 + 3];
        }
    }
    return r;
}

Vec3 Mat4::transformPoint(const Vec3& p) const {
    return {m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
            m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
            m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]};
}

ScePspFMatrix4 Mat4::toPsp() const {
    ScePspFMatrix4 out;
    memcpy(&out, m, sizeof(m));
    return out;
}

Frustum Frustum::fromMatrix(const Mat4& vp) {
    // Fila i de la matriz: (m[i], m[4+i], m[8+i], m[12+i]).
    auto row = [&vp](float sign, int base) {
        Plane p;
        p.n = {vp.m[3] + sign * vp.m[base], vp.m[7] + sign * vp.m[4 + base],
               vp.m[11] + sign * vp.m[8 + base]};
        p.d = vp.m[15] + sign * vp.m[12 + base];
        const float len = length(p.n);
        if (len > 1e-6f) {
            p.n = p.n * (1.0f / len);
            p.d /= len;
        }
        return p;
    };
    Frustum f;
    f.planes[0] = row(1.0f, 0);   // izquierdo
    f.planes[1] = row(-1.0f, 0);  // derecho
    f.planes[2] = row(1.0f, 1);   // inferior
    f.planes[3] = row(-1.0f, 1);  // superior
    f.planes[4] = row(1.0f, 2);   // cercano
    f.planes[5] = row(-1.0f, 2);  // lejano
    return f;
}

bool Frustum::sphereVisible(const Vec3& center, float radius) const {
    for (const Plane& p : planes) {
        if (dot(p.n, center) + p.d < -radius) return false;
    }
    return true;
}

}  // namespace math3d
