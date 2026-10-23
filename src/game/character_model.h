// Modelo de personaje low-poly hecho de prismas, compartido por jugador y bots.
//
// La forma sale de Referencias/Personaje (pelo, camiseta sin mangas con panuelo,
// brazalete, guantes, pantalon de camuflaje con correas, botas con rodilleras) y lleva
// un rifle de prismas en la mano derecha (color de Referencias/Armas). Mide ~1.9 m,
// pies en Y = 0, mira hacia -Z local.
//
// Cinco piezas: cuerpo, 2 piernas, 2 brazos. Las mallas se crean una vez por paleta;
// el estado de animacion (CharacterAnim) es de cada entidad. Asi 30 bots comparten
// 4 juegos de mallas.
#pragma once

#include <vector>

#include "game/character.h"
#include "world/scene_builder.h"
#include "world/textures.h"

namespace game {

struct CharacterPalette {
    u32 skin, hair, shirt, scarf, belt, pantsTint, strap, boot, kneePad, glove, armband;
    bool hasScarf;
    bool hasHelmet;  // casco en lugar de pelo
};

namespace palettes {
extern const CharacterPalette kPlayer;    // como Referencias/Personaje
extern const CharacterPalette kCommando;  // bots: negro y camuflaje gris
extern const CharacterPalette kOrange;    // bots: mono naranja
extern const CharacterPalette kRanger;    // bots: rojo y azul con casco
}  // namespace palettes

// Estado de animacion de una entidad (solo visual, avanza con el tiempo de frame).
// Ademas del paso y el salto: respiracion en reposo, retroceso al disparar, sacudida al
// recibir un impacto y caida al ser eliminado (todo con 5 matrices, sin coste extra).
struct CharacterAnim {
    float walkPhase = 0.0f;
    float leftLeg = 0.0f, rightLeg = 0.0f, leftArm = 0.0f, rightArm = 0.0f;
    float idleTime = 0.0f;     // respiracion
    float recoil = 0.0f;       // 1 al disparar, decae
    float flinch = 0.0f;       // 1 al recibir un impacto, decae
    float deathSeconds = 0.0f; // tiempo desde la eliminacion (0 = vivo)
    float swing = 0.0f;        // 1 al golpear con el pico, decae

    void update(const CharacterState& state, bool aiming, bool alive, float dt);
    void onShot() { recoil = 1.0f; }
    void onHit() { flinch = 1.0f; }
    void onSwing() { swing = 1.0f; }
};

// Lo que lleva en la mano derecha.
enum class HeldItem : u8 { Rifle, Pickaxe, Empty };

class CharacterModel {
public:
    bool build(const CharacterPalette& palette);
    void destroy();

    // Dibuja sombra + personaje. `eliminated`: cae y queda tumbado boca arriba (si la
    // animacion no ha contado la caida, aparece ya tumbado). Requiere gfx::begin3D().
    void draw(const CharacterState& state, const CharacterAnim& anim,
              const world::TextureLibrary& textures, bool eliminated,
              HeldItem held = HeldItem::Rifle) const;

    int triangles() const { return m_triangles; }

private:
    struct Part {
        std::vector<world::StaticBatch> batches;
        math3d::Vec3 pivot;  // articulacion en coordenadas del modelo
    };

    void drawPart(const Part& part, const math3d::Mat4& model,
                  const world::TextureLibrary& textures) const;

    Part m_body, m_leftLeg, m_rightLeg, m_leftArm, m_rightArm;
    Part m_rightArmPickaxe, m_rightArmEmpty;  // variantes del brazo derecho
    std::vector<world::StaticBatch> m_shadow;
    int m_triangles = 0;
};

}  // namespace game
