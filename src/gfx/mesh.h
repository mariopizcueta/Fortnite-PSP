// Mallas estaticas indexadas en RAM, listas para sceGuDrawArray.
#pragma once

#include <pspgu.h>
#include <psptypes.h>

namespace gfx {

// Orden de componentes que exige la GE: textura, color, (normal), posicion.
struct Vertex {
    float u, v;
    u32 color;  // 0xAABBGGRR, con la iluminacion ya horneada
    float x, y, z;
};
static_assert(sizeof(Vertex) == 24, "Vertex debe coincidir con kMeshVertexFormat");

constexpr int kMeshVertexFormat = GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF |
                                  GU_INDEX_16BIT | GU_TRANSFORM_3D;
// Los indices son de 16 bits.
constexpr int kMaxMeshVertices = 65535;

struct Mesh {
    Vertex* vertices = nullptr;
    u16* indices = nullptr;
    int vertexCount = 0;
    int indexCount = 0;
};

// Copia vertices e indices a buffers propios alineados y vuelca la cache para la GE.
// Se crea una sola vez; despues se dibuja cada frame sin regenerar nada.
bool createMesh(Mesh& out, const Vertex* vertices, int vertexCount, const u16* indices,
                int indexCount);
void destroyMesh(Mesh& mesh);

}  // namespace gfx
