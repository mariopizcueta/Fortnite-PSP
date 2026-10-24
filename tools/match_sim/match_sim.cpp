// Simulador de host (no PSP) de partidas completas sobre el mapa real.
//
// Compila el MISMO codigo de mundo, colisiones, navegacion, IA, disparos y partida que el
// juego, y comprueba los criterios de la fase 4:
//   1. navegacion: hay camino desde la aparicion a todos los puntos de interes;
//   2. colisiones en el mapa real: paredes de las naves, escalera al altillo, barandilla;
//   3. disparos: impactan en campo abierto y a traves de vallas, NO a traves de paredes;
//   4. vida, eliminaciones, contador de vivos y victoria;
//   5. partidas completas con 10 y 30 bots: terminan con una sola entidad, el contador de
//      vivos siempre cuadra, nadie se mete dentro de un solido, ningun impacto atraviesa
//      paredes y los bots no se quedan atascados; ademas mide el coste de la IA;
//   6. (fases 5 y 6) arma, disparo desde la camara, construccion y sucesos de sonido.
//
// Uso: sh tools/match_sim/build_and_run.sh   (o: make match-sim). Sale con 1 si algo falla.
#include <math.h>
#include <stdio.h>

#include <string>
#include <utility>
#include <vector>

#include "game/fixed_timestep.h"
#include "game/match.h"
#include "game/match_internal.h"
#include "game/nav_grid.h"
#include "gfx/graphics.h"
#include "world/storm.h"
#include "world/world.h"
#include "world/world_units.h"

// El mundo referencia funciones de dibujo que aqui no se usan.
namespace gfx {
void setModelMatrix(const math3d::Mat4&) {}
void bindTexture(const Texture*) {}
void drawMesh(const Mesh&) {}
void setBlend(bool) {}
}  // namespace gfx

using game::Match;
using game::MatchConfig;
using math3d::Vec3;

namespace {

int g_failures = 0;
const Vec3 kSpawn{0.0f, 0.0f, 24.0f};

void check(bool ok, const char* what) {
    printf("  [%s] %s\n", ok ? " OK " : "FALLO", what);
    if (!ok) ++g_failures;
}

float distXZ(const Vec3& a, const Vec3& b) {
    return sqrtf((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z));
}

bool penetrates(const world::CollisionWorld& w, const game::CharacterState& s) {
    const float r = game::tuning::kBodyRadius - 0.01f;
    const world::Aabb body{{s.position.x - r, s.position.y + game::tuning::kStepHeight, s.position.z - r},
                           {s.position.x + r, s.position.y + game::tuning::kBodyHeight - 0.01f, s.position.z + r}};
    return w.overlapsAny(body, world::kBlocksMovement);
}

// Mueve al jugador con el stick fijo durante `seconds` en el mapa real.
game::CharacterState walk(const world::CollisionWorld& w, Vec3 start, float yaw, float moveX, float moveY,
                          float seconds, bool* penetrated) {
    game::PlayerController pc;
    pc.reset(start, yaw, w);
    game::PlayerInput in;
    in.moveX = moveX;
    in.moveY = moveY;
    in.cameraYaw = yaw;
    *penetrated = false;
    for (int i = 0; i < static_cast<int>(seconds * 60.0f); ++i) {
        pc.setInput(in);
        pc.step(game::FixedTimestep::kStep, w);
        if (penetrates(w, pc.state())) *penetrated = true;
    }
    return pc.state();
}

void testNavigation(world::World& world, game::NavGrid& nav) {
    printf("\n1) Navegacion\n");
    printf("  %d cajas de colision, %d de %d nodos transitables\n", world.collision().count(),
           nav.walkableCount(), nav.size() * nav.size());
    Vec3 path[game::kMaxPathPoints];
    int ok = 0, maxExpansions = 0;
    for (const Vec3& poi : world.pointsOfInterest()) {
        int expansions = 0;
        const int n = nav.findPath(kSpawn, poi, path, game::kMaxPathPoints, &expansions);
        if (expansions > maxExpansions) maxExpansions = expansions;
        if (n > 0) ++ok;
        else printf("  sin camino a (%.0f, %.0f)\n", poi.x, poi.z);
    }
    printf("  caminos desde la aparicion a %d/%d puntos de interes (max %d nodos expandidos)\n", ok,
           static_cast<int>(world.pointsOfInterest().size()), maxExpansions);
    check(ok == static_cast<int>(world.pointsOfInterest().size()),
          "hay camino a todos los puntos de interes (interiores de las naves incluidos)");
}

void testMapCollisions(world::World& world) {
    printf("\n2) Colisiones en el mapa real\n");
    const world::CollisionWorld& w = world.collision();
    bool pen = false;
    // Dentro de la nave gris, corriendo al oeste contra la pared (cara interior x = -7.6).
    game::CharacterState s = walk(w, {0.0f, 0.0f, -14.0f}, 0.0f, -1.0f, 0.0f, 3.0f, &pen);
    printf("  nave gris hacia la pared oeste: x final %.2f\n", s.position.x);
    check(s.position.x >= -7.6f + game::tuning::kBodyRadius - 0.01f && !pen, "la pared de la nave bloquea al jugador");

    // Escalera de la nave azul (x -29.6..-28, sube hacia -Z hasta el altillo a 3.6 m).
    s = walk(w, {-28.8f, 0.0f, -13.0f}, 0.0f, 0.0f, -1.0f, 4.0f, &pen);
    printf("  escalera de la nave azul: final (%.2f, %.2f, %.2f)\n", s.position.x, s.position.y, s.position.z);
    check(fabsf(s.position.y - 3.6f) < 0.01f && s.position.z < -20.5f && !pen, "la escalera sube al altillo");

    // En el altillo, andando hacia la barandilla (al sur): no se cae.
    s = walk(w, {-22.0f, 3.6f, -23.0f}, 0.0f, 0.0f, 1.0f, 2.0f, &pen);
    printf("  altillo hacia la barandilla: (%.2f, %.2f, %.2f)\n", s.position.x, s.position.y, s.position.z);
    check(fabsf(s.position.y - 3.6f) < 0.01f && s.position.z <= -20.0f - game::tuning::kBodyRadius + 0.05f,
          "la barandilla impide caerse del altillo");

    // Contra el muro de bloques del recinto (2.4 m, no se salta).
    s = walk(w, {-12.0f, 0.0f, 20.0f}, 0.0f, 0.0f, -1.0f, 3.0f, &pen);
    printf("  contra el muro sur: z final %.2f\n", s.position.z);
    check(s.position.z >= 14.4f + game::tuning::kBodyRadius - 0.01f && !pen, "el muro del recinto bloquea");

    // Hacia el lago: el agua bloquea el paso.
    s = walk(w, {-30.0f, 0.0f, 38.0f}, 0.0f, 0.0f, 1.0f, 4.0f, &pen);
    printf("  hacia el lago: z final %.2f (orilla norte ~ z 44)\n", s.position.z);
    check(s.position.z < 46.0f && !pen, "el agua del lago no se puede cruzar");
}

void testShots(world::World& world, game::NavGrid& nav) {
    printf("\n3) Disparos (raycast) y obstaculos\n");
    MatchConfig cfg = MatchConfig::classic();
    cfg.botCount = 1;
    Match m;
    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);

    // Campo abierto.
    m.teleport(0, {70.0f, 0.0f, 25.0f}, 0.0f);
    m.teleport(1, {70.0f, 0.0f, 10.0f}, 0.0f);
    Vec3 dir = math3d::normalize(m.chestOf(1) - m.chestOf(0));
    game::ShotResult r = m.fireShot(0, m.chestOf(0), dir, 60.0f, 20.0f);
    printf("  campo abierto a 15 m: impacto %s, vida del bot %.0f\n", r.hitEntity ? "si" : "no", m.body(1).state().health);
    check(r.hitEntity && m.body(1).state().health == 80.0f, "un disparo con linea de vision resta vida");

    // Pared de la nave gris entre los dos (jugador dentro, bot en el pasillo).
    m.teleport(0, {0.0f, 0.0f, -14.0f}, 0.0f);
    m.teleport(1, {-11.0f, 0.0f, -14.0f}, 0.0f);
    dir = math3d::normalize(m.chestOf(1) - m.chestOf(0));
    r = m.fireShot(0, m.chestOf(0), dir, 60.0f, 20.0f);
    printf("  a traves de la pared de la nave: impacto %s, bloqueado %s, vida %.0f\n", r.hitEntity ? "si" : "no",
           r.blockedByWorld ? "si" : "no", m.body(1).state().health);
    check(!r.hitEntity && r.blockedByWorld && m.body(1).state().health == 80.0f, "las paredes solidas detienen los disparos");
    check(!m.lineOfSight(m.eyeOf(1), m.chestOf(0)), "la pared tambien corta la linea de vision de la IA");

    // Valla metalica (bloquea el paso pero no los disparos).
    m.teleport(0, {0.0f, 0.0f, 18.0f}, 0.0f);
    m.teleport(1, {0.0f, 0.0f, 10.0f}, 0.0f);
    dir = math3d::normalize(m.chestOf(1) - m.chestOf(0));
    r = m.fireShot(0, m.chestOf(0), dir, 60.0f, 20.0f);
    printf("  a traves de la valla metalica: impacto %s, vida %.0f\n", r.hitEntity ? "si" : "no", m.body(1).state().health);
    check(r.hitEntity && m.body(1).state().health == 60.0f, "la valla metalica deja pasar los disparos");

    // Disparo del jugador como en el juego: rayo de la camara (detras y por encima del
    // jugador), mira desviada 4 grados del bot, R mantenido 0.5 s (cadencia 0.22 s).
    auto playerFires = [&](const Vec3& player, const Vec3& bot, float offsetDegrees) {
        Match fresh;
        MatchConfig one = MatchConfig::classic();
        one.botCount = 1;
        fresh.start(one, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
        fresh.teleport(0, player, 0.0f);
        fresh.teleport(1, bot, 0.0f);
        const Vec3 eye = player + Vec3{0.0f, 2.9f, 4.8f};
        Vec3 dir = math3d::normalize(fresh.chestOf(1) - eye);
        const float a = math3d::toRadians(offsetDegrees);
        dir = math3d::normalize(Vec3{dir.x * cosf(a) + dir.z * sinf(a), dir.y, -dir.x * sinf(a) + dir.z * cosf(a)});
        game::PlayerInput in;
        in.aiming = true;
        game::PlayerAim aim{eye, dir, true};
        for (int i = 0; i < 30; ++i) {
            fresh.setPlayerInput(in, aim);
            fresh.step(game::FixedTimestep::kStep);
        }
        return fresh.alive(1) ? fresh.body(1).state().health : 0.0f;
    };
    const float near = playerFires({70.0f, 0.0f, 25.0f}, {70.0f, 0.0f, 10.0f}, 2.0f);
    const float far = playerFires({70.0f, 0.0f, 25.0f}, {70.0f, 0.0f, 10.0f}, 5.0f);
    printf("  mira desviada 2 grados (ayuda de 3): vida del bot %.0f; desviada 5 grados: %.0f\n", near, far);
    check(near < 100.0f, "la ayuda de punteria corrige una mira desviada menos de 3 grados");
    check(far == 100.0f, "con mas desviacion manda el centro de la mira: el disparo falla");
    const float behindWall = playerFires({0.0f, 0.0f, -14.0f}, {-11.0f, 0.0f, -14.0f}, 0.0f);
    printf("  jugador dispara a un bot detras de la pared de la nave: vida del bot %.0f\n", behindWall);
    check(behindWall == 100.0f, "ni la ayuda de punteria ni el disparo atraviesan paredes");
}

void testEliminations(world::World& world, game::NavGrid& nav) {
    printf("\n4) Vida, eliminaciones, contador de vivos y victoria\n");
    MatchConfig cfg = MatchConfig::classic();
    cfg.botCount = 5;
    Match m;
    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    check(m.aliveCount() == 6, "empiezan 6 entidades vivas (jugador + 5 bots), 100 de vida cada una");
    bool countOk = true;
    for (int id = 1; id <= 5; ++id) {
        m.applyDamage(id, 60.0f, 0);
        countOk &= m.alive(id) && m.aliveCount() == 7 - id;  // 60 no basta
        m.applyDamage(id, 60.0f, 0);
        countOk &= !m.alive(id) && m.aliveCount() == 6 - id;
        countOk &= m.bot(id - 1).state == game::BotState::Eliminated;
    }
    printf("  tras eliminar 5 bots: vivos %d, fase %s, ganador %s, bajas del jugador %d\n", m.aliveCount(),
           game::matchPhaseName(m.phase()), m.name(m.winner()), m.playerKills());
    check(countOk, "cada eliminacion resta uno al contador de vivos");
    check(m.phase() == game::MatchPhase::Finished && m.winner() == 0 && m.playerKills() == 5,
          "victoria del jugador cuando solo queda el");
    m.applyDamage(1, 50.0f, 0);
    check(m.aliveCount() == 1, "un eliminado no vuelve a recibir dano ni cambia el contador");

    // El jugador cae: la partida sigue entre bots hasta que queda uno.
    cfg.botCount = 3;
    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    m.applyDamage(0, 100.0f, 2);
    printf("  jugador eliminado: fase %s, puesto %d\n", game::matchPhaseName(m.phase()), m.playerPlace());
    check(m.phase() == game::MatchPhase::Spectating && m.playerPlace() == 4, "el jugador queda 4 de 4 y la partida sigue");
    m.applyDamage(2, 100.0f, 1);
    m.applyDamage(3, 100.0f, 1);
    printf("  fin: fase %s, ganador %s con %d bajas\n", game::matchPhaseName(m.phase()), m.name(m.winner()),
           m.bot(0).kills);
    check(m.phase() == game::MatchPhase::Finished && m.winner() == 1, "la partida termina cuando queda una entidad");
}

// --- Partidas completas ---------------------------------------------------------------------

struct SimContext {
    const world::CollisionWorld* world;
    int hits = 0;
    int hitsThroughWalls = 0;
    game::ShotRecord firstBad;  // primer impacto que atraviesa algo (diagnostico)
    world::RayHit badHit;
};

void onShot(void* ctx, const game::ShotRecord& shot) {
    SimContext& c = *static_cast<SimContext*>(ctx);
    if (shot.victim < 0) return;
    ++c.hits;
    // Verificacion independiente: del origen al punto de impacto no hay ningun solido.
    const Vec3 d = shot.end - shot.origin;
    const float len = math3d::length(d);
    if (len < 0.05f) return;
    const Vec3 end = shot.origin + d * ((len - 0.02f) / len);
    world::RayHit hit;
    if (c.world->raycast(shot.origin, end, world::kBlocksShots, &hit)) {
        if (++c.hitsThroughWalls == 1) {
            c.firstBad = shot;
            c.badHit = hit;
        }
    }
}

struct SimResult {
    bool finished = false;
    float duration = 0.0f;
    int winner = -1;
    int aliveMismatches = 0;
    int penetrations = 0;
    int maxStuckSeconds = 0;
    float stuckRatio = 0.0f;
    float metersPerBotMinute = 0.0f;
    double aiMicrosPerStep = 0.0;
    double physicsMicrosPerStep = 0.0;
    game::MatchStats stats;
    SimContext shots;
    int armedAt60 = 0;       // bots con arma (o ya eliminados) al minuto de partida
    int chestsOpened = 0;
    int airdropsOpened = 0;
};

SimResult simulate(world::World& world, game::NavGrid& nav, int bots, u32 seed, float maxSeconds,
                   bool realRules = false) {
    // realRules: las reglas de la fase 7 (autobus, botin, bots que buscan armas, cazan y
    // construyen); si no, las de las fases 4-6.
    MatchConfig cfg = realRules ? MatchConfig{} : MatchConfig::classic();
    cfg.botCount = bots;
    cfg.seed = seed;
    Match m;
    SimResult r;
    r.shots.world = &world.collision();
    m.setShotObserver(onShot, &r.shots);
    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);

    std::vector<Vec3> lastSample(bots + 1);
    std::vector<int> stuckRun(bots + 1, 0);
    for (int id = 0; id <= bots; ++id) lastSample[id] = m.body(id).state().position;
    int movingSamples = 0, stuckSamples = 0;
    double meters = 0.0;
    int prevAlive = m.aliveCount();

    const int maxSteps = static_cast<int>(maxSeconds * 60.0f);
    for (int step = 1; step <= maxSteps; ++step) {
        m.setPlayerInput(game::PlayerInput{}, game::PlayerAim{});  // el jugador se queda quieto
        m.step(game::FixedTimestep::kStep);

        int recount = 0;
        for (int id = 0; id <= bots; ++id) {
            if (!m.alive(id)) continue;
            ++recount;
            if (penetrates(world.collision(), m.body(id).state())) ++r.penetrations;
        }
        if (recount != m.aliveCount() || m.aliveCount() > prevAlive) ++r.aliveMismatches;
        prevAlive = m.aliveCount();

        if (step % 60 == 0) {  // muestreo cada segundo
            for (int i = 0; i < bots; ++i) {
                const game::Bot& b = m.bot(i);
                const Vec3 p = b.body.state().position;
                const float moved = distXZ(p, lastSample[i + 1]);
                lastSample[i + 1] = p;
                if (b.state == game::BotState::Eliminated) continue;
                meters += moved;
                const bool wantsToMove = m.airState(i + 1) == game::AirState::Ground &&
                                         (b.state == game::BotState::Travel || b.state == game::BotState::Loot ||
                                          (b.state == game::BotState::Attack && b.pathIndex < b.pathLength));
                if (!wantsToMove) {
                    stuckRun[i + 1] = 0;
                    continue;
                }
                ++movingSamples;
                if (moved < 0.3f) {
                    ++stuckSamples;
                    if (++stuckRun[i + 1] > r.maxStuckSeconds) r.maxStuckSeconds = stuckRun[i + 1];
                } else {
                    stuckRun[i + 1] = 0;
                }
            }
        }
        if (step == 60 * 60) {
            for (int i = 0; i < bots; ++i) r.armedAt60 += m.bot(i).armed || !m.alive(i + 1) ? 1 : 0;
        }
        if (m.phase() == game::MatchPhase::Finished) {
            if (step < 60 * 60) {  // acabo antes del minuto: se cuenta como estaba al final
                for (int i = 0; i < bots; ++i) r.armedAt60 += m.bot(i).armed || !m.alive(i + 1) ? 1 : 0;
            }
            r.finished = true;
            break;
        }
    }
    for (const game::Container& c : m.containers()) {
        if (c.opened) ++(c.kind == game::ContainerKind::Airdrop ? r.airdropsOpened : r.chestsOpened);
    }
    r.duration = m.elapsed();
    r.winner = m.winner();
    r.stats = m.stats();
    r.stuckRatio = movingSamples ? static_cast<float>(stuckSamples) / movingSamples : 0.0f;
    r.metersPerBotMinute = static_cast<float>(meters / bots / (r.duration / 60.0f));
    r.aiMicrosPerStep = static_cast<double>(r.stats.aiMicros) / r.stats.steps;
    r.physicsMicrosPerStep = static_cast<double>(r.stats.physicsMicros) / r.stats.steps;
    return r;
}

void testFullMatches(world::World& world, game::NavGrid& nav) {
    printf("\n5) Partidas completas (jugador quieto en la aparicion, maximo 20 min de juego)\n");
    printf("  bots semilla  fin  duracion ganador  disparos impactos pared atascos max_atasco ratio  m/min "
           "IA_us/paso fis_us/paso\n");
    struct Case {
        int bots;
        u32 seed;
    };
    const Case cases[] = {{10, 1}, {10, 2}, {10, 3}, {10, 4}, {10, 5}, {30, 1}, {30, 2}, {30, 3}};
    bool allFinished = true, aliveOk = true, noPenetration = true, noWallHits = true, stuckOk = true;
    double ai10 = 0, ai30 = 0;
    int n10 = 0, n30 = 0;
    for (const Case& c : cases) {
        const SimResult r = simulate(world, nav, c.bots, c.seed, 20.0f * 60.0f);
        char winner[16];
        snprintf(winner, sizeof(winner), "%s", r.winner == 0 ? "Jugador" : (r.winner > 0 ? "bot" : "-"));
        if (r.winner > 0) snprintf(winner, sizeof(winner), "Bot %d", r.winner);
        printf("  %4d %6u %4s %6.0f s %-8s %8d %8d %5d %7d %8d s %5.1f%% %5.0f %10.1f %11.1f\n", c.bots,
               static_cast<unsigned>(c.seed), r.finished ? "si" : "NO", static_cast<double>(r.duration), winner,
               r.stats.shotsFired, r.stats.shotsHitEntity, r.stats.shotsBlockedByWorld, r.stats.stuckEvents,
               r.maxStuckSeconds, static_cast<double>(r.stuckRatio * 100.0f), static_cast<double>(r.metersPerBotMinute),
               r.aiMicrosPerStep, r.physicsMicrosPerStep);
        allFinished &= r.finished;
        aliveOk &= r.aliveMismatches == 0;
        noPenetration &= r.penetrations == 0;
        noWallHits &= r.shots.hitsThroughWalls == 0 && r.shots.hits == r.stats.shotsHitEntity;
        stuckOk &= r.maxStuckSeconds <= 6 && r.stuckRatio < 0.05f;
        if (c.bots == 10) {
            ai10 += r.aiMicrosPerStep;
            ++n10;
        } else {
            ai30 += r.aiMicrosPerStep;
            ++n30;
        }
    }
    check(allFinished, "todas las partidas terminan con una sola entidad viva");
    check(aliveOk, "el contador de vivos coincide siempre con el recuento y nunca sube");
    check(noPenetration, "ningun personaje queda dentro de un solido");
    check(noWallHits, "ningun impacto atraviesa una pared (verificado con raycast independiente)");
    check(stuckOk, "los bots no se quedan atascados (max 6 s seguidos, < 5 % del tiempo en marcha)");
    printf("  coste medio de la IA en el Mac: %.1f us/paso con 10 bots, %.1f us/paso con 30 bots (x%.1f)\n",
           ai10 / n10, ai30 / n30, (ai30 / n30) / (ai10 / n10));
}

// --- 6) Arma ------------------------------------------------------------------------------

void testWeapon() {
    printf("\n6) Arma: SCAR legendaria (municion, cadencia, recarga)\n");
    const game::WeaponConfig scar;
    printf("  %s %s: dano %.0f, cargador %d, reserva %d, %.2f disparos/s, recarga %.1f s\n", scar.name,
           scar.rarity, scar.damage, scar.magazineSize, scar.reserveAmmo, 1.0f / scar.fireInterval, scar.reloadTime);
    check(scar.damage == 36.0f && scar.magazineSize == 30, "dano 36 y cargador de 30 balas");

    // Gatillo mantenido 10 s a 60 pasos/s: como mucho un disparo por paso y por intervalo.
    game::Weapon w;
    w.reset(scar);
    int shots = 0, maxPerStep = 0;
    float firstShot = -1.0f, thirtiethShot = -1.0f, lastShot = -1.0f, minGap = 1e9f, longestPause = 0.0f;
    for (int step = 0; step < 600; ++step) {
        const float t = step / 60.0f;
        w.update(1.0f / 60.0f);
        int inStep = 0;
        while (w.tryFire()) {  // se intenta varias veces en el mismo paso a proposito
            ++inStep;
            ++shots;
            if (firstShot < 0.0f) firstShot = t;
            if (shots == 30) thirtiethShot = t;
            if (lastShot >= 0.0f) {
                if (t - lastShot < minGap) minGap = t - lastShot;
                if (t - lastShot > longestPause) longestPause = t - lastShot;
            }
            lastShot = t;
            if (inStep > 5) break;
        }
        if (inStep > maxPerStep) maxPerStep = inStep;
    }
    printf("  10 s con el gatillo mantenido: %d disparos, max %d por paso, separacion minima %.3f s, "
           "30 balas en %.2f s, pausa de recarga %.2f s, reserva final %d\n",
           shots, maxPerStep, static_cast<double>(minGap), static_cast<double>(thirtiethShot - firstShot),
           static_cast<double>(longestPause), w.reserve());
    check(maxPerStep == 1, "mantener el gatillo nunca da mas de un disparo por paso");
    check(minGap >= scar.fireInterval - 1.0f / 60.0f, "la separacion entre disparos respeta la cadencia");
    check(fabsf((thirtiethShot - firstShot) - 29.0f * scar.fireInterval) < 0.05f,
          "las 30 balas del cargador salen a 5.5 disparos/s");
    check(longestPause >= scar.reloadTime && longestPause < scar.reloadTime + 0.25f,
          "al vaciar el cargador recarga sola durante 2 s");
    check(w.reserve() == scar.reserveAmmo - 30 * (w.reserve() < scar.reserveAmmo ? (scar.reserveAmmo - w.reserve()) / 30 : 0),
          "cada recarga toma las balas de la reserva");

    // Los mismos disparos a 20, 60 y 144 FPS (la cadencia va en pasos fijos).
    int counts[3];
    const float fpsList[3] = {20.0f, 60.0f, 144.0f};
    for (int k = 0; k < 3; ++k) {
        game::Weapon weapon;
        weapon.reset(scar);
        game::FixedTimestep ts;
        int n = 0;
        for (double t = 0.0; t < 5.0; t += 1.0 / fpsList[k]) {
            const int steps = ts.advance(1.0f / fpsList[k]);
            for (int i = 0; i < steps; ++i) {
                weapon.update(game::FixedTimestep::kStep);
                if (weapon.tryFire()) ++n;
            }
        }
        counts[k] = n;
    }
    printf("  5 s de fuego continuo: %d disparos a 20 FPS, %d a 60 FPS, %d a 144 FPS\n", counts[0], counts[1], counts[2]);
    check(counts[0] == counts[1] && counts[1] == counts[2], "la cadencia no depende de los FPS");

    // Reserva limitada: se agota y el arma deja de disparar.
    game::WeaponConfig small = scar;
    small.reserveAmmo = 40;
    w.reset(small);
    int total = 0;
    for (int step = 0; step < 60 * 20; ++step) {
        w.update(1.0f / 60.0f);
        if (w.tryFire()) ++total;
    }
    printf("  reserva de 40: %d disparos en total, cargador %d, reserva %d\n", total, w.ammo(), w.reserve());
    check(total == 70 && w.empty() && !w.tryFire(), "sin balas en el cargador ni en la reserva no se dispara");
    w.reset(scar);
    check(!w.startReload(), "con el cargador lleno no se recarga");
    w.tryFire();
    check(w.startReload() && w.reloading() && !w.tryFire(), "recarga manual: mientras recarga no dispara");
}

// --- 7) Disparo desde la camara ------------------------------------------------------------------

// Ojo de la camara en tercera persona detras de un jugador que mira al norte.
Vec3 cameraEyeBehind(const Vec3& player) { return player + Vec3{0.0f, 2.89f, 4.86f}; }

void testCameraShots(world::World& world, game::NavGrid& nav) {
    printf("\n7) Disparo desde la camara (raycast por la mira)\n");
    MatchConfig cfg = MatchConfig::classic();
    cfg.botCount = 2;
    Match m;
    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    const Vec3 player{70.0f, 0.0f, 25.0f};
    m.teleport(0, player, 0.0f);
    m.teleport(1, {70.0f, 0.0f, 15.0f}, 0.0f);
    m.teleport(2, {70.0f, 0.0f, 8.0f}, 0.0f);
    game::PlayerAim aim;
    aim.eye = cameraEyeBehind(player);
    aim.direction = math3d::normalize(m.chestOf(1) - aim.eye);
    game::ShotResult r = m.playerShot(aim, 36.0f, 80.0f);
    printf("  dos bots en linea: el cercano tiene %.0f, el lejano %.0f\n", m.body(1).state().health,
           m.body(2).state().health);
    check(r.hitEntity && r.victim == 1 && m.body(1).state().health == 64.0f && m.body(2).state().health == 100.0f,
          "el primer objeto alcanzado por la mira recibe el impacto (36) y el de detras no");
    bool number = false;
    for (int i = 0; i < Match::kMaxDamageNumbers; ++i) {
        const game::DamageNumber& d = m.damageNumbers()[i];
        if (d.age < 0.01f && d.amount == 36.0f && !d.structure && distXZ(d.position, m.body(1).state().position) < 1.0f) number = true;
    }
    check(number, "aparece el numero de dano 36 sobre el bot alcanzado");
    m.playerShot(aim, 36.0f, 80.0f);
    m.playerShot(aim, 36.0f, 80.0f);
    printf("  tras 3 impactos: bot %s, vivos %d\n", m.alive(1) ? "vivo" : "eliminado", m.aliveCount());
    check(!m.alive(1) && m.aliveCount() == 2, "3 impactos de 36 eliminan a un bot de 100");

    // La camara ve por encima de un murete de 1.6 m, pero el arma (a la altura del
    // pecho) no: el disparo termina en el murete.
    MatchConfig one = MatchConfig::classic();
    one.botCount = 1;
    m.start(one, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    m.teleport(0, player, 0.0f);
    m.teleport(1, {70.0f, 0.0f, 10.0f}, 0.0f);
    const int low = world.mutableCollision().addDynamic({{68.0f, 0.0f, 23.6f}, {72.0f, 1.6f, 24.0f}}, world::kSolid);
    aim.eye = cameraEyeBehind(player);
    aim.direction = math3d::normalize(m.chestOf(1) - aim.eye);
    const bool cameraSees = !world.collision().raycast(aim.eye, m.chestOf(1), world::kBlocksShots, nullptr);
    r = m.playerShot(aim, 36.0f, 80.0f);
    world.mutableCollision().removeDynamic(low);
    printf("  murete de 1.6 m: la camara lo ve %s; impacto %s, bloqueado %s, vida %.0f\n", cameraSees ? "por encima" : "tapado",
           r.hitEntity ? "si" : "no", r.blockedByWorld ? "si" : "no", m.body(1).state().health);
    check(cameraSees && !r.hitEntity && r.blockedByWorld && m.body(1).state().health == 100.0f,
          "si el arma no tiene linea de tiro, el disparo termina en el obstaculo");

    // Gatillo mantenido 2 s a traves de Match (como en el juego).
    m.start(one, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    m.teleport(0, player, 0.0f);
    m.teleport(1, {-80.0f, 0.0f, 80.0f}, 0.0f);
    aim.eye = cameraEyeBehind(player);
    aim.direction = {0.0f, 0.0f, -1.0f};
    aim.fire = true;
    game::PlayerInput in;
    in.aiming = true;
    const int before = m.stats().shotsFired;
    for (int i = 0; i < 120; ++i) {
        m.setPlayerInput(in, aim);
        m.step(game::FixedTimestep::kStep);
    }
    const int fired = m.stats().shotsFired - before;
    printf("  2 s con R mantenido: %d disparos, quedan %d balas\n", fired, m.playerWeapon().ammo());
    check(fired == 11 && m.playerWeapon().ammo() == 30 - fired, "2 s de fuego = 11 disparos (5.5/s) y 11 balas gastadas");
}

// --- 8) Construccion ----------------------------------------------------------------------------

game::CharacterState walkOn(const world::World& world, Vec3 start, float yaw, float moveY, float seconds) {
    bool pen = false;
    return walk(world.collision(), start, yaw, 0.0f, moveY, seconds, &pen);
}

void testBuilding(world::World& world, game::NavGrid& nav) {
    printf("\n8) Construccion de paredes\n");
    const float pi = math3d::kPi;
    MatchConfig cfg = MatchConfig::classic();
    cfg.botCount = 1;
    cfg.build.maxWalls = 3;
    Match m;
    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    const Vec3 player{70.0f, 0.0f, 25.0f};
    m.teleport(0, player, 0.0f);
    m.teleport(1, {-80.0f, 0.0f, 80.0f}, 0.0f);

    game::WallPlacement plan = m.planWall(0.0f);
    printf("  mirando al norte: pared x %.2f..%.2f z %.3f..%.3f alto %.1f -> %s\n", plan.box.min.x, plan.box.max.x,
           plan.box.min.z, plan.box.max.z, plan.box.max.y - plan.box.min.y, plan.valid ? "valida" : game::placementErrorName(plan.error));
    check(plan.valid && m.buildWall(0.0f) >= 0 && m.structures().count() == 1 && world.collision().dynamicCount() == 1,
          "se coloca una pared valida delante del jugador (colision dinamica creada)");
    plan = m.planWall(0.0f);
    check(!plan.valid && plan.error == game::PlacementError::OverlapsStructure, "no se puede repetir una pared en el mismo sitio");

    m.teleport(1, {68.0f, 0.0f, 26.0f}, 0.0f);
    plan = m.planWall(pi * 0.5f);
    printf("  con un bot donde iria la pared oeste: %s\n", game::placementErrorName(plan.error));
    check(plan.error == game::PlacementError::OverlapsBot, "no se construye encima de un bot");
    m.teleport(1, {-80.0f, 0.0f, 80.0f}, 0.0f);
    check(m.buildWall(pi * 0.5f) >= 0, "la pared perpendicular que hace esquina si se puede colocar");
    check(m.buildWall(pi) >= 0 && m.structures().count() == 3, "tercera pared (sur)");
    plan = m.planWall(-pi * 0.5f);
    check(plan.error == game::PlacementError::LimitReached, "con el limite (3) alcanzado no se construye mas");

    // Quitar apuntando: la mira hacia la pared norte (la sur queda entre camara y jugador).
    game::PlayerAim aim;
    aim.eye = cameraEyeBehind(player);
    aim.direction = math3d::normalize(Vec3{70.0f, 1.5f, 24.0f} - aim.eye);
    const int targeted = m.targetedWall(aim);
    const bool removed = m.removeWallAt(aim);
    printf("  quitar la pared apuntada: pared %d, quitada %s, quedan %d (cajas dinamicas %d)\n", targeted,
           removed ? "si" : "no", m.structures().count(), world.collision().dynamicCount());
    check(removed && m.structures().wall(targeted).active == false && m.structures().count() == 2 &&
              world.collision().dynamicCount() == 2,
          "CUADRADO quita la pared apuntada y su colision");

    // Validaciones contra el mapa.
    MatchConfig open = cfg;
    open.build.maxWalls = 20;
    m.start(open, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    check(world.collision().dynamicCount() == 0, "una partida nueva empieza sin paredes");
    m.teleport(1, {-80.0f, 0.0f, 80.0f}, 0.0f);
    m.teleport(0, {0.0f, 0.0f, -23.0f}, 0.0f);
    plan = m.planWall(0.0f);
    printf("  en la nave gris contra el contenedor: %s\n", game::placementErrorName(plan.error));
    check(plan.error == game::PlacementError::OverlapsStructure, "no se construye dentro de otra estructura del mapa");
    m.teleport(0, {-27.0f, 0.0f, 3.2f}, pi);
    plan = m.planWall(pi);
    printf("  pegado a la fachada de la nave azul, pared al otro lado: %s\n", game::placementErrorName(plan.error));
    check(plan.error == game::PlacementError::Blocked, "no se construye al otro lado de una pared");
    m.teleport(0, {-6.0f, 0.0f, -10.0f}, pi * 0.5f);
    plan = m.planWall(pi * 0.5f);
    printf("  en la puerta lateral de la nave gris: %s, alto %.2f m\n", plan.valid ? "valida" : game::placementErrorName(plan.error),
           plan.box.max.y - plan.box.min.y);
    check(plan.valid && fabsf(plan.box.max.y - 2.8f) < 0.01f, "en una puerta la pared se recorta bajo el dintel (2.8 m)");

    // Bloquea el movimiento y los disparos; los disparos la danan hasta romperla.
    m.teleport(0, player, 0.0f);
    m.buildWall(0.0f);
    const game::CharacterState s = walkOn(world, {70.0f, 0.0f, 27.0f}, 0.0f, -1.0f, 2.0f);
    printf("  andando hacia la pared: z final %.2f (cara de la pared 24.125)\n", s.position.z);
    check(s.position.z >= 24.125f + game::tuning::kBodyRadius - 0.01f, "la pared construida bloquea el movimiento");
    m.teleport(1, {70.0f, 0.0f, 15.0f}, 0.0f);
    const Vec3 dir = math3d::normalize(m.chestOf(1) - m.chestOf(0));
    game::ShotResult r = m.fireShot(0, m.chestOf(0), dir, 80.0f, 36.0f);
    const int wall = m.structures().wallFromCollider(0) >= 0 ? m.structures().wallFromCollider(0) : 0;
    float wallHealth = 0.0f;
    for (int i = 0; i < m.structures().capacity(); ++i) {
        if (m.structures().wall(i).active) wallHealth = m.structures().wall(i).health;
    }
    (void)wall;
    printf("  disparo a un bot detras de la pared: impacto %s, vida del bot %.0f, vida de la pared %.0f\n",
           r.hitEntity ? "si" : "no", m.body(1).state().health, wallHealth);
    check(!r.hitEntity && m.body(1).state().health == 100.0f && wallHealth == 114.0f,
          "la pared detiene el disparo y pierde 36 de sus 150 de vida");
    check(!m.lineOfSight(m.eyeOf(1), m.chestOf(0)), "la pared corta la linea de vision de la IA");
    int shotsToBreak = 1;
    while (m.structures().count() > 0 && shotsToBreak < 10) {
        m.fireShot(0, m.chestOf(0), dir, 80.0f, 36.0f);
        ++shotsToBreak;
    }
    r = m.fireShot(0, m.chestOf(0), dir, 80.0f, 36.0f);
    printf("  la pared cae al disparo %d; el siguiente %s al bot\n", shotsToBreak, r.hitEntity ? "alcanza" : "no alcanza");
    check(shotsToBreak == 5 && r.hitEntity && world.collision().dynamicCount() == 0,
          "tras 5 impactos (180 > 150) la pared se rompe y deja de bloquear");
}

// Cruza el segmento p->q la barrera z = 44, x en [56, 68]?
bool crossesBarrier(const Vec3& p, const Vec3& q) {
    if ((p.z - 44.0f) * (q.z - 44.0f) > 0.0f || fabsf(q.z - p.z) < 1e-5f) return false;
    const float t = (44.0f - p.z) / (q.z - p.z);
    const float x = p.x + (q.x - p.x) * t;
    return x > 55.6f && x < 68.4f;
}

int countEvents(const Match& m, u32 from, game::GameEventType type);

void testFloorsAndRamps(world::World& world, game::NavGrid& nav) {
    printf("\n8b) Suelos y rampas\n");
    const float pi = math3d::kPi;
    MatchConfig cfg = MatchConfig::classic();
    cfg.botCount = 1;
    Match m;
    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    const int walkable = nav.walkableCount();
    const Vec3 player{70.0f, 0.0f, 25.0f};
    m.teleport(0, player, 0.0f);
    m.teleport(1, {-80.0f, 0.0f, 80.0f}, 0.0f);

    // Rampa al norte: celda z 20..24, sube hacia -Z de 0 a 3 m en escalones de 0.375 m.
    const game::WallPlacement ramp = m.planWall(0.0f, game::PieceType::Ramp);
    printf("  rampa: x %.1f..%.1f z %.1f..%.1f y %.2f..%.2f -> %s\n", ramp.box.min.x, ramp.box.max.x, ramp.box.min.z,
           ramp.box.max.z, ramp.box.min.y, ramp.box.max.y, ramp.valid ? "valida" : game::placementErrorName(ramp.error));
    check(ramp.valid && ramp.dir == 3 && m.buildWall(0.0f, game::PieceType::Ramp) >= 0 &&
              world.collision().dynamicCount() == game::kRampSteps && m.structures().count() == 1,
          "se coloca una rampa (8 escalones de colision) y cuenta como una pieza");
    check(!m.planWall(0.0f, game::PieceType::Ramp).valid, "no se repite una rampa en la misma celda");

    // Subirla andando: la altura de los pies sigue a la rampa y nadie se mete en un solido.
    float topY = 0.0f, midY = 0.0f;
    bool pen = false, anyPen = false;
    for (int k = 1; k <= 12; ++k) {
        const game::CharacterState s = walk(world.collision(), player, 0.0f, 0.0f, -1.0f, k * 0.1f, &pen);
        anyPen |= pen;
        if (s.position.z <= 22.2f && s.position.z >= 21.8f) midY = s.position.y;
        if (s.position.y > topY) topY = s.position.y;
    }
    printf("  subiendo: a mitad de la rampa %.2f m, lo mas alto %.2f m\n", static_cast<double>(midY),
           static_cast<double>(topY));
    check(midY > 1.0f && midY < 2.0f && topY > 2.6f && !anyPen, "se sube la rampa andando (sin saltar)");
    world::RayHit hit;
    const bool blocks = world.collision().raycast({65.0f, 1.5f, 21.0f}, {75.0f, 1.5f, 21.0f}, world::kBlocksShots, &hit) &&
                        m.structures().wallFromCollider(hit.dynamicHandle) == 0;
    check(blocks, "la rampa para los disparos y se sabe que pieza es");

    // Suelo al oeste (celda x 64..68), a ras del suelo: se pisa (0.2 m) sin saltar.
    const game::WallPlacement floor = m.planWall(pi * 0.5f, game::PieceType::Floor);
    check(floor.valid && m.buildWall(pi * 0.5f, game::PieceType::Floor) >= 0 && m.structures().count() == 2,
          "se coloca un suelo");
    const game::CharacterState onFloor = walkOn(world, player, pi * 0.5f, -1.0f, 0.6f);
    printf("  suelo: x %.1f..%.1f, pies sobre el a %.2f m (x %.1f)\n", floor.box.min.x, floor.box.max.x,
           static_cast<double>(onFloor.position.y), static_cast<double>(onFloor.position.x));
    check(onFloor.position.x < 67.5f && fabsf(onFloor.position.y - 0.2f) < 0.01f, "se camina sobre el suelo");

    // En lo alto de la rampa el siguiente suelo va un piso mas arriba (3 m).
    m.teleport(0, {70.0f, 3.0f, 21.0f}, 0.0f);  // sobre lo alto de la rampa
    const float feet = m.body(0).state().position.y;
    const game::WallPlacement upper = m.planWall(0.0f, game::PieceType::Floor);
    printf("  desde la rampa (pies a %.2f m): suelo siguiente a %.2f m\n", static_cast<double>(feet),
           static_cast<double>(upper.box.min.y));
    check(fabsf(upper.box.min.y - 3.0f) < 0.01f, "desde lo alto de la rampa el suelo se pone un piso mas arriba");

    // La partida siguiente retira las piezas y deja la navegacion como estaba.
    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    check(world.collision().dynamicCount() == 0 && nav.walkableCount() == walkable,
          "al quitar suelos y rampas la colision y la navegacion vuelven a estar como antes");
}

void testStormZone(world::World& world, game::NavGrid& nav) {
    printf("\n13b) Tormenta que se cierra\n");
    game::StormConfig cfg;
    core::Random rng(7);
    game::StormZone z;
    z.reset(cfg, rng, &nav);
    check(z.radius() > 120.0f && !z.shrinking() && fabsf(z.timeLeft() - 60.0f) < 0.01f,
          "empieza abarcando todo el mapa, con el primer cierre a los 60 s");
    int closes = 0;
    bool nested = true, walkable = true, monotonic = true;
    float lastRadius = z.radius();
    Vec3 target = z.nextCenter();
    float targetRadius = z.nextRadius();
    for (int step = 1; step <= 60 * 420; ++step) {
        const float t = step / 60.0f;
        if (z.update(t, rng)) {
            ++closes;
            if (closes <= 2) printf("  cierre %d a los %.0f s: radio %.0f -> %.0f\n", closes, static_cast<double>(t),
                                    static_cast<double>(z.radius()), static_cast<double>(z.nextRadius()));
        }
        monotonic &= z.radius() <= lastRadius + 0.001f;
        lastRadius = z.radius();
        if (!z.shrinking() && z.nextRadius() < targetRadius - 0.01f) {
            // Acaba de elegirse el siguiente circulo: dentro del actual y sobre suelo transitable.
            nested &= math3d::length(z.nextCenter() - z.center()) + z.nextRadius() <= z.radius() + 0.01f;
            int ix, iz;
            walkable &= nav.nearestNode(z.nextCenter(), 0, &ix, &iz) && nav.walkable(ix, iz);
            target = z.nextCenter();
            targetRadius = z.nextRadius();
        }
    }
    printf("  %d cierres; radio final %.1f m, dano final %.0f/s\n", closes, static_cast<double>(z.radius()),
           static_cast<double>(z.damage()));
    check(closes == game::StormZone::kPhases && z.radius() < 0.01f && z.timeLeft() < 0.0f,
          "se cierra cada 40 s (8 cierres) hasta desaparecer la zona");
    check(nested && walkable && monotonic, "cada circulo esta dentro del anterior, en suelo transitable, y nunca crece");

    // Dano: con cierres rapidos, quien se queda fuera recibe el dano de la fase.
    MatchConfig mc = MatchConfig::classic();
    mc.botCount = 1;
    mc.stormCloses = true;
    mc.stormFirstClose = 1.0f;
    mc.stormPhaseSeconds = 2.0f;
    mc.stormShrinkSeconds = 1.0f;
    Match m;
    m.start(mc, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    m.teleport(1, {-80.0f, 0.0f, 80.0f}, 0.0f);
    u32 cursor = m.eventCount();
    for (int i = 0; i < 60 * 20 && m.phase() != game::MatchPhase::Finished; ++i) {
        m.step(game::FixedTimestep::kStep);
    }
    const int closing = countEvents(m, cursor, game::GameEventType::StormClosing);
    bool stormKill = false;
    for (int i = 0; i < m.killEventCount(); ++i) stormKill |= m.killEvent(i).killer == game::kStormKiller;
    printf("  partida con cierres cada 2 s: %d avisos de cierre, %s\n", closing,
           stormKill ? "la tormenta elimina" : "nadie cae por la tormenta");
    check(closing >= 1 && stormKill && m.stormDepthAt(0.0f, 0.0f) > 0.0f,
          "cada cierre se avisa y al final la tormenta lo cubre todo y elimina");
}

void testBotsRespectWalls(world::World& world, game::NavGrid& nav) {
    printf("\n9) La IA respeta las paredes construidas\n");
    MatchConfig cfg = MatchConfig::classic();
    cfg.botCount = 1;
    Match m;
    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    m.teleport(1, {-80.0f, 0.0f, 80.0f}, 0.0f);
    const Vec3 from{62.0f, 0.0f, 38.0f}, to{62.0f, 0.0f, 52.0f};
    Vec3 path[game::kMaxPathPoints];
    int n = nav.findPath(from, to, path, game::kMaxPathPoints);
    bool crossedBefore = false;
    Vec3 prev = from;
    for (int i = 0; i < n; ++i) {
        crossedBefore |= crossesBarrier(prev, path[i]);
        prev = path[i];
    }
    // Barrera de 3 paredes (12 m) en z = 44 entre los dos puntos.
    for (float x : {58.0f, 62.0f, 66.0f}) {
        m.teleport(0, {x, 0.0f, 46.9f}, 0.0f);
        m.buildWall(0.0f);
    }
    m.teleport(0, {-60.0f, 0.0f, 84.0f}, 0.0f);
    n = nav.findPath(from, to, path, game::kMaxPathPoints);
    bool crossesAfter = false;
    prev = from;
    for (int i = 0; i < n; ++i) {
        crossesAfter |= crossesBarrier(prev, path[i]);
        prev = path[i];
    }
    printf("  camino de (62, 38) a (62, 52): antes cruza la linea %s; con 3 paredes: %d puntos, cruza %s\n",
           crossedBefore ? "si" : "no", n, crossesAfter ? "SI" : "no");
    check(m.structures().count() == 3 && crossedBefore && n > 0 && !crossesAfter,
          "la navegacion se actualiza: el camino rodea la barrera");

    m.teleport(1, from, 0.0f);
    m.commandBotTravel(0, to);
    float best = 1e9f;
    bool crossed = false, penetrated = false;
    Vec3 last = from;
    for (int step = 0; step < 60 * 30; ++step) {
        m.setPlayerInput(game::PlayerInput{}, game::PlayerAim{});
        m.step(game::FixedTimestep::kStep);
        const Vec3 p = m.body(1).state().position;
        crossed |= crossesBarrier(last, p);
        penetrated |= penetrates(world.collision(), m.body(1).state());
        last = p;
        const float d = distXZ(p, to);
        if (d < best) best = d;
    }
    printf("  bot enviado al otro lado: distancia minima al destino %.2f m, atraviesa la barrera %s, "
           "dentro de una pared %s\n", best, crossed ? "SI" : "no", penetrated ? "SI" : "no");
    check(best < 2.0f && !crossed && !penetrated, "el bot rodea las paredes y llega a su destino");
}

void testFort(world::World& world, game::NavGrid& nav) {
    printf("\n10) Combate: un bot dispara al jugador y este se encierra con paredes\n");
    MatchConfig cfg = MatchConfig::classic();
    cfg.botCount = 1;
    cfg.seed = 3;
    cfg.startMaterials[0] = 100;  // 4 paredes de 10 de madera
    Match m;
    SimContext ctx;
    ctx.world = &world.collision();
    m.setShotObserver(onShot, &ctx);
    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    const Vec3 player{70.0f, 0.0f, 25.0f};
    m.teleport(0, player, 0.0f);
    m.teleport(1, {70.0f, 0.0f, 12.0f}, math3d::kPi);  // mirando al jugador
    auto run = [&](float seconds) {
        for (int step = 0; step < static_cast<int>(seconds * 60.0f); ++step) {
            m.setPlayerInput(game::PlayerInput{}, game::PlayerAim{});
            m.step(game::FixedTimestep::kStep);
            if (!m.alive(0)) break;
        }
    };
    run(6.0f);
    const float healthBefore = m.body(0).state().health;
    printf("  6 s a cielo abierto: vida del jugador %.0f, bot en estado %s\n", healthBefore,
           game::botStateName(m.bot(0).state));
    check(healthBefore < 100.0f, "a cielo abierto el bot ve al jugador y le acierta");

    // Una sola pared de 4 m se puede rodear (el bot flanquea); cuatro cierran al jugador.
    m.teleport(0, player, 0.0f);
    int built = 0;
    for (float yaw : {0.0f, math3d::kPi, math3d::kPi * 0.5f, -math3d::kPi * 0.5f}) built += m.buildWall(yaw) >= 0 ? 1 : 0;
    const int structureHitsBefore = m.stats().shotsHitStructure;
    int violations = 0;
    for (int step = 0; step < 60 * 10; ++step) {
        const float health = m.body(0).state().health;
        m.setPlayerInput(game::PlayerInput{}, game::PlayerAim{});
        m.step(game::FixedTimestep::kStep);
        if (m.structures().count() == 4 && m.body(0).state().health < health) ++violations;
    }
    printf("  10 s encerrado en 4 paredes: vida %.0f -> %.0f, disparos del bot contra las paredes %d, "
           "paredes en pie %d, bot en (%.0f, %.0f)\n", healthBefore, m.body(0).state().health,
           m.stats().shotsHitStructure - structureHitsBefore, m.structures().count(),
           m.body(1).state().position.x, m.body(1).state().position.z);
    check(built == 4 && violations == 0,
          "con las 4 paredes en pie ningun disparo alcanza al jugador");
    check(m.stats().shotsHitStructure > structureHitsBefore,
          "el bot no se queda quieto: dispara a la pared que tapa al jugador");
    check(ctx.hitsThroughWalls == 0, "ningun disparo del bot atraviesa la pared (raycast independiente)");
}

// --- 11) Sucesos de la partida -----------------------------------------------------------------

void testEvents(world::World& world, game::NavGrid& nav) {
    printf("\n11) Sucesos de la partida (de ellos salen los sonidos y las reacciones de las animaciones)\n");
    using game::GameEventType;
    MatchConfig cfg = MatchConfig::classic();
    cfg.botCount = 2;
    Match m;
    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    const Vec3 player{70.0f, 0.0f, 25.0f};
    m.teleport(0, player, 0.0f);
    m.teleport(1, {70.0f, 0.0f, 15.0f}, 0.0f);
    m.teleport(2, {-80.0f, 0.0f, 80.0f}, 0.0f);
    check(m.eventCount() == 0, "una partida nueva empieza sin sucesos");

    u32 cursor = 0;
    auto count = [&](GameEventType type, int entity, int other) {
        int n = 0;
        for (u32 i = cursor; i < m.eventCount(); ++i) {
            const game::GameEvent& e = m.event(i);
            if (e.type == type && (entity == -2 || e.entity == entity) && (other == -2 || e.other == other)) ++n;
        }
        return n;
    };

    game::PlayerAim aim;
    aim.eye = cameraEyeBehind(player);
    aim.direction = math3d::normalize(m.chestOf(1) - aim.eye);
    for (int i = 0; i < 3; ++i) m.playerShot(aim, 36.0f, 80.0f);
    check(count(GameEventType::Shot, 0, -2) == 3 && count(GameEventType::HitEntity, 1, 0) == 3 &&
              count(GameEventType::Eliminated, 1, 0) == 1,
          "3 disparos del jugador: 3 x disparo, 3 x impacto en el bot 1 y su eliminacion");

    cursor = m.eventCount();
    m.applyDamage(0, 10.0f, 2);
    check(count(GameEventType::PlayerHurt, 0, 2) == 1, "el dano al jugador emite 'jugador herido' con el atacante");

    cursor = m.eventCount();
    const int wall = m.buildWall(0.0f);
    check(wall >= 0 && count(GameEventType::WallBuilt, -2, -2) == 1, "construir emite 'pared construida'");
    game::PlayerAim at;
    at.eye = m.eyeOf(0);
    at.direction = math3d::normalize(Vec3{0.0f, -0.25f, -1.0f});
    cursor = m.eventCount();
    check(m.removeWallAt(at) && count(GameEventType::WallRemoved, -2, -2) == 1, "quitar una pared emite 'pared quitada'");

    // Gastar balas con el gatillo y recargar a mano.
    m.teleport(0, player, 0.0f);
    game::PlayerAim fire;
    fire.eye = cameraEyeBehind(player);
    fire.direction = {0.0f, 0.0f, -1.0f};
    fire.fire = true;
    for (int i = 0; i < 30; ++i) {
        m.setPlayerInput(game::PlayerInput{}, fire);
        m.step(game::FixedTimestep::kStep);
    }
    cursor = m.eventCount();
    game::PlayerCommands reload;
    reload.reload = true;
    m.setPlayerInput(game::PlayerInput{}, game::PlayerAim{}, reload);
    m.step(game::FixedTimestep::kStep);
    check(count(GameEventType::Reload, 0, -2) == 1, "recargar emite 'recarga'");

    // La cola es circular: con muchos sucesos, los ultimos kMaxEvents siguen accesibles.
    const u32 before = m.eventCount();
    for (int i = 0; i < Match::kMaxEvents + 10; ++i) m.playerShot(fire, 0.0f, 5.0f);
    const game::GameEvent& last = m.event(m.eventCount() - 1);
    check(m.eventCount() == before + Match::kMaxEvents + 10 && last.type == GameEventType::Shot && last.entity == 0,
          "la cola circular conserva los sucesos mas recientes");
}

// --- 12) Inventario, escudo, curas, pico y materiales ---------------------------------------------

// Pasos de partida con la entrada dada (el jugador quieto salvo lo que digan los comandos).
void runSteps(Match& m, int steps, const game::PlayerAim& aim = game::PlayerAim{},
              const game::PlayerCommands& first = game::PlayerCommands{}) {
    for (int i = 0; i < steps; ++i) {
        game::PlayerInput in;
        in.cameraYaw = 0.0f;
        m.setPlayerInput(in, aim, i == 0 ? first : game::PlayerCommands{});
        m.step(game::FixedTimestep::kStep);
    }
}

int countEvents(const Match& m, u32 from, game::GameEventType type) {
    int n = 0;
    for (u32 i = from; i < m.eventCount(); ++i) n += m.event(i).type == type ? 1 : 0;
    return n;
}

void testInventory(world::World& world, game::NavGrid& nav) {
    printf("\n12) Inventario, escudo, curas, pico y materiales\n");
    using game::ItemType;
    MatchConfig cfg = MatchConfig::classic();
    cfg.botCount = 1;
    Match m;
    m.setHarvestables(&world.harvestables());
    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    const Vec3 player{70.0f, 0.0f, 25.0f};
    m.teleport(0, player, 0.0f);
    m.teleport(1, {-80.0f, 0.0f, 80.0f}, 0.0f);
    const game::Inventory& inv = m.inventory();
    int wood = 0, stone = 0, metal = 0, colliders = 0;
    for (const world::Harvestable& h : world.harvestables()) {
        wood += h.resource == world::Resource::Wood ? 1 : 0;
        stone += h.resource == world::Resource::Stone ? 1 : 0;
        metal += h.resource == world::Resource::Metal ? 1 : 0;
        colliders += h.colliderCount;
    }
    printf("  objetos del mapa que se pueden picar: %d (%d de madera, %d de piedra, %d de metal), %d de %d cajas fijas\n",
           static_cast<int>(world.harvestables().size()), wood, stone, metal, colliders, world.collision().count());
    check(inv.slot(0).type == ItemType::AssaultRifle && inv.selected() == 0 && inv.slot(1).type == ItemType::Bandage &&
              inv.slot(1).count == 5 && inv.slot(2).type == ItemType::SmallShield && inv.slot(2).count == 3 &&
              m.materials().get(world::Resource::Wood) == 30,
          "inventario inicial: SCAR en la mano, 5 vendas, 3 minipociones y 30 de madera");

    // Cruceta: derecha dos veces -> minipociones. R las usa: 2 s despues, +25 de escudo.
    game::PlayerCommands next;
    next.cycleSlot = 1;
    runSteps(m, 1, game::PlayerAim{}, next);
    runSteps(m, 1, game::PlayerAim{}, next);
    check(inv.selected() == 2 && inv.activeType() == ItemType::SmallShield && !inv.activeWeapon(),
          "la cruceta cambia de ranura (SCAR -> vendas -> minipociones)");
    u32 cursor = m.eventCount();
    game::PlayerCommands use;
    use.useItem = true;
    runSteps(m, 60, game::PlayerAim{}, use);
    const bool notYet = m.body(0).state().shield == 0.0f && m.useProgress() > 0.4f;
    runSteps(m, 62);
    printf("  minipocion: a 1 s escudo %.0f, a 2 s escudo %.0f, quedan %d\n", notYet ? 0.0 : -1.0,
           static_cast<double>(m.body(0).state().shield), inv.slot(2).count);
    check(notYet && m.body(0).state().shield == 25.0f && inv.slot(2).count == 2 &&
              countEvents(m, cursor, game::GameEventType::ItemUsed) == 1,
          "la minipocion tarda 2 s, da +25 de escudo y gasta una unidad");

    m.applyDamage(0, 30.0f, 1);
    printf("  30 de dano con 25 de escudo: escudo %.0f, vida %.0f\n", static_cast<double>(m.body(0).state().shield),
           static_cast<double>(m.body(0).state().health));
    check(m.body(0).state().shield == 0.0f && m.body(0).state().health == 95.0f,
          "el escudo absorbe el dano antes que la vida");

    game::PlayerCommands prev;
    prev.cycleSlot = -1;
    runSteps(m, 1, game::PlayerAim{}, prev);  // vendas
    cursor = m.eventCount();
    runSteps(m, 5, game::PlayerAim{}, use);
    check(m.useProgress() < 0.0f && m.lastUseError() == game::UseError::FullHealth,
          "con 95 de vida no se usan vendas (curan hasta 75): aviso 'Vida al maximo'");
    m.applyDamage(0, 45.0f, 1);  // 50 de vida
    cursor = m.eventCount();
    runSteps(m, 60, game::PlayerAim{}, use);
    runSteps(m, 1, game::PlayerAim{}, next);  // cambiar de ranura a mitad: se cancela
    runSteps(m, 200);
    check(m.body(0).state().health == 50.0f && inv.slot(1).count == 5, "cambiar de ranura cancela la venda sin gastarla");
    check(countEvents(m, cursor, game::GameEventType::UseStarted) == 1 &&
              countEvents(m, cursor, game::GameEventType::UseCanceled) == 1,
          "empezar y cancelar el uso emiten sucesos (el sonido de beber empieza y se corta)");
    runSteps(m, 1, game::PlayerAim{}, prev);
    runSteps(m, static_cast<int>(3.5f * 60.0f) + 2, game::PlayerAim{}, use);
    printf("  vendas desde 50: vida %.0f, quedan %d\n", static_cast<double>(m.body(0).state().health), inv.slot(1).count);
    check(m.body(0).state().health == 65.0f && inv.slot(1).count == 4, "las vendas tardan 3.5 s y dan +15 de vida");

    // Coste de las paredes: 10 de madera.
    check(m.buildWall(0.0f) >= 0 && m.materials().get(world::Resource::Wood) == 20, "una pared gasta 10 de madera");
    m.buildWall(math3d::kPi * 0.5f);
    m.buildWall(math3d::kPi);
    const game::WallPlacement plan = m.planWall(-math3d::kPi * 0.5f);
    check(m.materials().get(world::Resource::Wood) == 0 && plan.error == game::PlacementError::NoMaterials,
          "sin madera no se construye (aviso 'Falta madera')");

    // Pico: un arbol da 8 de madera por golpe y tiene 300 de vida (6 golpes de 50): al
    // sexto desaparece.
    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    m.teleport(1, {80.0f, 0.0f, -80.0f}, 0.0f);
    int treeIndex = -1;
    for (int i = 0; i < static_cast<int>(world.harvestables().size()); ++i) {
        const world::Harvestable& h = world.harvestables()[i];
        if (h.resource != world::Resource::Wood || h.health != 300.0f) continue;
        // Un arbol con hueco al sur para el jugador, fuera de la tormenta del borde.
        const Vec3 c = (h.box.min + h.box.max) * 0.5f;
        if (world::stormDepth(c.x, c.z) > -5.0f) continue;
        const world::Aabb spot{{c.x - 0.5f, 0.1f, h.box.max.z + 0.3f}, {c.x + 0.5f, 1.8f, h.box.max.z + 1.4f}};
        if (!world.collision().overlapsAny(spot, world::kBlocksMovement)) {
            treeIndex = i;
            break;
        }
    }
    const world::Harvestable& tree = world.harvestables()[treeIndex];
    const Vec3 tc = (tree.box.min + tree.box.max) * 0.5f;
    m.teleport(0, {tc.x, 0.0f, tree.box.max.z + 0.9f}, 0.0f);  // al sur, mirando al norte
    game::PlayerCommands pick;
    pick.togglePickaxe = true;
    runSteps(m, 1, game::PlayerAim{}, pick);
    check(inv.pickaxe() && !inv.activeWeapon(), "TRIANGULO saca el pico (se guarda la SCAR)");
    game::PlayerAim swing;
    swing.eye = m.eyeOf(0);
    swing.direction = {0.0f, 0.0f, -1.0f};
    swing.fire = true;
    cursor = m.eventCount();
    const int woodBefore = m.materials().get(world::Resource::Wood);
    runSteps(m, 61, swing);  // R mantenido 1 s: golpes en 0, 0.5 y 1.0 s
    const int swings = countEvents(m, cursor, game::GameEventType::PickaxeSwing);
    printf("  1 s con R y el pico frente a un arbol: %d golpes, madera %d -> %d, vida del arbol %.0f\n", swings,
           woodBefore, m.materials().get(world::Resource::Wood), static_cast<double>(m.objectHealth(treeIndex)));
    check(swings == 3 && m.materials().get(world::Resource::Wood) == woodBefore + 24 &&
              countEvents(m, cursor, game::GameEventType::Harvest) == 3,
          "con R mantenido el pico golpea cada 0.5 s y cada golpe al arbol da 8 de madera");
    check(m.objectHealth(treeIndex) == 150.0f, "cada golpe quita 50 de vida al arbol");
    runSteps(m, 60 * 3, swing);
    const bool gone = m.objectDestroyed(treeIndex) && !world.collision().staticEnabled(tree.collider);
    world::RayHit through;
    const bool rayPasses = !world.collision().raycast(m.chestOf(0), m.chestOf(0) + Vec3{0.0f, 0.0f, -2.0f},
                                                      world::kBlocksShots, &through) ||
                           !tree.owns(through.collider);
    printf("  tras 4 s: madera %d, arbol %s\n", m.materials().get(world::Resource::Wood), gone ? "destruido" : "en pie");
    check(gone && rayPasses && m.materials().get(world::Resource::Wood) == woodBefore + 48 &&
              countEvents(m, cursor, game::GameEventType::ObjectDestroyed) == 1,
          "al sexto golpe el arbol desaparece (sin colision) tras dar 48 de madera");
    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    check(!m.objectDestroyed(treeIndex) && world.collision().staticEnabled(tree.collider) &&
              m.objectHealth(treeIndex) == 300.0f,
          "en la partida siguiente el arbol vuelve a estar en pie");
    m.teleport(1, {80.0f, 0.0f, -80.0f}, 0.0f);
    runSteps(m, 1, game::PlayerAim{}, pick);

    // Roca (piedra) y bot.
    int rockIndex = -1;
    for (int i = 0; i < static_cast<int>(world.harvestables().size()) && rockIndex < 0; ++i) {
        if (world.harvestables()[i].resource == world::Resource::Stone) rockIndex = i;
    }
    const world::Harvestable& rock = world.harvestables()[rockIndex];
    const Vec3 rc = (rock.box.min + rock.box.max) * 0.5f;
    m.teleport(0, {rc.x, 0.0f, rock.box.max.z + 0.8f}, 0.0f);
    m.swingPickaxe();
    check(m.materials().get(world::Resource::Stone) == rock.perHit, "una roca da piedra");
    m.teleport(0, player, 0.0f);
    m.teleport(1, {player.x, 0.0f, player.z - 1.3f}, math3d::kPi);
    swing.eye = m.eyeOf(0);
    m.setPlayerInput(game::PlayerInput{}, swing);
    m.swingPickaxe();
    printf("  pico contra un bot a 1.3 m: vida del bot %.0f\n", static_cast<double>(m.body(1).state().health));
    check(m.body(1).state().health == 80.0f, "el pico hace 20 de dano a un bot");

    // Volver a la SCAR con la cruceta y que siga disparando.
    runSteps(m, 1, game::PlayerAim{}, next);
    check(!inv.pickaxe() && inv.activeType() != ItemType::None, "la cruceta guarda el pico y vuelve a una ranura");
}

// --- 13) Tormenta del limite del mapa ------------------------------------------------------

void testStorm(world::World& world, game::NavGrid& nav) {
    printf("\n13) Limite del mapa: tormenta en lugar de pared invisible\n");
    check(world::stormDepth(88.0f, 0.0f) == 0.0f && world::stormDepth(100.0f, 0.0f) == 12.0f &&
              world::stormDepth(70.0f, 70.0f) < 0.0f && world::stormDepth(88.0f, 88.0f) > 0.0f,
          "zona segura: cuadrado de 176 m con esquinas redondeadas (tormenta a partir de 88 m)");
    MatchConfig cfg = MatchConfig::classic();
    cfg.botCount = 1;
    Match m;
    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    m.teleport(1, {0.0f, 0.0f, -60.0f}, 0.0f);

    // Antes el jugador se detenia en x = 95.5: ahora sigue andando hacia la tormenta.
    m.teleport(0, {93.0f, 0.0f, 0.0f}, -math3d::kPi * 0.5f);
    for (int i = 0; i < 60; ++i) {
        game::PlayerInput in;
        in.moveY = -1.0f;
        in.cameraYaw = -math3d::kPi * 0.5f;  // mirando al este
        m.setPlayerInput(in, game::PlayerAim{});
        m.step(game::FixedTimestep::kStep);
    }
    const float x = m.body(0).state().position.x;
    printf("  1 s corriendo hacia el este desde x = 93: x = %.1f, vida %.0f\n", static_cast<double>(x),
           static_cast<double>(m.body(0).state().health));
    check(x > 97.0f, "ya no hay pared invisible en el borde del mapa (x = 95.5)");

    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    m.teleport(1, {0.0f, 0.0f, -60.0f}, 0.0f);
    m.giveItem(game::ItemType::ShieldPotion, 1, game::Rarity::Rare);
    m.teleport(0, {80.0f, 0.0f, 0.0f}, 0.0f);
    runSteps(m, 180);
    check(m.body(0).state().health == 100.0f && !m.inStorm(0), "a 8 m del borde no hay dano");
    // Con escudo: la tormenta no lo toca, va directa a la vida (como en Fortnite).
    game::PlayerCommands next;
    next.cycleSlot = 1;
    for (int k = 0; k < 3; ++k) runSteps(m, 1, game::PlayerAim{}, next);
    game::PlayerCommands use;
    use.useItem = true;
    runSteps(m, 5 * 60 + 5, game::PlayerAim{}, use);
    const float shield = m.body(0).state().shield;
    u32 cursor = m.eventCount();
    m.teleport(0, {100.0f, 0.0f, 0.0f}, 0.0f);
    runSteps(m, 1);
    const float afterEnter = m.body(0).state().health;
    runSteps(m, 60);
    const float afterTick = m.body(0).state().health;
    printf("  dentro de la tormenta (12 m): vida 100 -> %.0f al entrar -> %.0f al segundo; escudo %.0f -> %.0f\n",
           static_cast<double>(afterEnter), static_cast<double>(afterTick), static_cast<double>(shield),
           static_cast<double>(m.body(0).state().shield));
    check(countEvents(m, cursor, game::GameEventType::StormEnter) == 1 && afterEnter == 70.0f && afterTick == 40.0f,
          "la tormenta quita 30 al entrar y 30 cada segundo");
    check(shield > 0.0f && m.body(0).state().shield == shield, "el dano de la tormenta no gasta el escudo");
    runSteps(m, 2 * 60);
    bool stormKill = false;
    for (int i = 0; i < m.killEventCount(); ++i) {
        stormKill |= m.killEvent(i).victim == 0 && m.killEvent(i).killer == game::kStormKiller;
    }
    check(!m.alive(0) && stormKill, "a los 4 golpes el jugador cae y la eliminacion es de la tormenta");

    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    m.teleport(1, {-100.0f, 0.0f, 20.0f}, 0.0f);
    runSteps(m, 1);
    check(m.body(1).state().health == 70.0f && m.inStorm(1), "los bots dentro de la tormenta tambien reciben dano");

    // El tope fisico esta al final del suelo exterior (144 m), lejos de la zona segura.
    m.teleport(1, {0.0f, 0.0f, -60.0f}, 0.0f);
    cfg.invulnerablePlayer = true;
    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    m.teleport(0, {140.0f, 0.0f, 0.0f}, -math3d::kPi * 0.5f);
    for (int i = 0; i < 120; ++i) {
        game::PlayerInput in;
        in.moveY = -1.0f;
        in.cameraYaw = -math3d::kPi * 0.5f;
        m.setPlayerInput(in, game::PlayerAim{});
        m.step(game::FixedTimestep::kStep);
    }
    check(m.body(0).state().position.x <= world::kWorldExtent, "al final del suelo exterior (144 m) hay un tope");
}

// --- 14) Botin, armas, autobus y bots de la fase 7 ---------------------------------------------

int countActive(const Match& m, game::ItemType type) {
    int n = 0;
    for (const game::Pickup& p : m.pickups()) n += p.active && p.kind == game::PickupKind::Item && p.item == type ? 1 : 0;
    return n;
}

void testMapObjects(world::World& world, game::NavGrid& nav) {
    printf("\n12b) Objetos del mapa: tejados, paredes que se rompen y materiales de construccion\n");
    // Tejado de la nave gris: quien cae del autobus o planea se posa encima.
    game::CharacterBody falling;
    falling.reset({0.0f, 0.0f, -11.0f}, 0.0f, world.collision());
    falling.place({0.0f, 40.0f, -11.0f});
    game::MoveIntent glide;
    glide.fallSpeed = 20.0f;
    for (int i = 0; i < 60 * 6; ++i) falling.step(glide, game::FixedTimestep::kStep, world.collision());
    printf("  cayendo sobre la nave gris desde 40 m: pies a %.1f m (%s)\n",
           static_cast<double>(falling.state().position.y), falling.state().grounded() ? "apoyado" : "en el aire");
    check(falling.state().grounded() && falling.state().position.y > 10.0f, "el tejado de las naves no se atraviesa");
    // Por fuera del tejado, los planeadores bajan resbalando hasta el suelo.
    falling.place({-5.0f, 40.0f, -11.0f});
    for (int i = 0; i < 60 * 6; ++i) falling.step(glide, game::FixedTimestep::kStep, world.collision());
    check(falling.state().position.y > 7.0f, "tambien en el faldon (lejos de la cumbrera)");

    // Pared lateral de la nave gris (x 7.6..8): 6 golpes de pico y se puede pasar.
    MatchConfig cfg = MatchConfig::classic();
    cfg.botCount = 1;
    Match m;
    m.setHarvestables(&world.harvestables());
    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    m.teleport(1, {-80.0f, 0.0f, 80.0f}, 0.0f);
    int panel = -1;
    for (int i = 0; i < static_cast<int>(world.harvestables().size()); ++i) {
        const world::Harvestable& h = world.harvestables()[i];
        if (h.box.min.x >= 7.5f && h.box.max.x <= 8.1f && h.box.min.z <= -18.0f && h.box.max.z >= -18.0f &&
            h.box.min.y < 0.5f) {
            panel = i;
        }
    }
    const Vec3 outside{9.2f, 0.0f, -18.0f};
    m.teleport(0, outside, math3d::kPi * 0.5f);  // mirando al oeste (-X)
    game::PlayerAim swing;
    swing.eye = m.eyeOf(0);
    swing.direction = {-1.0f, 0.0f, 0.0f};
    m.setPlayerInput(game::PlayerInput{}, swing);
    const int metalBefore = m.materials().get(world::Resource::Metal);
    int hits = 0;
    while (panel >= 0 && !m.objectDestroyed(panel) && hits < 10) {
        m.swingPickaxe();
        ++hits;
    }
    bool pen = false;
    const game::CharacterState in = walk(world.collision(), outside, math3d::kPi * 0.5f, 0.0f, -1.0f, 1.0f, &pen);
    printf("  panel de pared (%s): %d golpes, metal %d -> %d; andando hacia dentro: x %.1f\n",
           panel >= 0 ? "encontrado" : "no encontrado", hits, metalBefore, m.materials().get(world::Resource::Metal),
           static_cast<double>(in.position.x));
    check(panel >= 0 && m.objectDestroyed(panel) && hits == 6 && m.materials().get(world::Resource::Metal) > metalBefore,
          "las paredes de las naves se pican: dan metal y a los 6 golpes desaparecen");
    check(in.position.x < 7.0f && !pen, "por el hueco de la pared se entra en la nave");
    int roofs = 0;
    for (const world::Harvestable& h : world.harvestables()) roofs += h.colliderCount > 10 ? 1 : 0;
    check(roofs == 12, "los tejados (4 tramos por nave) tambien son objetos que se pueden romper");

    // Materiales de construccion: CUADRADO elige; cada uno con su vida.
    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    m.teleport(0, {70.0f, 0.0f, 25.0f}, 0.0f);
    m.teleport(1, {-80.0f, 0.0f, 80.0f}, 0.0f);
    m.giveMaterials(world::Resource::Metal, 10);
    game::PlayerCommands cycle;
    cycle.cycleMaterial = true;
    m.setPlayerInput(game::PlayerInput{}, game::PlayerAim{}, cycle);
    m.setPlayerInput(game::PlayerInput{}, game::PlayerAim{}, cycle);
    const int metalWall = m.buildWall(0.0f);
    const bool metalOk = metalWall >= 0 && m.structures().wall(metalWall).material == world::Resource::Metal &&
                         m.structures().wall(metalWall).maxHealth == 500.0f && m.materials().get(world::Resource::Metal) == 0;
    // Sin metal: la siguiente se hace con lo que haya (madera).
    const int woodBefore = m.materials().get(world::Resource::Wood);
    const int woodWall = m.buildWall(math3d::kPi * 0.5f);
    const bool fallbackOk = woodWall >= 0 && m.structures().wall(woodWall).material == world::Resource::Wood &&
                            m.structures().wall(woodWall).maxHealth == 150.0f &&
                            m.materials().get(world::Resource::Wood) == woodBefore - 10;
    printf("  pared de metal: %s (vida %.0f); sin metal, la siguiente de madera: %s\n", metalOk ? "si" : "no",
           metalWall >= 0 ? static_cast<double>(m.structures().wall(metalWall).maxHealth) : 0.0,
           fallbackOk ? "si" : "no");
    check(metalOk && m.buildMaterial() == world::Resource::Metal, "CUADRADO elige el metal: pared de 500 de vida");
    check(fallbackOk, "sin el material elegido se construye con el que haya");
    // Una partida nueva retira las paredes (la colision es compartida con las pruebas siguientes).
    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
}

void testBotInventory(world::World& world, game::NavGrid& nav) {
    printf("\n14b) Inventario de los bots\n");
    using game::ItemType;
    using game::Rarity;
    MatchConfig cfg;
    cfg.botCount = 1;
    cfg.startInBus = false;
    cfg.loot = true;
    cfg.chestCount = cfg.floorLootCount = cfg.ammoBoxCount = 0;
    cfg.airdropTimes[0] = cfg.airdropTimes[1] = 1e9f;
    cfg.stormCloses = false;
    Match m;
    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    const Vec3 at{70.0f, 0.0f, 25.0f};
    m.teleport(1, at, 0.0f);
    m.teleport(0, {-80.0f, 0.0f, 80.0f}, 0.0f);
    auto give = [&](ItemType type, Rarity rarity, int count) {
        game::Pickup p;
        p.kind = game::PickupKind::Item;
        p.item = type;
        p.rarity = rarity;
        p.count = count;
        p.position = at;
        game::Interactable it;
        it.kind = game::Interactable::Kind::Pickup;
        it.index = m.dropPickup(p);
        return m.interact(1, it);
    };
    const game::Bot& b = m.bot(0);
    const bool rifle = give(ItemType::AssaultRifle, Rarity::Common, 1);
    const bool shotgun = give(ItemType::Shotgun, Rarity::Rare, 1);
    const bool bandages = give(ItemType::Bandage, Rarity::Common, 5);
    const bool worseRifle = give(ItemType::AssaultRifle, Rarity::Common, 1);
    const bool betterRifle = give(ItemType::AssaultRifle, Rarity::Epic, 1);
    int weapons = 0, items = 0;
    for (int i = 0; i < game::Inventory::kSlots; ++i) {
        const ItemType t = b.bag.slot(i).type;
        weapons += game::isWeapon(t) ? 1 : 0;
        items += t != ItemType::None && !game::isWeapon(t) ? 1 : 0;
    }
    printf("  recoge fusil %s, escopeta %s, vendas %s, otro fusil igual %s, fusil epico %s -> %d armas y %d consumibles; "
           "en la mano: %s\n", rifle ? "si" : "no", shotgun ? "si" : "no", bandages ? "si" : "no",
           worseRifle ? "si" : "no", betterRifle ? "si" : "no", weapons, items, m.botWeaponName(0));
    check(rifle && shotgun && bandages && !worseRifle && betterRifle && weapons == 2 && items == 1 && b.armed,
          "los bots llevan varias armas y consumibles (y cambian un arma por otra mejor del mismo tipo)");

    // Herido y sin nadie cerca: usa las vendas (3.5 s, +15 de vida).
    m.applyDamage(1, 50.0f, game::kStormKiller, true);
    for (int i = 0; i < 60 * 5; ++i) m.step(game::FixedTimestep::kStep);
    printf("  con 50 de vida y vendas: %.0f de vida a los 5 s, quedan %d vendas\n",
           static_cast<double>(m.body(1).state().health), b.bag.count(ItemType::Bandage));
    check(m.body(1).state().health > 50.0f && b.bag.count(ItemType::Bandage) < 5,
          "un bot herido sin nadie a la vista se cura con lo que lleva");

    // Al caer, todo su inventario queda en el suelo.
    int before = 0;
    for (const game::Pickup& p : m.pickups()) before += p.active ? 1 : 0;
    m.applyDamage(1, 500.0f, 0, true);
    int weaponsDropped = 0, itemsDropped = 0;
    for (const game::Pickup& p : m.pickups()) {
        if (!p.active || p.kind != game::PickupKind::Item) continue;
        weaponsDropped += game::isWeapon(p.item) ? 1 : 0;
        itemsDropped += game::isWeapon(p.item) ? 0 : 1;
    }
    printf("  al caer: %d armas y %d consumibles en el suelo (antes habia %d objetos)\n", weaponsDropped, itemsDropped,
           before);
    check(!m.alive(1) && weaponsDropped >= 2 && itemsDropped >= 1, "al caer suelta todas sus armas y consumibles");
}

void testHeadshotsAndCrouch(world::World& world, game::NavGrid& nav) {
    printf("\n14c) Disparos a la cabeza, agacharse y ayuda del francotirador\n");
    world.mutableCollision().clearDynamic();  // paredes que dejaron las pruebas anteriores
    MatchConfig cfg = MatchConfig::classic();
    cfg.botCount = 1;
    cfg.aimAssistDegrees = 0.0f;
    Match m;
    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    const Vec3 player{70.0f, 0.0f, 25.0f};
    m.teleport(0, player, 0.0f);
    m.teleport(1, {70.0f, 0.0f, 15.0f}, 0.0f);
    game::PlayerAim aim;
    aim.eye = m.eyeOf(0);
    aim.direction = math3d::normalize(m.body(1).state().position + Vec3{0.0f, 1.72f, 0.0f} - aim.eye);
    const game::ShotResult head = m.playerShot(aim, 30.0f, 100.0f);
    const float afterHead = m.body(1).state().health;
    aim.direction = math3d::normalize(m.chestOf(1) - aim.eye);
    const game::ShotResult chest = m.playerShot(aim, 30.0f, 100.0f);
    printf("  30 de dano: a la cabeza %s -> vida %.0f; al pecho %s -> vida %.0f\n", head.headshot ? "(cabeza)" : "(cuerpo)",
           static_cast<double>(afterHead), chest.headshot ? "(cabeza)" : "(cuerpo)",
           static_cast<double>(m.body(1).state().health));
    check(head.hitEntity && head.headshot && afterHead == 40.0f && chest.hitEntity && !chest.headshot &&
              m.body(1).state().health == 10.0f && m.playerHeadshots() == 1,
          "un disparo a la cabeza hace el doble de dano (al pecho, el normal)");

    // Agachado: caja de impacto mas baja y los bots fallan mas (misma partida, mismo bot).
    auto hitsOnPlayer = [&](bool crouch) {
        MatchConfig c = MatchConfig::classic();
        c.botCount = 1;
        c.invulnerablePlayer = true;
        c.seed = 77;
        Match fight;
        fight.start(c, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
        fight.teleport(0, player, 0.0f);
        fight.teleport(1, {70.0f, 0.0f, 7.0f}, 0.0f);
        game::PlayerCommands first;
        first.toggleCrouch = crouch;
        const u32 from = fight.eventCount();
        int hits = 0, heads = 0;
        u32 cursor = from;
        for (int i = 0; i < 60 * 20; ++i) {
            game::PlayerInput in;
            fight.setPlayerInput(in, game::PlayerAim{}, i == 0 ? first : game::PlayerCommands{});
            fight.step(game::FixedTimestep::kStep);
            for (; cursor < fight.eventCount(); ++cursor) {
                const game::GameEvent& e = fight.event(cursor);
                if (e.type == game::GameEventType::HitEntity && e.entity == 0) {
                    ++hits;
                    heads += e.amount;
                }
            }
        }
        return std::make_pair(hits * 1000 + heads, fight.playerCrouched());
    };
    const auto standing = hitsOnPlayer(false), crouched = hitsOnPlayer(true);
    printf("  bot a 18 m durante 20 s: de pie %d impactos (%d en la cabeza); agachado %d (%d en la cabeza)\n",
           standing.first / 1000, standing.first % 1000, crouched.first / 1000, crouched.first % 1000);
    check(crouched.second && crouched.first / 1000 < standing.first / 1000,
          "agachado recibe menos impactos que de pie");
    const int botHeads = standing.first % 1000 + crouched.first % 1000;
    const int botHits = standing.first / 1000 + crouched.first / 1000;
    check(botHeads * 6 <= botHits + 3, "los bots rara vez aciertan en la cabeza");

    // Francotirador con L: la ayuda de punteria llega a 4 grados (con el resto, 1).
    MatchConfig real;
    real.botCount = 1;
    real.startInBus = false;
    real.loot = false;
    real.stormCloses = false;
    Match s;
    s.start(real, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    s.teleport(0, player, 0.0f);
    s.teleport(1, {70.0f, 0.0f, 12.0f}, 0.0f);
    s.giveItem(game::ItemType::Sniper, 1, game::Rarity::Legendary);
    s.giveAmmo(game::AmmoType::Heavy, 12);
    game::PlayerCommands put;
    put.togglePickaxe = true;  // guarda el pico: el francotirador en la mano
    game::PlayerAim off;
    off.eye = s.eyeOf(0);
    const Vec3 to = math3d::normalize(s.chestOf(1) - off.eye);
    off.direction = math3d::normalize(game::detail::rotateY(to, math3d::toRadians(1.2f)));  // ~1.8 grados con la altura
    auto assistWith = [&](bool aiming, const game::PlayerCommands& cmd) {
        game::PlayerInput in;
        in.aiming = aiming;
        s.setPlayerInput(in, off, cmd);
        s.step(game::FixedTimestep::kStep);
        return s.playerAssistTarget();
    };
    const int hip = assistWith(false, put);
    const int scoped = assistWith(true, game::PlayerCommands{});
    printf("  francotirador a ~1.8 grados del bot: sin L ayuda a %d, con L ayuda a %d\n", hip, scoped);
    check(s.inventory().activeType() == game::ItemType::Sniper && hip < 0 && scoped == 1,
          "con el francotirador y L la ayuda de punteria es mayor");
}

void testLoot(world::World& world, game::NavGrid& nav) {
    printf("\n14) Armas, botin, autobus y bots (fase 7)\n");
    using game::ItemType;
    using game::Rarity;
    // Catalogo de armas.
    check(game::weaponConfig(ItemType::AssaultRifle, Rarity::Common).damage == 30.0f &&
              game::weaponConfig(ItemType::AssaultRifle, Rarity::Legendary).damage == 36.0f &&
              std::string(game::weaponConfig(ItemType::AssaultRifle, Rarity::Epic).name) == "SCAR" &&
              game::weaponConfig(ItemType::Shotgun, Rarity::Rare).pellets == 10 &&
              game::weaponConfig(ItemType::Sniper, Rarity::Legendary).damage == 116.0f &&
              !game::weaponHasRarity(ItemType::Sniper, Rarity::Common) &&
              !game::weaponHasRarity(ItemType::Shotgun, Rarity::Common),
          "armas con dano por rareza (fusil 30-36, escopeta de 10 perdigones, francotirador 105-116)");

    // Reparto del botin.
    MatchConfig cfg;  // reglas de la fase 7
    cfg.botCount = 4;
    cfg.startInBus = false;
    Match m;
    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    int chests = 0, boxes = 0, unsafe = 0;
    for (const game::Container& c : m.containers()) {
        chests += c.kind == game::ContainerKind::Chest ? 1 : 0;
        boxes += c.kind == game::ContainerKind::AmmoBox ? 1 : 0;
        unsafe += world::stormDepth(c.position.x, c.position.z) > -5.0f ? 1 : 0;
    }
    int weapons = 0, items = 0;
    for (const game::Pickup& p : m.pickups()) {
        if (!p.active) continue;
        ++items;
        weapons += game::isWeapon(p.item) && p.kind == game::PickupKind::Item ? 1 : 0;
        unsafe += world::stormDepth(p.position.x, p.position.z) > -5.0f ? 1 : 0;
    }
    printf("  reparto: %d cofres, %d cajas de municion, %d objetos en el suelo (%d armas)\n", chests, boxes, items, weapons);
    check(chests == cfg.chestCount && boxes == cfg.ammoBoxCount && items >= cfg.floorLootCount && unsafe == 0,
          "cofres, cajas de municion y botin de suelo repartidos fuera de la tormenta");
    check(m.inventory().pickaxe() && m.inventory().slot(0).type == ItemType::None,
          "se empieza solo con el pico (como en Fortnite)");

    // Abrir un cofre con CUADRADO y recoger lo que suelta.
    const game::Container chest = m.containers()[0];
    const Vec3 front = chest.position + game::forwardFromYaw(chest.yaw) * 1.3f;
    m.teleport(0, front, game::yawTowards(front, chest.position));
    for (int b = 1; b <= 4; ++b) m.teleport(b, {-80.0f, 0.0f, 80.0f - b * 3.0f}, 0.0f);
    game::PlayerInput look;
    look.cameraYaw = game::yawTowards(front, chest.position);
    m.setPlayerInput(look, game::PlayerAim{});
    const game::Interactable target = m.playerInteractable();
    u32 cursor = m.eventCount();
    game::PlayerCommands square;
    square.reload = true;  // CUADRADO
    m.setPlayerInput(look, game::PlayerAim{}, square);
    m.step(game::FixedTimestep::kStep);
    check(target.kind == game::Interactable::Kind::Container && m.containers()[0].opened &&
              countEvents(m, cursor, game::GameEventType::ContainerOpened) == 1,
          "CUADRADO junto a un cofre lo abre");
    int picked = 0;
    for (int k = 0; k < 6; ++k) {
        m.setPlayerInput(look, game::PlayerAim{}, square);
        m.step(game::FixedTimestep::kStep);
        m.setPlayerInput(look, game::PlayerAim{});
        m.step(game::FixedTimestep::kStep);
    }
    for (int i = 0; i < game::Inventory::kSlots; ++i) picked += game::isWeapon(m.inventory().slot(i).type) ? 1 : 0;
    // La municion se recoge sola al pasar por encima.
    for (const game::Pickup& p : m.pickups()) {
        if (p.active && p.kind == game::PickupKind::Ammo && distXZ(p.position, chest.position) < 2.0f) {
            m.teleport(0, p.position, look.cameraYaw);
            m.setPlayerInput(look, game::PlayerAim{});
            m.step(game::FixedTimestep::kStep);
        }
    }
    const game::Weapon& got = m.playerWeapon();
    printf("  del cofre: %s %s, municion de reserva %d\n", got.config().name, got.config().rarity, m.playerReserve());
    check(picked == 1 && m.playerReserve() > 0, "el cofre suelta un arma y su municion; CUADRADO la recoge y la municion se coge sola");

    // Inventario lleno: se cambia por lo de la ranura seleccionada (que cae al suelo).
    m.giveItem(ItemType::Bandage, 5);
    m.giveItem(ItemType::SmallShield, 3, Rarity::Uncommon);
    m.giveItem(ItemType::Medkit, 1, Rarity::Uncommon);
    m.giveItem(ItemType::ShieldPotion, 1, Rarity::Rare);
    game::Pickup sniper;
    sniper.kind = game::PickupKind::Item;
    sniper.item = ItemType::Sniper;
    sniper.rarity = Rarity::Legendary;
    sniper.count = 1;
    sniper.position = m.body(0).state().position + game::forwardFromYaw(look.cameraYaw) * 0.8f;
    m.dropPickup(sniper);
    const ItemType held = m.inventory().slot(m.inventory().selected()).type;
    const int before = countActive(m, held);
    m.setPlayerInput(look, game::PlayerAim{}, square);
    m.step(game::FixedTimestep::kStep);
    check(m.inventory().full() && m.inventory().slot(m.inventory().selected()).type == ItemType::Sniper &&
              countActive(m, held) == before + 1,
          "con el inventario lleno, recoger cambia lo que se lleva en la mano (que queda en el suelo)");

    // Escopeta: dano segun la distancia.
    MatchConfig dummy = MatchConfig::classic();
    dummy.botCount = 1;
    dummy.startItems[0] = {ItemType::Shotgun, 1, Rarity::Legendary};
    dummy.startAmmo[static_cast<int>(game::AmmoType::Shells)] = 20;
    dummy.weaponSpread = true;
    dummy.aimAssistDegrees = 0.0f;
    float closeDamage = 0.0f, farDamage = 0.0f;
    for (float distance : {3.0f, 20.0f}) {
        Match s;
        s.start(dummy, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
        const Vec3 p{70.0f, 0.0f, 25.0f};
        s.teleport(0, p, 0.0f);
        s.teleport(1, {70.0f, 0.0f, 25.0f - distance}, 0.0f);
        game::PlayerAim aim;
        aim.eye = s.chestOf(0) + Vec3{0.0f, 0.0f, 0.3f};
        aim.direction = math3d::normalize(s.chestOf(1) - aim.eye);
        aim.fire = true;
        game::PlayerInput in;
        in.cameraYaw = 0.0f;
        s.setPlayerInput(in, aim);
        s.step(game::FixedTimestep::kStep);
        (distance < 5.0f ? closeDamage : farDamage) = 100.0f - s.body(1).state().health;
    }
    printf("  escopeta legendaria (105): %.0f de dano a 3 m, %.0f a 20 m\n", static_cast<double>(closeDamage),
           static_cast<double>(farDamage));
    check(closeDamage >= 70.0f && farDamage < closeDamage * 0.5f, "la escopeta pega fuerte de cerca y poco de lejos");

    // Dispersion: a 30 m, disparando sin apuntar se fallan balas; apuntando quieto, el
    // primer disparo (cada 0.5 s) va exacto.
    MatchConfig spreadCfg = MatchConfig::classic();
    spreadCfg.botCount = 1;
    spreadCfg.weaponSpread = true;
    spreadCfg.aimAssistDegrees = 0.0f;
    spreadCfg.invulnerablePlayer = true;
    int hipHits = 0, aimHits = 0;
    for (int mode = 0; mode < 2; ++mode) {
        for (int shot = 0; shot < 10; ++shot) {
            // Una partida por disparo: el bot no muere y cada disparo es el primero.
            Match s;
            spreadCfg.seed = 100 + static_cast<u32>(shot);
            s.start(spreadCfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
            s.teleport(0, {70.0f, 0.0f, 25.0f}, 0.0f);
            s.teleport(1, {70.0f, 0.0f, -5.0f}, 0.0f);
            const float hp = s.body(1).state().health;
            game::PlayerAim aim;
            aim.eye = s.chestOf(0) + Vec3{0.0f, 0.0f, 0.3f};
            aim.direction = math3d::normalize(s.chestOf(1) - aim.eye);
            aim.fire = true;
            game::PlayerInput in;
            in.aiming = mode == 1;
            s.setPlayerInput(in, aim);
            s.step(game::FixedTimestep::kStep);
            (mode == 0 ? hipHits : aimHits) += s.body(1).state().health < hp ? 1 : 0;
        }
    }
    printf("  fusil a 30 m: %d/10 impactos sin apuntar, %d apuntando quieto\n", hipHits, aimHits);
    check(hipHits < 10 && aimHits >= 9, "sin apuntar hay dispersion; apuntando quieto, el primer disparo es preciso");

    // Autobus y caida.
    MatchConfig busCfg;
    busCfg.botCount = 6;
    Match bus;
    bus.start(busCfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    bool allInBus = true;
    for (int id = 0; id <= 6; ++id) allInBus &= bus.airState(id) == game::AirState::Bus;
    bus.setPlayerInput(game::PlayerInput{}, game::PlayerAim{});
    for (int k = 0; k < 60; ++k) bus.step(game::FixedTimestep::kStep);
    check(allInBus && bus.airState(0) == game::AirState::Bus &&
              math3d::length(bus.body(0).state().position - busCfg.busPosition) < 0.01f,
          "todos empiezan en el autobus, quieto en el cielo");
    cursor = bus.eventCount();
    game::PlayerInput jump;
    jump.jumpPressed = true;
    jump.moveY = -1.0f;
    bus.setPlayerInput(jump, game::PlayerAim{});
    bus.step(game::FixedTimestep::kStep);
    const bool jumped = countEvents(bus, cursor, game::GameEventType::BusJump) >= 1 && bus.airState(0) == game::AirState::Freefall;
    const float jumpedAt = bus.elapsed();
    float glideAt = -1.0f, landedAt = -1.0f, glideHeight = 0.0f;
    for (int k = 0; k < 60 * 30 && landedAt < 0.0f; ++k) {
        game::PlayerInput fly;
        fly.moveY = -1.0f;
        bus.setPlayerInput(fly, game::PlayerAim{});
        bus.step(game::FixedTimestep::kStep);
        if (glideAt < 0.0f && bus.airState(0) == game::AirState::Glide) {
            glideAt = bus.elapsed();
            glideHeight = bus.heightAboveGround(0);
        }
        if (bus.airState(0) == game::AirState::Ground) landedAt = bus.elapsed();
    }
    printf("  salto: caida libre %.1f s, ala delta abierta a %.0f m, en el suelo a los %.1f s; %.0f m del autobus\n",
           static_cast<double>(glideAt - jumpedAt), static_cast<double>(glideHeight),
           static_cast<double>(landedAt - jumpedAt), static_cast<double>(distXZ(bus.body(0).state().position, busCfg.busPosition)));
    check(jumped && glideAt > 0.0f && landedAt > 0.0f && glideHeight < 30.0f && landedAt - jumpedAt < 15.0f,
          "X salta del autobus, el ala delta se abre sola cerca del suelo y se aterriza");
    check(distXZ(bus.body(0).state().position, busCfg.busPosition) > 30.0f, "en el aire se dirige la caida (avanza en horizontal)");
    for (int k = 0; k < 60 * 25; ++k) {
        bus.setPlayerInput(game::PlayerInput{}, game::PlayerAim{});
        bus.step(game::FixedTimestep::kStep);
    }
    int landedBots = 0;
    for (int id = 1; id <= 6; ++id) landedBots += bus.airState(id) == game::AirState::Ground || !bus.alive(id) ? 1 : 0;
    check(landedBots == 6, "los bots saltan solos en momentos distintos y aterrizan");

    // Bots que se cubren con una pared al recibir disparos.
    MatchConfig coverCfg = MatchConfig::classic();
    coverCfg.botCount = 1;
    coverCfg.botsBuild = true;
    Match cover;
    cover.start(coverCfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    int botWalls = 0;
    for (int attempt = 0; attempt < 20 && botWalls == 0; ++attempt) {
        cover.teleport(0, {70.0f, 0.0f, 25.0f}, 0.0f);
        cover.teleport(1, {70.0f, 0.0f, 10.0f}, math3d::kPi);
        game::PlayerAim aim;
        aim.eye = cover.chestOf(0);
        aim.direction = math3d::normalize(cover.chestOf(1) - aim.eye);
        cover.playerShot(aim, 1.0f, 80.0f);
        for (int k = 0; k < 150; ++k) {
            cover.setPlayerInput(game::PlayerInput{}, game::PlayerAim{});
            cover.step(game::FixedTimestep::kStep);
        }
        botWalls = cover.structures().countOwnedBy(1);
    }
    check(botWalls > 0 && cover.structures().count() == 0,
          "un bot al que disparan se cubre con una pared (que no cuenta en el limite del jugador)");

    // Airdrop: cae con el globo y trae armas legendarias.
    MatchConfig dropCfg;
    dropCfg.botCount = 1;
    dropCfg.startInBus = false;
    dropCfg.airdropTimes[0] = 1.0f;
    dropCfg.invulnerablePlayer = true;
    Match drop;
    drop.start(dropCfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    drop.teleport(1, {-80.0f, 0.0f, 80.0f}, 0.0f);
    int airdrop = -1;
    float landedTime = -1.0f;
    for (int k = 0; k < 60 * 30; ++k) {
        drop.setPlayerInput(game::PlayerInput{}, game::PlayerAim{});
        drop.step(game::FixedTimestep::kStep);
        for (size_t c = 0; c < drop.containers().size(); ++c) {
            if (drop.containers()[c].kind == game::ContainerKind::Airdrop) airdrop = static_cast<int>(c);
        }
        if (airdrop >= 0 && landedTime < 0.0f && drop.containers()[airdrop].landed) landedTime = drop.elapsed();
    }
    int legendaries = 0;
    if (airdrop >= 0) {
        game::Interactable it;
        it.kind = game::Interactable::Kind::Container;
        it.index = airdrop;
        drop.interact(0, it);
        for (const game::Pickup& p : drop.pickups()) {
            legendaries += p.active && game::isWeapon(p.item) && p.rarity == Rarity::Legendary &&
                                   distXZ(p.position, drop.containers()[airdrop].position) < 2.5f
                               ? 1
                               : 0;
        }
    }
    printf("  airdrop: tarda %.0f s en caer; trae %d armas legendarias\n", static_cast<double>(landedTime - 1.0f), legendaries);
    check(airdrop >= 0 && landedTime > 10.0f && legendaries == 2, "el airdrop baja con su globo y trae dos armas legendarias");
}

void testRealMatches(world::World& world, game::NavGrid& nav) {
    printf("\n15) Partidas completas con las reglas de la fase 7 (autobus, botin, bots que cazan y construyen)\n");
    printf("  bots semilla  fin  duracion ganador  armados_1min cofres airdrops paredes atascos max_atasco\n");
    struct Case {
        int bots;
        u32 seed;
    };
    const Case cases[] = {{10, 11}, {10, 12}, {10, 13}, {30, 11}};
    bool allFinished = true, noPenetration = true, noWallHits = true, armedOk = true, stuckOk = true, builds = false;
    for (const Case& c : cases) {
        const SimResult r = simulate(world, nav, c.bots, c.seed, 20.0f * 60.0f, true);
        printf("  %4d %6u %4s %6.0f s %-8d %8d/%-3d %6d %8d %7d %7d %8d s  (dentro de solidos %d, a traves %d)\n",
               c.bots, static_cast<unsigned>(c.seed), r.finished ? "si" : "NO", static_cast<double>(r.duration), r.winner,
               r.armedAt60, c.bots, r.chestsOpened, r.airdropsOpened, r.stats.wallsBuilt, r.stats.stuckEvents,
               r.maxStuckSeconds, r.penetrations, r.shots.hitsThroughWalls);
        if (r.shots.hitsThroughWalls > 0) {
            const game::ShotRecord& b = r.shots.firstBad;
            printf("    primer caso: tirador %d (%.1f %.1f %.1f) -> victima %d (%.1f %.1f %.1f); caja %d dinamica %d en t=%.2f\n",
                   b.shooter, b.origin.x, b.origin.y, b.origin.z, b.victim, b.end.x, b.end.y, b.end.z,
                   r.shots.badHit.collider, r.shots.badHit.dynamicHandle, static_cast<double>(r.shots.badHit.t));
        }
        allFinished &= r.finished;
        noPenetration &= r.penetrations == 0;
        noWallHits &= r.shots.hitsThroughWalls == 0;
        armedOk &= r.armedAt60 * 10 >= c.bots * 7;
        stuckOk &= r.maxStuckSeconds <= 8;
        builds |= r.stats.wallsBuilt > 0;
    }
    check(allFinished, "todas las partidas terminan (como mucho 20 min)");
    check(armedOk, "al minuto, al menos el 70 % de los bots ha encontrado un arma (o ya ha caido)");
    check(builds, "los bots construyen paredes para cubrirse");
    check(noPenetration && noWallHits, "nadie queda dentro de un solido y ningun disparo atraviesa paredes");
    check(stuckOk, "ningun bot se queda atascado mas de 8 s seguidos");
}


// --- Parche (fase 8), bloque A ------------------------------------------------------------------

void stepWith(Match& m, const game::PlayerInput& in, const game::PlayerAim& aim,
              const game::PlayerCommands& cmd = game::PlayerCommands{}) {
    m.setPlayerInput(in, aim, cmd);
    m.step(game::FixedTimestep::kStep);
}

// Ranuras + objetos del suelo de cada tipo (para comprobar que nada se pierde ni se duplica).
int totalOf(const Match& m, game::ItemType type) {
    int n = m.inventory().count(type);
    for (const game::Pickup& p : m.pickups()) {
        if (p.active && p.kind == game::PickupKind::Item && p.item == type) n += p.count;
    }
    return n;
}

int selectSlotOf(Match& m, game::ItemType type) {
    game::PlayerInput in;
    for (int k = 0; k < 6; ++k) {
        if (!m.inventory().pickaxe() && m.inventory().activeType() == type) return m.inventory().selected();
        game::PlayerCommands c;
        if (m.inventory().pickaxe()) c.togglePickaxe = true;
        else c.cycleSlot = 1;
        stepWith(m, in, game::PlayerAim{}, c);
    }
    return -1;
}

void testPatchA(world::World& world, game::NavGrid& nav) {
    printf("\n16) Parche A: cambiar objetos (tambien curativos), curarse quieto, agacharse y retroceso\n");
    world.mutableCollision().clearDynamic();
    using game::ItemType;
    using game::Rarity;
    MatchConfig cfg;
    cfg.botCount = 1;
    cfg.startInBus = false;
    cfg.loot = false;
    cfg.stormCloses = false;
    cfg.useSeconds[static_cast<int>(ItemType::Bandage)] = 1.2f;  // tiempo de uso configurable
    Match m;
    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    const Vec3 spot{70.0f, 0.0f, 25.0f};
    m.teleport(0, spot, 0.0f);
    m.teleport(1, {-80.0f, 0.0f, 80.0f}, 0.0f);
    m.giveItem(ItemType::AssaultRifle, 1, Rarity::Rare);
    m.giveItem(ItemType::Bandage, 5);
    m.giveItem(ItemType::SmallShield, 3, Rarity::Uncommon);
    m.giveItem(ItemType::Medkit, 1, Rarity::Uncommon);
    m.giveItem(ItemType::ShieldPotion, 1, Rarity::Rare);
    m.giveAmmo(game::AmmoType::Medium, 60);
    game::PlayerInput still;
    // El fusil dispara hasta quedarse con 7 balas en el cargador.
    selectSlotOf(m, ItemType::AssaultRifle);
    game::PlayerAim sky;
    sky.eye = m.eyeOf(0);
    sky.direction = math3d::normalize(Vec3{0.0f, 1.0f, -1.0f});
    sky.fire = true;
    for (int i = 0; i < 60 * 8 && m.inventory().slot(0).weapon.ammo() > 7; ++i) stepWith(m, still, sky);
    stepWith(m, still, game::PlayerAim{});
    const int loaded = m.inventory().slot(0).weapon.ammo();
    const int reserve = m.inventory().ammo(game::AmmoType::Medium);

    // Con las vendas en la mano, CUADRADO sobre un arma: el inventario esta lleno y hay que
    // elegir que soltar (el selector de la aplicacion manda la ranura elegida).
    const int bandSlot = selectSlotOf(m, ItemType::Bandage);
    game::Pickup sniper;
    sniper.kind = game::PickupKind::Item;
    sniper.item = ItemType::Sniper;
    sniper.rarity = Rarity::Legendary;
    sniper.count = 1;
    sniper.position = spot + Vec3{0.0f, 0.0f, -0.8f};
    const int sniperIndex = m.dropPickup(sniper);
    const int bandagesBefore = totalOf(m, ItemType::Bandage), snipersBefore = totalOf(m, ItemType::Sniper);
    const bool full = m.inventory().full() && m.pickupRoom(sniperIndex) == 0;
    game::PlayerCommands swap;
    swap.swapPickup = sniperIndex;
    swap.swapSlot = bandSlot;
    stepWith(m, still, game::PlayerAim{}, swap);
    int snipersOnFloor = 0;
    for (const game::Pickup& p : m.pickups()) snipersOnFloor += p.active && p.item == ItemType::Sniper ? 1 : 0;
    check(full && m.inventory().slot(bandSlot).type == ItemType::Sniper && totalOf(m, ItemType::Bandage) == bandagesBefore &&
              totalOf(m, ItemType::Sniper) == snipersBefore && snipersOnFloor == 0,
          "inventario lleno con un curativo en la mano: se cambia por la ranura elegida y las vendas quedan en el suelo");
    // Las vendas soltadas se recogen de nuevo eligiendo la ranura del fusil (con 7 balas).
    int bandIndex = -1;
    for (size_t i = 0; i < m.pickups().size(); ++i) {
        const game::Pickup& p = m.pickups()[i];
        if (p.active && p.item == ItemType::Bandage) bandIndex = static_cast<int>(i);
    }
    swap.swapPickup = bandIndex;
    swap.swapSlot = 0;
    stepWith(m, still, game::PlayerAim{}, swap);
    int rifleIndex = -1;
    for (size_t i = 0; i < m.pickups().size(); ++i) {
        const game::Pickup& p = m.pickups()[i];
        if (p.active && p.item == ItemType::AssaultRifle) rifleIndex = static_cast<int>(i);
    }
    check(bandIndex >= 0 && m.inventory().slot(0).type == ItemType::Bandage && rifleIndex >= 0 &&
              m.pickups()[rifleIndex].loaded == loaded,
          "el objeto sustituido aparece en el suelo y se puede recoger (el fusil cae con sus balas)");
    swap.swapPickup = rifleIndex;
    swap.swapSlot = 3;
    stepWith(m, still, game::PlayerAim{}, swap);
    printf("  fusil con %d balas en el cargador: soltado y recogido -> %d; reserva %d -> %d\n", loaded,
           m.inventory().slot(3).weapon.ammo(), reserve, m.inventory().ammo(game::AmmoType::Medium));
    check(m.inventory().slot(3).type == ItemType::AssaultRifle && m.inventory().slot(3).weapon.ammo() == loaded &&
              m.inventory().ammo(game::AmmoType::Medium) == reserve,
          "al recoger un arma soltada no se duplica municion (conserva su cargador)");
    // Fuera de alcance no se cambia nada.
    swap.swapPickup = bandIndex >= 0 ? rifleIndex : -1;
    m.teleport(0, spot + Vec3{6.0f, 0.0f, 0.0f}, 0.0f);
    for (size_t i = 0; i < m.pickups().size(); ++i) {
        if (m.pickups()[i].active) swap.swapPickup = static_cast<int>(i);
    }
    const ItemType inSlot = m.inventory().slot(3).type;
    stepWith(m, still, game::PlayerAim{}, swap);
    check(m.inventory().slot(3).type == inSlot, "sin el objeto al alcance no se cambia nada");
    m.teleport(0, spot, 0.0f);

    // Curarse: quieto y sin saltar mientras dura (la camara puede girar); tiempo configurable.
    m.applyDamage(0, 60.0f, -1);
    const int bandSlotNow = selectSlotOf(m, ItemType::Bandage);
    game::PlayerCommands use;
    use.useItem = true;
    stepWith(m, still, game::PlayerAim{}, use);
    const Vec3 at = m.body(0).state().position;
    game::PlayerInput push;
    push.moveY = -1.0f;
    push.jumpPressed = true;
    push.jumpHeld = true;
    push.cameraYaw = 1.2f;  // la camara gira
    bool grounded = true;
    for (int i = 0; i < 60; ++i) {
        stepWith(m, push, game::PlayerAim{});
        grounded &= m.body(0).state().grounded();
    }
    const float moved = distXZ(m.body(0).state().position, at);
    const bool usingAfter1s = m.useProgress() >= 0.0f;
    for (int i = 0; i < 20; ++i) stepWith(m, still, game::PlayerAim{});
    printf("  vendas (1.2 s configurado): con el stick y X se mueve %.2f m; vida %.0f -> %.0f\n", static_cast<double>(moved),
           40.0, static_cast<double>(m.body(0).state().health));
    check(bandSlotNow >= 0 && usingAfter1s && moved < 0.01f && grounded, "curandose no se mueve ni salta");
    check(m.body(0).state().health == 55.0f && m.useProgress() < 0.0f, "la cura termina en su tiempo configurado (1.2 s)");
    // Interrumpida (cambiar de ranura): no se gasta.
    const int count = m.inventory().slot(bandSlotNow).count;
    stepWith(m, still, game::PlayerAim{}, use);
    for (int i = 0; i < 30; ++i) stepWith(m, still, game::PlayerAim{});
    game::PlayerCommands next;
    next.cycleSlot = 1;
    stepWith(m, still, game::PlayerAim{}, next);
    for (int i = 0; i < 90; ++i) stepWith(m, still, game::PlayerAim{});
    check(m.inventory().slot(bandSlotNow).count == count && m.body(0).state().health == 55.0f,
          "si la cura se interrumpe el objeto no se gasta");
    // Y tras curarse se vuelve a andar.
    const Vec3 before = m.body(0).state().position;
    game::PlayerInput walkIn;
    walkIn.moveY = -1.0f;
    for (int i = 0; i < 30; ++i) stepWith(m, walkIn, game::PlayerAim{});
    check(distXZ(m.body(0).state().position, before) > 1.0f, "sin curarse se anda con normalidad");

    // Agacharse: menos dispersion segun el arma, pero sin quitar el retroceso.
    struct Case {
        ItemType type;
        Rarity rarity;
    };
    const Case cases[] = {{ItemType::AssaultRifle, Rarity::Rare},
                          {ItemType::Shotgun, Rarity::Rare},
                          {ItemType::Smg, Rarity::Rare},
                          {ItemType::Sniper, Rarity::Rare}};
    bool lower = true, notPerfect = true;
    for (const Case& c : cases) {
        Match w;
        MatchConfig wc = cfg;
        wc.startItems[0] = {c.type, 1, c.rarity};
        w.start(wc, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
        w.teleport(0, spot, 0.0f);
        stepWith(w, still, game::PlayerAim{});
        const float standing = w.playerSpreadNow();
        game::PlayerCommands crouch;
        crouch.toggleCrouch = true;
        stepWith(w, still, game::PlayerAim{}, crouch);
        const float crouched = w.playerSpreadNow();
        printf("  %-14s cadera: de pie %.2f grados, agachado %.2f\n", game::weaponConfig(c.type, c.rarity).name,
               static_cast<double>(standing), static_cast<double>(crouched));
        lower &= crouched < standing;
        notPerfect &= crouched >= standing * 0.45f && crouched > 0.5f;
    }
    check(lower, "agachado la dispersion de cada arma es menor");
    check(notPerfect, "pero ninguna arma pasa a ser perfectamente precisa (cada una conserva su caracter)");

    // Retroceso: rafaga del fusil de pie, se agacha: la parte del retroceso se mantiene.
    Match r;
    MatchConfig rc = cfg;
    rc.startItems[0] = {ItemType::AssaultRifle, 1, Rarity::Rare};
    rc.startAmmo[static_cast<int>(game::AmmoType::Medium)] = 90;
    r.start(rc, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    r.teleport(0, spot, 0.0f);
    r.teleport(1, {-80.0f, 0.0f, 80.0f}, 0.0f);
    stepWith(r, still, game::PlayerAim{});
    const float base = r.playerSpreadNow();
    sky.eye = r.eyeOf(0);
    for (int i = 0; i < 60; ++i) stepWith(r, still, sky);  // ~5 disparos
    const float bloom = r.recoilBloom(), burst = r.playerSpreadNow();
    game::PlayerCommands crouch;
    crouch.toggleCrouch = true;
    r.setPlayerInput(still, game::PlayerAim{}, crouch);
    const float crouchedBurst = r.playerSpreadNow();
    printf("  fusil: dispersion %.2f -> %.2f tras 1 s disparando (retroceso %.2f); agachado %.2f\n",
           static_cast<double>(base), static_cast<double>(burst), static_cast<double>(bloom),
           static_cast<double>(crouchedBurst));
    check(bloom > 1.0f && burst > base + 1.0f, "cada disparo abre la dispersion (retroceso acumulado)");
    check(fabsf(crouchedBurst - (base * 0.6f + bloom)) < 0.05f, "agacharse reduce la dispersion del arma pero no el retroceso");
    for (int i = 0; i < 90; ++i) stepWith(r, still, game::PlayerAim{});
    check(r.recoilBloom() < 0.01f, "el retroceso se recupera al dejar de disparar");

    // Efecto real en los disparos: a 30 m sin apuntar, agachado se acierta mas.
    int hits[2] = {0, 0};
    for (int crouched = 0; crouched < 2; ++crouched) {
        Match h;
        MatchConfig hc = cfg;
        hc.aimAssistDegrees = 0.0f;
        hc.seed = 99;
        hc.startItems[0] = {ItemType::AssaultRifle, 1, Rarity::Rare};
        h.start(hc, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
        h.teleport(0, spot, 0.0f);
        h.teleport(1, spot + Vec3{0.0f, 0.0f, -30.0f}, 0.0f);
        game::PlayerCommands c0;
        c0.toggleCrouch = crouched == 1;
        stepWith(h, still, game::PlayerAim{}, c0);
        game::PlayerAim aim;
        aim.eye = h.eyeOf(0) + Vec3{0.0f, 0.5f, 3.0f};
        aim.direction = math3d::normalize(h.chestOf(1) - aim.eye);
        for (int k = 0; k < 400; ++k) hits[crouched] += h.playerShot(aim, 0.001f, 80.0f, h.playerSpreadNow()).hitEntity ? 1 : 0;
    }
    printf("  400 disparos a 30 m sin apuntar: de pie %d impactos, agachado %d\n", hits[0], hits[1]);
    check(hits[1] > hits[0] + 20 && hits[1] < 400, "agachado se acierta mas (sin acertar siempre)");
}

// --- Parche (fase 8), bloque B --------------------------------------------------------------------

Vec3 lastExplosion(const Match& m, u32 from) {
    Vec3 at{0.0f, -99.0f, 0.0f};
    for (u32 i = from; i < m.eventCount(); ++i) {
        if (m.event(i).type == game::GameEventType::Explosion) at = m.event(i).position;
    }
    return at;
}

int countPickups(const Match& m, game::PickupKind kind) {
    int n = 0;
    for (const game::Pickup& p : m.pickups()) n += p.active && p.kind == kind ? 1 : 0;
    return n;
}

void testPatchB(world::World& world, game::NavGrid& nav) {
    printf("\n17) Parche B: armas nuevas, explosivos, materiales de los bots y llama\n");
    world.mutableCollision().clearDynamic();
    using game::ItemType;
    using game::Rarity;
    // Catalogo: cada arma nueva con sus estadisticas y rarezas.
    const ItemType news[] = {ItemType::BurstRifle,  ItemType::ScopedRifle,    ItemType::SubmachineGun,
                             ItemType::TacticalShotgun, ItemType::Pistol,     ItemType::Revolver,
                             ItemType::SemiAutoSniper,  ItemType::RocketLauncher, ItemType::GrenadeLauncher};
    bool catalog = true;
    for (ItemType t : news) {
        int rarities = 0;
        for (int r = 0; r < 5; ++r) rarities += game::weaponHasRarity(t, static_cast<Rarity>(r)) ? 1 : 0;
        Rarity first = Rarity::Common;
        while (!game::weaponHasRarity(t, first)) first = static_cast<Rarity>(static_cast<int>(first) + 1);
        const game::WeaponConfig w = game::weaponConfig(t, first);
        printf("  %-28s %d rarezas, %3.0f dano, %4.2f s entre disparos, cargador %2d, recarga %.1f s, alcance %3.0f m, "
               "dispersion %.1f/%.1f, retroceso %.2f%s%s\n",
               game::itemName(t), rarities, static_cast<double>(w.damage), static_cast<double>(w.fireInterval),
               w.magazineSize, static_cast<double>(w.reloadTime), static_cast<double>(w.range),
               static_cast<double>(w.hipSpread), static_cast<double>(w.aimSpread), static_cast<double>(w.recoil),
               w.burstCount > 1 ? ", rafagas de 3" : "", w.projectile != game::ProjectileKind::None ? ", proyectil" : "");
        catalog &= game::isWeapon(t) && rarities >= 2 && w.damage > 0.0f && w.magazineSize > 0 && w.reloadTime > 0.0f &&
                   w.range > 0.0f && w.crouchSpreadScale < 1.0f;
    }
    check(catalog, "las 9 armas nuevas tienen dano, cadencia, cargador, recarga, alcance, dispersion y retroceso");
    check(std::string(game::weaponConfig(ItemType::AssaultRifle, Rarity::Common).name) == "Fusil de asalto" &&
              std::string(game::weaponConfig(ItemType::AssaultRifle, Rarity::Rare).name) == "Fusil de asalto" &&
              std::string(game::weaponConfig(ItemType::AssaultRifle, Rarity::Legendary).name) == "SCAR" &&
              game::weaponConfig(ItemType::AssaultRifle, Rarity::Uncommon).damage == 31.0f,
          "el fusil gris, verde y azul es el fusil de asalto generico (la SCAR, solo morada y dorada)");
    check(game::itemDef(ItemType::Grenade).kind == game::ItemKind::Throwable && game::itemDef(ItemType::Grenade).maxStack == 6,
          "la granada es un arrojadizo que se apila (6)");

    MatchConfig cfg;
    cfg.botCount = 2;
    cfg.startInBus = false;
    cfg.loot = false;
    cfg.stormCloses = false;
    cfg.aimAssistDegrees = 0.0f;
    const Vec3 spot{70.0f, 0.0f, 25.0f};
    game::PlayerInput still;
    auto fresh = [&](Match& m, ItemType weapon, Rarity rarity, game::AmmoType ammo, int rounds) {
        MatchConfig c = cfg;
        c.startItems[0] = {weapon, 1, rarity};
        c.startAmmo[static_cast<int>(ammo)] = rounds;
        m.start(c, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
        m.teleport(0, spot, 0.0f);
        m.teleport(1, {-80.0f, 0.0f, 80.0f}, 0.0f);
        m.teleport(2, {-76.0f, 0.0f, 80.0f}, 0.0f);
        stepWith(m, still, game::PlayerAim{});
    };

    // Rafagas: una pulsacion corta dispara 3 balas.
    {
        Match m;
        fresh(m, ItemType::BurstRifle, Rarity::Rare, game::AmmoType::Medium, 60);
        game::PlayerAim aim;
        aim.eye = m.eyeOf(0);
        aim.direction = math3d::normalize(Vec3{0.0f, 0.3f, -1.0f});
        aim.fire = true;
        const int before = m.stats().shotsFired;
        stepWith(m, still, aim);
        aim.fire = false;
        for (int i = 0; i < 40; ++i) stepWith(m, still, aim);
        const int burst = m.stats().shotsFired - before;
        printf("  fusil de rafagas: una pulsacion -> %d balas\n", burst);
        check(burst == 3 && m.inventory().slot(0).weapon.ammo() == 27, "el fusil de rafagas dispara 3 balas por pulsacion");
    }

    // Lanzacohetes contra una pared de madera a 9 m con un bot detras y otro al lado.
    {
        Match m;
        fresh(m, ItemType::RocketLauncher, Rarity::Legendary, game::AmmoType::Rockets, 6);
        m.giveMaterials(world::Resource::Wood, 50);
        // La pared, construida desde 9 m mas adelante (mirando hacia el jugador) a la rejilla.
        m.teleport(0, spot + Vec3{0.0f, 0.0f, -10.0f}, 0.0f);
        const int wall = m.buildWall(0.0f);
        m.teleport(0, spot, 0.0f);
        const world::Aabb box = m.structures().wall(wall).box;
        const float wallZ = (box.min.z + box.max.z) * 0.5f;
        m.teleport(1, {spot.x, 0.0f, wallZ - 1.2f}, 0.0f);  // detras de la pared
        m.teleport(2, {spot.x + 2.6f, 0.0f, wallZ + 1.0f}, 0.0f);  // delante, a un lado
        game::PlayerAim aim;
        aim.eye = m.eyeOf(0);
        aim.direction = math3d::normalize(Vec3{spot.x, 1.2f, wallZ} - aim.eye);
        aim.fire = true;
        const u32 cursor = m.eventCount();
        stepWith(m, still, aim);
        aim.fire = false;
        bool flying = false;
        for (int i = 0; i < 60; ++i) {
            for (int k = 0; k < Match::kMaxProjectiles; ++k) flying |= m.projectiles()[k].active;
            stepWith(m, still, aim);
        }
        const int explosions = countEvents(m, cursor, game::GameEventType::Explosion);
        const float behind = m.body(1).state().health, side = m.body(2).state().health;
        const Vec3 at = lastExplosion(m, cursor);
        printf("  (explosion en %.1f %.1f %.1f; jugador %.0f; pared z %.1f)\n", static_cast<double>(at.x), static_cast<double>(at.y),
               static_cast<double>(at.z), static_cast<double>(m.body(0).state().health), static_cast<double>(wallZ));
        printf("  cohete contra pared de madera a %.0f m: explosiones %d, pared %s, bot detras %.0f, bot al lado %.0f, "
               "cohetes %d\n",
               static_cast<double>(spot.z - wallZ), explosions, m.structures().wall(wall).active ? "en pie" : "destruida",
               static_cast<double>(behind), static_cast<double>(side), m.inventory().ammo(game::AmmoType::Rockets));
        // (El bot herido se cubre con su propia pared: se cuentan solo las del jugador.)
        check(flying && explosions == 1 && m.structures().count() == 0, "el cohete vuela, explota al tocar la pared y la derriba");
        check(side < 100.0f && behind == 100.0f, "la explosion dana a quien la ve y la pared protege a quien esta detras");
        const game::Weapon& rl = m.inventory().slot(0).weapon;
        check(rl.ammo() == 0 && rl.reloading() && m.inventory().ammo(game::AmmoType::Rockets) == 6,
              "cada cohete gasta su municion (el cargador de 1 se vacia y empieza a recargar)");
    }

    // Lanzagranadas: la granada rebota y explota con la espoleta (o al tocar a alguien).
    {
        Match m;
        fresh(m, ItemType::GrenadeLauncher, Rarity::Rare, game::AmmoType::Rockets, 6);
        // El bot se queda quieto en su sitio (si no, la IA lo mueve mientras vuela la granada).
        const Vec3 target = spot + Vec3{0.0f, 0.0f, -12.0f};
        m.teleport(1, target, 0.0f);
        game::PlayerAim aim;
        aim.eye = m.eyeOf(0);
        aim.direction = math3d::normalize(m.chestOf(1) - aim.eye);
        aim.fire = true;
        const u32 cursor = m.eventCount();
        stepWith(m, still, aim);
        aim.fire = false;
        for (int i = 0; i < 150; ++i) {
            m.teleport(1, target, 0.0f);
            stepWith(m, still, aim);
        }
        const Vec3 at = lastExplosion(m, cursor);
        printf("  (explosion en %.1f %.1f %.1f; bot en %.1f %.1f)\n", static_cast<double>(at.x), static_cast<double>(at.y),
               static_cast<double>(at.z), static_cast<double>(m.body(1).state().position.x),
               static_cast<double>(m.body(1).state().position.z));
        printf("  lanzagranadas a un bot a 12 m: explosiones %d, vida del bot %.0f\n",
               countEvents(m, cursor, game::GameEventType::Explosion), static_cast<double>(m.body(1).state().health));
        check(countEvents(m, cursor, game::GameEventType::Explosion) == 1 && m.body(1).state().health < 100.0f,
              "la granada del lanzagranadas explota y dana al bot");
        // Hacia el suelo lejos de todos: rebota y explota a los 2 s.
        m.teleport(1, {-80.0f, 0.0f, 80.0f}, 0.0f);
        aim.direction = math3d::normalize(Vec3{0.0f, -0.25f, -1.0f});
        aim.fire = true;
        const int shotsBefore = m.stats().shotsFired;
        for (int i = 0; i < 120 && m.stats().shotsFired == shotsBefore; ++i) stepWith(m, still, aim);
        aim.fire = false;
        const u32 second = m.eventCount();
        int frames = 0;
        while (countEvents(m, second, game::GameEventType::Explosion) == 0 && frames < 300) {
            stepWith(m, still, aim);
            ++frames;
        }
        printf("  granada al suelo: explota a los %.2f s\n", frames / 60.0);
        check(frames > 60 && frames < 150, "contra el suelo rebota y explota con la espoleta");
    }

    // Granada de mano: R la lanza (gasta una) y explota a los 2.2 s cerca de un bot.
    {
        Match m;
        fresh(m, ItemType::Pistol, Rarity::Common, game::AmmoType::Light, 0);
        m.giveItem(ItemType::Grenade, 3);
        const Vec3 target = spot + Vec3{0.0f, 0.0f, -10.0f};
        m.teleport(1, target, 0.0f);
        const int slot = selectSlotOf(m, ItemType::Grenade);
        game::PlayerAim aim;
        aim.eye = m.eyeOf(0);
        aim.direction = math3d::normalize(m.chestOf(1) - aim.eye);
        game::PlayerCommands use;
        use.useItem = true;
        const u32 cursor = m.eventCount();
        stepWith(m, still, aim, use);
        const int left = m.inventory().slot(slot).count;
        for (int i = 0; i < 170; ++i) {
            m.teleport(1, target, 0.0f);
            stepWith(m, still, aim);
        }
        const Vec3 at = lastExplosion(m, cursor);
        printf("  (explosion a %.1f m del bot)\n", static_cast<double>(distXZ(at, target)));
        printf("  granada de mano a 10 m: quedan %d, explosiones %d, vida del bot %.0f\n", left,
               countEvents(m, cursor, game::GameEventType::Explosion), static_cast<double>(m.body(1).state().health));
        check(slot >= 0 && left == 2 && countEvents(m, cursor, game::GameEventType::Thrown) == 1, "R lanza la granada y gasta una");
        check(countEvents(m, cursor, game::GameEventType::Explosion) == 1 && m.body(1).state().health < 100.0f,
              "la granada explota con su espoleta y dana al bot cercano");
    }
    // La explosion tambien rompe objetos del mapa y hace dano a quien la lanza.
    {
        Match m;
        fresh(m, ItemType::Pistol, Rarity::Common, game::AmmoType::Light, 0);
        m.explode(m.body(0).state().position + Vec3{1.0f, 0.3f, 0.0f}, 0, 100.0f, 3.5f, 300.0f, -1);
        check(m.body(0).state().health < 100.0f, "una explosion cerca de quien la lanza tambien le hace dano");
    }

    // Materiales de los bots: al caer sueltan madera (y a veces piedra y metal), una vez.
    {
        MatchConfig c = cfg;
        c.loot = true;
        c.chestCount = c.ammoBoxCount = c.floorLootCount = 0;
        c.airdropTimes[0] = c.airdropTimes[1] = 9999.0f;
        c.botCount = 6;
        Match m;
        m.start(c, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
        m.teleport(0, spot, 0.0f);
        // Dos bots en el mismo sitio exacto.
        for (int b = 1; b <= 6; ++b) m.teleport(b, spot + Vec3{b <= 2 ? 6.0f : 6.0f + b, 0.0f, 0.0f}, 0.0f);
        for (int b = 1; b <= 6; ++b) {
            m.applyDamage(b, 500.0f, 0);
            m.applyDamage(b, 500.0f, 0);  // la misma eliminacion procesada otra vez: nada nuevo
        }
        int wood = 0, piles = 0, overlaps = 0;
        int byResource[3] = {0, 0, 0};
        for (size_t i = 0; i < m.pickups().size(); ++i) {
            const game::Pickup& p = m.pickups()[i];
            if (!p.active || p.kind != game::PickupKind::Materials) continue;
            ++piles;
            byResource[static_cast<int>(p.resource)] += p.count;
            if (p.resource == world::Resource::Wood) ++wood;
            for (size_t j = i + 1; j < m.pickups().size(); ++j) {
                const game::Pickup& q = m.pickups()[j];
                if (q.active && distXZ(p.position, q.position) < 0.25f) ++overlaps;
            }
        }
        printf("  6 bots eliminados: %d montones de materiales (madera %d, piedra %d, metal %d), %d solapados\n", piles,
               byResource[0], byResource[1], byResource[2], overlaps);
        check(wood == 6 && byResource[0] >= 180 && byResource[0] <= 360, "cada bot suelta un monton de madera (30-60), una sola vez");
        check(overlaps == 0, "los objetos de dos bots que caen en el mismo sitio no quedan en la misma posicion");
        // Se recogen solos al pasar por encima.
        const int before = m.materials().get(world::Resource::Wood);
        for (size_t i = 0; i < m.pickups().size(); ++i) {
            const game::Pickup& p = m.pickups()[i];
            if (!p.active || p.kind != game::PickupKind::Materials) continue;
            m.teleport(0, p.position, 0.0f);
            stepWith(m, still, game::PlayerAim{});
        }
        check(m.materials().get(world::Resource::Wood) == before + byResource[0] &&
                  countPickups(m, game::PickupKind::Materials) == 0,
              "los materiales se recogen al pasar por encima y suman al inventario");
    }

    // Llama: 5 % por partida (una tirada al empezar), en un sitio valido, como mucho una.
    {
        MatchConfig c = cfg;
        c.loot = true;
        c.botCount = 1;
        int with = 0, bad = 0, many = 0;
        const int runs = 600;
        for (int i = 0; i < runs; ++i) {
            c.seed = 1000u + static_cast<u32>(i) * 7919u;
            Match m;
            m.start(c, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
            int llamas = 0;
            for (const game::Container& k : m.containers()) llamas += k.kind == game::ContainerKind::Llama ? 1 : 0;
            many += llamas > 1 ? 1 : 0;
            if (llamas == 0) continue;
            ++with;
            const game::Container& l = m.containers()[m.llama()];
            const world::Aabb room{{l.position.x - 0.55f, l.position.y + 0.15f, l.position.z - 0.55f},
                                   {l.position.x + 0.55f, l.position.y + 1.7f, l.position.z + 0.55f}};
            bad += world.collision().overlapsAny(room, world::kBlocksMovement) ||
                           world::stormDepth(l.position.x, l.position.z) > -10.0f ||
                           world.collision().ceilingHeight(l.position.x, l.position.z, 0.5f, l.position.y + 0.5f) <
                               l.position.y + 30.0f
                       ? 1
                       : 0;
        }
        printf("  %d partidas: %d con llama (%.1f %%), %d mal colocadas, %d con mas de una\n", runs, with,
               100.0 * with / runs, bad, many);
        check(with >= runs * 0.025 && with <= runs * 0.08, "la llama aparece en ~1 de cada 20 partidas (5 %)");
        check(bad == 0 && many == 0, "siempre al aire libre, accesible, fuera de solidos y de la tormenta, y nunca mas de una");
        // La tirada es la unica fuente de variacion: forzada, siempre encuentra sitio.
        int placed = 0;
        for (int i = 0; i < 150; ++i) {
            MatchConfig f = c;
            f.forceLlama = true;
            f.seed = 777u + static_cast<u32>(i) * 104729u;
            Match m;
            m.start(f, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
            placed += m.llama() >= 0 ? 1 : 0;
        }
        printf("  con la llama forzada: colocada en %d de 150 partidas\n", placed);
        check(placed == 150, "cuando sale la tirada, la llama siempre encuentra un sitio valido");

        // Abrirla con CUADRADO o romperla a tiros: suelta materiales, municion y objetos, una vez.
        for (int mode = 0; mode < 2; ++mode) {
            c.forceLlama = true;
            c.seed = 4242u + mode;
            c.startItems[0] = {ItemType::AssaultRifle, 1, Rarity::Legendary};
            c.startAmmo[static_cast<int>(game::AmmoType::Medium)] = 200;
            Match m;
            m.start(c, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
            m.teleport(1, {-80.0f, 0.0f, 80.0f}, 0.0f);
            const game::Container l = m.containers()[m.llama()];
            const float yaw = 0.0f;
            const Vec3 front = l.position + Vec3{0.0f, 0.0f, mode == 0 ? 1.6f : 8.0f};
            m.teleport(0, front, yaw);
            stepWith(m, still, game::PlayerAim{});
            const int matsBefore = countPickups(m, game::PickupKind::Materials);
            const u32 cursor = m.eventCount();
            if (mode == 0) {
                game::PlayerCommands square;
                square.reload = true;
                game::PlayerInput look;
                look.cameraYaw = game::yawTowards(front, l.position);
                stepWith(m, look, game::PlayerAim{}, square);
            } else {
                game::PlayerAim aim;
                aim.eye = m.eyeOf(0);
                aim.direction = math3d::normalize(l.position + Vec3{0.0f, 0.9f, 0.0f} - aim.eye);
                aim.fire = true;
                for (int i = 0; i < 60 * 4 && !m.containers()[m.llama()].opened; ++i) stepWith(m, still, aim);
            }
            stepWith(m, still, game::PlayerAim{});
            const int opened = countEvents(m, cursor, game::GameEventType::ContainerOpened);
            // Los materiales que caen al lado del jugador se recogen solos: se cuentan los
            // del suelo y los ya recogidos (200 de cada tipo).
            int matsNow = countPickups(m, game::PickupKind::Materials);
            for (int r = 0; r < 3; ++r) matsNow += m.materials().get(static_cast<world::Resource>(r)) >= 200 ? 1 : 0;
            printf("  llama %s: abierta %s (%d sucesos), %d montones de materiales, %d objetos en total\n",
                   mode == 0 ? "con CUADRADO" : "a tiros", m.containers()[m.llama()].opened ? "si" : "no", opened,
                   matsNow - matsBefore,
                   countPickups(m, game::PickupKind::Materials) + countPickups(m, game::PickupKind::Ammo) +
                       countPickups(m, game::PickupKind::Item));
            check(m.containers()[m.llama()].opened && opened == 1 && matsNow - matsBefore == 3,
                  mode == 0 ? "CUADRADO abre la llama y suelta su recompensa" : "a tiros la llama se rompe y suelta su recompensa (una vez)");
        }
    }
}

// --- Parche (fase 8), bloque C: trampas ----------------------------------------------------------

void testPatchCTraps(world::World& world, game::NavGrid& nav) {
    printf("\n18) Parche C: trampas de pinchos (suelo, pared y techo)\n");
    world.mutableCollision().clearDynamic();
    MatchConfig cfg;
    cfg.botCount = 2;
    cfg.startInBus = false;
    cfg.loot = false;
    cfg.stormCloses = false;
    cfg.botsBuild = false;
    cfg.trapDamage = 40.0f;  // (configurable) asi el bot aguanta dos saltos
    Match m;
    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    const Vec3 spot{70.0f, 0.0f, 25.0f};
    m.teleport(0, spot, 0.0f);
    m.teleport(1, {-80.0f, 0.0f, 80.0f}, 0.0f);
    m.teleport(2, {-76.0f, 0.0f, 80.0f}, 0.0f);
    game::PlayerInput still;
    auto aimAt = [&](const Vec3& target) {
        game::PlayerAim a;
        a.eye = m.eyeOf(0) + Vec3{0.0f, 1.2f, 4.0f};  // como la camara: detras y por encima
        a.direction = math3d::normalize(target - a.eye);
        return a;
    };
    // Sin trampas no se puede.
    const game::TrapPlacement none = m.planTrap(aimAt(spot + Vec3{0.0f, 0.0f, -3.0f}));
    check(!none.valid && none.error == game::TrapError::NoTraps && none.surface == game::TrapSurface::Floor,
          "sin trampas no se coloca ninguna (el suelo si es valido)");
    // Se recogen del suelo (no ocupan ranura).
    game::Pickup trap;
    trap.kind = game::PickupKind::Item;
    trap.item = game::ItemType::SpikeTrap;
    trap.rarity = game::Rarity::Uncommon;
    trap.count = 3;
    trap.position = spot;
    game::Interactable it;
    it.kind = game::Interactable::Kind::Pickup;
    it.index = m.dropPickup(trap);
    m.interact(0, it);
    m.giveTraps(3);
    check(m.trapStock() == 6 && m.inventory().slot(0).type == game::ItemType::None, "las trampas se recogen sin ocupar ranuras");

    // Suelo: delante del jugador; un bot que la pisa recibe el dano una vez por activacion.
    const Vec3 floorPoint = spot + Vec3{0.0f, 0.0f, -3.0f};
    const int floorTrap = m.placeTrap(aimAt(floorPoint));
    const game::Trap& ft = m.traps()[floorTrap >= 0 ? floorTrap : 0];
    check(floorTrap >= 0 && ft.place.surface == game::TrapSurface::Floor && m.trapStock() == 5 &&
              fabsf(ft.place.center.y) < 0.01f && ft.place.normal.y > 0.9f,
          "trampa de suelo: sobre el suelo, mirando hacia arriba, gasta una");
    // El jugador (quien la puso) la pisa sin dano.
    m.teleport(0, ft.place.center, 0.0f);
    for (int i = 0; i < 30; ++i) stepWith(m, still, game::PlayerAim{});
    check(m.body(0).state().health == 100.0f, "quien pone la trampa no recibe dano al pisarla");
    m.teleport(0, spot, 0.0f);
    const u32 cursor = m.eventCount();
    m.teleport(1, ft.place.center, 0.0f);
    float after1 = 0.0f, after2 = 0.0f;
    for (int i = 0; i < 60; ++i) {
        m.teleport(1, ft.place.center, 0.0f);  // se queda encima
        stepWith(m, still, game::PlayerAim{});
    }
    after1 = m.body(1).state().health;
    const int triggers1 = countEvents(m, cursor, game::GameEventType::TrapTriggered);
    for (int i = 0; i < 150; ++i) {
        m.teleport(1, ft.place.center, 0.0f);
        stepWith(m, still, game::PlayerAim{});
    }
    after2 = m.body(1).state().health;
    printf("  bot sobre la trampa: vida %.0f tras 1 s (saltos %d), %.0f tras 3.5 s (saltos %d)\n", static_cast<double>(after1),
           triggers1, static_cast<double>(after2), countEvents(m, cursor, game::GameEventType::TrapTriggered));
    check(triggers1 == 1 && after1 == 100.0f - cfg.trapDamage, "salta una sola vez al pisarla (dano configurable)");
    check(countEvents(m, cursor, game::GameEventType::TrapTriggered) == 2 && after2 == 100.0f - 2.0f * cfg.trapDamage,
          "pasada la espera (3 s) vuelve a saltar si sigue dentro");

    // Pared construida: en su cara, ajustada a la pieza; en la otra cara tambien; en la misma, no.
    m.giveMaterials(world::Resource::Wood, 50);
    m.teleport(0, spot, 0.0f);
    const int wall = m.buildWall(0.0f);
    const world::Aabb wb = m.structures().wall(wall).box;
    const Vec3 face{(wb.min.x + wb.max.x) * 0.5f, 1.5f, wb.max.z};
    const game::TrapPlacement wp = m.planTrap(aimAt(face + Vec3{0.6f, -0.3f, 0.0f}));
    printf("  pared: %s, superficie %d, centro %.2f %.2f %.2f, %.2f x %.2f m\n", wp.valid ? "valida" : game::trapErrorName(wp.error),
           static_cast<int>(wp.surface), static_cast<double>(wp.center.x), static_cast<double>(wp.center.y),
           static_cast<double>(wp.center.z), static_cast<double>(wp.halfU * 2), static_cast<double>(wp.halfV * 2));
    const int wallTrap = m.placeTrap(aimAt(face + Vec3{0.6f, -0.3f, 0.0f}));
    check(wp.valid && wp.surface == game::TrapSurface::Wall && wp.normal.z > 0.9f && fabsf(wp.center.x - face.x) < 0.01f &&
              fabsf(wp.center.z - wb.max.z) < 0.01f && wp.halfU > 1.5f && wallTrap >= 0,
          "trampa de pared: en la cara apuntada de la pieza, centrada y del tamano de la pieza");
    check(m.planTrap(aimAt(face)).error == game::TrapError::Occupied, "en la misma cara no cabe otra");
    // Un bot pegado a esa cara la activa.
    m.teleport(2, {face.x, 0.0f, wb.max.z + 0.5f}, 0.0f);
    const u32 c2 = m.eventCount();
    stepWith(m, still, game::PlayerAim{});
    check(countEvents(m, c2, game::GameEventType::TrapTriggered) == 1 && m.body(2).state().health < 100.0f,
          "un bot junto a la pared la activa");
    // Si la pared se rompe, la trampa cae con ella.
    m.explode(face + Vec3{0.0f, 0.0f, -0.3f}, -1, 0.0f, 2.0f, 1000.0f, -1);
    stepWith(m, still, game::PlayerAim{});
    check(!m.traps()[wallTrap].active, "al romperse la pared desaparece su trampa");

    // Rampa: no.
    m.teleport(0, spot + Vec3{-8.0f, 0.0f, 0.0f}, 0.0f);
    const int ramp = m.buildWall(0.0f, game::PieceType::Ramp);
    const world::Aabb rb = m.structures().wall(ramp).box;
    const game::TrapPlacement rp = m.planTrap(aimAt(Vec3{(rb.min.x + rb.max.x) * 0.5f, 1.0f, (rb.min.z + rb.max.z) * 0.5f}));
    check(ramp >= 0 && rp.error == game::TrapError::Ramp, "en una rampa no se puede poner");

    // Techo: bajo una caja grande del mapa (altillo, tejado...) con suelo libre debajo.
    int ceilingBox = -1;
    for (int i = 0; i < world.collision().count() && ceilingBox < 0; ++i) {
        const world::Aabb& b = world.collision().collider(i).box;
        if (b.min.y < 2.2f || b.min.y > 4.2f || b.max.x - b.min.x < 3.0f || b.max.z - b.min.z < 3.0f) continue;
        const Vec3 c{(b.min.x + b.max.x) * 0.5f, 0.0f, (b.min.z + b.max.z) * 0.5f};
        if (world::stormDepth(c.x, c.z) > -5.0f) continue;
        const world::Aabb below{{c.x - 0.5f, 0.1f, c.z - 0.5f}, {c.x + 0.5f, b.min.y - 0.2f, c.z + 0.5f}};
        if (!world.collision().overlapsAny(below, world::kSolid)) ceilingBox = i;
    }
    if (ceilingBox >= 0) {
        const world::Aabb& b = world.collision().collider(ceilingBox).box;
        const Vec3 c{(b.min.x + b.max.x) * 0.5f, 0.0f, (b.min.z + b.max.z) * 0.5f};
        m.teleport(0, c, 0.0f);
        game::PlayerAim up;
        up.eye = m.eyeOf(0);
        up.direction = math3d::normalize(Vec3{0.2f, 1.0f, -0.3f});
        const game::TrapPlacement cp = m.planTrap(up);
        printf("  techo a %.1f m: %s, superficie %d, %.1f x %.1f m\n", static_cast<double>(b.min.y),
               cp.valid ? "valida" : game::trapErrorName(cp.error), static_cast<int>(cp.surface),
               static_cast<double>(cp.halfU * 2), static_cast<double>(cp.halfV * 2));
        const int ct = m.placeTrap(up);
        check(cp.valid && cp.surface == game::TrapSurface::Ceiling && cp.normal.y < -0.9f && ct >= 0,
              "trampa de techo: bajo una superficie del mapa, mirando hacia abajo");
    } else {
        check(false, "hay un techo del mapa para probar la trampa de techo");
    }
    // Un tronco de arbol es demasiado pequeno.
    bool tooSmall = false;
    for (const world::Harvestable& h : world.harvestables()) {
        if (h.resource != world::Resource::Wood || h.box.max.x - h.box.min.x > 0.8f) continue;
        const Vec3 c = (h.box.min + h.box.max) * 0.5f;
        if (world::stormDepth(c.x, c.z) > -5.0f) continue;
        m.teleport(0, {c.x, 0.0f, c.z + 2.0f}, 0.0f);
        game::PlayerAim a;
        a.eye = m.eyeOf(0);
        a.direction = math3d::normalize(Vec3{c.x, 1.2f, h.box.max.z} - a.eye);
        const game::TrapPlacement tp = m.planTrap(a);
        if (tp.error == game::TrapError::TooSmall || tp.error == game::TrapError::Obstructed) tooSmall = true;
        break;
    }
    check(tooSmall, "en un tronco de arbol no cabe (o hay algo en medio)");
}

}  // namespace

int main() {
    world::World world;
    if (!world.build()) {
        fprintf(stderr, "ERROR construyendo el mundo\n");
        return 1;
    }
    game::NavGrid nav;
    nav.build(world.collision(), world::kMapHalfSize);

    testNavigation(world, nav);
    testMapCollisions(world);
    testShots(world, nav);
    testEliminations(world, nav);
    testFullMatches(world, nav);
    testWeapon();
    testCameraShots(world, nav);
    testBuilding(world, nav);
    testFloorsAndRamps(world, nav);
    testBotsRespectWalls(world, nav);
    testFort(world, nav);
    testEvents(world, nav);
    testInventory(world, nav);
    testMapObjects(world, nav);
    testStorm(world, nav);
    testStormZone(world, nav);
    testLoot(world, nav);
    testBotInventory(world, nav);
    testHeadshotsAndCrouch(world, nav);
    testPatchA(world, nav);
    testPatchB(world, nav);
    testPatchCTraps(world, nav);
    testRealMatches(world, nav);

    printf("\n%s (%d fallos)\n", g_failures == 0 ? "TODO CORRECTO" : "HAY FALLOS", g_failures);
    world.destroy();
    return g_failures == 0 ? 0 : 1;
}
