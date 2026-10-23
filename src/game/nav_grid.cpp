#include "game/nav_grid.h"

#include <math.h>
#include <stdlib.h>

#include "game/character.h"

namespace game {

using math3d::Vec3;

namespace {

const int kDx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
const int kDz[8] = {0, 0, 1, -1, 1, -1, 1, -1};
const float kCost[8] = {1, 1, 1, 1, 1.41421f, 1.41421f, 1.41421f, 1.41421f};

// Caja del agente: desde por encima de un escalon hasta la cabeza.
world::Aabb agentBox(const Vec3& p, float r) {
    return {{p.x - r, tuning::kStepHeight, p.z - r}, {p.x + r, tuning::kBodyHeight, p.z + r}};
}

}  // namespace

bool NavGrid::clearWalk(const world::CollisionWorld& world, const Vec3& a, const Vec3& b,
                        float radius) {
    const Vec3 d = b - a;
    const float len = sqrtf(d.x * d.x + d.z * d.z);
    const int samples = static_cast<int>(len / 0.5f) + 1;
    for (int i = 0; i <= samples; ++i) {
        const Vec3 p = a + d * (static_cast<float>(i) / samples);
        if (world.overlapsAny(agentBox(p, radius), world::kBlocksMovement)) return false;
    }
    return true;
}

bool NavGrid::computeWalkable(const world::CollisionWorld& world, int ix, int iz) const {
    const Vec3 c = center(ix, iz);
    // Transitable: cabe el agente y el apoyo esta a ras de suelo (no encima de una caja:
    // los bots no planifican subirse a obstaculos).
    const bool free = !world.overlapsAny(agentBox(c, kAgentRadius), world::kBlocksMovement);
    const bool ground =
        world.supportHeight(c.x, c.z, tuning::kFootRadius, tuning::kStepHeight) < tuning::kStepHeight;
    return free && ground;
}

u8 NavGrid::computeLinks(const world::CollisionWorld& world, int ix, int iz) const {
    if (!walkable(ix, iz)) return 0;
    u8 links = 0;
    for (int d = 0; d < 8; ++d) {
        const int nx = ix + kDx[d], nz = iz + kDz[d];
        if (!walkable(nx, nz)) continue;
        if (d >= 4 && (!walkable(ix + kDx[d], iz) || !walkable(ix, iz + kDz[d]))) continue;
        if (clearWalk(world, center(ix, iz), center(nx, nz))) links |= static_cast<u8>(1 << d);
    }
    return links;
}

void NavGrid::updateRegion(const world::CollisionWorld& world, const world::Aabb& changed) {
    if (m_size == 0) return;
    // Nodos cuya caja de agente o cuyas conexiones pueden tocar la zona cambiada.
    const float margin = kAgentRadius + kCellSize;
    int x0, z0, x1, z1;
    cellOf({changed.min.x - margin, 0.0f, changed.min.z - margin}, &x0, &z0);
    cellOf({changed.max.x + margin, 0.0f, changed.max.z + margin}, &x1, &z1);
    auto clampCell = [this](int v) { return v < 0 ? 0 : (v >= m_size ? m_size - 1 : v); };
    x0 = clampCell(x0);
    z0 = clampCell(z0);
    x1 = clampCell(x1);
    z1 = clampCell(z1);

    for (int iz = z0; iz <= z1; ++iz) {
        for (int ix = x0; ix <= x1; ++ix) {
            const u8 before = m_walkable[index(ix, iz)];
            const u8 now = computeWalkable(world, ix, iz) ? 1 : 0;
            m_walkable[index(ix, iz)] = now;
            m_walkableCount += static_cast<int>(now) - static_cast<int>(before);
        }
    }
    // Conexiones de la zona y de su anillo exterior, en las dos direcciones.
    for (int iz = clampCell(z0 - 1); iz <= clampCell(z1 + 1); ++iz) {
        for (int ix = clampCell(x0 - 1); ix <= clampCell(x1 + 1); ++ix) {
            m_links[index(ix, iz)] = computeLinks(world, ix, iz);
        }
    }
}

void NavGrid::build(const world::CollisionWorld& world, float halfExtent) {
    m_origin = -halfExtent;
    m_size = static_cast<int>(2.0f * halfExtent / kCellSize);
    const int total = m_size * m_size;
    m_walkable.assign(total, 0);
    m_links.assign(total, 0);
    m_walkableCount = 0;

    for (int iz = 0; iz < m_size; ++iz) {
        for (int ix = 0; ix < m_size; ++ix) {
            if (computeWalkable(world, ix, iz)) {
                m_walkable[index(ix, iz)] = 1;
                ++m_walkableCount;
            }
        }
    }
    // Las conexiones son simetricas: se comprueba cada pareja una sola vez (direcciones
    // 0, 2, 4 y 5) y se marca tambien la inversa. Es lo mas caro de la carga.
    const int kForward[4] = {0, 2, 4, 5};
    const int kReverse[8] = {1, 0, 3, 2, 7, 6, 5, 4};
    for (int iz = 0; iz < m_size; ++iz) {
        for (int ix = 0; ix < m_size; ++ix) {
            if (!walkable(ix, iz)) continue;
            for (int d : kForward) {
                const int nx = ix + kDx[d], nz = iz + kDz[d];
                if (!walkable(nx, nz)) continue;
                // Diagonal solo si las dos ortogonales tambien lo son (no cortar esquinas).
                if (d >= 4 && (!walkable(ix + kDx[d], iz) || !walkable(ix, iz + kDz[d]))) continue;
                if (clearWalk(world, center(ix, iz), center(nx, nz))) {
                    m_links[index(ix, iz)] |= static_cast<u8>(1 << d);
                    m_links[index(nx, nz)] |= static_cast<u8>(1 << kReverse[d]);
                }
            }
        }
    }

    m_g.assign(total, 0.0f);
    m_parent.assign(total, -1);
    m_visitStamp.assign(total, 0);
    m_closed.assign(total, 0);
    m_heap.clear();
    m_heap.reserve(total);
    m_stamp = 0;
}

bool NavGrid::walkable(int ix, int iz) const {
    return ix >= 0 && iz >= 0 && ix < m_size && iz < m_size && m_walkable[index(ix, iz)];
}

Vec3 NavGrid::center(int ix, int iz) const {
    return {m_origin + (ix + 0.5f) * kCellSize, 0.0f, m_origin + (iz + 0.5f) * kCellSize};
}

bool NavGrid::cellOf(const Vec3& p, int* ix, int* iz) const {
    *ix = static_cast<int>(floorf((p.x - m_origin) / kCellSize));
    *iz = static_cast<int>(floorf((p.z - m_origin) / kCellSize));
    return *ix >= 0 && *iz >= 0 && *ix < m_size && *iz < m_size;
}

bool NavGrid::nearestNode(const Vec3& p, int maxRing, int* outX, int* outZ) const {
    int cx, cz;
    cellOf(p, &cx, &cz);
    float best = 1e30f;
    bool found = false;
    for (int ring = 0; ring <= maxRing && !found; ++ring) {
        for (int iz = cz - ring; iz <= cz + ring; ++iz) {
            for (int ix = cx - ring; ix <= cx + ring; ++ix) {
                if (ring > 0 && ix != cx - ring && ix != cx + ring && iz != cz - ring && iz != cz + ring) continue;
                if (!walkable(ix, iz)) continue;
                const Vec3 c = center(ix, iz);
                const float d = (c.x - p.x) * (c.x - p.x) + (c.z - p.z) * (c.z - p.z);
                if (d < best) {
                    best = d;
                    *outX = ix;
                    *outZ = iz;
                    found = true;
                }
            }
        }
    }
    return found;
}

bool NavGrid::randomPointNear(const Vec3& c, float radius, core::Random& rng, Vec3* out) const {
    for (int attempt = 0; attempt < 12; ++attempt) {
        const float a = rng.range(0.0f, 2.0f * math3d::kPi);
        const float r = radius * sqrtf(rng.uniform());
        const Vec3 p{c.x + cosf(a) * r, 0.0f, c.z + sinf(a) * r};
        int ix, iz;
        if (nearestNode(p, 2, &ix, &iz)) {
            *out = center(ix, iz);
            return true;
        }
    }
    return false;
}

int NavGrid::findPath(const Vec3& from, const Vec3& to, Vec3* out, int maxPoints, int* expansions) {
    int sx, sz, gx, gz;
    if (expansions) *expansions = 0;
    if (!nearestNode(from, 2, &sx, &sz) || !nearestNode(to, 2, &gx, &gz)) return 0;
    const int start = index(sx, sz), goal = index(gx, gz);
    if (start == goal) {
        out[0] = center(gx, gz);
        return 1;
    }

    if (++m_stamp == 0) {
        for (u32& s : m_visitStamp) s = 0;
        m_stamp = 1;
    }
    auto heuristic = [&](int node) {
        const int dx = abs(node % m_size - gx), dz = abs(node / m_size - gz);
        return (dx + dz) + (1.41421f - 2.0f) * static_cast<float>(dx < dz ? dx : dz);  // octil
    };
    // Monticulo binario de minimos por f = g + h.
    m_heap.clear();
    auto f = [&](int node) { return m_g[node] + heuristic(node); };
    auto push = [&](int node) {
        m_heap.push_back(node);
        int i = static_cast<int>(m_heap.size()) - 1;
        while (i > 0) {
            const int p = (i - 1) / 2;
            if (f(m_heap[p]) <= f(m_heap[i])) break;
            const int tmp = m_heap[p];
            m_heap[p] = m_heap[i];
            m_heap[i] = tmp;
            i = p;
        }
    };
    auto pop = [&]() {
        const int top = m_heap[0];
        m_heap[0] = m_heap.back();
        m_heap.pop_back();
        int i = 0;
        const int n = static_cast<int>(m_heap.size());
        for (;;) {
            const int l = 2 * i + 1, r = l + 1;
            int m = i;
            if (l < n && f(m_heap[l]) < f(m_heap[m])) m = l;
            if (r < n && f(m_heap[r]) < f(m_heap[m])) m = r;
            if (m == i) break;
            const int tmp = m_heap[m];
            m_heap[m] = m_heap[i];
            m_heap[i] = tmp;
            i = m;
        }
        return top;
    };

    m_visitStamp[start] = m_stamp;
    m_g[start] = 0.0f;
    m_parent[start] = -1;
    m_closed[start] = 0;
    push(start);
    int expanded = 0;
    bool reached = false;
    while (!m_heap.empty()) {
        const int node = pop();
        if (m_closed[node]) continue;
        m_closed[node] = 1;
        ++expanded;
        if (node == goal) {
            reached = true;
            break;
        }
        const int nx0 = node % m_size, nz0 = node / m_size;
        const u8 links = m_links[node];
        for (int d = 0; d < 8; ++d) {
            if (!(links & (1 << d))) continue;
            const int next = index(nx0 + kDx[d], nz0 + kDz[d]);
            const float g = m_g[node] + kCost[d];
            if (m_visitStamp[next] != m_stamp) {
                m_visitStamp[next] = m_stamp;
                m_closed[next] = 0;
                m_g[next] = g;
                m_parent[next] = node;
                push(next);
            } else if (!m_closed[next] && g < m_g[next]) {
                m_g[next] = g;
                m_parent[next] = node;
                push(next);  // entrada duplicada: la antigua se ignora al salir (closed)
            }
        }
    }
    if (expansions) *expansions = expanded;
    if (!reached) return 0;

    // Reconstruye al reves y da la vuelta; si es mas largo que maxPoints, se queda con el
    // tramo inicial (el bot volvera a pedir camino al llegar).
    int count = 0;
    for (int n = goal; n != start && n >= 0; n = m_parent[n]) ++count;
    int skip = count > maxPoints ? count - maxPoints : 0;
    int written = count - skip;
    int i = written - 1;
    for (int n = goal; n != start && n >= 0; n = m_parent[n]) {
        if (skip > 0) {
            --skip;
            continue;
        }
        out[i--] = center(n % m_size, n / m_size);
    }
    return written;
}

}  // namespace game
