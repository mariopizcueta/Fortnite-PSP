#include "gfx/mesh.h"

#include <malloc.h>
#include <string.h>

#include <pspkernel.h>

namespace gfx {

bool createMesh(Mesh& out, const Vertex* vertices, int vertexCount, const u16* indices,
                int indexCount) {
    if (vertexCount <= 0 || vertexCount > kMaxMeshVertices || indexCount <= 0) return false;

    const size_t vbytes = sizeof(Vertex) * vertexCount;
    const size_t ibytes = sizeof(u16) * indexCount;
    Mesh mesh;
    mesh.vertices = static_cast<Vertex*>(memalign(16, vbytes));
    mesh.indices = static_cast<u16*>(memalign(16, ibytes));
    if (!mesh.vertices || !mesh.indices) {
        destroyMesh(mesh);
        return false;
    }
    memcpy(mesh.vertices, vertices, vbytes);
    memcpy(mesh.indices, indices, ibytes);
    mesh.vertexCount = vertexCount;
    mesh.indexCount = indexCount;

    sceKernelDcacheWritebackRange(mesh.vertices, vbytes);
    sceKernelDcacheWritebackRange(mesh.indices, ibytes);
    out = mesh;
    return true;
}

void destroyMesh(Mesh& mesh) {
    free(mesh.vertices);
    free(mesh.indices);
    mesh = Mesh{};
}

}  // namespace gfx
