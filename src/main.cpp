// FortnitePSP - punto de entrada.
//
// Orden de arranque: log -> callbacks del sistema -> reloj -> GU -> texto -> mando -> app.
// Orden de cierre: app -> GU -> log -> sceKernelExitGame() (vuelve al XMB).

#include <malloc.h>

#include <pspkernel.h>
#include <psppower.h>

#include "app/app.h"
#include "core/config.h"
#include "core/timer.h"
#include "gfx/debug_text.h"
#include "gfx/graphics.h"
#include "input/input.h"
#include "platform/callbacks.h"
#include "platform/log.h"

PSP_MODULE_INFO("FortnitePSP", PSP_MODULE_USER, 0, 6);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);  // toda la RAM de usuario salvo 1 MB para el sistema

namespace {

const char* exitReason(bool byCombo) { return byCombo ? "L+R+START" : "sistema (HOME)"; }

}  // namespace

int main(int argc, char* argv[]) {
    platform::log::open(argc > 0 ? argv[0] : nullptr);
    platform::log::write("%s v%s arrancando (%s)", config::kGameTitle, config::kVersion,
                         argc > 0 ? argv[0] : "?");

    if (!platform::setupCallbacks()) platform::log::write("aviso: sin exit callback");
    scePowerSetClockFrequency(config::kCpuClockMhz, config::kCpuClockMhz, config::kBusClockMhz);

    if (!gfx::init()) {
        platform::log::write("ERROR: fallo al iniciar GU");
        platform::log::close();
        sceKernelExitGame();
        return 1;
    }
    gfx::text::init();
    platform::log::write("GU iniciado");

    input::Input input;
    input.init();
    input.setTrackedCombo(config::kExitCombo);

    app::App app;
    app.init();
    const struct mallinfo mem = mallinfo();
    platform::log::write("heap en uso %u KB; entrando en el bucle principal",
                         static_cast<unsigned>(mem.uordblks / 1024));

    core::FrameTimer timer;
    timer.reset();
    float workMs = 0.0f;

    while (platform::isRunning()) {
        timer.tick();
        const u64 workStart = core::nowMicros();

        input.update(timer.deltaSeconds());
        app.update(input, timer.deltaSeconds());
        if (app.exitRequested()) break;

        const app::FrameInfo frame{timer, input, workMs};

        gfx::beginFrame(app.clearColor());
        app.render(frame);
        gfx::endFrame();

        gfx::text::setTarget(gfx::cpuDrawBuffer());
        app.renderOverlay(frame);

        workMs = static_cast<float>(core::nowMicros() - workStart) / 1000.0f;
        gfx::present(app.vsync());
    }

    platform::log::write("salida por %s tras %u frames", exitReason(app.exitRequested()),
                         static_cast<unsigned>(timer.frameCount()));
    app.shutdown();
    gfx::shutdown();
    platform::log::close();
    sceKernelExitGame();
    return 0;
}
