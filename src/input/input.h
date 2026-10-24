// Lectura de los controles de la PSP (botones + stick analogico).
#pragma once

#include <pspctrl.h>
#include <psptypes.h>

namespace input {

class Input {
public:
    void init();

    // Muestrea el mando sin bloquear. Llamar una vez por frame.
    void update(float dt);

    bool held(u32 mask) const { return (m_current & mask) == mask; }
    bool pressed(u32 mask) const { return (m_pressed & mask) != 0; }
    bool released(u32 mask) const { return (m_released & mask) != 0; }

    u32 rawButtons() const { return m_current; }
    u32 pressedMask() const { return m_pressed; }
    u8 rawAnalogX() const { return m_rawX; }
    u8 rawAnalogY() const { return m_rawY; }

    // Stick normalizado: vector de longitud <= 1 con zona muerta radial (Y positivo = abajo).
    float analogX() const { return m_analogX; }
    float analogY() const { return m_analogY; }

    // Segundos que lleva mantenida completa una combinacion; 0 si no lo esta.
    float comboHeldSeconds() const { return m_comboHeld; }
    void setTrackedCombo(u32 mask) { m_combo = mask; }

    u32 sampleCount() const { return m_samples; }

private:
    static void normalizeStick(u8 rawX, u8 rawY, float& outX, float& outY);

    u32 m_current = 0;
    u32 m_previous = 0;
    u32 m_pressed = 0;
    u32 m_released = 0;
    u8 m_rawX = 128;
    u8 m_rawY = 128;
    float m_analogX = 0.0f;
    float m_analogY = 0.0f;
    u32 m_combo = 0;
    float m_comboHeld = 0.0f;
    u32 m_samples = 0;
};

// Zona muerta radial del stick (0..1; por defecto config::kAnalogDeadzone). Ajustes del
// jugador (parche, fase 8).
void setDeadzone(float deadzone);
float deadzone();

// Nombre corto de un boton (un solo bit). Devuelve nullptr si no se conoce.
const char* buttonName(u32 bit);

}  // namespace input
