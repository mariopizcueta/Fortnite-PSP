// Modo Supervivencia zombi en la aplicacion (v0.14, docs/ZOMBIS.md): reglas de la partida,
// avisos, HUD (dia y noche, objetivos), menu de crafteo, marcas del minimapa, la noche
// (oscuridad y vineta), el despertar del coma y los resultados. Tambien el cielo con
// degradado, que usan todos los modos.
#include "app/app.h"

#include <math.h>
#include <string.h>

#include <vector>

#include "app/app_common.h"
#include "audio/audio_system.h"
#include "platform/log.h"
#include "world/storm.h"
#include "world/water.h"
#include "world/world_units.h"
#include "world/zombie_sites.h"

namespace app {
namespace {

using namespace common;
using math3d::Vec3;
using i18n::T;
using i18n::tr;
using B = ui::Button;
namespace atlas = assets::ui;

// Niebla y cielo del modo zombi: gris verdoso de dia, casi negro de noche.
constexpr u32 kZombieFogDay = rgba(104, 112, 98);
constexpr u32 kZombieFogNight = rgba(14, 18, 26);
constexpr float kWakeSeconds = 6.5f;
constexpr u32 kZombieGreen = rgba(150, 220, 110);
constexpr u32 kNightRed = rgba(255, 110, 90);

u32 lerpColor(u32 a, u32 b, float t) {
    auto ch = [&](int shift) {
        const float x = static_cast<float>((a >> shift) & 0xFF), y = static_cast<float>((b >> shift) & 0xFF);
        return static_cast<u32>(x + (y - x) * t);
    };
    return rgba(ch(0), ch(8), ch(16), ch(24));
}

// Menu de crafteo.
constexpr float kCraftX = 70.0f, kCraftY = 30.0f, kCraftW = 340.0f, kCraftH = 210.0f, kCraftRowH = 15.0f;
constexpr float kCraftListY = kCraftY + 40.0f;
const ui::Hint kCraftHints[] = {{B::Up, T::None}, {B::Down, T::HintSelect}, {B::Cross, T::HintCraft}, {B::Circle, T::HintClose}};

}  // namespace

// --- Partida ------------------------------------------------------------------------------------

void App::configureZombieMatch() {
    // Llamado desde startMatch con el modo zombi elegido: sus reglas (con la semilla y los
    // trucos de la partida) y el escenario en ruinas.
    const u32 seed = m_matchConfig.seed;
    const game::Cheats cheats = m_matchConfig.cheats;
    m_matchConfig = game::MatchConfig::zombies();
    m_matchConfig.seed = seed;
    m_matchConfig.cheats = cheats;
    if (cheats.legendaryKit) {
        using game::ItemType;
        using game::Rarity;
        m_matchConfig.startItems[0] = {ItemType::Shotgun, 1, Rarity::Legendary};
        m_matchConfig.startItems[1] = {ItemType::AssaultRifle, 1, Rarity::Legendary};
        m_matchConfig.startItems[2] = {ItemType::RocketLauncher, 1, Rarity::Legendary};
        m_matchConfig.startItems[3] = {ItemType::ShieldPotion, 3, Rarity::Rare};
        m_matchConfig.startItems[4] = {ItemType::Medkit, 3, Rarity::Uncommon};
        for (int a = 0; a < static_cast<int>(game::AmmoType::Count); ++a) m_matchConfig.startAmmo[a] = 120;
        for (int r = 0; r < static_cast<int>(world::Resource::Count); ++r) m_matchConfig.startMaterials[r] = 500;
    }
}

void App::updateZombieFog(float* start, float* end, u32* color) const {
    // De dia, niebla gris verdosa y mas cerca que en el battle royale; de noche, casi negra y
    // a pocos metros (se ve lo justo para que los zombis aparezcan de la oscuridad).
    const float d = m_match.darkness();
    *color = lerpColor(kZombieFogDay, kZombieFogNight, d);
    const float baseEnd = fminf(m_settings.fogEnd(), 72.0f);
    *start = 16.0f + (5.0f - 16.0f) * d;
    *end = baseEnd + (34.0f - baseEnd) * d;
}

void App::showZombieMessage(T text, const char* name, int number, u32 color, float seconds) {
    m_zMessage = text;
    snprintf(m_zMessageName, sizeof(m_zMessageName), "%s", name ? name : "");
    m_zMessageNumber = number;
    m_zMessageColor = color;
    m_zMessageTime = seconds;
}

void App::processZombieEvent(const game::GameEvent& e) {
    switch (e.type) {
        case game::GameEventType::NightStarted:
            playSfx(audio::Sfx::Horde, 1.0f);
            showZombieMessage(T::ZNightFall, nullptr, e.other, kNightRed, 5.0f);
            break;
        case game::GameEventType::DayStarted: {
            playSfx(audio::Sfx::Dawn, 0.9f);
            showZombieMessage(T::ZDawn, nullptr, e.other, config::kColorGold, 4.0f);
            // Record de noches seguidas (logros "primera noche", "tres noches", "una semana").
            const int nights = m_match.nightsSurvived();
            if (nights > m_profile.zombieBestNights) addStat(Stat::ZombieNights, nights - m_profile.zombieBestNights);
            break;
        }
        case game::GameEventType::HordeIncoming:
            break;
        case game::GameEventType::ZombieAttack:
            if (e.entity > 0) m_botAnims[e.entity - 1].onSwing();
            playSpatial(audio::Sfx::ZombieHit, e.position, e.other == 0 ? 1.0f : 0.7f);
            break;
        case game::GameEventType::ZombieGroan:
            playSpatial(audio::Sfx::ZombieGroan, e.position, 0.55f);
            break;
        case game::GameEventType::SurvivorFound:
            showZombieMessage(T::ZSurvivorFound, m_match.name(e.entity), 0, kYellow);
            playSfx(audio::Sfx::MenuMove, 0.8f);
            break;
        case game::GameEventType::SurvivorGift:
            if (e.other >= 0) playSfx(audio::Sfx::Pickup, 0.8f);
            if (e.amount != 0) {
                const T needs = e.amount == 3 ? T::ZNeedsBoth : (e.amount == 1 ? T::ZNeedsWeapon : T::ZNeedsHeal);
                showZombieMessage(needs, m_match.name(e.entity), 0, kWhite, 2.5f);
            }
            break;
        case game::GameEventType::SurvivorRescued:
            playSfx(audio::Sfx::MenuAccept, 1.0f);
            showZombieMessage(T::ZSurvivorRescued, m_match.name(e.entity), 0, kZombieGreen);
            addStat(Stat::SurvivorsRescued, 1);
            break;
        case game::GameEventType::WorkbenchFound:
            showZombieMessage(T::ZWorkbenchFound, nullptr, 0, rgba(255, 180, 90));
            playSfx(audio::Sfx::MenuMove, 0.8f);
            break;
        case game::GameEventType::WorkbenchOpened:
            m_craftOpen = true;
            m_craftMessage = T::None;
            m_buildMode = false;
            m_dancing = false;
            playSfx(audio::Sfx::MenuAccept, 0.7f);
            break;
        case game::GameEventType::Crafted:
            m_craftMessageTime = 2.0f;
            if (e.amount > 0) {
                playSfx(audio::Sfx::Craft, 0.9f);
                m_craftMessage = T::CraftDone;
                m_craftMessageError = false;
                addStat(Stat::ItemsCrafted, 1);
            } else {
                playSfx(audio::Sfx::MenuMove, 0.4f);
                m_craftMessage = T::CraftNoMats;
                m_craftMessageError = true;
            }
            break;
        case game::GameEventType::BlueprintFound:
            playSfx(audio::Sfx::Pickup, 1.0f);
            showZombieMessage(T::ZBlueprintFound, tr(game::blueprint(e.other).name), 0, rgba(120, 190, 255), 4.0f);
            break;
        case game::GameEventType::BlueprintPiece:
            break;
        case game::GameEventType::BlueprintDone:
            playSfx(audio::Sfx::Dawn, 0.8f);
            showZombieMessage(T::ZBlueprintDone, tr(game::blueprint(e.other).name), 0, rgba(120, 190, 255), 4.0f);
            addStat(Stat::BlueprintsDone, 1);
            break;
        default:
            break;
    }
}

// --- Menu de crafteo ----------------------------------------------------------------------------

void App::updateCraftMenu(const input::Input& in) {
    const int n = game::recipeCount();
    const int step = (in.pressed(PSP_CTRL_DOWN) ? 1 : 0) - (in.pressed(PSP_CTRL_UP) ? 1 : 0);
    if (step != 0) {
        m_craftCursor = (m_craftCursor + n + step) % n;
        playSfx(audio::Sfx::MenuMove, 0.5f);
    }
    if (in.pressed(PSP_CTRL_CROSS)) {
        m_match.craft(m_craftCursor);  // el resultado (y su sonido) llega como suceso
    } else if (in.pressed(PSP_CTRL_CIRCLE)) {
        m_craftOpen = false;
        playSfx(audio::Sfx::MenuMove, 0.6f);
    }
}

void App::renderCraftMenu() const {
    gfx::fillRect(0, 0, gfx::kScreenWidth, gfx::kScreenHeight, rgba(0, 0, 0, 120));
    gfx::fillRect(kCraftX, kCraftY, kCraftW, kCraftH, rgba(28, 22, 16, 240));
    ui::frame(kCraftX, kCraftY, kCraftW, kCraftH, 2, rgba(220, 140, 60));
    gfx::fillRect(kCraftX, kCraftY, kCraftW, 20, rgba(140, 70, 30));
    for (int i = 0; i < game::recipeCount(); ++i) {
        const float y = kCraftListY + i * kCraftRowH;
        const bool can = m_match.canCraft(i);
        if (i == m_craftCursor) {
            gfx::fillRect(kCraftX + 6, y - 3, kCraftW - 12, kCraftRowH - 1, can ? rgba(170, 100, 40, 230) : rgba(90, 40, 30, 230));
            ui::frame(kCraftX + 6, y - 3, kCraftW - 12, kCraftRowH - 1, 1, kWhite);
        } else if (i % 2 == 0) {
            gfx::fillRect(kCraftX + 6, y - 3, kCraftW - 12, kCraftRowH - 1, rgba(255, 255, 255, 14));
        }
    }
    ui::drawHintIcons(m_atlas, centeredHintsX(kCraftHints, 4), static_cast<int>(kCraftY + kCraftH) - 16, kCraftHints, 4);
}

void App::overlayCraftMenu() const {
    printCenteredAt(240, static_cast<int>(kCraftY) + 6, kWhite, tr(T::CraftTitle));
    const game::Materials& m = m_match.materials();
    char line[64];
    snprintf(line, sizeof(line), tr(T::CraftHave), m.get(world::Resource::Wood), m.get(world::Resource::Stone),
             m.get(world::Resource::Metal));
    printCenteredAt(240, static_cast<int>(kCraftY) + 25, kLightGrey, line);
    for (int i = 0; i < game::recipeCount(); ++i) {
        const game::Recipe& r = game::recipe(i);
        const int y = static_cast<int>(kCraftListY + i * kCraftRowH);
        const bool can = m_match.canCraft(i);
        const u32 color = can ? kWhite : kDimGrey;
        gfx::text::printShadow(static_cast<int>(kCraftX) + 12, y, color, "%s", tr(r.name));
        snprintf(line, sizeof(line), tr(T::CraftCost), r.cost[0], r.cost[1], r.cost[2]);
        gfx::text::printShadow(static_cast<int>(kCraftX + kCraftW) - 12 - gfx::text::width(line), y, can ? kYellow : kErrorRed,
                               "%s", line);
    }
    int y = static_cast<int>(kCraftListY + game::recipeCount() * kCraftRowH) + 2;
    const game::Recipe& chosen = game::recipe(m_craftCursor);
    if (chosen.item == game::ItemType::None) printCenteredAt(240, y, kLightGrey, tr(T::CraftAmmoNote));
    if (m_craftMessageTime > 0.0f && m_craftMessage != T::None) {
        if (m_craftMessage == T::CraftDone) snprintf(line, sizeof(line), tr(T::CraftDone), tr(chosen.name));
        else snprintf(line, sizeof(line), "%s", tr(m_craftMessage));
        printCenteredAt(240, y + 11, m_craftMessageError ? kErrorRed : kZombieGreen, line);
    }
    ui::printHints(centeredHintsX(kCraftHints, 4), static_cast<int>(kCraftY + kCraftH) - 16, kLightGrey, kCraftHints, 4);
}

// --- HUD ----------------------------------------------------------------------------------------

void App::renderNightOverlay() const {
    // Siempre algo de penumbra verdosa y una vineta; de noche, oscuridad y vineta cerrada.
    const float d = m_match.darkness();
    gfx::fillRect(0, 0, gfx::kScreenWidth, gfx::kScreenHeight, rgba(14, 26, 12, static_cast<u32>(34.0f + 20.0f * d)));
    if (d > 0.01f) gfx::fillRect(0, 0, gfx::kScreenWidth, gfx::kScreenHeight, rgba(0, 2, 12, static_cast<u32>(64.0f * d)));
    if (m_vignette.pixels) {
        const u32 alpha = static_cast<u32>(140.0f + 90.0f * d);
        gfx::drawTextureRect(m_vignette, 0, 0, 64, 64, 0, 0, gfx::kScreenWidth, gfx::kScreenHeight, rgba(2, 4, 10, alpha));
    }
}

void App::renderZombieHud() const {
    // Reloj del dia (arriba en el centro) y objetivos (a la izquierda).
    const bool night = m_match.night();
    // A la izquierda, encima de los objetivos (arriba en el centro esta la brujula).
    constexpr float kX = 4.0f, kY = 102.0f, kW = 164.0f, kH = 28.0f;
    gfx::fillRect(kX, kY, kW, kH, rgba(0, 0, 0, 120));
    ui::frame(kX, kY, kW, kH, 1, night ? rgba(120, 140, 220) : config::kColorGold);
    // Sol o luna.
    const float cx = kX + 11.0f, cy = kY + 13.0f;
    const u32 disc = night ? rgba(220, 230, 255) : rgba(255, 210, 80);
    gfx::fillRect(cx - 4, cy - 6, 8, 12, disc);
    gfx::fillRect(cx - 6, cy - 4, 12, 8, disc);
    if (night) gfx::fillRect(cx - 1, cy - 6, 7, 10, rgba(0, 0, 0, 200));  // creciente
    // Barra de lo que queda del dia o de la noche.
    const float length = night ? m_matchConfig.zombie.nightLength : m_matchConfig.zombie.dayLength;
    const float left = m_match.timeToChange() / fmaxf(length, 1.0f);
    gfx::fillRect(kX + 24, kY + 23, kW - 30, 2, rgba(255, 255, 255, 50));
    gfx::fillRect(kX + 24, kY + 23, (kW - 30) * left, 2, night ? rgba(120, 140, 220) : config::kColorGold);
    // Objetivos.
    gfx::fillRect(4, 132, 164, 40, rgba(0, 0, 0, 100));
    // Aviso grande.
    if (m_zMessageTime > 0.0f && m_zMessage != T::None) {
        const float a = fminf(1.0f, m_zMessageTime * 2.0f);
        gfx::fillRect(0, 92, gfx::kScreenWidth, 18, rgba(0, 0, 0, static_cast<u32>(130.0f * a)));
    }
    if (m_craftOpen) renderCraftMenu();
    // Despertar: fundido desde negro con parpadeos de los parpados.
    if (m_wakeTime < kWakeSeconds) {
        const float t = m_wakeTime;
        float black = t < 0.8f ? 1.0f : fmaxf(0.0f, 1.0f - (t - 0.8f) / 2.6f);
        // Dos parpadeos: los parpados se cierran un momento.
        const float blink = fmaxf(fmaxf(0.0f, 1.0f - fabsf(t - 1.6f) / 0.25f), fmaxf(0.0f, 1.0f - fabsf(t - 2.6f) / 0.2f));
        black = fmaxf(black, 0.85f * blink);
        if (black > 0.0f) gfx::fillRect(0, 0, gfx::kScreenWidth, gfx::kScreenHeight, rgba(0, 0, 0, static_cast<u32>(255.0f * black)));
        const float lid = 136.0f * fmaxf(blink, t < 1.2f ? 1.0f - (t - 0.8f) / 0.4f : 0.0f);
        if (lid > 0.0f) {
            gfx::fillRect(0, 0, gfx::kScreenWidth, lid, rgba(0, 0, 0));
            gfx::fillRect(0, gfx::kScreenHeight - lid, gfx::kScreenWidth, lid, rgba(0, 0, 0));
        }
    }
}

void App::overlayZombieHud() const {
    char line[64];
    if (m_wakeTime < kWakeSeconds && m_wakeTime > 0.5f) {
        const u32 grey = rgba(220, 220, 220, 255);
        if (m_wakeTime < 2.6f) printCenteredShadow(120, grey, tr(T::ZWake1));
        if (m_wakeTime > 2.6f) printCenteredShadow(120, grey, tr(T::ZWake2));
        if (m_wakeTime > 3.6f) printCenteredShadow(132, grey, tr(T::ZWake3));
    }
    if (m_wakeTime < 3.4f) return;
    const int day = m_match.day();
    snprintf(line, sizeof(line), tr(m_match.night() ? T::ZNight : T::ZDay), day);
    gfx::text::printShadow(28, 105, m_match.night() ? rgba(170, 190, 255) : kWhite, "%s", line);
    const int left = static_cast<int>(m_match.timeToChange());
    snprintf(line, sizeof(line), tr(m_match.night() ? T::ZUntilDawn : T::ZUntilNight), left / 60, left % 60);
    gfx::text::printShadow(28, 115, kLightGrey, "%s", line);

    // Objetivos.
    int benches = 0;
    for (const game::Container& c : m_match.containers()) benches += c.kind == game::ContainerKind::Workbench && c.discovered ? 1 : 0;
    int found = 0;
    for (int i = 0; i < game::kBlueprintCount; ++i) found += m_match.blueprintFound(i) ? 1 : 0;
    snprintf(line, sizeof(line), tr(T::ZObjSurvivors), m_match.survivorsRescued(), game::kSurvivorCount);
    gfx::text::printShadow(8, 135, m_match.survivorsRescued() > 0 ? kZombieGreen : kWhite, "%s", line);
    snprintf(line, sizeof(line), tr(T::ZObjBlueprints), found, game::kBlueprintCount, m_match.blueprintsDone());
    gfx::text::printShadow(8, 146, found > 0 ? rgba(140, 200, 255) : kWhite, "%s", line);
    snprintf(line, sizeof(line), tr(T::ZObjBenches), benches, world::zsites::kWorkbenchCount);
    gfx::text::printShadow(8, 157, benches > 0 ? rgba(255, 190, 110) : kWhite, "%s", line);

    if (m_zMessageTime > 0.0f && m_zMessage != T::None) {
        const char* format = tr(m_zMessage);
        if (strstr(format, "%s")) snprintf(line, sizeof(line), format, m_zMessageName);
        else if (strstr(format, "%d")) snprintf(line, sizeof(line), format, m_zMessageNumber);
        else if (m_zMessageName[0]) snprintf(line, sizeof(line), "%s: %s", m_zMessageName, format);
        else snprintf(line, sizeof(line), "%s", format);
        printCenteredShadow(97, m_zMessageColor, line);
    }
}

const char* App::zombieInteractionText(const game::Interactable& it, char* text, size_t size) const {
    // Texto de CUADRADO junto a un superviviente, una pieza fantasma o un centro de crafteo.
    switch (it.kind) {
        case game::Interactable::Kind::Survivor: {
            const int needs = m_match.survivorNeeds(it.index);
            const T need = needs == 3 ? T::ZNeedsBoth : (needs == 1 ? T::ZNeedsWeapon : T::ZNeedsHeal);
            const game::Inventory& inv = m_match.inventory();
            const game::ItemType held = inv.pickaxe() ? game::ItemType::None : inv.slot(inv.selected()).type;
            const game::ItemDef& def = game::itemDef(held);
            const bool fits = ((needs & 1) && game::isWeapon(held)) ||
                              ((needs & 2) && def.kind == game::ItemKind::Consumable && (def.health > 0.0f || def.shield > 0.0f));
            if (fits) {
                const char* name = game::isWeapon(held) ? inv.slot(inv.selected()).weapon.config().name : game::itemName(held);
                snprintf(text, size, "%s %s: %s", tr(T::HintGive), m_match.name(it.index + 1), name);
            } else {
                snprintf(text, size, "%s: %s", m_match.name(it.index + 1), tr(need));
            }
            return text;
        }
        case game::Interactable::Kind::BlueprintPiece: {
            const game::Materials& m = m_match.materials();
            const int cost = m_matchConfig.zombie.pieceCost;
            const bool enough = m.get(world::Resource::Wood) >= cost || m.get(world::Resource::Stone) >= cost ||
                                m.get(world::Resource::Metal) >= cost || m_match.cheats().infiniteMaterials;
            if (enough) snprintf(text, size, tr(T::ZPieceInfo), cost);
            else snprintf(text, size, "%s", tr(T::ZPieceNoMats));
            return text;
        }
        case game::Interactable::Kind::Container:
            snprintf(text, size, "%s: %s", tr(T::CraftTitle), tr(T::HintCraft));
            return text;
        default:
            return nullptr;
    }
}

void App::drawZombieMarkers(float ox, float oy, float k, float x0, float y0, float x1, float y1, bool big) const {
    auto at = [&](const Vec3& p, float* sx, float* sy) {
        *sx = ox + p.x * k;
        *sy = oy + p.z * k;
        return *sx >= x0 && *sx < x1 - 1 && *sy >= y0 && *sy < y1 - 1;
    };
    float sx, sy;
    const float s = big ? 2.0f : 1.0f;
    // La base: recuadro verde.
    const Vec3 base = world::zsites::kBaseCenter;
    if (at(base, &sx, &sy)) {
        const float h = 6.0f * k;
        ui::frame(sx - h, sy - h, 2 * h, 2 * h, 1, kZombieGreen);
        gfx::fillRect(sx - 1 * s, sy - 1 * s, 3 * s, 3 * s, kZombieGreen);
    }
    // Centros de crafteo descubiertos (naranja) y supervivientes (amarillo esperando, verde
    // rescatados).
    for (const game::Container& c : m_match.containers()) {
        if (c.kind != game::ContainerKind::Workbench || !c.discovered || !at(c.position, &sx, &sy)) continue;
        gfx::fillRect(sx - 2 * s, sy - 2 * s, 5 * s, 5 * s, rgba(30, 20, 10));
        gfx::fillRect(sx - 1 * s, sy - 1 * s, 3 * s, 3 * s, rgba(255, 150, 60));
    }
    for (int i = 0; i < m_match.botCount(); ++i) {
        const game::Bot& b = m_match.bot(i);
        if (b.kind != game::BotKind::Survivor || !b.discovered || b.state == game::BotState::Eliminated) continue;
        if (!at(renderState(i + 1).position, &sx, &sy)) continue;
        gfx::fillRect(sx - 1 * s, sy - 1 * s, 3 * s, 3 * s, b.rescued ? kZombieGreen : kYellow);
    }
    // Planos encontrados: sus piezas por construir en azul (sobre la base).
    if (big) {
        for (int bp = 0; bp < game::kBlueprintCount; ++bp) {
            if (!m_match.blueprintFound(bp)) continue;
            for (int p = 0; p < game::blueprint(bp).pieceCount; ++p) {
                if (m_match.pieceBuilt(bp, p)) continue;
                const world::Aabb box = m_match.blueprintPiece(bp, p).box;
                if (at((box.min + box.max) * 0.5f, &sx, &sy)) gfx::fillRect(sx - 1, sy - 1, 3, 3, rgba(110, 180, 255));
            }
        }
    }
}

// --- Minimapa en ruinas, vineta y cielo ----------------------------------------------------------

void App::buildZombieMinimap() {
    // El mismo mapa con los colores del modo zombi: hierba seca, agua turbia, todo mas oscuro.
    if (!m_minimap.pixels || !gfx::createTexture(m_minimapZombie, m_minimap.width, m_minimap.height, GU_PSM_5551)) return;
    const u16* src = static_cast<const u16*>(m_minimap.pixels);
    u16* dst = static_cast<u16*>(m_minimapZombie.pixels);
    for (int v = 0; v < m_minimap.height; ++v) {
        for (int u = 0; u < m_minimap.width; ++u) {
            const u16 c = src[v * m_minimap.stride + u];
            const float r = static_cast<float>((c & 31) << 3), g = static_cast<float>(((c >> 5) & 31) << 3);
            const float b = static_cast<float>(((c >> 10) & 31) << 3);
            const float grey = (r + g + b) / 3.0f;
            // Mitad gris, tirando a ocre; el agua (azul), a verde oscuro.
            float nr = (r * 0.4f + grey * 0.6f) * 0.75f + 10.0f, ng = (g * 0.4f + grey * 0.6f) * 0.72f + 6.0f;
            float nb = (b * 0.3f + grey * 0.5f) * 0.6f;
            if (b > r + 30.0f && b > g) {
                nr = 56.0f;
                ng = 70.0f;
                nb = 50.0f;
            }
            dst[v * m_minimapZombie.stride + u] = gfx::pack5551(static_cast<u32>(fminf(nr, 255.0f)), static_cast<u32>(fminf(ng, 255.0f)),
                                                                static_cast<u32>(fminf(nb, 255.0f)));
        }
    }
    gfx::uploadTexture(m_minimapZombie);
    // Vineta: transparente en el centro y opaca en las esquinas (se tine al dibujarla).
    if (!gfx::createTexture(m_vignette, 64, 64, GU_PSM_8888)) return;
    u32* px = static_cast<u32*>(m_vignette.pixels);
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            const float dx = (x + 0.5f - 32.0f) / 32.0f, dy = (y + 0.5f - 32.0f) / 32.0f;
            const float d = sqrtf(dx * dx * 0.8f + dy * dy);
            const float a = math3d::clampf((d - 0.45f) / 0.75f, 0.0f, 1.0f);
            px[y * m_vignette.stride + x] = rgba(255, 255, 255, static_cast<u32>(a * a * 255.0f));
        }
    }
    gfx::uploadTexture(m_vignette);
}

void App::buildSky() {
    // Cupula del cielo (v0.14): del color de la niebla en el horizonte (transparente: se ve el
    // color de limpieza, que es el de la niebla) a azul arriba, con nubes y el sol. Se dibuja
    // la primera, sin niebla, centrada en la camara. La del modo zombi, encapotada.
    constexpr int kSides = 20, kRings = 6;
    for (int theme = 0; theme < 2; ++theme) {
        std::vector<gfx::Vertex> v;
        std::vector<u16> idx;
        const float radius = 110.0f;
        auto cloud = [](float a, float e, int seed) {
            const float n = sinf(a * 3.0f + seed) * cosf(e * 9.0f + a * 2.0f) + 0.6f * sinf(a * 7.0f - e * 5.0f + seed * 2.0f);
            return math3d::clampf((n - 0.35f) * 1.6f, 0.0f, 1.0f);
        };
        for (int r = 0; r <= kRings; ++r) {
            const float e = (static_cast<float>(r) / kRings) * 1.5f;  // elevacion 0 .. ~86 grados
            const float t = static_cast<float>(r) / kRings;
            for (int i = 0; i <= kSides; ++i) {
                const float a = 2.0f * math3d::kPi * i / kSides;
                const Vec3 p{cosf(a) * cosf(e) * radius, sinf(e) * radius * 0.6f - 4.0f, sinf(a) * cosf(e) * radius};
                u32 color;
                if (theme == 0) {
                    const float c = r > 0 && r < kRings ? cloud(a, e, 3) : 0.0f;
                    const float rr = 140.0f + (64.0f - 140.0f) * t, gg = 194.0f + (132.0f - 194.0f) * t, bb = 216.0f + (214.0f - 216.0f) * t;
                    color = rgba(static_cast<u32>(rr + (245.0f - rr) * c), static_cast<u32>(gg + (248.0f - gg) * c),
                                 static_cast<u32>(bb + (252.0f - bb) * c), static_cast<u32>(fminf(255.0f, 255.0f * t * 2.2f)));
                } else {
                    const float c = r > 0 && r < kRings ? cloud(a, e, 11) : 0.0f;
                    const float base = 100.0f - 30.0f * t;
                    color = rgba(static_cast<u32>(base + 14.0f * c), static_cast<u32>(base + 10.0f + 12.0f * c),
                                 static_cast<u32>(base - 2.0f + 8.0f * c), static_cast<u32>(fminf(255.0f, 255.0f * t * 1.8f)));
                }
                v.push_back({0.0f, 0.0f, color, p.x, p.y, p.z});
            }
        }
        for (int r = 0; r < kRings; ++r) {
            for (int i = 0; i < kSides; ++i) {
                const u16 a = static_cast<u16>(r * (kSides + 1) + i), b = static_cast<u16>(a + kSides + 1);
                idx.insert(idx.end(), {a, static_cast<u16>(a + 1), static_cast<u16>(b + 1), a, static_cast<u16>(b + 1), b});
            }
        }
        if (theme == 0) {
            // Sol: un disco blanco amarillento en la direccion de la luz horneada.
            const Vec3 sun = math3d::normalize({0.35f, 0.62f, -0.40f}) * (radius - 4.0f);
            const Vec3 right = math3d::normalize(math3d::cross(sun, {0.0f, 1.0f, 0.0f}));
            const Vec3 up = math3d::normalize(math3d::cross(right, sun));
            const u16 center = static_cast<u16>(v.size());
            v.push_back({0.0f, 0.0f, rgba(255, 252, 220), sun.x, sun.y, sun.z});
            for (int i = 0; i <= 10; ++i) {
                const float a = 2.0f * math3d::kPi * i / 10;
                const Vec3 p = sun + (right * cosf(a) + up * sinf(a)) * 5.0f;
                v.push_back({0.0f, 0.0f, rgba(255, 244, 190, 220), p.x, p.y, p.z});
            }
            for (int i = 0; i < 10; ++i) idx.insert(idx.end(), {center, static_cast<u16>(center + 1 + i), static_cast<u16>(center + 2 + i)});
        }
        gfx::Mesh& mesh = theme == 0 ? m_sky : m_skyZombie;
        if (!gfx::createMesh(mesh, v.data(), static_cast<int>(v.size()), idx.data(), static_cast<int>(idx.size()))) return;
    }
    m_skyReady = true;
}

void App::renderSky(const math3d::Mat4& /*view*/) const {
    if (!m_skyReady || !m_fog) return;
    const Vec3 eye = cameraEye();
    gfx::setFog(false);
    gfx::setDepthWrite(false);
    gfx::setBlend(true);
    gfx::bindTexture(nullptr);
    gfx::setModelMatrix(math3d::Mat4::translation({eye.x, eye.y, eye.z}));
    gfx::drawMesh(m_match.zombieMode() ? m_skyZombie : m_sky);
    gfx::setBlend(false);
    gfx::setDepthWrite(true);
    gfx::setFog(true);
    gfx::setModelMatrix(math3d::Mat4::identity());
}

// --- Resultados ---------------------------------------------------------------------------------

void App::recordZombieMatch() {
    // Noches sobrevividas (record), y pavos por noche y por plano completado.
    const int nights = m_match.nightsSurvived();
    if (nights > m_profile.zombieBestNights) addStat(Stat::ZombieNights, nights - m_profile.zombieBestNights);
    m_matchVbucks = nights * kVbucksPerNight + m_match.blueprintsDone() * kVbucksPerBlueprint;
    m_profile.vbucks += m_matchVbucks;
    platform::log::write("modo zombi: %d noches, %d zombis abatidos, %d rescatados, %d planos, %d fabricados, +%d pavos",
                         nights, m_match.zombieKills(), m_match.survivorsRescued(), m_match.blueprintsDone(),
                         m_match.itemsCrafted(), m_matchVbucks);
}

void App::renderZombieResults() const {
    gfx::fillRect(0, 0, gfx::kScreenWidth, gfx::kScreenHeight, rgba(0, 0, 0, 120));
    gfx::fillRect(0, 34, gfx::kScreenWidth, 64, rgba(40, 60, 20, 210));
    gfx::fillRect(84, 112, 312, 70, kPanel);
    gfx::fillRect(84 + 156, 118, 1, 58, rgba(255, 255, 255, 60));
    const int nights = m_match.nightsSurvived();
    int w = bigNumberWidth(nights, 4.0f);
    drawBigNumber(162 - w * 0.5f, 140, nights, 4.0f, config::kColorGold);
    w = bigNumberWidth(m_match.zombieKills(), 4.0f);
    drawBigNumber(318 - w * 0.5f, 140, m_match.zombieKills(), 4.0f, kWhite);
    if (m_matchVbucks > 0 && m_atlas.ready()) {
        char line[48];
        snprintf(line, sizeof(line), tr(T::ResultVbucks), m_matchVbucks);
        m_atlas.draw(atlas::kPavos, 240 - gfx::text::width(line) / 2 - 17, 205);
    }
}

void App::overlayZombieResults() const {
    char line[80];
    printCentered(48, kZombieGreen, tr(T::ModeZombies));
    printCenteredShadow(66, kErrorRed, tr(T::ResultZDied));
    snprintf(line, sizeof(line), tr(T::ResultZDayN), m_match.day());
    printCenteredShadow(80, kLightGrey, line);
    printCenteredAt(162, 118, kLightGrey, tr(T::ResultZDays));
    printCenteredAt(318, 118, kLightGrey, tr(T::ResultZKills));
    snprintf(line, sizeof(line), tr(T::ResultZRescued), m_match.survivorsRescued(), m_match.blueprintsDone(), m_match.itemsCrafted());
    printCenteredShadow(188, kWhite, line);
    const int best = m_match.nightsSurvived() > m_matchBestNights ? m_match.nightsSurvived() : m_matchBestNights;
    snprintf(line, sizeof(line), tr(T::ResultZBest), best);
    printCenteredShadow(198, m_match.nightsSurvived() > m_matchBestNights ? config::kColorGold : kDimGrey, line);
    if (m_matchVbucks > 0) {
        snprintf(line, sizeof(line), tr(T::ResultVbucks), m_matchVbucks);
        gfx::text::printShadow(240 - gfx::text::width(line) / 2, 209, kYellow, "%s", line);
    }
}

}  // namespace app
