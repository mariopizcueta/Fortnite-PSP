// Stub de host para tools/scene_preview: memalign() no existe en macOS.
#pragma once
#include <stdlib.h>
static inline void* memalign(size_t alignment, size_t size) {
    void* p = 0;
    return posix_memalign(&p, alignment < sizeof(void*) ? sizeof(void*) : alignment, size) == 0 ? p : 0;
}
