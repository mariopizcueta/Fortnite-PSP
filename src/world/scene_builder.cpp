#include "world/scene_builder.h"

#include <math.h>

#include "world/world_units.h"

namespace world {

using math3d::Mat4;
using math3d::Vec3;

namespace {

// Direccion hacia el sol: alto, desde el sureste (+X este, +Z sur).
const Vec3 kSunDir = math3d::normalize({0.45f, 0.80f, 0.35f});
constexpr float kAmbient = 0.58f;
constexpr float kDiffuse = 0.42f;

int segmentsFor(float length, float maxSegment) {
    const int n = static_cast<int>(ceilf(length / maxSegment - 0.001f));
    return n < 1 ? 1 : n;
}

u32 scaleColor(u32 color, float s) {
    auto channel = [s](u32 c) {
        const float v = static_cast<float>(c) * s;
        return v >= 255.0f ? 255u : static_cast<u32>(v);
    };
    return (color & 0xFF000000u) | (channel((color >> 16) & 0xFF) << 16) |
           (channel((color >> 8) & 0xFF) << 8) | channel(color & 0xFF);
}

}  // namespace

SceneBuilder::SceneBuilder(float maxSegment)
    : m_maxSegment(maxSegment), m_transform(Mat4::identity()) {}

void SceneBuilder::setTransform(const Mat4& transform) {
    m_transform = transform;
    m_hasTransform = true;
}

void SceneBuilder::resetTransform() {
    m_transform = Mat4::identity();
    m_hasTransform = false;
}

u32 SceneBuilder::shade(u32 color, const Vec3& normal) const {
    const float d = math3d::dot(normal, kSunDir);
    return scaleColor(color, kAmbient + kDiffuse * (d > 0.0f ? d : 0.0f));
}

SceneBuilder::Bucket& SceneBuilder::bucketFor(int texture, int verticesNeeded) {
    const int slot = (texture >= 0 && texture < kTexCount) ? texture + 1 : 0;
    Bucket& b = m_buckets[slot];
    if (static_cast<int>(b.vertices.size()) + verticesNeeded > gfx::kMaxMeshVertices) {
        emitBucket(slot);
    }
    return b;
}

void SceneBuilder::emitBucket(int slot) {
    Bucket& b = m_buckets[slot];
    if (b.indices.empty()) return;

    Vec3 lo = {b.vertices[0].x, b.vertices[0].y, b.vertices[0].z};
    Vec3 hi = lo;
    for (const gfx::Vertex& v : b.vertices) {
        if (v.x < lo.x) lo.x = v.x;
        if (v.y < lo.y) lo.y = v.y;
        if (v.z < lo.z) lo.z = v.z;
        if (v.x > hi.x) hi.x = v.x;
        if (v.y > hi.y) hi.y = v.y;
        if (v.z > hi.z) hi.z = v.z;
    }

    StaticBatch batch;
    batch.texture = slot - 1;
    batch.center = (lo + hi) * 0.5f;
    batch.radius = math3d::length(hi - batch.center);
    batch.triangles = static_cast<int>(b.indices.size() / 3);
    if (gfx::createMesh(batch.mesh, b.vertices.data(), static_cast<int>(b.vertices.size()),
                        b.indices.data(), static_cast<int>(b.indices.size()))) {
        m_pending.push_back(batch);
    } else {
        m_failed = true;
    }
    // Libera la memoria temporal (clear() no la devuelve).
    std::vector<gfx::Vertex>().swap(b.vertices);
    std::vector<u16>().swap(b.indices);
}

void SceneBuilder::flushGroup(std::vector<StaticBatch>& out) {
    for (int slot = 0; slot <= kTexCount; ++slot) emitBucket(slot);
    out.insert(out.end(), m_pending.begin(), m_pending.end());
    m_pending.clear();
}

void SceneBuilder::quad(const Material& m, const Vec3& q0, const Vec3& q1, const Vec3& q2,
                        const Vec3& q3) {
    const Vec3 p0 = m_hasTransform ? m_transform.transformPoint(q0) : q0;
    const Vec3 p1 = m_hasTransform ? m_transform.transformPoint(q1) : q1;
    const Vec3 p2 = m_hasTransform ? m_transform.transformPoint(q2) : q2;
    const Vec3 p3 = m_hasTransform ? m_transform.transformPoint(q3) : q3;

    const Vec3 normal = math3d::normalize(math3d::cross(p1 - p0, p3 - p0));
    const u32 color = shade(m.color, normal);

    const float lenU = fmaxf(math3d::length(p1 - p0), math3d::length(p2 - p3));
    const float lenV = fmaxf(math3d::length(p3 - p0), math3d::length(p2 - p1));
    const int nu = segmentsFor(lenU, m_maxSegment);
    const int nv = segmentsFor(lenV, m_maxSegment);
    const float uScale = lenU / m.metersPerRepeat;
    const float vScale = lenV / m.metersPerRepeat;

    Bucket& b = bucketFor(m.texture, (nu + 1) * (nv + 1));
    const int base = static_cast<int>(b.vertices.size());
    for (int j = 0; j <= nv; ++j) {
        const float t = static_cast<float>(j) / nv;
        for (int i = 0; i <= nu; ++i) {
            const float s = static_cast<float>(i) / nu;
            const Vec3 p = math3d::lerp(math3d::lerp(p0, p1, s), math3d::lerp(p3, p2, s), t);
            b.vertices.push_back({s * uScale, (1.0f - t) * vScale, color, p.x, p.y, p.z});
        }
    }
    for (int j = 0; j < nv; ++j) {
        for (int i = 0; i < nu; ++i) {
            const u16 a = static_cast<u16>(base + j * (nu + 1) + i);
            const u16 bb = static_cast<u16>(a + 1);
            const u16 c = static_cast<u16>(a + (nu + 1) + 1);
            const u16 d = static_cast<u16>(a + (nu + 1));
            b.indices.insert(b.indices.end(), {a, bb, c, a, c, d});
        }
    }
}

void SceneBuilder::triangle(const Material& m, const Vec3& ta, const Vec3& tb, const Vec3& tc) {
    const Vec3 a = m_hasTransform ? m_transform.transformPoint(ta) : ta;
    const Vec3 b = m_hasTransform ? m_transform.transformPoint(tb) : tb;
    const Vec3 c = m_hasTransform ? m_transform.transformPoint(tc) : tc;

    const Vec3 normal = math3d::normalize(math3d::cross(b - a, c - a));
    const u32 color = shade(m.color, normal);
    // Ejes de textura en el plano del triangulo: U a lo largo de a->b, V hacia "abajo".
    const Vec3 uAxis = math3d::normalize(b - a);
    const Vec3 vAxis = math3d::cross(normal, uAxis);

    const float maxEdge =
        fmaxf(math3d::length(b - a), fmaxf(math3d::length(c - b), math3d::length(a - c)));
    const int n = segmentsFor(maxEdge, m_maxSegment);

    Bucket& bucket = bucketFor(m.texture, (n + 1) * (n + 2) / 2);
    const int base = static_cast<int>(bucket.vertices.size());
    // Rejilla baricentrica: fila i (hacia c) con n - i + 1 vertices (hacia b).
    for (int i = 0; i <= n; ++i) {
        for (int j = 0; j <= n - i; ++j) {
            const Vec3 p = a + (b - a) * (static_cast<float>(j) / n) +
                           (c - a) * (static_cast<float>(i) / n);
            const Vec3 rel = p - a;
            bucket.vertices.push_back({math3d::dot(rel, uAxis) / m.metersPerRepeat,
                                       -math3d::dot(rel, vAxis) / m.metersPerRepeat, color, p.x,
                                       p.y, p.z});
        }
    }
    auto index = [base, n](int i, int j) {
        // Inicio de la fila i = suma de (n - k + 1) para k < i.
        return static_cast<u16>(base + i * (n + 1) - (i * (i - 1)) / 2 + j);
    };
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n - i; ++j) {
            bucket.indices.insert(bucket.indices.end(),
                                  {index(i, j), index(i, j + 1), index(i + 1, j)});
            if (j < n - i - 1) {
                bucket.indices.insert(bucket.indices.end(),
                                      {index(i, j + 1), index(i + 1, j + 1), index(i + 1, j)});
            }
        }
    }
}

void SceneBuilder::box(const Material& sides, const Material& top, const Vec3& mn,
                       const Vec3& mx, bool bottom) {
    // Sur (+Z)
    quad(sides, {mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z}, {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z});
    // Norte (-Z)
    quad(sides, {mx.x, mn.y, mn.z}, {mn.x, mn.y, mn.z}, {mn.x, mx.y, mn.z}, {mx.x, mx.y, mn.z});
    // Este (+X)
    quad(sides, {mx.x, mn.y, mx.z}, {mx.x, mn.y, mn.z}, {mx.x, mx.y, mn.z}, {mx.x, mx.y, mx.z});
    // Oeste (-X)
    quad(sides, {mn.x, mn.y, mn.z}, {mn.x, mn.y, mx.z}, {mn.x, mx.y, mx.z}, {mn.x, mx.y, mn.z});
    // Techo (+Y)
    quad(top, {mn.x, mx.y, mx.z}, {mx.x, mx.y, mx.z}, {mx.x, mx.y, mn.z}, {mn.x, mx.y, mn.z});
    if (bottom) {
        quad(sides, {mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z}, {mx.x, mn.y, mx.z}, {mn.x, mn.y, mx.z});
    }
}

void SceneBuilder::cylinder(const Material& side, const Material& cap, const Vec3& base,
                            float radius, float height, int segments, bool capTop) {
    const Vec3 up{0.0f, height, 0.0f};
    const Vec3 topCenter = base + up;
    for (int i = 0; i < segments; ++i) {
        // Angulo hacia -Z para que el orden sea antihorario visto desde fuera.
        const float a0 = 2.0f * math3d::kPi * i / segments;
        const float a1 = 2.0f * math3d::kPi * (i + 1) / segments;
        const Vec3 p0 = base + Vec3{cosf(a0) * radius, 0.0f, -sinf(a0) * radius};
        const Vec3 p1 = base + Vec3{cosf(a1) * radius, 0.0f, -sinf(a1) * radius};
        quad(side, p0, p1, p1 + up, p0 + up);
        if (capTop) triangle(cap, topCenter, p0 + up, p1 + up);
    }
}

void SceneBuilder::cone(const Material& m, const Vec3& base, const float radius, float height,
                        int segments) {
    const Vec3 apex = base + Vec3{0.0f, height, 0.0f};
    for (int i = 0; i < segments; ++i) {
        const float a0 = 2.0f * math3d::kPi * i / segments;
        const float a1 = 2.0f * math3d::kPi * (i + 1) / segments;
        const Vec3 p0 = base + Vec3{cosf(a0) * radius, 0.0f, -sinf(a0) * radius};
        const Vec3 p1 = base + Vec3{cosf(a1) * radius, 0.0f, -sinf(a1) * radius};
        triangle(m, p0, p1, apex);
    }
}

void SceneBuilder::groundTile(const Material& m, float x0, float z0, float size,
                              float brightness) {
    const float x1 = x0 + size, z1 = z0 + size;
    const u32 color = scaleColor(shade(m.color, {0.0f, 1.0f, 0.0f}), brightness);
    const float k = 1.0f / m.metersPerRepeat;

    Bucket& b = bucketFor(m.texture, 4);
    const u16 base = static_cast<u16>(b.vertices.size());
    b.vertices.push_back({x0 * k, z1 * k, color, x0, kGroundY, z1});
    b.vertices.push_back({x1 * k, z1 * k, color, x1, kGroundY, z1});
    b.vertices.push_back({x1 * k, z0 * k, color, x1, kGroundY, z0});
    b.vertices.push_back({x0 * k, z0 * k, color, x0, kGroundY, z0});
    b.indices.insert(b.indices.end(), {base, static_cast<u16>(base + 1), static_cast<u16>(base + 2),
                                       base, static_cast<u16>(base + 2), static_cast<u16>(base + 3)});
}

}  // namespace world
