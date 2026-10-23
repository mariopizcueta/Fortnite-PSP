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

#include <vector>

#include "game/fixed_timestep.h"
#include "game/match.h"
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
    MatchConfig cfg;
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
        MatchConfig one;
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
    MatchConfig cfg;
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
    if (c.world->raycast(shot.origin, end, world::kBlocksShots, nullptr)) ++c.hitsThroughWalls;
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
};

SimResult simulate(world::World& world, game::NavGrid& nav, int bots, u32 seed, float maxSeconds) {
    MatchConfig cfg;
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
                const bool wantsToMove = b.state == game::BotState::Travel ||
                                         (b.state == game::BotState::Attack && b.pathIndex < b.pathLength);
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
        if (m.phase() == game::MatchPhase::Finished) {
            r.finished = true;
            break;
        }
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
    MatchConfig cfg;
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
    MatchConfig one;
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
    MatchConfig cfg;
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

void testBotsRespectWalls(world::World& world, game::NavGrid& nav) {
    printf("\n9) La IA respeta las paredes construidas\n");
    MatchConfig cfg;
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
    MatchConfig cfg;
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
    MatchConfig cfg;
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
    MatchConfig cfg;
    cfg.botCount = 1;
    Match m;
    m.setHarvestables(&world.harvestables());
    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    const Vec3 player{70.0f, 0.0f, 25.0f};
    m.teleport(0, player, 0.0f);
    m.teleport(1, {-80.0f, 0.0f, 80.0f}, 0.0f);
    const game::Inventory& inv = m.inventory();
    int trees = 0, rocks = 0, metal = 0;
    for (const world::Harvestable& h : world.harvestables()) {
        trees += h.total == 48 ? 1 : 0;
        rocks += h.resource == world::Resource::Stone ? 1 : 0;
        metal += h.resource == world::Resource::Metal ? 1 : 0;
    }
    printf("  objetos recolectables en el mapa: %d (%d arboles, %d rocas, %d de metal)\n",
           static_cast<int>(world.harvestables().size()), trees, rocks, metal);
    check(inv.slot(0).type == ItemType::Scar && inv.selected() == 0 && inv.slot(1).type == ItemType::Bandage &&
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

    // Pico: un arbol da 8 de madera por golpe y se agota tras 48.
    m.start(cfg, world.mutableCollision(), nav, world.pointsOfInterest(), kSpawn, 0.0f);
    m.teleport(1, {80.0f, 0.0f, -80.0f}, 0.0f);
    int treeIndex = -1;
    for (int i = 0; i < static_cast<int>(world.harvestables().size()); ++i) {
        const world::Harvestable& h = world.harvestables()[i];
        if (h.total != 48) continue;
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
    printf("  1 s con R y el pico frente a un arbol: %d golpes, madera %d -> %d, quedan %d en el arbol\n", swings,
           woodBefore, m.materials().get(world::Resource::Wood), m.harvestLeft(treeIndex));
    check(swings == 3 && m.materials().get(world::Resource::Wood) == woodBefore + 24 &&
              countEvents(m, cursor, game::GameEventType::Harvest) == 3,
          "con R mantenido el pico golpea cada 0.5 s y cada golpe al arbol da 8 de madera");
    runSteps(m, 60 * 3, swing);
    check(m.harvestLeft(treeIndex) == 0 && m.materials().get(world::Resource::Wood) == woodBefore + 48,
          "el arbol se agota tras 48 de madera (los golpes siguientes no dan nada)");

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
    MatchConfig cfg;
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
    m.giveItem(game::ItemType::ShieldPotion, 1);
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
    testBotsRespectWalls(world, nav);
    testFort(world, nav);
    testEvents(world, nav);
    testInventory(world, nav);
    testStorm(world, nav);

    printf("\n%s (%d fallos)\n", g_failures == 0 ? "TODO CORRECTO" : "HAY FALLOS", g_failures);
    world.destroy();
    return g_failures == 0 ? 0 : 1;
}
