// Modelo de personaje low-poly compartido por jugador y bots, con aspecto de skin de
// Fortnite en estilo PS1 (Referencias/Personaje: pelo rubio con tupe, camiseta sin
// mangas con panuelo, brazalete, guantes, pantalon de camuflaje con correas, botas con
// rodilleras).
//
// Fase 7: deja de ser un conjunto de cajas. Cada pieza es un "loft" de anillos elipticos
// (volumenes redondeados y afinados con proporciones heroicas: hombros anchos, cintura
// estrecha, manos y botas grandes), con sombreado suave horneado por vertice y una
// textura de detalle de 64x64 sin filtrar (cara, camuflaje, tejido, pelo, suela) que se
// tine con la paleta. Mide ~1.9 m, pies en Y = 0, mira hacia -Z local.
//
// Nueve piezas articuladas: cuerpo (con cabeza), brazos y antebrazos (codos), muslos y
// espinillas (rodillas). Las mallas se crean una vez por paleta y en dos niveles de
// detalle (el bajo para los bots lejanos); el estado de animacion (CharacterAnim) es de
// cada entidad. Asi 30 bots comparten 4 juegos de mallas.
#pragma once

#include <vector>

#include "game/character.h"
#include "game/inventory.h"
#include "game/item_models.h"
#include "gfx/mesh.h"
#include "gfx/texture.h"
#include "world/scene_builder.h"
#include "world/textures.h"

namespace game {

struct CharacterPalette {
    u32 skin, hair, brow;
    u32 shirt, vest, scarf;
    u32 belt, buckle;
    u32 pants, strap;
    u32 boot, kneePad;
    u32 glove, armband;
    bool hasScarf;
    bool hasHelmet;   // casco en lugar de pelo (usa el color `hair`)
    bool camoPants;   // pantalon con el estampado de camuflaje
};

namespace palettes {
extern const CharacterPalette kPlayer;    // como Referencias/Personaje
extern const CharacterPalette kCommando;  // bots: negro y camuflaje gris
extern const CharacterPalette kOrange;    // bots: mono naranja
extern const CharacterPalette kRanger;    // bots: rojo y azul con casco
}  // namespace palettes

// Estado de animacion de una entidad (solo visual, avanza con el tiempo de frame).
// Paso con flexion de rodillas y codos, salto, respiracion en reposo, rifle sujeto con
// las dos manos al apuntar, retroceso al disparar, sacudida al recibir un impacto, golpe
// de pico y caida al ser eliminado.
struct CharacterAnim {
    float walkPhase = 0.0f;
    float leftLeg = 0.0f, rightLeg = 0.0f, leftArm = 0.0f, rightArm = 0.0f;
    float leftKnee = 0.0f, rightKnee = 0.0f, leftElbow = 0.0f, rightElbow = 0.0f;
    float leftArmIn = 0.0f, rightArmIn = 0.0f;  // giro del brazo hacia el centro
    float idleTime = 0.0f;     // respiracion
    float recoil = 0.0f;       // 1 al disparar, decae
    float flinch = 0.0f;       // 1 al recibir un impacto, decae
    float deathSeconds = 0.0f; // tiempo desde la eliminacion (0 = vivo)
    float swing = 0.0f;        // 1 al golpear con el pico, decae
    u32 footsteps = 0;         // pisadas dadas (sube cada vez que un pie apoya: sonido de pasos)

    void update(const CharacterState& state, bool aiming, bool alive, float dt);
    void onShot() { recoil = 1.0f; }
    void onHit() { flinch = 1.0f; }
    void onSwing() { swing = 1.0f; }
};

// Postura del cuerpo: normal, caida libre (tumbado boca abajo, brazos y piernas abiertos),
// colgado del ala delta (brazos arriba), agachado (CRUCETA abajo) o bailando (CRUCETA
// arriba). Lo que se lleva en la mano es un game::ItemModel (item_models.h).
enum class BodyPose : u8 { Normal, Skydive, Glide, Crouch, Dance };

class CharacterModel {
public:
    bool build(const CharacterPalette& palette);
    void destroy();

    // Dibuja sombra + personaje. `eliminated`: cae y queda tumbado boca arriba (si la
    // animacion no ha contado la caida, aparece ya tumbado). `lowDetail`: version de
    // pocos poligonos para personajes lejanos. Requiere gfx::begin3D().
    // `held`: lo que lleva en la mano derecha (de `items`; None o sin `items`: nada).
    void draw(const CharacterState& state, const CharacterAnim& anim,
              const world::TextureLibrary& textures, bool eliminated,
              ItemModel held = ItemModel::None, bool lowDetail = false,
              BodyPose pose = BodyPose::Normal, const ItemModels* items = nullptr) const;

    // Triangulos del nivel de detalle alto / bajo (sin lo que lleva en la mano).
    int triangles() const { return m_triangles[0]; }
    int lowDetailTriangles() const { return m_triangles[1]; }

private:
    enum PartId { kBody, kUpperArmL, kUpperArmR, kForearmL, kForearmR, kThighL, kThighR, kShinL, kShinR, kPartCount };
    struct Lod {
        gfx::Mesh parts[kPartCount];
    };

    bool buildLod(const CharacterPalette& palette, int sides, Lod& out);

    Lod m_lods[2];
    gfx::Mesh m_shadow;
    gfx::Texture m_skin;  // textura de detalle (64x64, 5551)
    int m_triangles[2] = {0, 0};
};

}  // namespace game
