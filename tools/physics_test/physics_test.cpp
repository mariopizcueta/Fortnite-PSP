// Banco de pruebas de host del movimiento del jugador (no se compila para PSP).
//
// Ejecuta el MISMO PlayerController y FixedTimestep que el juego con un guion de
// entrada fijo, simulando distintas tasas de fotogramas, y comprueba que:
//   1. las cuatro direcciones del stick mueven al jugador relativo a la camara;
//   2. el resultado no depende de los FPS (60, 30, 20, 15, 144, irregular);
//   3. salto y gravedad: altura maxima esperada, vuelta al suelo, nunca por debajo;
//   4. un tiron (frame de 0.5 s) no produce un salto de posicion: se limita.
//
// Uso: sh tools/physics_test/build_and_run.sh   (o: make physics-test)
// Sale con codigo 1 si falla alguna comprobacion.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <initializer_list>
#include <vector>

#include "camera/third_person_camera.h"
#include "game/fixed_timestep.h"
#include "game/player.h"

using game::FixedTimestep;
using game::Player;
using game::PlayerController;
using game::PlayerInput;
using math3d::Vec3;

namespace {

int g_failures = 0;

// Mundo sin obstaculos (solo el suelo Y = 0) para las pruebas de movimiento basico.
const world::CollisionWorld& flatWorld() {
    static world::CollisionWorld w;
    static bool built = false;
    if (!built) {
        w.build(96.0f);
        built = true;
    }
    return w;
}

void check(bool ok, const char* what) {
    printf("  [%s] %s\n", ok ? " OK " : "FALLO", what);
    if (!ok) ++g_failures;
}

constexpr float kCameraYaw = 0.7f;  // camara girada: prueba que el movimiento es relativo
constexpr float kDuration = 5.0f;

// Guion: adelante, derecha, atras, izquierda, quieto; salto largo y salto corto.
// Los cambios caen en multiplos de 1/60, 1/30, 1/20 y 1/15 s.
PlayerInput scriptAt(double t) {
    PlayerInput in;
    in.cameraYaw = kCameraYaw;
    // Literales double: comparar con 2.2f (= 2.2000000477) haria que algun perfil
    // soltase X un frame mas tarde por la representacion del tiempo, no por la fisica.
    if (t < 1.0) in.moveY = -1.0f;
    else if (t < 1.8) in.moveX = 1.0f;
    else if (t < 2.6) in.moveY = 1.0f;
    else if (t < 3.4) in.moveX = -1.0f;
    in.jumpHeld = (t >= 0.4 && t < 1.2) || (t >= 2.0 && t < 2.2);
    return in;
}

struct RunResult {
    Vec3 finalPosition;
    float apexFirstJump = 0.0f;
    float apexSecondJump = 0.0f;
    float minY = 1e9f;
    float maxFrameDisplacement = 0.0f;
    bool landedAfterFirst = false;
    int frames = 0;
};

// frameDt(i) devuelve la duracion del frame i.
template <typename FrameDt>
RunResult simulate(FrameDt frameDt) {
    PlayerController pc;
    pc.reset({0.0f, 0.0f, 0.0f}, 0.0f, flatWorld());
    FixedTimestep ts;
    RunResult r;
    double t = 0.0;  // double: la suma de miles de dt en float derivaria
    bool prevHeld = false;
    for (int frame = 0; t < kDuration; ++frame) {
        const float dt = frameDt(frame);
        PlayerInput in = scriptAt(t);
        in.jumpPressed = in.jumpHeld && !prevHeld;
        prevHeld = in.jumpHeld;
        pc.setInput(in);

        const Vec3 before = pc.state().position;
        const int steps = ts.advance(dt);
        for (int s = 0; s < steps; ++s) {
            pc.step(FixedTimestep::kStep, flatWorld());
            const Player& p = pc.state();
            if (p.position.y < r.minY) r.minY = p.position.y;
            if (t < 2.0) {
                if (p.position.y > r.apexFirstJump) r.apexFirstJump = p.position.y;
                if (t > 1.0 && p.grounded()) r.landedAfterFirst = true;
            } else if (p.position.y > r.apexSecondJump) {
                r.apexSecondJump = p.position.y;
            }
        }
        const float moved = math3d::length(pc.state().position - before);
        if (moved > r.maxFrameDisplacement) r.maxFrameDisplacement = moved;
        t += dt;
        ++r.frames;
    }
    r.finalPosition = pc.state().position;
    return r;
}

// Desplazamiento tras mantener una direccion 1 s desde parado.
Vec3 holdDirection(float moveX, float moveY) {
    PlayerController pc;
    pc.reset({0.0f, 0.0f, 0.0f}, 0.0f, flatWorld());
    PlayerInput in;
    in.moveX = moveX;
    in.moveY = moveY;
    in.cameraYaw = kCameraYaw;
    pc.setInput(in);
    for (int i = 0; i < 60; ++i) pc.step(FixedTimestep::kStep, flatWorld());
    return pc.state().position;
}

void testDirections() {
    printf("\n1) Cuatro direcciones relativas a la camara (yaw %.2f rad, 1 s cada una)\n", kCameraYaw);
    const Vec3 forward{-sinf(kCameraYaw), 0.0f, -cosf(kCameraYaw)};
    const Vec3 right{cosf(kCameraYaw), 0.0f, -sinf(kCameraYaw)};
    struct Case {
        const char* name;
        float mx, my;
        Vec3 expected;
    } cases[] = {
        {"adelante  (stick arriba)", 0, -1, forward},
        {"atras     (stick abajo)", 0, 1, forward * -1.0f},
        {"derecha   (stick dcha)", 1, 0, right},
        {"izquierda (stick izda)", -1, 0, right * -1.0f},
    };
    for (const Case& c : cases) {
        const Vec3 d = holdDirection(c.mx, c.my);
        const float dist = math3d::length(d);
        const float alignment = math3d::dot(math3d::normalize(d), c.expected);
        printf("  %-26s desplazamiento (%+.2f, %+.2f, %+.2f) = %.2f m, alineacion %.4f\n", c.name,
               d.x, d.y, d.z, dist, alignment);
        check(alignment > 0.999f && dist > 5.0f && dist < 5.5f, c.name);
    }
}

void testFrameRateIndependence() {
    printf("\n2) Mismo guion a distintas tasas de fotogramas\n");
    const RunResult ref = simulate([](int) { return 1.0f / 60.0f; });

    struct Profile {
        const char* name;
        float tolerance;  // metros respecto a 60 FPS
        RunResult result;
    };
    srand(1234);
    std::vector<float> jitter(4000);
    for (float& j : jitter) j = 1.0f / 144.0f + (1.0f / 12.0f - 1.0f / 144.0f) * (rand() / (float)RAND_MAX);

    std::vector<Profile> profiles = {
        {"60 FPS", 0.001f, ref},
        {"30 FPS", 0.01f, simulate([](int) { return 1.0f / 30.0f; })},
        {"20 FPS", 0.01f, simulate([](int) { return 1.0f / 20.0f; })},
        {"15 FPS", 0.01f, simulate([](int) { return 1.0f / 15.0f; })},
        {"144 FPS (sin VSync)", 0.10f, simulate([](int) { return 1.0f / 144.0f; })},
        {"irregular 12-144 FPS", 0.40f, simulate([&jitter](int i) { return jitter[i % jitter.size()]; })},
    };

    printf("  %-22s %-26s %8s %8s %8s %6s\n", "perfil", "posicion final", "dif.(m)", "apex1", "apex2",
           "minY");
    for (Profile& p : profiles) {
        const RunResult& r = p.result;
        const float diff = math3d::length(r.finalPosition - ref.finalPosition);
        printf("  %-22s (%+7.3f,%+6.3f,%+7.3f) %8.4f %8.3f %8.3f %6.3f\n", p.name, r.finalPosition.x,
               r.finalPosition.y, r.finalPosition.z, diff, r.apexFirstJump, r.apexSecondJump, r.minY);
        char label[128];
        snprintf(label, sizeof(label), "%s: posicion final a <= %.2f m de la de 60 FPS", p.name, p.tolerance);
        check(diff <= p.tolerance, label);
        snprintf(label, sizeof(label), "%s: altura del salto largo igual (+-2 cm)", p.name);
        check(fabsf(r.apexFirstJump - ref.apexFirstJump) <= 0.02f, label);
        snprintf(label, sizeof(label), "%s: nunca por debajo del suelo", p.name);
        check(r.minY >= 0.0f, label);
    }
    check(ref.landedAfterFirst, "el jugador vuelve al suelo tras el primer salto");
    const float theoretical = game::tuning::kJumpSpeed * game::tuning::kJumpSpeed /
                              (2.0f * game::tuning::kGravity);
    printf("  altura teorica v^2/2g = %.3f m, simulada %.3f m\n", theoretical, ref.apexFirstJump);
    check(fabsf(ref.apexFirstJump - theoretical) < 0.08f, "salto largo ~ v^2 / 2g");
    check(ref.apexSecondJump < ref.apexFirstJump - 0.05f,
          "soltar X a los 0.2 s (antes de la cima) recorta el salto");
    check(ref.finalPosition.y == 0.0f, "termina apoyado en el suelo (Y = 0)");
}

// Salto con X mantenido `holdFrames` frames a 60 FPS; devuelve la altura maxima.
float jumpApex(int holdFrames) {
    PlayerController pc;
    pc.reset({0.0f, 0.0f, 0.0f}, 0.0f, flatWorld());
    float apex = 0.0f;
    for (int f = 0; f < 90; ++f) {
        PlayerInput in;
        in.jumpHeld = f < holdFrames;
        in.jumpPressed = f == 0;
        pc.setInput(in);
        pc.step(FixedTimestep::kStep, flatWorld());
        if (pc.state().position.y > apex) apex = pc.state().position.y;
    }
    return apex;
}

void testVariableJump() {
    printf("\n3) Salto variable (X mantenido N frames a 60 FPS)\n");
    const int holds[] = {1, 4, 8, 12, 20, 40};
    float previous = 0.0f;
    bool increasing = true;
    for (int h : holds) {
        const float apex = jumpApex(h);
        printf("  %2d frames (%3.0f ms): altura %.3f m\n", h, h * 1000.0f / 60.0f, apex);
        if (apex < previous) increasing = false;
        previous = apex;
    }
    check(jumpApex(4) < 0.6f * jumpApex(40), "un toque corto salta menos del 60 % del salto completo");
    check(increasing, "mantener X mas tiempo nunca salta menos");
}

void testHitchAndFalls() {
    printf("\n4) Tirones y caidas\n");
    // Un frame de 0.5 s en mitad de la carrera.
    const RunResult hitch = simulate([](int i) { return i == 90 ? 0.5f : 1.0f / 60.0f; });
    const float maxPerFrame = FixedTimestep::kMaxFrameTime *
                              sqrtf(game::tuning::kRunSpeed * game::tuning::kRunSpeed +
                                    game::tuning::kMaxFallSpeed * game::tuning::kMaxFallSpeed);
    printf("  frame de 0.5 s: desplazamiento maximo en un frame %.3f m (limite %.3f m)\n",
           hitch.maxFrameDisplacement, maxPerFrame);
    check(hitch.maxFrameDisplacement <= 6.0f * FixedTimestep::kStep * 5.6f,
          "un tiron mueve como mucho 6 pasos de carrera (0.55 m)");
    check(hitch.minY >= 0.0f, "con tiron: nunca por debajo del suelo");

    // Salto con frames de 0.1 s (6 pasos por frame): el aterrizaje no atraviesa el suelo.
    PlayerController pc2;
    pc2.reset({10.0f, 0.0f, 10.0f}, 0.0f, flatWorld());
    FixedTimestep ts;
    PlayerInput jump;
    jump.jumpPressed = jump.jumpHeld = true;
    pc2.setInput(jump);
    float minY = 1e9f;
    for (int f = 0; f < 30; ++f) {
        const int steps = ts.advance(0.1f);
        for (int s = 0; s < steps; ++s) {
            pc2.step(FixedTimestep::kStep, flatWorld());
            if (pc2.state().position.y < minY) minY = pc2.state().position.y;
        }
    }
    printf("  salto a 10 FPS: Y final %.3f, minimo %.3f, estado %s\n", pc2.state().position.y, minY,
           game::jumpStateName(pc2.state().jumpState));
    check(minY >= 0.0f && pc2.state().grounded(), "a 10 FPS aterriza sin atravesar el suelo");
}

struct CameraRun {
    float maxLag = 0.0f;         // distancia horizontal maxima foco-jugador corriendo
    float maxEyeJump = 0.0f;     // mayor salto del ojo en un frame
    float maxPlayerStep = 0.0f;  // mayor avance del jugador en un frame (referencia)
    float settleError = 0.0f;    // distancia foco-jugador 1 s despues de parar
    Vec3 eyeAfterRun;            // posicion del ojo al terminar de correr
};

// Corre 2 s hacia delante (camara detras) y para 1 s, a `fps` constantes.
CameraRun cameraFollow(float fps) {
    PlayerController pc;
    pc.reset({0.0f, 0.0f, 0.0f}, 0.0f, flatWorld());
    FixedTimestep ts;
    camera::ThirdPersonCamera cam;
    cam.reset(pc.state().position, 0.0f);
    input::Input noButtons;  // la cruceta no se toca: solo seguimiento
    CameraRun r;
    const float dt = 1.0f / fps;
    Vec3 prevEye = cam.eye();
    Vec3 prevPos = pc.state().position;
    double t = 0.0;
    const int frames = static_cast<int>(3.0f * fps + 0.5f);
    for (int f = 0; f < frames; ++f) {
        PlayerInput in;
        in.cameraYaw = cam.yaw();
        if (t < 2.0) in.moveY = -1.0f;
        pc.setInput(in);
        const int steps = ts.advance(dt);
        for (int s = 0; s < steps; ++s) pc.step(FixedTimestep::kStep, flatWorld());
        const Player p = pc.interpolated(ts.alpha());
        cam.update(noButtons, p.position, p.yaw, dt, false, nullptr, nullptr);

        const Vec3 focusXZ{cam.focus().x, 0.0f, cam.focus().z};
        const float lag = math3d::length(focusXZ - Vec3{p.position.x, 0.0f, p.position.z});
        if (t < 2.0 && lag > r.maxLag) r.maxLag = lag;
        const float eyeJump = math3d::length(cam.eye() - prevEye);
        const float playerStep = math3d::length(p.position - prevPos);
        if (eyeJump > r.maxEyeJump) r.maxEyeJump = eyeJump;
        if (playerStep > r.maxPlayerStep) r.maxPlayerStep = playerStep;
        prevEye = cam.eye();
        prevPos = p.position;
        t += dt;
        if (f == static_cast<int>(2.0f * fps + 0.5f) - 1) r.eyeAfterRun = cam.eye();
        r.settleError = lag;
    }
    return r;
}

void testCameraFollow() {
    printf("\n5) Camara en tercera persona: seguimiento suave\n");
    const CameraRun ref = cameraFollow(60.0f);
    const float fpsList[] = {60.0f, 30.0f, 20.0f, 144.0f};
    for (float fps : fpsList) {
        const CameraRun r = cameraFollow(fps);
        const float diff = math3d::length(r.eyeAfterRun - ref.eyeAfterRun);
        printf("  %5.0f FPS: retraso max %.3f m, mayor salto del ojo %.3f m/frame (jugador %.3f), "
               "error tras parar %.4f m, ojo vs 60 FPS %.3f m\n",
               fps, r.maxLag, r.maxEyeJump, r.maxPlayerStep, r.settleError, diff);
        char label[160];
        snprintf(label, sizeof(label), "%.0f FPS: la camara va por detras (retraso > 0) pero cerca (< 1 m)", fps);
        check(r.maxLag > 0.05f && r.maxLag < 1.0f, label);
        snprintf(label, sizeof(label), "%.0f FPS: el ojo no da tirones (salto <= 1.5x el del jugador)", fps);
        check(r.maxEyeJump <= r.maxPlayerStep * 1.5f + 0.01f, label);
        snprintf(label, sizeof(label), "%.0f FPS: tras parar, la camara se centra (< 2 cm)", fps);
        check(r.settleError < 0.02f, label);
        snprintf(label, sizeof(label), "%.0f FPS: misma posicion de camara que a 60 FPS (< 10 cm)", fps);
        check(diff < 0.10f, label);
    }
}

// --- 6) Colisiones con cajas (AABB) ----------------------------------------------------

world::CollisionWorld makeWorld(std::initializer_list<world::Aabb> boxes) {
    world::CollisionWorld w;
    for (const world::Aabb& box : boxes) w.add(box, world::kSolid);
    w.build(96.0f);
    return w;
}

// El cuerpo (por encima de un escalon) no debe solapar ninguna caja de movimiento.
bool penetrates(const world::CollisionWorld& w, const Player& p) {
    const float r = game::tuning::kBodyRadius - 0.01f;
    const world::Aabb body{{p.position.x - r, p.position.y + game::tuning::kStepHeight, p.position.z - r},
                           {p.position.x + r, p.position.y + game::tuning::kBodyHeight - 0.01f, p.position.z + r}};
    return w.overlapsAny(body, world::kBlocksMovement);
}

struct CollisionRun {
    Player final;
    float maxY = 0.0f;
    bool landedHigh = false;  // estuvo en suelo por encima de 1 m
    bool penetrated = false;
};

CollisionRun runOn(const world::CollisionWorld& w, Vec3 spawn, float moveX, float moveY, float seconds,
                   float fps, float jumpAt = -1.0f) {
    PlayerController pc;
    pc.reset(spawn, 0.0f, w);
    FixedTimestep ts;
    CollisionRun r;
    const float dt = 1.0f / fps;
    double t = 0.0;
    bool jumped = false;
    while (t < seconds) {
        PlayerInput in;
        in.moveX = moveX;
        in.moveY = moveY;
        if (jumpAt >= 0.0f && !jumped && t >= jumpAt) {
            in.jumpPressed = true;
            jumped = true;
        }
        in.jumpHeld = jumped && t < jumpAt + 0.5;
        pc.setInput(in);
        const int steps = ts.advance(dt);
        for (int i = 0; i < steps; ++i) {
            pc.step(FixedTimestep::kStep, w);
            const Player& p = pc.state();
            if (p.position.y > r.maxY) r.maxY = p.position.y;
            if (p.grounded() && p.position.y > 1.0f) r.landedHigh = true;
            if (penetrates(w, p)) r.penetrated = true;
        }
        t += dt;
    }
    r.final = pc.state();
    return r;
}

void testCollisions() {
    printf("\n6) Colisiones con cajas AABB (stick a la derecha = +X)\n");
    const float r = game::tuning::kBodyRadius;

    {
        // Obstaculos largos: en 2 s se corren ~10 m y no debe llegarse a su final.
        const auto w = makeWorld({{{2.0f, 0.0f, -30.0f}, {2.4f, 3.0f, 30.0f}}});
        const CollisionRun c = runOn(w, {0, 0, 0}, 1.0f, 0.0f, 2.0f, 60.0f);
        printf("  pared en x=2: x final %.3f (limite %.3f)\n", c.final.position.x, 2.0f - r);
        check(c.final.position.x <= 2.0f - r + 0.005f && c.final.position.x > 1.5f && !c.penetrated,
              "una pared detiene al jugador sin que la atraviese");
        const CollisionRun d = runOn(w, {0, 0, 0}, 1.0f, -1.0f, 2.0f, 60.0f);
        printf("  en diagonal contra la pared: (%.2f, %.2f)\n", d.final.position.x, d.final.position.z);
        check(d.final.position.x <= 2.0f - r + 0.005f && d.final.position.z < -2.0f && !d.penetrated,
              "en diagonal se desliza a lo largo de la pared");
    }
    {
        const auto w = makeWorld({{{2.0f, 0.0f, -5.0f}, {2.1f, 3.0f, 5.0f}}});
        const CollisionRun c = runOn(w, {0, 0, 0}, 1.0f, 0.0f, 2.0f, 10.0f);
        printf("  pared de 10 cm a 10 FPS (6 pasos por frame): x final %.3f\n", c.final.position.x);
        check(c.final.position.x < 2.0f && !c.penetrated, "sin tunelado a traves de una pared fina a 10 FPS");
    }
    {
        const auto w = makeWorld({{{2.0f, 0.0f, -5.0f}, {30.0f, 0.36f, 5.0f}}});
        const CollisionRun c = runOn(w, {0, 0, 0}, 1.0f, 0.0f, 2.0f, 60.0f);
        printf("  escalon de 0.36 m: (%.2f, %.2f)\n", c.final.position.x, c.final.position.y);
        check(c.final.position.x > 4.0f && fabsf(c.final.position.y - 0.36f) < 0.001f, "sube solo un escalon de 0.36 m");
    }
    {
        const auto w = makeWorld({{{2.0f, 0.0f, -5.0f}, {10.0f, 0.6f, 5.0f}}});
        const CollisionRun c = runOn(w, {0, 0, 0}, 1.0f, 0.0f, 2.0f, 60.0f);
        printf("  bordillo de 0.6 m: (%.2f, %.2f)\n", c.final.position.x, c.final.position.y);
        check(c.final.position.x <= 2.0f - r + 0.005f && c.final.position.y == 0.0f, "un obstaculo de 0.6 m bloquea andando");
    }
    {
        const auto w = makeWorld({{{2.0f, 0.0f, -1.0f}, {5.0f, 1.2f, 1.0f}}});
        const CollisionRun c = runOn(w, {0, 0, 0}, 1.0f, 0.0f, 2.0f, 60.0f, 0.15f);
        printf("  saltar a una caja de 1.2 m: maxima Y %.2f, aterriza encima %s, final (%.2f, %.2f)\n", c.maxY,
               c.landedHigh ? "si" : "no", c.final.position.x, c.final.position.y);
        check(c.landedHigh && !c.penetrated, "saltando se sube encima de una caja de 1.2 m");
        check(c.final.position.y == 0.0f && c.final.position.x > 5.0f, "al pasar el borde cae de nuevo al suelo");
    }
    {
        const auto w = makeWorld({{{-2.0f, 2.4f, -2.0f}, {2.0f, 3.0f, 2.0f}}});
        const CollisionRun c = runOn(w, {0, 0, 0}, 0.0f, 0.0f, 1.0f, 60.0f, 0.05f);
        printf("  techo a 2.4 m: Y maxima de los pies %.3f (limite %.3f)\n", c.maxY, 2.4f - game::tuning::kBodyHeight);
        check(c.maxY <= 2.4f - game::tuning::kBodyHeight + 0.001f, "la cabeza choca con un techo");
    }
    {
        // Caida desde 40 m sobre una plataforma de 10 cm de grosor: velocidad terminal.
        const auto w = makeWorld({{{-3.0f, 4.9f, -3.0f}, {3.0f, 5.0f, 3.0f}}});
        game::CharacterBody body;
        body.reset({0.0f, 0.0f, 0.0f}, 0.0f, w);
        body.state().position.y = 40.0f;
        body.state().jumpState = game::JumpState::Falling;
        game::MoveIntent none;
        float maxSpeed = 0.0f;
        for (int i = 0; i < 300 && !body.state().grounded(); ++i) {
            body.step(none, FixedTimestep::kStep, w);
            if (-body.state().velocity.y > maxSpeed) maxSpeed = -body.state().velocity.y;
        }
        printf("  caida de 35 m sobre plataforma de 10 cm: Y final %.3f, velocidad max %.1f m/s\n",
               body.state().position.y, maxSpeed);
        check(body.state().grounded() && fabsf(body.state().position.y - 5.0f) < 0.001f,
              "aterriza sobre una plataforma fina sin atravesarla");
    }
    {
        // Esquina en L: empujando en diagonal hacia la esquina nunca se mete dentro.
        const auto w = makeWorld({{{2.0f, 0.0f, -5.0f}, {2.4f, 3.0f, 5.0f}}, {{-5.0f, 0.0f, -2.4f}, {2.4f, 3.0f, -2.0f}}});
        const CollisionRun c = runOn(w, {0, 0, 0}, 1.0f, -1.0f, 2.0f, 60.0f);
        printf("  esquina interior: (%.3f, %.3f)\n", c.final.position.x, c.final.position.z);
        check(!c.penetrated && c.final.position.x <= 2.0f - r + 0.005f && c.final.position.z >= -2.0f + r - 0.005f,
              "en una esquina queda pegado a las dos paredes sin atravesarlas");
    }
}

}  // namespace

int main() {
    printf("Pruebas de movimiento del jugador (paso fijo %.4f s, maximo %.2f s por frame)\n",
           FixedTimestep::kStep, FixedTimestep::kMaxFrameTime);
    testDirections();
    testFrameRateIndependence();
    testVariableJump();
    testHitchAndFalls();
    testCameraFollow();
    testCollisions();
    printf("\n%s (%d fallos)\n", g_failures == 0 ? "TODO CORRECTO" : "HAY FALLOS", g_failures);
    return g_failures == 0 ? 0 : 1;
}
