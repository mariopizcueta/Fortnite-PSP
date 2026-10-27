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

#include <psptypes.h>

namespace game {

// Proyectiles (lanzacohetes, lanzagranadas, granada de mano): vuelan con su velocidad (y
// gravedad las granadas) y explotan con dano en un radio (ver game::Match).
enum class ProjectileKind : u8 { None, Rocket, LauncherGrenade, HandGrenade };
// Gravedad de las granadas (m/s^2; algo menos que la de los personajes: mas alcance). La usa
// tambien la mira del lanzagranadas para marcar la caida.
constexpr float kGrenadeGravity = 20.0f;

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
    // Retroceso: cada disparo abre la dispersion `recoil` grados (hasta maxRecoil) y se
    // recupera a recoilRecovery grados por segundo; la camara sube un poco con cada disparo.
    float recoil = 0.0f;
    float maxRecoil = 0.0f;
    float recoilRecovery = 8.0f;
    // Agachado: fraccion de la dispersion que queda (segun el tipo de arma).
    float crouchSpreadScale = 1.0f;
    // Rafagas (fusil de rafagas): cada pulsacion dispara burstCount balas separadas
    // burstGap s; fireInterval es la espera entre rafagas.
    int burstCount = 1;
    float burstGap = 0.07f;
    // Proyectil en vez de bala instantanea: velocidad (m/s), radio de la explosion (m),
    // dano a construcciones y objetos del mapa (los cohetes las derriban) y espoleta (s).
    ProjectileKind projectile = ProjectileKind::None;
    float projectileSpeed = 0.0f;
    float blastRadius = 0.0f;
    float structureDamage = 0.0f;
    float fuseSeconds = 0.0f;
    // Mirilla con L (0 = sin mirilla): campo de vision vertical en grados.
    float scopeFov = 0.0f;
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
    // recargar sola. En un arma de rafagas, el primer disparo empieza la rafaga y las
    // demas balas salen solas (burstPending) aunque se suelte el gatillo.
    bool tryFire();
    bool burstPending() const { return m_burstLeft > 0; }
    // Recarga manual (si falta municion en el cargador y hay reserva).
    bool startReload();

    // Reserva (municion del inventario del jugador). -1 = infinita.
    void setReserve(int reserve) { m_reserve = reserve; }
    // Segundos desde el ultimo disparo (precision del primer disparo).
    float sinceShot() const { return m_sinceShot; }

    // Balas en el cargador (un arma recogida del suelo trae las que tenia al soltarla).
    void setLoaded(int rounds) { m_ammo = rounds < 0 ? 0 : (rounds > m_config.magazineSize ? m_config.magazineSize : rounds); }

    // Interrumpe la recarga (al cambiar de objeto en la mano); las balas no cambian.
    void cancelReload() {
        m_reloadTimer = 0.0f;
        m_burstLeft = 0;
    }

    // Anade un retardo extra antes del siguiente disparo (los bots varian su cadencia).
    void addCooldown(float seconds) { m_cooldown += seconds; }

    // Trucos del menu de administrador: la recarga en curso termina en el siguiente paso,
    // el cargador se llena sin gastar reserva y la espera entre disparos se acorta.
    void finishReload() {
        if (m_reloadTimer > 0.0f) m_reloadTimer = 1e-6f;
    }
    void fillMagazine() { m_ammo = m_config.magazineSize; }
    void scaleCooldown(float factor) {
        if (m_cooldown > 0.0f) m_cooldown *= factor;
    }

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
    int m_burstLeft = 0;  // balas que quedan de la rafaga en curso
};

}  // namespace game
