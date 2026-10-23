#include "world/collision.h"

#include <math.h>

namespace world {

using math3d::Vec3;

bool segmentVsAabb(const Vec3& p, const Vec3& d, const Aabb& box, float* tEnter) {
    float tmin = 0.0f, tmax = 1.0f;
    const float origin[3] = {p.x, p.y, p.z};
    const float dir[3] = {d.x, d.y, d.z};
    const float lo[3] = {box.min.x, box.min.y, box.min.z};
    const float hi[3] = {box.max.x, box.max.y, box.max.z};
    for (int a = 0; a < 3; ++a) {
        if (fabsf(dir[a]) < 1e-8f) {
            if (origin[a] < lo[a] || origin[a] > hi[a]) return false;
            continue;
        }
        const float inv = 1.0f / dir[a];
        float t0 = (lo[a] - origin[a]) * inv;
        float t1 = (hi[a] - origin[a]) * inv;
        if (t0 > t1) {
            const float tmp = t0;
            t0 = t1;
            t1 = tmp;
        }
        if (t0 > tmin) tmin = t0;
        if (t1 < tmax) tmax = t1;
        if (tmin > tmax) return false;
    }
    *tEnter = tmin;
    return true;
}

int CollisionWorld::addDynamic(const Aabb& box, u8 flags) {
    for (int i = 0; i < kMaxDynamic; ++i) {
        if (m_dynamic[i].active) continue;
        m_dynamic[i].collider = {box, flags};
        m_dynamic[i].active = true;
        if (i >= m_dynamicEnd) m_dynamicEnd = i + 1;
        return i;
    }
    return -1;
}

void CollisionWorld::removeDynamic(int handle) {
    if (handle >= 0 && handle < kMaxDynamic) m_dynamic[handle].active = false;
    while (m_dynamicEnd > 0 && !m_dynamic[m_dynamicEnd - 1].active) --m_dynamicEnd;
}

void CollisionWorld::clearDynamic() {
    for (DynamicSlot& d : m_dynamic) d.active = false;
    m_dynamicEnd = 0;
}

int CollisionWorld::dynamicCount() const {
    int n = 0;
    for (const DynamicSlot& d : m_dynamic) n += d.active ? 1 : 0;
    return n;
}

void CollisionWorld::clear() {
    clearDynamic();
    m_colliders.clear();
    m_baseFlags.clear();
    m_cellStart.clear();
    m_cellItems.clear();
    m_stamp.clear();
    m_cells = 0;
}

void CollisionWorld::add(const Aabb& box, u8 flags) { m_colliders.push_back({box, flags}); }

void CollisionWorld::cellRange(float minX, float minZ, float maxX, float maxZ, int* x0, int* z0,
                               int* x1, int* z1) const {
    auto cell = [this](float v) {
        int c = static_cast<int>(floorf((v - m_origin) / kCellSize));
        return c < 0 ? 0 : (c >= m_cells ? m_cells - 1 : c);
    };
    *x0 = cell(minX);
    *z0 = cell(minZ);
    *x1 = cell(maxX);
    *z1 = cell(maxZ);
}

void CollisionWorld::build(float halfExtent) {
    m_origin = -halfExtent;
    m_cells = static_cast<int>(ceilf(2.0f * halfExtent / kCellSize));
    const int total = m_cells * m_cells;

    // Dos pasadas: contar por celda y luego rellenar (CSR, sin listas dinamicas).
    std::vector<int> counts(total + 1, 0);
    for (const Collider& c : m_colliders) {
        int x0, z0, x1, z1;
        cellRange(c.box.min.x, c.box.min.z, c.box.max.x, c.box.max.z, &x0, &z0, &x1, &z1);
        for (int cz = z0; cz <= z1; ++cz)
            for (int cx = x0; cx <= x1; ++cx) ++counts[cz * m_cells + cx];
    }
    m_cellStart.assign(total + 1, 0);
    for (int i = 0; i < total; ++i) m_cellStart[i + 1] = m_cellStart[i] + counts[i];
    m_cellItems.assign(m_cellStart[total], 0);
    std::vector<int> fill(m_cellStart.begin(), m_cellStart.end() - 1);
    for (int i = 0; i < count(); ++i) {
        const Collider& c = m_colliders[i];
        int x0, z0, x1, z1;
        cellRange(c.box.min.x, c.box.min.z, c.box.max.x, c.box.max.z, &x0, &z0, &x1, &z1);
        for (int cz = z0; cz <= z1; ++cz)
            for (int cx = x0; cx <= x1; ++cx) m_cellItems[fill[cz * m_cells + cx]++] = static_cast<u16>(i);
    }
    m_stamp.assign(m_colliders.size(), 0);
    m_stampCounter = 0;
    m_baseFlags.resize(m_colliders.size());
    for (size_t i = 0; i < m_colliders.size(); ++i) m_baseFlags[i] = m_colliders[i].flags;
}

void CollisionWorld::setStaticEnabled(int i, bool enabled) {
    if (i < 0 || i >= count()) return;
    m_colliders[i].flags = enabled ? m_baseFlags[i] : 0;
}

void CollisionWorld::enableAllStatic() {
    for (size_t i = 0; i < m_colliders.size(); ++i) m_colliders[i].flags = m_baseFlags[i];
}

u32 CollisionWorld::nextStamp() const {
    if (++m_stampCounter == 0) {
        // Desbordamiento (tras 4.000 millones de consultas): se reinician las marcas.
        for (u32& s : m_stamp) s = 0;
        m_stampCounter = 1;
    }
    return m_stampCounter;
}

bool CollisionWorld::overlapsAny(const Aabb& box, u8 mask) const {
    bool found = false;
    query(box, mask, [&found](const Collider&) { found = true; });
    return found;
}

bool CollisionWorld::overlapsAnyStatic(const Aabb& box, u8 mask) const {
    bool found = false;
    query(box, mask, [&found](const Collider&) { found = true; }, false);
    return found;
}

bool CollisionWorld::raycast(const Vec3& from, const Vec3& to, u8 mask, RayHit* hit) const {
    const Vec3 d = to - from;
    float best = 2.0f;
    int bestIdx = -1;
    int bestDynamic = -1;
    // Primero las cajas dinamicas (pocas): su impacto tambien acota el recorrido de celdas.
    for (int i = 0; i < m_dynamicEnd; ++i) {
        const DynamicSlot& slot = m_dynamic[i];
        if (!slot.active || !(slot.collider.flags & mask)) continue;
        float t;
        if (segmentVsAabb(from, d, slot.collider.box, &t) && t < best) {
            best = t;
            bestDynamic = i;
        }
    }
    if (m_cells == 0) {
        if (bestDynamic < 0) return false;
        if (hit) *hit = {best, from + d * best, -1, bestDynamic};
        return true;
    }
    const u32 stamp = nextStamp();

    // Recorrido DDA de las celdas que cruza el segmento en XZ (Amanatides-Woo).
    const float fx = (from.x - m_origin) / kCellSize;
    const float fz = (from.z - m_origin) / kCellSize;
    const float dx = d.x / kCellSize;
    const float dz = d.z / kCellSize;
    int ix = static_cast<int>(floorf(fx));
    int iz = static_cast<int>(floorf(fz));
    const int stepX = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
    const int stepZ = dz > 0 ? 1 : (dz < 0 ? -1 : 0);
    const float inf = 1e30f;
    float tMaxX = stepX ? ((stepX > 0 ? (ix + 1 - fx) : (fx - ix)) / fabsf(dx)) : inf;
    float tMaxZ = stepZ ? ((stepZ > 0 ? (iz + 1 - fz) : (fz - iz)) / fabsf(dz)) : inf;
    const float tDeltaX = stepX ? 1.0f / fabsf(dx) : inf;
    const float tDeltaZ = stepZ ? 1.0f / fabsf(dz) : inf;

    for (int guard = 0; guard < 4 * m_cells + 4; ++guard) {
        if (ix >= 0 && iz >= 0 && ix < m_cells && iz < m_cells) {
            const int cell = iz * m_cells + ix;
            for (int k = m_cellStart[cell]; k < m_cellStart[cell + 1]; ++k) {
                const int idx = m_cellItems[k];
                if (m_stamp[idx] == stamp) continue;
                m_stamp[idx] = stamp;
                const Collider& c = m_colliders[idx];
                if (!(c.flags & mask)) continue;
                float t;
                if (segmentVsAabb(from, d, c.box, &t) && t < best) {
                    best = t;
                    bestIdx = idx;
                    bestDynamic = -1;
                }
            }
        }
        const float tNext = tMaxX < tMaxZ ? tMaxX : tMaxZ;
        // Un impacto anterior a la siguiente celda ya no puede ser superado.
        if (tNext > 1.0f || best <= tNext) break;
        if (tMaxX < tMaxZ) {
            ix += stepX;
            tMaxX += tDeltaX;
        } else {
            iz += stepZ;
            tMaxZ += tDeltaZ;
        }
    }

    if (bestIdx < 0 && bestDynamic < 0) return false;
    if (hit) {
        hit->t = best;
        hit->point = from + d * best;
        hit->collider = bestDynamic >= 0 ? -1 : bestIdx;
        hit->dynamicHandle = bestDynamic;
    }
    return true;
}

float CollisionWorld::supportHeight(float x, float z, float r, float maxTop) const {
    float support = 0.0f;  // el suelo del mapa
    const Aabb foot{{x - r, -1.0f, z - r}, {x + r, maxTop + 0.001f, z + r}};
    query(foot, kBlocksMovement, [&](const Collider& c) {
        if (c.box.max.y <= maxTop && c.box.max.y > support) support = c.box.max.y;
    });
    return support;
}

float CollisionWorld::ceilingHeight(float x, float z, float r, float minBottom) const {
    float ceiling = 1e9f;
    const Aabb column{{x - r, minBottom, z - r}, {x + r, 1e6f, z + r}};
    query(column, kBlocksMovement, [&](const Collider& c) {
        if (c.box.min.y >= minBottom && c.box.min.y < ceiling) ceiling = c.box.min.y;
    });
    return ceiling;
}

}  // namespace world
