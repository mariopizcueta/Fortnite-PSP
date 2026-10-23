// Arma de disparo instantaneo: cargador, reserva, cadencia, recarga y dano configurables.
//
// El jugador lleva la SCAR (fusil de asalto legendario, como Referencias/Armas/images.jpeg)
// y los bots un fusil generico con la misma mecanica y otros valores (MatchConfig).
//
// Cadencia: tryFire() solo dispara si ha pasado fireInterval desde el disparo anterior y
// la espera se reinicia a fireInterval exacto, sin acumular. Mantener el gatillo nunca da
// mas de un disparo por paso de fisica ni rafagas tras un tiron de FPS.
#pragma once

namespace game {

struct WeaponConfig {
    const char* name = "SCAR";
    const char* rarity = "Legendaria";
    float damage = 36.0f;
    float fireInterval = 1.0f / 5.5f;  // 5.5 disparos por segundo
    int magazineSize = 30;
    int reserveAmmo = 120;             // -1 = infinita
    float reloadTime = 2.0f;           // s
    float range = 80.0f;               // m
};

class Weapon {
public:
    void reset(const WeaponConfig& config);
    // Avanza la cadencia y la recarga (llamar en cada paso fijo).
    void update(float dt);

    // Intenta disparar: true si sale una bala (gasta una). Con el cargador vacio empieza a
    // recargar sola.
    bool tryFire();
    // Recarga manual (si falta municion en el cargador y hay reserva).
    bool startReload();

    // Anade un retardo extra antes del siguiente disparo (los bots varian su cadencia).
    void addCooldown(float seconds) { m_cooldown += seconds; }

    const WeaponConfig& config() const { return m_config; }
    int ammo() const { return m_ammo; }
    int reserve() const { return m_reserve; }  // -1 = infinita
    bool reloading() const { return m_reloadTimer > 0.0f; }
    float reloadProgress() const;               // 0..1
    bool empty() const { return m_ammo == 0 && m_reserve == 0; }
    int shotsFired() const { return m_shotsFired; }

private:
    WeaponConfig m_config;
    int m_ammo = 0;
    int m_reserve = 0;
    float m_cooldown = 0.0f;
    float m_reloadTimer = 0.0f;
    int m_shotsFired = 0;
};

}  // namespace game
