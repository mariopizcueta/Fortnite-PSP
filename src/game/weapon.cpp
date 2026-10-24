#include "game/weapon.h"

namespace game {

namespace {
constexpr float kMaxCarry = 0.02f;
}  // namespace

void Weapon::reset(const WeaponConfig& config) {
    m_config = config;
    m_ammo = config.magazineSize;
    m_reserve = config.reserveAmmo;
    m_cooldown = 0.0f;
    m_reloadTimer = 0.0f;
    m_sinceShot = 99.0f;
    m_shotsFired = 0;
    m_burstLeft = 0;
}

void Weapon::update(float dt) {
    m_sinceShot += dt;
    if (m_cooldown > 0.0f) m_cooldown -= dt;
    if (m_reloadTimer > 0.0f) {
        m_reloadTimer -= dt;
        if (m_reloadTimer <= 0.0f) {
            m_reloadTimer = 0.0f;
            const int missing = m_config.magazineSize - m_ammo;
            const int take = (m_reserve < 0 || m_reserve >= missing) ? missing : m_reserve;
            m_ammo += take;
            if (m_reserve > 0) m_reserve -= take;
        }
    }
}

bool Weapon::tryFire() {
    if (reloading() || m_cooldown > 0.0f) return false;
    if (m_ammo <= 0) {
        m_burstLeft = 0;
        startReload();
        return false;
    }
    --m_ammo;
    ++m_shotsFired;
    m_sinceShot = 0.0f;
    // Rafaga: la primera bala la empieza; entre balas, burstGap; tras la ultima, el
    // intervalo entre rafagas.
    if (m_burstLeft > 0) --m_burstLeft;
    else if (m_config.burstCount > 1) m_burstLeft = m_config.burstCount - 1;
    // Siguiente disparo dentro de un intervalo. Se arrastra como mucho 20 ms de sobrante
    // (para que la media sea la cadencia exacta con pasos de 1/60 s), nunca mas: aunque el
    // gatillo lleve rato pulsado o haya habido un tiron, no se acumulan disparos.
    const float carry = m_cooldown < -kMaxCarry ? -kMaxCarry : m_cooldown;
    m_cooldown = carry + (m_burstLeft > 0 ? m_config.burstGap : m_config.fireInterval);
    if (m_ammo == 0) {
        m_burstLeft = 0;
        startReload();
    }
    return true;
}

bool Weapon::startReload() {
    if (reloading() || m_ammo >= m_config.magazineSize || m_reserve == 0) return false;
    m_burstLeft = 0;
    m_reloadTimer = m_config.reloadTime;
    return true;
}

float Weapon::reloadProgress() const {
    if (!reloading() || m_config.reloadTime <= 0.0f) return 1.0f;
    return 1.0f - m_reloadTimer / m_config.reloadTime;
}

}  // namespace game
