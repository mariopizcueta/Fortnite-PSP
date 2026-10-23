#include "platform/callbacks.h"

#include <pspkernel.h>

namespace platform {
namespace {

volatile bool g_exitRequested = false;

int exitCallback(int, int, void*) {
    // Solo marcamos la peticion: el bucle principal libera recursos y llama a
    // sceKernelExitGame() desde su propio hilo.
    g_exitRequested = true;
    return 0;
}

int callbackThread(SceSize, void*) {
    int cbid = sceKernelCreateCallback("FortnitePSP Exit", exitCallback, nullptr);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

}  // namespace

bool setupCallbacks() {
    SceUID thid = sceKernelCreateThread("callback_thread", callbackThread, 0x11, 0xFA0,
                                        PSP_THREAD_ATTR_USER, nullptr);
    if (thid < 0) return false;
    sceKernelStartThread(thid, 0, nullptr);
    return true;
}

bool isRunning() { return !g_exitRequested; }

}  // namespace platform
