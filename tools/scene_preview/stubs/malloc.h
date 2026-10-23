// Stub de host para las herramientas de prueba: memalign() y mallinfo() no existen en
// macOS. mallinfo().uordblks se calcula con las estadisticas de la zona de malloc.
#pragma once
#include <malloc/malloc.h>
#include <stdlib.h>
static inline void* memalign(size_t alignment, size_t size) {
    void* p = 0;
    return posix_memalign(&p, alignment < sizeof(void*) ? sizeof(void*) : alignment, size) == 0 ? p : 0;
}
struct mallinfo {
    int arena, ordblks, smblks, hblks, hblkhd, usmblks, fsmblks, uordblks, fordblks, keepcost;
};
static inline struct mallinfo mallinfo(void) {
    malloc_statistics_t stats;
    malloc_zone_statistics(0, &stats);
    struct mallinfo m = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    m.uordblks = (int)stats.size_in_use;
    return m;
}
