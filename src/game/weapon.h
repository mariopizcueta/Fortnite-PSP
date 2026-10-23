// Arma de disparo instantaneo: cargador, reserva, cadencia, recarga y dano configurables.
//
// Las armas del botin (fusil de asalto/SCAR, escopeta, subfusil, francotirador, cada una
// con su rareza) salen de game::weaponConfig() (inventory.h). La reserva del jugador es la
// municion comun del inventario: se copia al arma antes de usarla (setReserve) y se
// devuelve despues; los bots tienen reserva infinita.
//
// Cadencia: tryFire() solo dispara si ha pasado fireInterval desde el disparo anterior y
// la espera se reinicia a fireInterval exacto, sin acumular. Mantener el gatillo nunca da
// mas de un disparo por paso de fisica ni rafagas tras un tiron de FPS.
#pragma once

namespace game {

struct WeaponConfig {
    const char* name = "SCAR";
    const char* rarity = "Legendaria";
    float damage = 36.0f;              // por disparo (escopeta: por disparo entero)
    float fireInterval = 1.0f / 5.5f;  // 5.5 disparos por segundo
    int magazineSize = 30;
    int reserveAmmo = 120;             // -1 = infinita
    float reloadTime = 2.0f;           // s
    float range = 80.0f;               // m
    int ammoType = 1;                  // game::AmmoType (inventory.h)
    int pellets = 1;                   // escopeta: perdigones que se reparten el dano
    float hipSpread = 0.0f;            // grados de dispersion sin apuntar
    float aimSpread = 0.0f;            // grados apuntando (L)
    float falloffStart = 1000.0f;      // m: a partir de aqui el dano baja...
    float falloffEnd = 1000.0f;        // ...hasta falloffMin de su valor
    float falloffMin = 1.0f;
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

    // Reserva (municion del inventario del jugador). -1 = infinita.
    void setReserve(int reserve) { m_reserve = reserve; }
    // Segundos desde el ultimo disparo (precision del primer disparo).
    float sinceShot() const { return m_sinceShot; }

    // Interrumpe la recarga (al cambiar de objeto en la mano); las balas no cambian.
    void cancelReload() { m_reloadTimer = 0.0f; }

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
    float m_sinceShot = 99.0f;
    int m_shotsFired = 0;
};

}  // namespace game
