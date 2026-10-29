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
    // v0.14: zombi (piel enferma, ojos hundidos y brillantes, ropa rota, heridas) y mochila a
    // la espalda (lo que mas se ve desde la camara: todos los humanos la llevan).
    bool zombie = false;
    bool backpack = true;
};

namespace palettes {
extern const CharacterPalette kPlayer;    // como Referencias/Personaje
extern const CharacterPalette kCommando;  // bots: negro y camuflaje gris
extern const CharacterPalette kOrange;    // bots: mono naranja
extern const CharacterPalette kRanger;    // bots: rojo y azul con casco
extern const CharacterPalette kZombie;        // v0.14: zombi con ropa de calle
extern const CharacterPalette kZombieWorker;  // v0.14: zombi con mono de trabajo
}  // namespace palettes

// Lo que hace el personaje este frame, ademas de su estado fisico (lo rellena la
// aplicacion para el jugador y los bots).
struct AnimInput {
    bool alive = true;
    bool aiming = false;        // apuntando o disparando: el arma sube a la cara
    float aimPitch = 0.0f;      // rad: hacia donde apunta en vertical (+ arriba)
    bool crouched = false;
    bool reloading = false;
    float reloadProgress = 0.0f;  // 0..1
    bool usingItem = false;       // curandose (el consumible en la mano)
    float useProgress = 0.0f;     // 0..1
};

// Estado de animacion de una entidad (solo visual, avanza con el tiempo de frame).
//
// Parche (fase 8): ademas de las articulaciones de antes (piernas y brazos sin arma), lleva
// mezclas suavizadas entre posturas: quieto / andando / corriendo (con la zancada segun la
// velocidad y hacia atras al retroceder), en el aire subiendo o cayendo, el aterrizaje,
// agachado, apuntar, recargar, curarse y el retroceso. Con un arma en la mano los brazos no
// se animan por angulos: se colocan con cinematica inversa sobre la empunadura y el
// guardamanos de una postura del arma que depende de su tipo (fusil, pistola, al hombro).
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
    // Mezclas (0..1, suavizadas) y estado de las acciones.
    float aimBlend = 0.0f;     // 0 = arma baja (correr), 1 = arma a la cara
    float runBlend = 0.0f;     // 0 = quieto, 1 = corriendo a la velocidad maxima
    float airBlend = 0.0f;     // 0 = en el suelo, 1 = en el aire
    float rise = 0.0f;         // en el aire: 1 subiendo, -1 cayendo
    float landing = 0.0f;      // 1 al tocar el suelo tras un salto, decae
    float crouchBlend = 0.0f;
    float aimPitch = 0.0f;     // suavizado
    float reload = 0.0f;       // 0..1 de la recarga (0 si no recarga)
    float reloadBlend = 0.0f;
    float use = 0.0f;          // 0..1 del consumible en uso
    float useBlend = 0.0f;
    float lean = 0.0f;         // inclinacion del tronco al correr (rad)
    float stride = 1.0f;       // +1 hacia delante, -1 hacia atras
    // v0.14: giro del tronco con cada zancada, balanceo lateral, cambio de peso en reposo,
    // hacia donde cae al ser eliminado, andar por el agua (levantando las rodillas) y el
    // giro de lado al cambiar de direccion.
    float twist = 0.0f;
    float sway = 0.0f;
    float deathSide = 1.0f;    // +1 de espaldas, -1 de bruces
    float wadeBlend = 0.0f;
    float turnLean = 0.0f;
    float lastYaw = 0.0f;

    void update(const CharacterState& state, const AnimInput& in, float dt);
    // Forma corta (lobby, previsualizaciones): solo apuntar y vivo.
    void update(const CharacterState& state, bool aiming, bool alive, float dt) {
        AnimInput in;
        in.aiming = aiming;
        in.alive = alive;
        update(state, in, dt);
    }
    void onShot() { recoil = 1.0f; }
    void onHit() { flinch = 1.0f; }
    void onSwing() { swing = 1.0f; }
};

// Postura del cuerpo: normal, caida libre (tumbado boca abajo, brazos y piernas abiertos),
// colgado del ala delta (brazos arriba), agachado (CRUCETA abajo) o bailando (CRUCETA
// arriba). Lo que se lleva en la mano es un game::ItemModel (item_models.h).
// v0.14: zombi (encorvado, brazos al frente, cojeando; el golpe es un zarpazo).
enum class BodyPose : u8 { Normal, Skydive, Glide, Crouch, Dance, Zombie };

class CharacterModel {
public:
    bool build(const CharacterPalette& palette);
    void destroy();

    // Dibuja sombra + personaje. `eliminated`: cae y queda tumbado boca arriba (si la
    // animacion no ha contado la caida, aparece ya tumbado). `lowDetail`: version de
    // pocos poligonos para personajes lejanos. Requiere gfx::begin3D().
    // `held`: lo que lleva en la mano derecha (de `items`; None o sin `items`: nada).
    // `scale`: tamano (los brutos del modo zombi, 1.3).
    void draw(const CharacterState& state, const CharacterAnim& anim,
              const world::TextureLibrary& textures, bool eliminated,
              ItemModel held = ItemModel::None, bool lowDetail = false,
              BodyPose pose = BodyPose::Normal, const ItemModels* items = nullptr, float scale = 1.0f) const;

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
