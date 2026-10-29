// Construccion de geometria low-poly estatica.
//
// Todo se genera UNA vez al cargar: las primitivas se acumulan en buffers por textura
// y flushGroup() las convierte en lotes (StaticBatch) con mallas indexadas en RAM y
// una esfera envolvente para el descarte. Despues no se crea geometria por frame.
//
// Convenciones:
//  - quad(p0, p1, p2, p3): p0 abajo-izquierda, p1 abajo-derecha, p2 arriba-derecha,
//    p3 arriba-izquierda vistos desde el lado visible (orden antihorario). La normal
//    (p1 - p0) x (p3 - p0) apunta hacia el observador y se usa para la iluminacion.
//  - La iluminacion es un sol direccional horneado en el color de vertice, con un
//    tono plano por cara (look flat-shaded de PS1).
#pragma once

#include <vector>

#include "gfx/mesh.h"
#include "math/math3d.h"
#include "world/textures.h"

namespace world {

struct Material {
    int texture = kTexNone;
    u32 color = 0xFFFFFFFF;        // tinte (0xAABBGGRR); con textura se multiplica
    float metersPerRepeat = 4.0f;  // metros que ocupa una repeticion de la textura
};

struct StaticBatch {
    gfx::Mesh mesh;
    int texture = kTexNone;
    math3d::Vec3 center;
    float radius = 0.0f;
    int triangles = 0;
    short object = -1;  // objeto destruible al que pertenece (World::harvestables()), o -1
};

class SceneBuilder {
public:
    explicit SceneBuilder(float maxSegment);

    // Transformacion aplicada a las primitivas siguientes (para colocar y orientar).
    void setTransform(const math3d::Mat4& transform);
    void resetTransform();

    void quad(const Material& m, const math3d::Vec3& p0, const math3d::Vec3& p1,
              const math3d::Vec3& p2, const math3d::Vec3& p3);
    // Triangulo antihorario visto desde el lado visible.
    void triangle(const Material& m, const math3d::Vec3& a, const math3d::Vec3& b,
                  const math3d::Vec3& c);
    // Caja alineada a los ejes (en el espacio de la transformacion actual). Sin cara
    // inferior salvo que se pida: casi todo apoya en el suelo.
    void box(const Material& sides, const Material& top, const math3d::Vec3& min,
             const math3d::Vec3& max, bool bottom = false);
    // Cilindro vertical de `segments` lados con base en `base`.
    void cylinder(const Material& side, const Material& cap, const math3d::Vec3& base,
                  float radius, float height, int segments, bool capTop = true);
    // Cono / piramide vertical.
    void cone(const Material& m, const math3d::Vec3& base, float radius, float height,
              int segments);
    // Losa horizontal de suelo con UV en coordenadas de mundo (continua entre losas). `y`:
    // altura (el suelo; la superficie del agua va algo mas arriba).
    void groundTile(const Material& m, float x0, float z0, float size, float brightness, float y = 0.0f);

    // Cierra el grupo actual y anade sus lotes (uno por textura) a `out`.
    void flushGroup(std::vector<StaticBatch>& out);

    bool failed() const { return m_failed; }

private:
    struct Bucket {
        std::vector<gfx::Vertex> vertices;
        std::vector<u16> indices;
    };

    Bucket& bucketFor(int texture, int verticesNeeded);
    void emitBucket(int slot);
    u32 shade(u32 color, const math3d::Vec3& normal) const;

    float m_maxSegment;
    math3d::Mat4 m_transform;
    bool m_hasTransform = false;
    bool m_failed = false;
    Bucket m_buckets[kTexCount + 1];  // ranura 0 = sin textura
    std::vector<StaticBatch> m_pending;
};

}  // namespace world
