#include "platform/log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <pspthreadman.h>

namespace platform {
namespace log {
namespace {

FILE* g_file = nullptr;
constexpr const char* kFileName = "fortnitepsp_log.txt";

}  // namespace

void open(const char* argv0) {
    char path[256];
    path[0] = '\0';
    if (argv0) {
        // "ms0:/PSP/GAME/FortnitePSP/EBOOT.PBP" -> "ms0:/PSP/GAME/FortnitePSP/"
        strncpy(path, argv0, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
        char* slash = strrchr(path, '/');
        if (slash) slash[1] = '\0';
        else path[0] = '\0';
    }
    strncat(path, kFileName, sizeof(path) - strlen(path) - 1);
    g_file = fopen(path, "w");  // si falla (p. ej. Memory Stick protegida) no se registra nada
}

void write(const char* fmt, ...) {
    if (!g_file) return;
    fprintf(g_file, "[%8.3f] ", static_cast<double>(sceKernelGetSystemTimeLow()) / 1e6);
    va_list args;
    va_start(args, fmt);
    vfprintf(g_file, fmt, args);
    va_end(args);
    fputc('\n', g_file);
    fflush(g_file);
}

void close() {
    if (!g_file) return;
    fclose(g_file);
    g_file = nullptr;
}

}  // namespace log
}  // namespace platform
