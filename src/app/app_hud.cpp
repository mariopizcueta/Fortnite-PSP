// HUD de la partida (Referencias/Referencia gameplay.png y Referencias/HUD/):
//   arriba izquierda   nombre y registro de eliminaciones
//   arriba centro      brujula; en modo construccion, el rotulo CONSTRUCCION
//   arriba derecha     minimapa con la zona, tiempo de zona, vivos y bajas, materiales
//   abajo izquierda    municion "30 | 120", escudo y vida
//   abajo derecha      ranuras de construccion y de armas (pico + SCAR)
// Ademas, las paginas de depuracion y de controles (SELECT).
#include "app/app.h"

#include <malloc.h>
#include <math.h>
#include <string.h>

#include <initializer_list>

#include "app/app_common.h"
#include "core/i18n.h"
#include "audio/audio_system.h"
#include "world/storm.h"
#include "world/world_units.h"

namespace app {
namespace {

using namespace common;
using math3d::Vec3;
using i18n::T;
using i18n::tr;
namespace atlas = assets::ui;

// Minimapa: ventana de 64 texeles (96 m) alrededor del jugador, dibujada en 76x76 px.
constexpr float kMapX = 398.0f, kMapY = 6.0f, kMapSize = 76.0f;
constexpr float kMapWindow = 64.0f;
constexpr float kMapTexels = 128.0f;
constexpr float kMetersPerTexel = 2.0f * world::kMapHalfSize / kMapTexels;

// Ranuras inferiores derechas.
constexpr float kBuildRowY = 196.0f;
constexpr float kBuildSlotX = 326.0f, kBuildSlotStep = 30.0f, kBuildSlotSize = 28.0f;
constexpr float kWeaponRowY = 230.0f;
constexpr float kWeaponSlotX = 254.0f, kWeaponSlotStep = 37.0f, kWeaponSlotSize = 34.0f;
constexpr int kWeaponSlots = 6;

// Barras de escudo y vida.
constexpr float kBarX = 32.0f, kBarW = 144.0f;
constexpr float kShieldY = 239.0f, kHealthY = 253.0f;

// Botones que se muestran en la lista de "Pulsado", en orden de lectura.
constexpr u32 kListedButtons[] = {
    PSP_CTRL_UP,       PSP_CTRL_DOWN,     PSP_CTRL_LEFT,     PSP_CTRL_RIGHT,
    PSP_CTRL_TRIANGLE, PSP_CTRL_CIRCLE,   PSP_CTRL_CROSS,    PSP_CTRL_SQUARE,
    PSP_CTRL_LTRIGGER, PSP_CTRL_RTRIGGER, PSP_CTRL_SELECT,   PSP_CTRL_START,
};

// Mando dibujado en la pagina de controles.
// Mando dibujado en la pagina de controles con los iconos de Recursos/Texturas PSP
// (centro de cada icono); se iluminan al pulsar el boton.
struct PadButton {
    u32 mask;
    ui::Button button;
    float cx, cy;
};
constexpr PadButton kPadButtons[] = {
    {PSP_CTRL_LTRIGGER, ui::Button::L, 342, 50},     {PSP_CTRL_RTRIGGER, ui::Button::R, 444, 50},
    {PSP_CTRL_UP, ui::Button::Up, 342, 72},          {PSP_CTRL_DOWN, ui::Button::Down, 342, 100},
    {PSP_CTRL_LEFT, ui::Button::Left, 328, 86},      {PSP_CTRL_RIGHT, ui::Button::Right, 356, 86},
    {PSP_CTRL_TRIANGLE, ui::Button::Triangle, 444, 72}, {PSP_CTRL_CROSS, ui::Button::Cross, 444, 100},
    {PSP_CTRL_SQUARE, ui::Button::Square, 430, 86},  {PSP_CTRL_CIRCLE, ui::Button::Circle, 458, 86},
    {PSP_CTRL_SELECT, ui::Button::Select, 378, 118}, {PSP_CTRL_START, ui::Button::Start, 408, 118},
};

// Filas de la pagina de controles: titulos o una o dos ayudas (columna izquierda y derecha).
struct ControlRow {
    T title;  // si no es T::None, la fila es un titulo
    ui::Hint left[3];
    int leftCount;
    ui::Hint right[2];
    int rightCount;
};
using B = ui::Button;
// Las acciones (saltar, construir...) se dibujan con el boton que tengan asignado.
const ControlRow kControlRows[] = {
    {T::ControlsCombat, {}, 0, {}, 0},
    {T::None, {{B::Stick, T::CtlMove}}, 1, {{B::Jump, T::CtlJump}}, 1},
    {T::None, {{B::Fire, T::CtlFire}}, 1, {{B::Interact, T::CtlReload}}, 1},
    {T::None, {{B::Aim, T::CtlAim}}, 1, {{B::Pickaxe, T::CtlPickaxe}}, 1},
    {T::None, {{B::Left, T::None}, {B::Right, T::CtlItem}}, 2, {{B::Aim, T::None}, {B::Stick, T::CtlLook}}, 2},
    {T::None, {{B::Crouch, T::CtlCrouch}}, 1, {{B::Emote, T::CtlEmote}}, 1},
    {T::ControlsBuilding, {}, 0, {}, 0},
    {T::None, {{B::Build, T::CtlBuild}}, 1, {{B::Fire, T::CtlPlacePiece}}, 1},
    {T::None, {{B::Left, T::None}, {B::Right, T::CtlPiece}}, 2, {{B::Interact, T::CtlMaterial}}, 1},
    {T::ControlsMatch, {}, 0, {}, 0},
    {T::None, {{B::Start, T::CtlPause}}, 1, {{B::Select, T::CtlMap}}, 1},
    {T::None, {{B::L, T::None}, {B::Select, T::CtlHudPages}}, 2, {}, 0},
    {T::None, {{B::L, T::None}, {B::R, T::None}, {B::Start, T::CtlQuit}}, 3, {}, 0},
};
constexpr int kControlsX = 14, kControlsColumn = 14 + 8 * 19, kControlsTop = 40;
constexpr int kTitleHeight = 12, kHintRowHeight = 14;

// Recorre las filas con su altura: fn(fila, y). Devuelve la y siguiente.
template <typename Fn>
int forEachControlRow(Fn&& fn) {
    int y = kControlsTop;
    for (const ControlRow& row : kControlRows) {
        if (row.title != T::None && row.title != kControlRows[0].title) y += 3;
        fn(row, y);
        y += row.title != T::None ? kTitleHeight : kHintRowHeight;
    }
    return y + 3;
}

const ui::Hint kHudPageHint[] = {{B::Select, T::HintNextHudPage}};
const ui::Hint kBackHint[] = {{B::Circle, T::HintBackToLobby}};
const ui::Hint kFreeCameraHint[] = {{B::Triangle, T::HintFreeCamera}};
constexpr int kFreeCameraHintY = 256;
const ui::Hint kReloadHint[] = {{B::Interact, T::HintReload}};
const ui::Hint kBuildHints[] = {{B::Fire, T::HintPlace}, {B::Interact, T::HintMaterial}, {B::Build, T::HintExit}};
const ui::Hint kTrapHints[] = {{B::Fire, T::HintPlaceTrap}, {B::Build, T::HintExit}};
const ui::Hint kBusHint[] = {{B::Jump, T::BusJump}};
const ui::Hint kGliderHint[] = {{B::Jump, T::GliderOpen}};
constexpr int kPromptY = 180;

constexpr float kStickBoxX = 361.0f;
constexpr float kStickBoxY = 130.0f;
constexpr float kStickBoxSize = 56.0f;

// Concatena los nombres de los botones mantenidos sin pasar de `capacity`.
void heldButtonsString(u32 buttons, char* out, size_t capacity) {
    out[0] = '\0';
    size_t len = 0;
    for (u32 mask : kListedButtons) {
        if (!(buttons & mask)) continue;
        const char* name = input::buttonName(mask);
        const size_t need = strlen(name) + (len ? 1 : 0);
        if (len + need + 1 > capacity) {
            if (len + 3 < capacity) strcat(out, "..");
            return;
        }
        if (len) strcat(out, " ");
        strcat(out, name);
        len += need;
    }
    if (len == 0) snprintf(out, capacity, "%s", tr(T::PadNone));
}

constexpr float kHarvestPopupSeconds = 0.9f;
constexpr T kBuildPieceNames[] = {T::PieceWall, T::PieceFloor, T::PieceRamp, T::ItemSpikeTrap};

// Fondo de ranura segun la rareza (gris, verde, azul, morado, dorado como en Fortnite).
u32 rarityColor(game::Rarity r) {
    switch (r) {
        case game::Rarity::Common: return rgba(150, 156, 164, 190);
        case game::Rarity::Uncommon: return rgba(70, 160, 60, 200);
        case game::Rarity::Rare: return rgba(50, 120, 210, 200);
        case game::Rarity::Epic: return rgba(150, 70, 200, 200);
        case game::Rarity::Legendary: return rgba(214, 138, 40, 200);
        case game::Rarity::Count: break;
    }
    return rgba(0, 0, 0, 50);
}

// Imagen de la ranura de cada objeto (Recursos/HUD, Recursos/Weapons y las siluetas de
// Recursos/HUD/Armas sobre el fondo de su rareza), o nullptr.
const assets::ui::Rect* slotArt(game::ItemType type, game::Rarity rarity) {
    namespace a = assets::ui;
    // Por rareza (comun ... legendaria); en las rarezas en que no existe el arma, la mas
    // cercana que si (no deberia ocurrir: game::weaponConfig hace lo mismo).
    using Row = const a::Rect* [5];
    static const Row kAR = {&a::kSlotAR0, &a::kSlotAR1, &a::kSlotAR2, &a::kSlotScar3, &a::kSlotScar};
    static const Row kShotgun = {&a::kSlotShotgun1, &a::kSlotShotgun1, &a::kSlotShotgun2, &a::kSlotShotgun3, &a::kSlotShotgun4};
    static const Row kSmg = {&a::kSlotSmg0, &a::kSlotSmg1, &a::kSlotSmg2, &a::kSlotSmg3, &a::kSlotSmg4};
    static const Row kSniper = {&a::kSlotSniper2, &a::kSlotSniper2, &a::kSlotSniper2, &a::kSlotSniper3, &a::kSlotSniper4};
    static const Row kBurst = {&a::kSlotBurst0, &a::kSlotBurst1, &a::kSlotBurst2, &a::kSlotBurst3, &a::kSlotBurst4};
    static const Row kScoped = {&a::kSlotScoped2, &a::kSlotScoped2, &a::kSlotScoped2, &a::kSlotScoped3, &a::kSlotScoped3};
    static const Row kSmg2 = {&a::kSlotSmg20, &a::kSlotSmg21, &a::kSlotSmg22, &a::kSlotSmg22, &a::kSlotSmg22};
    static const Row kTactical = {&a::kSlotTactical0, &a::kSlotTactical1, &a::kSlotTactical2, &a::kSlotTactical3,
                                  &a::kSlotTactical4};
    static const Row kPistol = {&a::kSlotPistol0, &a::kSlotPistol1, &a::kSlotPistol2, &a::kSlotPistol2, &a::kSlotPistol2};
    static const Row kRevolver = {&a::kSlotRevolver0, &a::kSlotRevolver1, &a::kSlotRevolver2, &a::kSlotRevolver2,
                                  &a::kSlotRevolver2};
    static const Row kSemi = {&a::kSlotSemiSniper1, &a::kSlotSemiSniper1, &a::kSlotSemiSniper2, &a::kSlotSemiSniper2,
                              &a::kSlotSemiSniper2};
    static const Row kRocket = {&a::kSlotRocket2, &a::kSlotRocket2, &a::kSlotRocket2, &a::kSlotRocket3, &a::kSlotRocket4};
    static const Row kLauncher = {&a::kSlotLauncher2, &a::kSlotLauncher2, &a::kSlotLauncher2, &a::kSlotLauncher3,
                                  &a::kSlotLauncher4};
    const int r = static_cast<int>(rarity) < 5 ? static_cast<int>(rarity) : 4;
    switch (type) {
        case game::ItemType::AssaultRifle: return kAR[r];
        case game::ItemType::Shotgun: return kShotgun[r];
        case game::ItemType::Smg: return kSmg[r];
        case game::ItemType::Sniper: return kSniper[r];
        case game::ItemType::BurstRifle: return kBurst[r];
        case game::ItemType::ScopedRifle: return kScoped[r];
        case game::ItemType::SubmachineGun: return kSmg2[r];
        case game::ItemType::TacticalShotgun: return kTactical[r];
        case game::ItemType::Pistol: return kPistol[r];
        case game::ItemType::Revolver: return kRevolver[r];
        case game::ItemType::SemiAutoSniper: return kSemi[r];
        case game::ItemType::RocketLauncher: return kRocket[r];
        case game::ItemType::GrenadeLauncher: return kLauncher[r];
        case game::ItemType::Grenade: return &a::kSlotGrenade0;
        case game::ItemType::SpikeTrap: return &a::kSlotTrap1;
        case game::ItemType::Bandage: return &a::kSlotVendas;
        case game::ItemType::Medkit: return &a::kSlotBotiquin;
        case game::ItemType::SmallShield: return &a::kSlotMinipocion;
        case game::ItemType::ShieldPotion: return &a::kSlotPocionEscudo;
        case game::ItemType::None:
        case game::ItemType::Count: break;
    }
    return nullptr;
}

// Texto de un objeto segun su rareza (algo mas claro que el fondo de la ranura).
u32 rarityTextColor(game::Rarity r) {
    switch (r) {
        case game::Rarity::Common: return rgba(220, 220, 220);
        case game::Rarity::Uncommon: return rgba(130, 230, 100);
        case game::Rarity::Rare: return rgba(110, 190, 255);
        case game::Rarity::Epic: return rgba(210, 140, 255);
        case game::Rarity::Legendary: return rgba(255, 190, 70);
        case game::Rarity::Count: break;
    }
    return rgba(255, 255, 255);
}

// Caja de ranura al estilo de la referencia: fondo translucido y borde claro.
void slotBox(float x, float y, float size, u32 fill, u32 border) {
    gfx::fillRect(x, y, size, size, fill);
    ui::frame(x, y, size, size, 1, border);
}

}  // namespace

// --- HUD: parte dibujada con GU -------------------------------------------------------------

const ui::Hint* App::buildHints(int* count) const {
    // Ayuda bajo el rotulo de construccion (solo cuando la pieza se puede colocar).
    if (m_buildPiece == kTrapPiece) {
        *count = 2;
        return kTrapHints;
    }
    *count = 3;
    return kBuildHints;
}

// Mapa a pantalla completa (SELECT): todo el mapa a 240x240, con la tormenta, el
// deposito, los airdrops, el autobus y el jugador.
constexpr float kFullMapSize = 240.0f, kFullMapX = 120.0f, kFullMapY = 20.0f;

void App::renderFullMap() const {
    gfx::fillRect(0, 0, gfx::kScreenWidth, gfx::kScreenHeight, rgba(0, 0, 0, 170));
    gfx::fillRect(kFullMapX - 3, kFullMapY - 3, kFullMapSize + 6, kFullMapSize + 6, rgba(20, 30, 50, 230));
    if (m_minimap.pixels) {
        gfx::drawTextureRect(m_minimap, 0, 0, kMapTexels, kMapTexels, kFullMapX, kFullMapY, kFullMapSize, kFullMapSize);
    }
    const float scale = kFullMapSize / (2.0f * world::kMapHalfSize);
    auto toMap = [&](float x, float z, float* sx, float* sy) {
        *sx = kFullMapX + (x + world::kMapHalfSize) * scale;
        *sy = kFullMapY + (z + world::kMapHalfSize) * scale;
        return *sx >= kFullMapX && *sx < kFullMapX + kFullMapSize - 1 && *sy >= kFullMapY &&
               *sy < kFullMapY + kFullMapSize - 1;
    };
    auto circle = [&](const Vec3& center, float radius, u32 color) {
        if (radius < 0.5f) return;
        for (int i = 0; i < 160; ++i) {
            const float a = 2.0f * math3d::kPi * i / 160;
            float sx, sy;
            if (toMap(center.x + cosf(a) * radius, center.z + sinf(a) * radius, &sx, &sy)) gfx::fillRect(sx, sy, 2, 2, color);
        }
    };
    const game::StormZone& storm = m_match.storm();
    if (storm.closes()) {
        circle(storm.center(), storm.radius(), rgba(190, 110, 255));
        if (storm.nextRadius() < storm.radius() - 0.5f) circle(storm.nextCenter(), storm.nextRadius(), kWhite);
    }
    float sx, sy;
    if (m_atlas.ready() && toMap(0.0f, -11.0f, &sx, &sy)) {
        const atlas::Rect& r = label(atlas::kMapaMinimapa, atlas::kMapaMinimapaEn);
        m_atlas.draw(r, floorf(sx - r.w * 0.5f), floorf(sy - r.h * 0.5f));
    }
    for (const game::Container& c : m_match.containers()) {
        if (c.kind != game::ContainerKind::Airdrop || c.opened || !toMap(c.position.x, c.position.z, &sx, &sy)) continue;
        gfx::fillRect(sx - 3, sy - 3, 7, 7, rgba(20, 20, 30));
        gfx::fillRect(sx - 2, sy - 2, 5, 5, rgba(90, 170, 255));
    }
    if (m_match.airState(0) == game::AirState::Bus && toMap(m_match.busPosition().x, m_match.busPosition().z, &sx, &sy)) {
        gfx::fillRect(sx - 3, sy - 4, 7, 8, config::kColorGold);
    }
    const game::CharacterState followed = renderState(spectateTarget());
    if (toMap(followed.position.x, followed.position.z, &sx, &sy)) {
        const float fx = -sinf(followed.yaw), fy = -cosf(followed.yaw);
        const float rx = -fy, ry = fx;
        const u32 c = m_match.alive(0) ? config::kColorGold : kErrorRed;
        const gfx::LineVertex arrow[] = {
            {c, sx + fx * 8, sy + fy * 8, 0}, {c, sx - fx * 5 + rx * 5, sy - fy * 5 + ry * 5, 0},
            {c, sx - fx * 5 + rx * 5, sy - fy * 5 + ry * 5, 0}, {c, sx - fx * 2, sy - fy * 2, 0},
            {c, sx - fx * 2, sy - fy * 2, 0}, {c, sx - fx * 5 - rx * 5, sy - fy * 5 - ry * 5, 0},
            {c, sx - fx * 5 - rx * 5, sy - fy * 5 - ry * 5, 0}, {c, sx + fx * 8, sy + fy * 8, 0},
        };
        gfx::drawLines2D(arrow, 8);
        gfx::fillRect(sx - 1, sy - 1, 3, 3, c);
    }
}

void App::renderGameHud() const {
    if (m_mapOpen) {
        renderFullMap();
        return;
    }
    // Dentro de la tormenta: la pantalla se tine de morado y late con cada golpe.
    if (m_match.alive(0) && m_match.inStorm(0)) {
        const float pulse = 0.5f + 0.5f * sinf(m_blinkTime * 6.0f);
        gfx::fillRect(0, 0, gfx::kScreenWidth, gfx::kScreenHeight, rgba(120, 40, 180, static_cast<u32>(55.0f + 30.0f * pulse)));
    }
    // Destello rojo al recibir un impacto.
    if (sniperScoped()) renderScope();
    const float hurt = m_match.sincePlayerDamaged();
    if (hurt < 0.35f) {
        gfx::fillRect(0, 0, gfx::kScreenWidth, gfx::kScreenHeight,
                      rgba(200, 20, 10, static_cast<u32>(110.0f * (1.0f - hurt / 0.35f))));
    }
    if (m_buildMode) {
        // Rotulo CONSTRUCCION sobre una franja azul: el modo se ve siempre.
        gfx::fillRect(150, 18, 180, 22, kBuildBlue);
        if (m_atlas.ready()) m_atlas.drawCentered(label(atlas::kConstruccion, atlas::kConstruccionEn), 240, 20);
        if (m_buildPiece == kTrapPiece ? m_trapPlan.valid : m_wallPlan.valid) {
            int n = 0;
            const ui::Hint* hints = buildHints(&n);
            ui::drawHintIcons(m_atlas, (gfx::kScreenWidth - ui::hintsWidth(hints, n)) / 2, 55, hints, n);
        }
    }
    if (m_match.alive(0) && m_match.airState(0) != game::AirState::Ground) {
        renderAirHud();
    } else {
        renderCrosshair();
        int n = 0;
        if (const ui::Hint* hint = m_swapOpen ? nullptr : interactionHint(&n)) {
            gfx::fillRect(0, kPromptY - 4, gfx::kScreenWidth, 16, rgba(0, 0, 0, 90));
            ui::drawHintIcons(m_atlas, (gfx::kScreenWidth - ui::hintsWidth(hint, n)) / 2, kPromptY, hint, n);
        }
    }
    if (m_match.cheats().wallhack) renderWallhack();
    renderMinimap();
    renderVitals();
    renderSlots();
    if (m_swapOpen) renderSwapChooser();
    renderDamageNumbers();
    if (m_inventoryOpen) renderInventoryScreen();
}

// --- Inventario (L + R a la vez, v0.13) ---------------------------------------------------------
// Las 5 ranuras en grande: izquierda / derecha eligen, X coge una y la vuelve a dejar en otra
// (se intercambian), CUADRADO la suelta al suelo, TRIANGULO la lleva a la mano y CIRCULO (o
// L + R otra vez) cierra. Debajo, lo elegido y la municion y los materiales que se llevan.
constexpr float kInvX = 56.0f, kInvY = 38.0f, kInvW = 368.0f, kInvH = 178.0f;
constexpr float kInvSlot = 44.0f, kInvStep = 54.0f, kInvSlotY = 64.0f;
const ui::Hint kInventoryHints[] = {{B::Left, T::None},          {B::Right, T::HintSelect}, {B::Cross, T::HintMove},
                                    {B::Square, T::HintDrop},    {B::Triangle, T::HintHand}, {B::Circle, T::HintClose}};

float inventorySlotX(int i) { return 240.0f - (game::Inventory::kSlots * kInvStep - (kInvStep - kInvSlot)) * 0.5f + i * kInvStep; }

void App::updateInventoryScreen(const input::Input& in) {
    const int slots = game::Inventory::kSlots;
    const int step = (in.pressed(PSP_CTRL_RIGHT) ? 1 : 0) - (in.pressed(PSP_CTRL_LEFT) ? 1 : 0);
    if (step != 0) {
        m_invCursor = (m_invCursor + slots + step) % slots;
        playSfx(audio::Sfx::MenuMove, 0.5f);
    }
    const game::Inventory& inv = m_match.inventory();
    if (in.pressed(PSP_CTRL_CROSS)) {
        if (m_invHeld < 0) {
            if (inv.slot(m_invCursor).type != game::ItemType::None) {
                m_invHeld = m_invCursor;
                playSfx(audio::Sfx::MenuAccept, 0.6f);
            }
        } else {
            if (m_invHeld != m_invCursor) m_match.moveSlot(m_invHeld, m_invCursor);
            m_invHeld = -1;
            playSfx(audio::Sfx::Pickup, 0.6f);
        }
    } else if (in.pressed(PSP_CTRL_SQUARE)) {
        if (m_match.dropInventorySlot(m_invCursor)) playSfx(audio::Sfx::Pickup, 0.6f);
        m_invHeld = -1;
    } else if (in.pressed(PSP_CTRL_TRIANGLE)) {
        if (inv.slot(m_invCursor).type != game::ItemType::None && m_match.selectSlot(m_invCursor)) {
            playSfx(audio::Sfx::MenuAccept, 0.6f);
        }
    } else if (in.pressed(PSP_CTRL_CIRCLE)) {
        if (m_invHeld >= 0) {
            m_invHeld = -1;  // primero suelta lo cogido
        } else {
            m_inventoryOpen = false;
        }
        playSfx(audio::Sfx::MenuMove, 0.5f);
    }
}

void App::renderInventoryScreen() const {
    gfx::fillRect(kInvX, kInvY, kInvW, kInvH, rgba(8, 18, 48, 235));
    ui::frame(kInvX, kInvY, kInvW, kInvH, 2, rgba(90, 160, 255));
    gfx::fillRect(kInvX, kInvY, kInvW, 18, kFortniteBlue);
    const game::Inventory& inv = m_match.inventory();
    for (int i = 0; i < game::Inventory::kSlots; ++i) {
        const game::InventorySlot& slot = inv.slot(i);
        const float x = inventorySlotX(i);
        const float y = kInvSlotY - (i == m_invHeld ? 6.0f : 0.0f);
        const atlas::Rect* art = slotArt(slot.type, slot.rarity);
        slotBox(x, y, kInvSlot, slot.type == game::ItemType::None ? rgba(0, 0, 0, 90) : rarityColor(slot.rarity),
                rgba(255, 255, 255, 150));
        if (art && m_atlas.ready()) m_atlas.draw(*art, x + 6, y + 6);
        // En la mano: una marca debajo.
        if (!inv.pickaxe() && inv.selected() == i) gfx::fillRect(x + kInvSlot * 0.5f - 6, kInvSlotY + kInvSlot + 3, 12, 3, kWhite);
        if (i == m_invCursor) {
            const u32 alpha = static_cast<u32>(200.0f + 55.0f * sinf(m_blinkTime * 8.0f));
            ui::frame(x - 3, y - 3, kInvSlot + 6, kInvSlot + 6, 2, rgba(250, 216, 40, alpha));
        }
        if (i == m_invHeld) ui::frame(x - 1, y - 1, kInvSlot + 2, kInvSlot + 2, 1, kWhite);
    }
    // Materiales con sus iconos.
    if (m_atlas.ready()) {
        const atlas::Rect* icons[3] = {&atlas::kMatMadera, &atlas::kMatLadrillo, &atlas::kMatMetal};
        for (int r = 0; r < 3; ++r) m_atlas.draw(*icons[r], kInvX + 238 + r * 42, kInvY + 124);
    }
    gfx::fillRect(kInvX + 8, kInvY + 120, kInvW - 16, 1, rgba(90, 160, 255, 160));
    ui::drawHintIcons(m_atlas, static_cast<int>(240 - ui::hintsWidth(kInventoryHints, 6) / 2), static_cast<int>(kInvY + kInvH) - 14,
                      kInventoryHints, 6);
}

void App::overlayInventoryScreen() const {
    printCenteredAt(240, static_cast<int>(kInvY) + 5, kWhite, tr(T::InventoryTitle));
    const game::Inventory& inv = m_match.inventory();
    const game::InventorySlot& slot = inv.slot(m_invCursor);
    char line[64];
    const int infoY = static_cast<int>(kInvSlotY + kInvSlot) + 10;
    if (slot.type == game::ItemType::None) {
        printCenteredAt(240, infoY, kDimGrey, tr(T::InventoryEmpty));
    } else {
        const bool weapon = game::isWeapon(slot.type);
        const char* name = weapon ? slot.weapon.config().name : game::itemName(slot.type);
        snprintf(line, sizeof(line), "%s - %s", name, game::rarityName(slot.rarity));
        printCenteredAt(240, infoY, rarityTextColor(slot.rarity), line);
        if (weapon) {
            const game::AmmoType ammo = static_cast<game::AmmoType>(slot.weapon.config().ammoType);
            snprintf(line, sizeof(line), tr(T::InventoryWeaponInfo), slot.weapon.ammo(), slot.weapon.config().magazineSize,
                     inv.ammo(ammo), game::ammoName(ammo));
        } else {
            snprintf(line, sizeof(line), tr(T::InventoryCount), slot.count, game::itemDef(slot.type).maxStack);
        }
        printCenteredAt(240, infoY + 11, kLightGrey, line);
    }
    if (m_invHeld >= 0) printCenteredAt(240, infoY + 22, kYellow, tr(T::InventoryMoving));
    // Municion y trampas (izquierda, en dos columnas) y materiales (derecha).
    const int x = static_cast<int>(kInvX) + 12, y = static_cast<int>(kInvY) + 124;
    for (int a = 0; a < static_cast<int>(game::AmmoType::Count); ++a) {
        gfx::text::print(x + (a % 2) * 110, y + (a / 2) * 11, kLightGrey, "%s %d", game::ammoName(static_cast<game::AmmoType>(a)),
                         inv.ammo(static_cast<game::AmmoType>(a)));
    }
    snprintf(line, sizeof(line), tr(T::InventoryTraps), m_match.trapStock());
    gfx::text::print(x + 110, y + 22, kLightGrey, "%s", line);
    for (int r = 0; r < 3; ++r) {
        gfx::text::printShadow(static_cast<int>(kInvX) + 258 + r * 42, y + 4, kWhite, "%d",
                               m_match.materials().get(static_cast<world::Resource>(r)));
    }
    ui::printHints(240 - ui::hintsWidth(kInventoryHints, 6) / 2, static_cast<int>(kInvY + kInvH) - 14, kLightGrey,
                   kInventoryHints, 6);
}

void App::renderWallhack() const {
    // Truco "wallhack": una caja roja por rival vivo, a traves de paredes, con su vida.
    for (int id = 1; id < m_match.entityCount(); ++id) {
        if (!m_match.alive(id) || m_match.airState(id) == game::AirState::Bus) continue;
        const game::CharacterState s = renderState(id);
        float x0, y0, x1, y1;
        if (!projectToScreen(m_viewProjection, s.position, &x0, &y0) ||
            !projectToScreen(m_viewProjection, s.position + Vec3{0.0f, 1.9f, 0.0f}, &x1, &y1)) {
            continue;
        }
        const float h = fmaxf(4.0f, y0 - y1), w = fmaxf(3.0f, h * 0.45f);
        const float left = x1 - w * 0.5f;
        if (left > gfx::kScreenWidth || left + w < 0.0f || y1 > gfx::kScreenHeight || y0 < 0.0f) continue;
        ui::frame(left, y1, w, h, 1, rgba(255, 60, 50, 220));
        const float hp = math3d::clampf(s.health / 100.0f, 0.0f, 1.0f), sh = math3d::clampf(s.shield / 100.0f, 0.0f, 1.0f);
        gfx::fillRect(left, y1 - 4, w, 2, rgba(0, 0, 0, 160));
        gfx::fillRect(left, y1 - 4, w * hp, 2, rgba(80, 220, 80));
        if (sh > 0.0f) gfx::fillRect(left, y1 - 6, w * sh, 2, kShieldBlue);
    }
}

// Selector de que soltar: la ranura elegida parpadea en amarillo sobre la fila de objetos
// y encima va lo que se recoge y los botones.
const ui::Hint kSwapHints[] = {{B::Left, T::None}, {B::Right, T::HintSelect}, {B::Interact, T::HintSwap},
                               {B::Build, T::HintCancel}};
constexpr float kSwapPanelY = 154.0f;

void App::renderSwapChooser() const {
    const float x = kWeaponSlotX + (m_swapCursor + 1) * kWeaponSlotStep;
    const u32 alpha = static_cast<u32>(200.0f + 55.0f * sinf(m_blinkTime * 8.0f));
    ui::frame(x - 3, kWeaponRowY - 6, kWeaponSlotSize + 6, kWeaponSlotSize + 6, 3, rgba(250, 216, 40, alpha));
    gfx::fillRect(x + kWeaponSlotSize * 0.5f - 4, kWeaponRowY - 13, 8, 5, kYellow);
    gfx::fillRect(110, kSwapPanelY, 260, 34, rgba(8, 18, 48, 210));
    ui::frame(110, kSwapPanelY, 260, 34, 1, kYellow);
    ui::drawHintIcons(m_atlas, (gfx::kScreenWidth - ui::hintsWidth(kSwapHints, 4)) / 2, static_cast<int>(kSwapPanelY) + 22,
                      kSwapHints, 4);
}

const ui::Hint* App::interactionHint(int* count) const {
    // CUADRADO: lo que se abriria o recogeria ahora (texto en m_promptText).
    static ui::Hint hint{ui::Button::Interact, T::None};
    static char text[64];
    // (Curandose no se puede recoger ni abrir nada: no se ofrece.)
    if (!m_match.alive(0) || m_buildMode || m_match.airState(0) != game::AirState::Ground || m_match.useProgress() >= 0.0f) {
        return nullptr;
    }
    const game::Interactable it = m_match.playerInteractable();
    if (it.kind == game::Interactable::Kind::None) return nullptr;
    if (it.kind == game::Interactable::Kind::Container) {
        const game::ContainerKind kind = m_match.containers()[it.index].kind;
        snprintf(text, sizeof(text), "%s", tr(kind == game::ContainerKind::Chest     ? T::OpenChest
                                              : kind == game::ContainerKind::AmmoBox ? T::OpenAmmoBox
                                              : kind == game::ContainerKind::Llama   ? T::OpenLlama
                                                                                     : T::OpenAirdrop));
    } else {
        const game::Pickup& p = m_match.pickups()[it.index];
        const bool swap = m_match.pickupRoom(it.index) <= 0;
        const char* name = game::isWeapon(p.item) ? game::weaponConfig(p.item, p.rarity).name : game::itemName(p.item);
        if (swap) {
            snprintf(text, sizeof(text), tr(T::SwapFor), name, game::rarityName(p.rarity));
        } else if (p.count > 1) {
            snprintf(text, sizeof(text), tr(T::PickUpCount), name, p.count);
        } else {
            snprintf(text, sizeof(text), tr(T::PickUpRarity), name, game::rarityName(p.rarity));
        }
    }
    hint.raw = text;
    *count = 1;
    return &hint;
}

void App::renderAirHud() const {
    // Autobus: saltar con X. Caida libre: abrir el ala delta con X. Altura sobre el suelo.
    const game::AirState air = m_match.airState(0);
    gfx::fillRect(140, 150, 200, air == game::AirState::Glide ? 14 : 30, rgba(0, 0, 0, 110));
    if (air == game::AirState::Bus) ui::drawHintIcons(m_atlas, (gfx::kScreenWidth - ui::hintsWidth(kBusHint, 1)) / 2, 154, kBusHint, 1);
    if (air == game::AirState::Freefall) {
        ui::drawHintIcons(m_atlas, (gfx::kScreenWidth - ui::hintsWidth(kGliderHint, 1)) / 2, 154, kGliderHint, 1);
    }
}

void App::renderScope() const {
    // Recursos/mirilla.png a pantalla completa (aro y reticula); un punto rojo en el centro.
    if (m_scope.pixels) {
        // El centro del aro esta en la fila 465 de 941 del original: 1.6 px mas arriba.
        gfx::drawTextureRect(m_scope, 0.0f, 0.0f, 240.0f, 136.0f, 0.0f, 1.6f, gfx::kScreenWidth, gfx::kScreenHeight);
    } else {
        gfx::fillRect(239, 100, 2, 72, rgba(0, 0, 0));
        gfx::fillRect(200, 135, 80, 2, rgba(0, 0, 0));
    }
    gfx::fillRect(239, 135, 2, 2, rgba(230, 40, 30));
}

void App::renderCrosshair() const {
    if (!m_match.alive(0) || m_freeCameraActive) return;
    if (m_buildMode) {
        // Reticula de construccion: cuatro esquinas.
        const bool valid = m_buildPiece == kTrapPiece ? m_trapPlan.valid : m_wallPlan.valid;
        const u32 c = valid ? kBuildReticle : kErrorRed;
        for (int sx : {-1, 1}) {
            for (int sy : {-1, 1}) {
                const float cx = 240.0f + sx * 10.0f, cy = 136.0f + sy * 10.0f;
                gfx::fillRect(sx < 0 ? cx : cx - 5.0f, cy, 6, 2, c);
                gfx::fillRect(cx, sy < 0 ? cy : cy - 5.0f, 2, 6, c);
            }
        }
        return;
    }
    if (sniperScoped()) return;  // la mirilla lleva su propia reticula
    // Mira segun lo que se lleva en la mano (roja si la ayuda de punteria tiene un
    // objetivo): escopeta, cuadrada; curativos, redonda; el resto, la cruz.
    const u32 c = m_match.playerAssistTarget() >= 0 || m_lockTarget > 0 ? kCrosshairLocked : kCrosshair;
    const game::Inventory& inv = m_match.inventory();
    const game::ItemType held = inv.pickaxe() ? game::ItemType::None : inv.slot(inv.selected()).type;
    // La mira se abre con la dispersion real del siguiente disparo (la misma que usa la
    // partida): moverse, saltar y el retroceso la abren; apuntar y agacharse la cierran.
    // Radio en pixeles del cono: tan(dispersion) * distancia focal (FOV vertical de 60).
    const float focal = 0.5f * gfx::kScreenHeight / tanf(math3d::toRadians(0.5f * config::kFovYDegrees));
    const float spreadPx = focal * tanf(math3d::toRadians(fminf(m_match.playerSpreadNow(), 30.0f)));
    if (held == game::ItemType::GrenadeLauncher) {
        // Lanzagranadas: un circulo y, debajo, marcas de la caida de la granada a 5, 10 y
        // 15 m (lo que baja la granada a esa distancia: se pone la marca sobre el objetivo).
        // Partidas en dos, con un hueco en medio, para que no las tape el personaje.
        const float r = 9.0f + floorf(fminf(spreadPx, 30.0f));
        for (int i = 0; i < 32; ++i) {
            const float a = 2.0f * math3d::kPi * i / 32;
            gfx::fillRect(floorf(239.0f + cosf(a) * r), floorf(135.0f + sinf(a) * r), 2, 2, c);
        }
        gfx::fillRect(239, 135, 2, 2, c);
        const float speed = game::weaponConfig(held, game::Rarity::Rare).projectileSpeed;
        for (int k = 1; k <= 3; ++k) {
            const float d = 5.0f * k, t = d / speed;
            const float drop = 0.5f * game::kGrenadeGravity * t * t;
            const float y = 136.0f + floorf(focal * drop / d);
            const float w = 18.0f - 3.0f * k;
            gfx::fillRect(234.0f - w, y, w, 2, c);
            gfx::fillRect(246.0f, y, w, 2, c);
        }
    } else if (game::weaponClass(held) == game::WeaponClass::Shotgun) {
        // Escopetas (corredera y tactica): la misma mira cuadrada.
        const float h = fmaxf(8.0f, floorf(spreadPx));
        gfx::fillRect(240 - h, 136 - h, 2 * h, 2, c);
        gfx::fillRect(240 - h, 134 + h, 2 * h, 2, c);
        gfx::fillRect(240 - h, 136 - h, 2, 2 * h - 2, c);
        gfx::fillRect(238 + h, 136 - h, 2, 2 * h - 2, c);
        gfx::fillRect(239, 135, 2, 2, c);
    } else if (held != game::ItemType::None && !game::isWeapon(held)) {
        for (int i = 0; i < 28; ++i) {
            const float a = 2.0f * math3d::kPi * i / 28;
            gfx::fillRect(239.0f + cosf(a) * 9.0f, 135.0f + sinf(a) * 9.0f, 2, 2, c);
        }
        gfx::fillRect(239, 135, 2, 2, c);
    } else {
        // Cruz: cuatro brazos iguales (6 px) a la misma distancia del punto central (2x2,
        // centrado en 240,136). Antes el izquierdo y el de arriba eran mas largos (pisaban el
        // centro) que el derecho y el de abajo.
        const float gap = 3.0f + floorf(fminf(spreadPx, 60.0f));
        constexpr float kArm = 6.0f;
        gfx::fillRect(239 - gap - kArm, 135, kArm, 2, c);
        gfx::fillRect(241 + gap, 135, kArm, 2, c);
        gfx::fillRect(239, 135 - gap - kArm, 2, kArm, c);
        gfx::fillRect(239, 137 + gap, 2, kArm, c);
        gfx::fillRect(239, 135, 2, 2, c);
    }
    if (m_match.sincePlayerHit() < 0.15f) {
        for (int k = 0; k < 4; ++k) {
            gfx::fillRect(233 - k, 129 - k, 2, 2, kWhite);
            gfx::fillRect(245 + k, 129 - k, 2, 2, kWhite);
            gfx::fillRect(233 - k, 141 + k, 2, 2, kWhite);
            gfx::fillRect(245 + k, 141 + k, 2, 2, kWhite);
        }
    }
}

void App::renderMinimap() const {
    gfx::fillRect(kMapX - 2, kMapY - 2, kMapSize + 4, kMapSize + 4, rgba(0, 0, 0, 150));
    const game::CharacterState followed = renderState(spectateTarget());
    // Ventana centrada en el jugador, sin salirse del mapa.
    auto clampWindow = [](float c) {
        const float half = kMapWindow * 0.5f;
        return c < half ? half : (c > kMapTexels - half ? kMapTexels - half : c);
    };
    const float u0 = clampWindow((followed.position.x + world::kMapHalfSize) / kMetersPerTexel) - kMapWindow * 0.5f;
    const float v0 = clampWindow((followed.position.z + world::kMapHalfSize) / kMetersPerTexel) - kMapWindow * 0.5f;
    if (m_minimap.pixels) {
        gfx::drawTextureRect(m_minimap, u0, v0, kMapWindow, kMapWindow, kMapX, kMapY, kMapSize, kMapSize);
    }
    const float pxPerMeter = kMapSize / (kMapWindow * kMetersPerTexel);
    auto toMap = [&](float x, float z, float* sx, float* sy) {
        *sx = kMapX + ((x + world::kMapHalfSize) / kMetersPerTexel - u0) * (kMapSize / kMapWindow);
        *sy = kMapY + ((z + world::kMapHalfSize) / kMetersPerTexel - v0) * (kMapSize / kMapWindow);
        return *sx >= kMapX && *sx < kMapX + kMapSize - 1 && *sy >= kMapY && *sy < kMapY + kMapSize - 1;
    };

    // Tormenta: borde actual en morado y, si se cierra, el circulo siguiente en blanco.
    auto circle = [&](const Vec3& center, float radius, u32 color) {
        if (radius < 0.5f) return;
        const int points = radius * pxPerMeter > 40.0f ? 96 : 48;
        for (int i = 0; i < points; ++i) {
            const float a = 2.0f * math3d::kPi * i / points;
            float sx, sy;
            if (toMap(center.x + cosf(a) * radius, center.z + sinf(a) * radius, &sx, &sy)) {
                gfx::fillRect(sx, sy, 2, 2, color);
            }
        }
    };
    const game::StormZone& storm = m_match.storm();
    if (storm.closes()) {
        circle(storm.center(), storm.radius(), rgba(190, 110, 255));
        if (storm.nextRadius() < storm.radius() - 0.5f) circle(storm.nextCenter(), storm.nextRadius(), kWhite);
    } else {
        circle(storm.center(), storm.radius(), kWhite);
    }

    // Nombre del deposito sobre las tres naves (recortado al minimapa).
    if (m_atlas.ready()) {
        const float sx = kMapX + ((0.0f + world::kMapHalfSize) / kMetersPerTexel - u0) * (kMapSize / kMapWindow);
        const float sy = kMapY + ((-11.0f + world::kMapHalfSize) / kMetersPerTexel - v0) * (kMapSize / kMapWindow);
        const atlas::Rect& r = label(atlas::kMapaMinimapa, atlas::kMapaMinimapaEn);
        m_atlas.drawClipped(r, floorf(sx - r.w * 0.5f), floorf(sy - r.h * 0.5f), kMapX, kMapY, kMapX + kMapSize,
                            kMapY + kMapSize);
    }

    // Flecha del jugador (o de a quien se sigue tras caer), con el norte arriba.
    float px, py;
    if (toMap(followed.position.x, followed.position.z, &px, &py)) {
        const float fx = -sinf(followed.yaw), fy = -cosf(followed.yaw);  // adelante en el mapa
        const float rx = -fy, ry = fx;
        const u32 c = m_match.alive(0) ? kWhite : kErrorRed;
        const gfx::LineVertex arrow[] = {
            {c, px + fx * 5, py + fy * 5, 0}, {c, px - fx * 3 + rx * 3, py - fy * 3 + ry * 3, 0},
            {c, px - fx * 3 + rx * 3, py - fy * 3 + ry * 3, 0}, {c, px - fx * 1.5f, py - fy * 1.5f, 0},
            {c, px - fx * 1.5f, py - fy * 1.5f, 0}, {c, px - fx * 3 - rx * 3, py - fy * 3 - ry * 3, 0},
            {c, px - fx * 3 - rx * 3, py - fy * 3 - ry * 3, 0}, {c, px + fx * 5, py + fy * 5, 0},
        };
        gfx::drawLines2D(arrow, 8);
        gfx::fillRect(px - 1, py - 1, 2, 2, c);
    }

    // Truco "radar de botin": cofres sin abrir (amarillo) y la llama (morado).
    if (m_match.cheats().lootRadar) {
        for (const game::Container& c : m_match.containers()) {
            if (c.opened || (c.kind != game::ContainerKind::Chest && c.kind != game::ContainerKind::Llama)) continue;
            float sx, sy;
            if (!toMap(c.position.x, c.position.z, &sx, &sy)) continue;
            gfx::fillRect(sx - 1, sy - 1, 3, 3, c.kind == game::ContainerKind::Llama ? rgba(200, 90, 255) : kYellow);
        }
    }
    // Airdrops sin abrir (cuadro azul) y el autobus mientras se va dentro (amarillo).
    for (const game::Container& c : m_match.containers()) {
        if (c.kind != game::ContainerKind::Airdrop || c.opened) continue;
        float sx, sy;
        if (toMap(c.position.x, c.position.z, &sx, &sy)) {
            gfx::fillRect(sx - 2, sy - 2, 5, 5, rgba(20, 20, 30));
            gfx::fillRect(sx - 1, sy - 1, 3, 3, rgba(90, 170, 255));
        }
    }
    if (m_match.airState(0) == game::AirState::Bus) {
        float sx, sy;
        const Vec3& bus = m_match.busPosition();
        if (toMap(bus.x, bus.z, &sx, &sy)) gfx::fillRect(sx - 2, sy - 3, 5, 6, config::kColorGold);
    }

    // Fila de iconos: zona, vivos y bajas.
    if (m_atlas.ready()) {
        m_atlas.draw(atlas::kIconZona, 360, 86);
        m_atlas.draw(atlas::kIconVivos, 412, 86);
        m_atlas.draw(atlas::kIconBajas, 448, 86);
    }
    // Materiales: madera, piedra y metal. Resaltado, el de construir (CUADRADO lo cambia en
    // construccion; si no queda bastante se usa otro, y se resalta ese).
    const int material = static_cast<int>(m_buildMode && m_wallPlan.error != game::PlacementError::NoMaterials
                                              ? m_wallPlan.material
                                              : m_match.buildMaterial());
    gfx::fillRect(398, 106, 78, 30, rgba(0, 0, 0, 90));
    gfx::fillRect(399 + material * 26, 107, 25, 28, rgba(130, 150, 210, m_buildMode ? 170 : 110));
    if (m_buildMode) ui::frame(399 + material * 26, 107, 25, 28, 1, kWhite);
    if (m_atlas.ready()) {
        const atlas::Rect* icons[3] = {&atlas::kMatMadera, &atlas::kMatLadrillo, &atlas::kMatMetal};
        for (int i = 0; i < 3; ++i) {
            m_atlas.draw(*icons[i], 402 + i * 26, 107, 1.0f, i == material ? kWhite : rgba(255, 255, 255, 170));
        }
    }
}

void App::renderVitals() const {
    const game::CharacterState& p = m_match.body(0).state();
    // Municion "30 | 120" en grande (como la referencia de gameplay), solo con un arma.
    if (const game::Weapon* w = m_match.inventory().activeWeapon()) {
        const u32 ammoColor = w->ammo() == 0 ? kErrorRed : (w->reloading() ? kLightGrey : kWhite);
        float x = 18.0f;
        x += drawBigNumber(x, 210, w->ammo(), 3.0f, ammoColor) + 7.0f;
        gfx::fillRect(x, 208, 2, 19, kLightGrey);
        x += 8.0f;
        if (w->reserve() >= 0) drawBigNumber(x, 210, w->reserve(), 3.0f, kWhite);
        if (w->reloading()) {
            gfx::fillRect(18, 229, 120, 3, rgba(0, 0, 0, 200));
            gfx::fillRect(18, 229, 120 * w->reloadProgress(), 3, config::kColorGold);
        } else if (!w->empty() && w->ammo() == 0) {
            ui::drawHintIcons(m_atlas, 18, 198, kReloadHint, 1);
        }
    }

    // Escudo y vida.
    gfx::fillRect(kBarX, kShieldY, kBarW, 10, rgba(0, 0, 0, 150));
    gfx::fillRect(kBarX, kHealthY, kBarW, 10, rgba(0, 0, 0, 150));
    const float shield = p.maxShield > 0.0f ? p.shield / p.maxShield : 0.0f;
    if (shield > 0.0f) {
        gfx::fillRect(kBarX + 1, kShieldY + 1, (kBarW - 2) * shield, 8, kShieldBlue);
        gfx::fillRect(kBarX + 1, kShieldY + 1, (kBarW - 2) * shield, 2, rgba(255, 255, 255, 60));
    }
    const float ratio = p.maxHealth > 0.0f ? p.health / p.maxHealth : 0.0f;
    if (ratio > 0.0f) {
        gfx::fillRect(kBarX + 1, kHealthY + 1, (kBarW - 2) * ratio, 8, ratio > 0.3f ? kHealthGreen : kErrorRed);
        gfx::fillRect(kBarX + 1, kHealthY + 1, (kBarW - 2) * ratio, 2, rgba(255, 255, 255, 60));
    }
    if (m_atlas.ready()) {
        m_atlas.draw(atlas::kIconEscudo, 16, kShieldY - 1);
        m_atlas.draw(atlas::kIconVida, 16, kHealthY - 1);
    }

    // Consumible en uso: barra en el centro, bajo la mira.
    const float use = m_match.useProgress();
    if (use >= 0.0f) {
        gfx::fillRect(180, 166, 120, 6, rgba(0, 0, 0, 170));
        gfx::fillRect(181, 167, 118 * use, 4, kShieldBlue);
    }
    // Material conseguido con el pico: icono y "+N" a la derecha de la mira.
    if (m_harvestAge < kHarvestPopupSeconds && m_atlas.ready()) {
        const atlas::Rect* icons[] = {&atlas::kMatMadera, &atlas::kMatLadrillo, &atlas::kMatMetal};
        const u32 alpha = static_cast<u32>(255.0f * (1.0f - m_harvestAge / kHarvestPopupSeconds));
        m_atlas.draw(*icons[m_harvestResource % 3], 262, 112 - m_harvestAge * 12.0f, 1.0f, rgba(255, 255, 255, alpha));
    }
}

void App::renderSlots() const {
    // Ranuras de construccion: pared, suelo, rampa y una vacia. La cruceta elige la pieza;
    // por ahora solo la pared se puede colocar.
    // La cuarta es la trampa de pinchos (Recursos/HUD/spiketrap.webp), con las que se llevan.
    const assets::ui::Rect* buildIcons[] = {&atlas::kPared, &atlas::kSuelo, &atlas::kRampa, &atlas::kTrampa};
    for (int i = 0; i < 5; ++i) {
        const float x = kBuildSlotX + i * kBuildSlotStep + (i == 4 ? 4.0f : 0.0f);
        const bool selected = m_buildMode && i == m_buildPiece;
        const bool trap = i == kTrapPiece;
        const u32 fill = i < 3 ? rgba(90, 170, 230, m_buildMode ? 150 : 70)
                         : trap ? rgba(104, 196, 62, m_match.trapStock() > 0 ? (m_buildMode ? 150 : 80) : 40)
                                : rgba(0, 0, 0, 50);
        slotBox(x, kBuildRowY, kBuildSlotSize, fill, rgba(255, 255, 255, m_buildMode ? 200 : 110));
        if (selected) ui::frame(x - 2, kBuildRowY - 2, kBuildSlotSize + 4, kBuildSlotSize + 4, 2, kWhite);
        if (i < 4 && m_atlas.ready() && (!trap || m_match.trapStock() > 0)) {
            const u32 tint = selected ? kWhite : rgba(255, 255, 255, m_buildMode ? 110 : 90);
            m_atlas.draw(*buildIcons[i], x + 1, kBuildRowY + 1, 1.0f, tint);
        }
    }

    // Fila de objetos: el pico (TRIANGULO) y las 5 ranuras del inventario (cruceta), con
    // el fondo del color de la rareza.
    const game::Inventory& inv = m_match.inventory();
    for (int i = 0; i < kWeaponSlots; ++i) {
        const bool selected = !m_buildMode && (i == 0 ? inv.pickaxe() : !inv.pickaxe() && inv.selected() == i - 1);
        const float x = kWeaponSlotX + i * kWeaponSlotStep;
        const float y = kWeaponRowY - (selected ? 3.0f : 0.0f);
        const game::ItemType type = i == 0 ? game::ItemType::None : inv.slot(i - 1).type;
        // Cada objeto tiene su ranura completa dibujada en Recursos (con su fondo de rareza).
        const atlas::Rect* art = slotArt(type, i == 0 ? game::Rarity::Common : inv.slot(i - 1).rarity);
        if (art && m_atlas.ready()) {
            m_atlas.draw(*art, x + 1, y + 1);
            ui::frame(x, y, kWeaponSlotSize, kWeaponSlotSize, 1, rgba(255, 255, 255, 170));
        } else {
            u32 fill = rgba(0, 0, 0, 50);
            if (i == 0) fill = rgba(170, 180, 200, 170);
            else if (type != game::ItemType::None) fill = rarityColor(game::itemDef(type).rarity);
            slotBox(x, y, kWeaponSlotSize, fill, rgba(255, 255, 255, 170));
        }
        if (selected) ui::frame(x - 2, y - 2, kWeaponSlotSize + 4, kWeaponSlotSize + 4, 2, kWhite);
        if (i == 0 && m_atlas.ready()) m_atlas.draw(atlas::kPico, x + 4, y + 4);
    }
}

void App::renderDamageNumbers() const {
    // Numeros de dano sobre lo que ha alcanzado el jugador: suben y se desvanecen.
    if (!m_settings.damageNumbers) return;
    const game::DamageNumber* numbers = m_match.damageNumbers();
    for (int i = 0; i < game::Match::kMaxDamageNumbers; ++i) {
        const game::DamageNumber& d = numbers[i];
        if (d.age >= kDamageNumberSeconds) continue;
        float x, y;
        if (!projectToScreen(m_viewProjection, d.position + Vec3{0.0f, d.age * 0.9f, 0.0f}, &x, &y)) continue;
        const float fade = d.age < 0.55f ? 1.0f : 1.0f - (d.age - 0.55f) / (kDamageNumberSeconds - 0.55f);
        const u32 alpha = static_cast<u32>(255.0f * fade);
        // Disparo a la cabeza: mas grande y en amarillo, como en Fortnite.
        const float scale = d.structure ? 2.0f : (d.headshot ? 3.5f : 3.0f);
        // Con escudo, azul (mas claro si es a la cabeza), como en Fortnite.
        const u32 color = d.structure  ? rgba(200, 220, 255, alpha)
                          : d.shield   ? (d.headshot ? rgba(120, 220, 255, alpha) : rgba(70, 160, 255, alpha))
                          : d.headshot ? rgba(255, 220, 40, alpha)
                                       : rgba(255, 255, 255, alpha);
        const int value = static_cast<int>(d.amount + 0.5f);
        drawBigNumber(x - bigNumberWidth(value, scale) * 0.5f, y - 2.5f * scale, value, scale, color);
    }
}

// --- HUD: texto -----------------------------------------------------------------------------

void App::overlayGame(const FrameInfo& frame) const {
    switch (m_hud) {
        case Hud::Game: overlayGameHud(frame); break;
        case Hud::Debug: overlayDebug(frame); break;
        case Hud::Controls: overlayControlsPage(frame); break;
        case Hud::Hidden:
        case Hud::Count:
            gfx::text::printShadow(400, 4, kWhite, "%5.1f FPS", frame.timer.fps());
            break;
    }
}

void App::overlayCompass() const {
    // Brujula: rumbos a menos de 70 grados de la camara (el oeste queda a la izquierda).
    static const char* const kNamesEs[] = {"N", "NO", "O", "SO", "S", "SE", "E", "NE"};
    static const char* const kNamesEn[] = {"N", "NW", "W", "SW", "S", "SE", "E", "NE"};
    const char* const* kNames = i18n::language() == i18n::Lang::En ? kNamesEn : kNamesEs;
    const float camera = headingDegrees(m_freeCameraActive ? 0.0f : m_followCamera.yaw());
    for (int i = 0; i < 8; ++i) {
        float diff = i * 45.0f - camera;
        while (diff > 180.0f) diff -= 360.0f;
        while (diff <= -180.0f) diff += 360.0f;
        if (fabsf(diff) > 70.0f) continue;
        const int x = static_cast<int>(240.0f - diff * 1.5f);
        printCenteredAt(x, 6, i % 2 == 0 ? kWhite : kDimGrey, kNames[i]);
    }
}

void App::overlayGameHud(const FrameInfo& frame) const {
    if (m_mapOpen) {
        // Mapa a pantalla completa: solo su leyenda.
        const game::StormZone& storm = m_match.storm();
        char line[64];
        if (storm.closes() && storm.timeLeft() >= 0.0f) {
            snprintf(line, sizeof(line), "%s %.0f s", tr(storm.shrinking() ? T::StormClosingIn : T::StormNextIn),
                     static_cast<double>(storm.timeLeft()));
            printCenteredShadow(4, rgba(210, 150, 255), line);
        }
        snprintf(line, sizeof(line), tr(T::MapAliveKills), m_match.aliveCount(), m_match.playerKills());
        gfx::text::printShadow(6, 262, kWhite, "%s", line);
        gfx::text::printShadow(380, 262, kLightGrey, "%s", tr(T::MapClose));
        return;
    }
    gfx::text::printShadow(6, 4, kLightGrey, "%s", m_match.name(0));
    if (m_settings.showFps) gfx::text::print(6, 14, kDimGrey, "%4.1f FPS", frame.timer.fps());
    overlayCompass();
    // Inventario abierto: solo su texto (el resto del HUD queda debajo del panel).
    if (m_inventoryOpen) {
        overlayInventoryScreen();
        return;
    }

    // Autobus y caida.
    const game::AirState air = m_match.airState(0);
    if (m_match.alive(0) && air != game::AirState::Ground) {
        if (air == game::AirState::Bus) {
            ui::printHints((gfx::kScreenWidth - ui::hintsWidth(kBusHint, 1)) / 2, 154, kWhite, kBusHint, 1);
            char line[48];
            snprintf(line, sizeof(line), tr(T::AutoJump), fmaxf(0.0f, m_match.busTimeLeft()));
            printCenteredShadow(168, kLightGrey, line);
        } else {
            if (air == game::AirState::Freefall) {
                ui::printHints((gfx::kScreenWidth - ui::hintsWidth(kGliderHint, 1)) / 2, 154, kWhite, kGliderHint, 1);
            }
            char line[32];
            snprintf(line, sizeof(line), tr(T::Altitude), m_match.heightAboveGround(0));
            printCenteredShadow(air == game::AirState::Glide ? 153 : 168, kLightGrey, line);
        }
    } else if (m_swapOpen) {
        const game::Pickup& p = m_match.pickups()[m_swapPickup];
        const game::InventorySlot& out = m_match.inventory().slot(m_swapCursor);
        const char* in = game::isWeapon(p.item) ? game::weaponConfig(p.item, p.rarity).name : game::itemName(p.item);
        const char* drop = out.type == game::ItemType::None ? tr(T::SwapEmptySlot)
                           : game::isWeapon(out.type) ? out.weapon.config().name
                                                       : game::itemName(out.type);
        char line[64];
        snprintf(line, sizeof(line), tr(T::SwapChoose), in, drop);
        printCenteredShadow(static_cast<int>(kSwapPanelY) + 6, kWhite, line);
        ui::printHints((gfx::kScreenWidth - ui::hintsWidth(kSwapHints, 4)) / 2, static_cast<int>(kSwapPanelY) + 22, kLightGrey,
                       kSwapHints, 4);
    } else {
        int n = 0;
        if (const ui::Hint* hint = interactionHint(&n)) {
            const game::Interactable it = m_match.playerInteractable();
            u32 color = kWhite;
            if (it.kind == game::Interactable::Kind::Pickup) {
                color = rarityTextColor(m_match.pickups()[it.index].rarity);
            }
            ui::printHints((gfx::kScreenWidth - ui::hintsWidth(hint, n)) / 2, kPromptY, color, hint, n);
        }
    }
    if (m_stormMessage > 0.0f && fmodf(m_stormMessage, 0.8f) > 0.2f) {
        printCenteredShadow(107, rgba(210, 150, 255), tr(T::StormClosingNow));
    }
    if (m_airdropMessage > 0.0f && fmodf(m_airdropMessage, 0.8f) > 0.2f) {
        printCenteredShadow(96, rgba(120, 190, 255), tr(T::AirdropIncoming));
    }

    // Construccion: paredes y por que no se puede colocar.
    if (m_buildMode) {
        const game::StructureSystem& s = m_match.structures();
        static const T kMaterialNames[] = {T::MaterialWood, T::MaterialStone, T::MaterialMetal};
        char banner[64];
        const bool trap = m_buildPiece == kTrapPiece;
        if (trap) {
            snprintf(banner, sizeof(banner), tr(T::TrapBanner), tr(kBuildPieceNames[m_buildPiece]), m_match.trapStock());
        } else {
            snprintf(banner, sizeof(banner), tr(T::BuildBanner), tr(kBuildPieceNames[m_buildPiece]),
                     tr(kMaterialNames[static_cast<int>(m_wallPlan.material)]), s.count(), s.config().maxWalls);
        }
        printCenteredShadow(44, kWhite, banner);
        if (trap ? !m_trapPlan.valid : !m_wallPlan.valid) {
            printCenteredShadow(55, kErrorRed, trap ? game::trapErrorName(m_trapPlan.error)
                                                    : game::placementErrorName(m_wallPlan.error));
        } else {
            int n = 0;
            const ui::Hint* hints = buildHints(&n);
            ui::printHints((gfx::kScreenWidth - ui::hintsWidth(hints, n)) / 2, 55, kLightGrey, hints, n);
        }
    }
    if (!m_match.alive(0)) {
        char line[40];
        snprintf(line, sizeof(line), tr(T::SpectatingName), m_match.name(spectateTarget()));
        printCenteredShadow(44, kErrorRed, line);
    } else {
        // Tormenta: aviso al acercarse al borde del mapa y dentro (del borde o de la que se cierra).
        const Vec3& p = m_match.body(0).state().position;
        const float depth = world::stormDepth(p.x, p.z);
        if (m_match.inStorm(0)) {
            char line[48];
            snprintf(line, sizeof(line), tr(T::StormDamage),
                     depth > 0.0f ? m_matchConfig.stormDamage : m_match.storm().damage());
            printCenteredShadow(70, rgba(230, 180, 255), line);
            printCenteredShadow(81, kWhite, tr(T::StormBack));
        } else if (depth > -12.0f) {
            char line[48];
            snprintf(line, sizeof(line), tr(T::StormEdge), -depth);
            printCenteredShadow(70, rgba(210, 160, 250), line);
        }
    }

    // Registro de eliminaciones.
    int y = 28;
    for (int i = 0; i < m_match.killEventCount(); ++i) {
        const game::KillEvent& e = m_match.killEvent(i);
        if (m_match.elapsed() - e.time > kKillFeedSeconds) continue;
        const u32 color = (e.killer == 0 || e.victim == 0) ? config::kColorGold : kWhite;
        if (e.killer >= 0) {
            gfx::text::printShadow(6, y, color, tr(T::FeedEliminated), m_match.name(e.killer), m_match.name(e.victim));
        } else if (e.killer == game::kStormKiller) {
            gfx::text::printShadow(6, y, color, tr(T::FeedStorm), m_match.name(e.victim));
        } else {
            gfx::text::printShadow(6, y, color, tr(T::FeedFell), m_match.name(e.victim));
        }
        y += 10;
    }

    // Bajo el minimapa: tiempo hasta el siguiente cierre de la tormenta (morado mientras
    // se cierra), vivos y bajas.
    const game::StormZone& storm = m_match.storm();
    char time[16];
    const float left = storm.closes() ? storm.timeLeft() : m_matchConfig.zoneShrinkSeconds - m_match.elapsed();
    formatTime(left > 0.0f ? left : 0.0f, time, sizeof(time));
    gfx::text::printShadow(376, 89, storm.shrinking() ? rgba(210, 150, 255) : kWhite, "%s", time);
    gfx::text::printShadow(428, 89, kWhite, "%d", m_match.aliveCount());
    gfx::text::printShadow(464, 89, kWhite, "%d", m_match.playerKills());
    // Materiales: madera, piedra y metal.
    const game::Materials& mats = m_match.materials();
    const int wood = mats.get(world::Resource::Wood);
    const int stone = mats.get(world::Resource::Stone), metal = mats.get(world::Resource::Metal);
    gfx::text::printShadow(wood >= 100 ? 400 : (wood >= 10 ? 404 : 408), 126, kWhite, "%d", wood);
    gfx::text::printShadow(stone >= 100 ? 426 : (stone >= 10 ? 430 : 434), 126, kWhite, "%d", stone);
    gfx::text::printShadow(metal >= 100 ? 452 : (metal >= 10 ? 456 : 460), 126, kWhite, "%d", metal);

    // Estado del arma (solo con un arma en la mano).
    const game::Inventory& inv = m_match.inventory();
    if (const game::Weapon* w = inv.activeWeapon()) {
        if (w->reloading()) gfx::text::printShadow(18, 198, config::kColorGold, "%s", tr(T::Reloading));
        else if (w->empty()) gfx::text::printShadow(18, 198, kErrorRed, "%s", tr(T::NoAmmo));
        else if (w->ammo() == 0) ui::printHints(18, 198, kErrorRed, kReloadHint, 1);
    }
    const game::CharacterState& p = m_match.body(0).state();
    gfx::text::print(static_cast<int>(kBarX) + 4, static_cast<int>(kShieldY) + 1, kWhite, "%.0f|%.0f", p.shield,
                     p.maxShield);
    gfx::text::print(static_cast<int>(kBarX) + 4, static_cast<int>(kHealthY) + 1, kWhite, "%.0f|%.0f",
                     p.health, p.maxHealth);

    // Trampas que se llevan, en su casilla de la barra de construccion.
    if (m_match.trapStock() > 0) {
        gfx::text::printShadow(static_cast<int>(kBuildSlotX + kTrapPiece * kBuildSlotStep) + 20,
                               static_cast<int>(kBuildRowY) + 19, kWhite, "%d", m_match.trapStock());
    }
    // Cantidades en las ranuras: balas en reserva del arma y unidades de los consumibles.
    for (int i = 0; i < game::Inventory::kSlots; ++i) {
        const game::InventorySlot& slot = inv.slot(i);
        if (slot.type == game::ItemType::None) continue;
        const bool weapon = game::itemDef(slot.type).kind == game::ItemKind::Weapon;
        const int n = weapon ? inv.ammo(static_cast<game::AmmoType>(slot.weapon.config().ammoType)) : slot.count;
        if (n < 0 || (!weapon && n <= 1 && game::itemDef(slot.type).maxStack == 1)) continue;
        const bool selected = !m_buildMode && !inv.pickaxe() && inv.selected() == i;
        const int sx = static_cast<int>(kWeaponSlotX + (i + 1) * kWeaponSlotStep) + 33 - 8 * (n >= 100 ? 3 : n >= 10 ? 2 : 1);
        gfx::text::printShadow(sx, static_cast<int>(kWeaponRowY) + 25 - (selected ? 3 : 0), kWhite, "%d", n);
    }
    // Nombre de lo que se lleva en la mano, sobre la fila de objetos.
    if (!m_buildMode && m_match.alive(0)) {
        const game::InventorySlot& held = inv.slot(inv.selected());
        const char* name = inv.pickaxe() ? tr(T::WeaponPickaxe)
                           : game::isWeapon(held.type) ? held.weapon.config().name
                                                        : game::itemName(held.type);
        if (name[0]) gfx::text::printShadow(static_cast<int>(kWeaponSlotX), static_cast<int>(kWeaponRowY) - 12, kWhite,
                                            "%s", name);
    }
    // Consumible en uso, o por que no se puede usar.
    const float use = m_match.useProgress();
    if (use >= 0.0f) {
        char line[48];
        snprintf(line, sizeof(line), tr(T::Using), game::itemName(m_match.usingItem()), (1.0f - use) * m_match.useDuration());
        printCenteredShadow(154, kWhite, line);
    } else if (m_match.sinceUseError() < 1.5f) {
        printCenteredShadow(154, kErrorRed, game::useErrorName(m_match.lastUseError()));
    }
    // Material conseguido con el pico.
    if (m_harvestAge < kHarvestPopupSeconds) {
        gfx::text::printShadow(280, static_cast<int>(116 - m_harvestAge * 12.0f), kWhite, "+%d", m_harvestAmount);
    }
}

void App::renderDebugPage() const {
    gfx::fillRect(0, 0, gfx::kScreenWidth, 16, kBar);
    gfx::fillRect(4, 20, 232, 248, kPanel);
    gfx::fillRect(240, 20, 236, 248, kPanel);
    ui::drawHintIcons(m_atlas, 8, kFreeCameraHintY, kFreeCameraHint, 1);
}

void App::overlayDebug(const FrameInfo& frame) const {
    const game::CharacterState& p = m_match.body(0).state();
    const game::MatchStats& s = m_match.stats();
    gfx::text::print(6, 4, config::kColorGold, "%s v%s", config::kGameTitle, config::kVersion);
    gfx::text::print(168, 4, kLightGrey, "%s", config::kPhase);
    gfx::text::print(400, 4, kWhite, "%5.1f FPS", frame.timer.fps());

    int y = 24;
    const int x = 8;
    gfx::text::print(x, y, config::kColorGold, "%s", tr(T::DebugPlayer));
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Pos %6.1f %5.1f %6.1f", p.position.x, p.position.y, p.position.z);
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Vel %4.2f vert %+5.2f", p.horizontalSpeed(), p.velocity.y);
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "%s, %s", game::jumpStateName(p.jumpState), game::locomotionName(p));
    y += kLineHeight + 4;
    gfx::text::print(x, y, config::kColorGold, tr(T::DebugPerformance), m_match.botCount());
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Frame %5.2f ms trab %5.2f", frame.timer.frameMsAvg(), frame.workMs);
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "IA %5.2f ms  fis %5.2f ms", m_aiMsPerFrame, m_physicsMsPerFrame);
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, tr(T::DebugDecisions), m_decisionsPerSecond);
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, "Tris %5d+%5d lotes %3d", m_stats.trianglesDrawn, m_characterTriangles,
                     m_stats.batchesDrawn);
    y += kLineHeight;
    const struct mallinfo mem = mallinfo();
    gfx::text::print(x, y, kWhite, "Heap %u KB (lobby %u KB)", static_cast<unsigned>(mem.uordblks / 1024),
                     m_startHeapBytes / 1024);
    y += kLineHeight;
    if (m_audioReady) {
        gfx::text::print(x, y, kWhite, tr(T::DebugAudio), audio::mixer().activeVoices(),
                         static_cast<unsigned>(audio::mixer().droppedCommands()));
    } else {
        gfx::text::print(x, y, kErrorRed, "%s", tr(T::DebugNoAudio));
    }
    y += kLineHeight + 4;
    gfx::text::print(x, y, config::kColorGold, tr(T::DebugMatch), m_matchesPlayed);
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, tr(T::DebugAlive), m_match.aliveCount(), m_match.entityCount(),
                     m_match.zoneRadius());
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, tr(T::DebugPaths), s.pathRequests, s.pathFailures);
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, tr(T::DebugNodes), s.maxPathExpansions, s.stuckEvents);
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, tr(T::DebugShots), s.shotsFired, s.shotsHitEntity,
                     s.shotsBlockedByWorld);
    y += kLineHeight;
    gfx::text::print(x, y, kWhite, tr(T::DebugWalls), s.wallsBuilt, s.wallsRemoved, s.wallsDestroyed);
    y += kLineHeight;

    // Lista de bots: estado, vida, objetivo y distancia.
    y = 24;
    const int bx = 244;
    gfx::text::print(bx, y, config::kColorGold, "%s", tr(T::DebugBotHeader));
    y += 10;
    const int maxRows = 23;
    for (int i = 0; i < m_match.botCount() && i < maxRows; ++i) {
        const game::Bot& b = m_match.bot(i);
        const bool dead = b.state == game::BotState::Eliminated;
        char target[16] = "-";
        if (!dead && b.target >= 0) snprintf(target, sizeof(target), "%s%d", b.target == 0 ? "J" : "B", b.target);
        gfx::text::print(bx, y, dead ? rgba(130, 130, 130) : (b.state == game::BotState::Attack ? kErrorRed : kWhite),
                         "%2d %-12s %3.0f %s", i + 1, game::botStateName(b.state), b.body.state().health, target);
        y += 10;
    }
    if (m_match.botCount() > maxRows) gfx::text::print(bx, y, kLightGrey, tr(T::DebugMore), m_match.botCount() - maxRows);
    ui::printHints(8, kFreeCameraHintY, config::kColorGold, kFreeCameraHint, 1);
}

// --- Pagina de controles (lobby -> CONTROLES, y SELECT en la partida) -----------------------

void App::renderControlsPage(const FrameInfo& frame) const {
    const u32 buttons = frame.input.rawButtons();

    gfx::fillRect(0, 0, gfx::kScreenWidth, 30, kNavy);
    gfx::fillRect(0, 30, gfx::kScreenWidth, 1, rgba(60, 90, 160));
    if (m_atlas.ready()) m_atlas.drawCentered(label(atlas::kControles, atlas::kControlsEn), 240, 4);
    gfx::fillRect(6, 36, 300, 208, rgba(0, 0, 0, 150));
    gfx::fillRect(312, 36, 162, 208, rgba(0, 0, 0, 150));
    gfx::fillRect(0, 250, gfx::kScreenWidth, 22, kNavy);

    forEachControlRow([&](const ControlRow& row, int y) {
        if (row.title != T::None) return;
        ui::drawHintIcons(m_atlas, kControlsX, y, row.left, row.leftCount);
        ui::drawHintIcons(m_atlas, kControlsColumn, y, row.right, row.rightCount);
    });

    // Mando: cada boton con su icono, apagado si no se pulsa.
    if (m_atlas.ready()) {
        for (const PadButton& b : kPadButtons) {
            const assets::ui::Rect& r = ui::buttonRect(b.button);
            const bool on = (buttons & b.mask) != 0;
            if (on) gfx::fillRect(b.cx - r.w * 0.5f - 2, b.cy - r.h * 0.5f - 2, r.w + 4, r.h + 4, kButtonOn);
            m_atlas.draw(r, b.cx - r.w * 0.5f, b.cy - r.h * 0.5f, 1.0f, on ? kWhite : rgba(255, 255, 255, 120));
        }
    }

    // Stick analogico: icono, cruz central y punto con la posicion normalizada.
    const float half = kStickBoxSize / 2.0f;
    const float cx = kStickBoxX + half;
    const float cy = kStickBoxY + half;
    gfx::fillRect(kStickBoxX, kStickBoxY, kStickBoxSize, kStickBoxSize, kButtonOff);
    if (m_atlas.ready()) {
        const assets::ui::Rect& stick = atlas::kBtnStick;
        m_atlas.draw(stick, cx - stick.w * 1.5f, cy - stick.h * 1.5f, 3.0f, rgba(255, 255, 255, 150));
    }
    gfx::fillRect(kStickBoxX, cy, kStickBoxSize, 1, kButtonOff);
    gfx::fillRect(cx, kStickBoxY, 1, kStickBoxSize, kButtonOff);
    const float dotX = cx + frame.input.analogX() * (half - 3.0f);
    const float dotY = cy + frame.input.analogY() * (half - 3.0f);
    gfx::fillRect(dotX - 3.0f, dotY - 3.0f, 6, 6, kButtonOn);

    const ui::Hint* footer = m_screen == Screen::Game ? kHudPageHint : kBackHint;
    ui::drawHintIcons(m_atlas, 8, 257, footer, 1);
}

void App::overlayControlsPage(const FrameInfo& frame) const {
    const input::Input& in = frame.input;
    gfx::text::print(400, 12, kLightGrey, "%5.1f FPS", frame.timer.fps());

    int y = forEachControlRow([&](const ControlRow& row, int rowY) {
        if (row.title != T::None) {
            gfx::text::print(kControlsX, rowY, config::kColorGold, "%s", tr(row.title));
            return;
        }
        ui::printHints(kControlsX, rowY, kWhite, row.left, row.leftCount);
        ui::printHints(kControlsColumn, rowY, kWhite, row.right, row.rightCount);
    });

    char held[40];
    heldButtonsString(in.rawButtons(), held, 26);
    gfx::text::print(kControlsX, y, config::kColorGold, "%s", tr(T::PadTest));
    y += kLineHeight;
    gfx::text::print(kControlsX, y, kWhite, tr(T::PadPressed), held);
    y += kLineHeight;
    gfx::text::print(kControlsX, y, kWhite, "Stick %+.2f, %+.2f  CPU %d/%d MHz", in.analogX(), in.analogY(), m_cpuMhz,
                     m_busMhz);
    gfx::text::print(357, 192, kLightGrey, "%s", tr(T::PadAnalog));

    const ui::Hint* footer = m_screen == Screen::Game ? kHudPageHint : kBackHint;
    ui::printHints(8, 257, kWhite, footer, 1);
}

}  // namespace app
