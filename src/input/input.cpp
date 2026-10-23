#include "input/input.h"

#include <math.h>

#include "core/config.h"

namespace input {

void Input::init() {
    sceCtrlSetSamplingCycle(0);  // 0 = sincronizado con VBlank
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
}

void Input::normalizeStick(u8 rawX, u8 rawY, float& outX, float& outY) {
    const float x = (static_cast<float>(rawX) - 127.5f) / 127.5f;
    const float y = (static_cast<float>(rawY) - 127.5f) / 127.5f;
    const float magnitude = sqrtf(x * x + y * y);
    // Zona muerta radial: no "engancha" las diagonales a los ejes como la zona por eje.
    if (magnitude < config::kAnalogDeadzone) {
        outX = outY = 0.0f;
        return;
    }
    // Reescala para que la salida empiece en 0 justo al salir de la zona muerta y no
    // pase de 1 (las esquinas del recorrido fisico dan magnitudes de hasta ~1.4).
    float scaled = (magnitude - config::kAnalogDeadzone) / (1.0f - config::kAnalogDeadzone);
    if (scaled > 1.0f) scaled = 1.0f;
    outX = x / magnitude * scaled;
    outY = y / magnitude * scaled;
}

void Input::update(float dt) {
    SceCtrlData pad;
    // Peek no bloquea: el ritmo del bucle lo marca el VSync, no el mando.
    if (sceCtrlPeekBufferPositive(&pad, 1) > 0) {
        m_previous = m_current;
        m_current = pad.Buttons;
        m_rawX = pad.Lx;
        m_rawY = pad.Ly;
        ++m_samples;
    } else {
        m_previous = m_current;
    }

    m_pressed = m_current & ~m_previous;
    m_released = m_previous & ~m_current;
    normalizeStick(m_rawX, m_rawY, m_analogX, m_analogY);

    if (m_combo != 0 && held(m_combo)) {
        m_comboHeld += dt;
    } else {
        m_comboHeld = 0.0f;
    }
}

const char* buttonName(u32 bit) {
    switch (bit) {
        case PSP_CTRL_SELECT: return "SELECT";
        case PSP_CTRL_START: return "START";
        case PSP_CTRL_UP: return "ARRIBA";
        case PSP_CTRL_RIGHT: return "DCHA";
        case PSP_CTRL_DOWN: return "ABAJO";
        case PSP_CTRL_LEFT: return "IZDA";
        case PSP_CTRL_LTRIGGER: return "L";
        case PSP_CTRL_RTRIGGER: return "R";
        case PSP_CTRL_TRIANGLE: return "TRIANGULO";
        case PSP_CTRL_CIRCLE: return "CIRCULO";
        case PSP_CTRL_CROSS: return "X";
        case PSP_CTRL_SQUARE: return "CUADRADO";
        case PSP_CTRL_HOME: return "HOME";
        case PSP_CTRL_HOLD: return "HOLD";
        case PSP_CTRL_NOTE: return "NOTE";
        case PSP_CTRL_SCREEN: return "SCREEN";
        case PSP_CTRL_VOLUP: return "VOL+";
        case PSP_CTRL_VOLDOWN: return "VOL-";
        default: return nullptr;
    }
}

}  // namespace input
