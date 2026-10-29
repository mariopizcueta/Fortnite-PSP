// Logros (parche, fase 8; docs/LOGROS.md).
//
// Cada logro es un contador del perfil (Profile::stats) y una meta: se desbloquea cuando el
// contador llega a la meta, por sucesos reales de la partida (eliminaciones, cofres,
// construcciones...). El estado (desbloqueado y la fecha) se guarda en perfil.txt con el
// resto del perfil. Un logro desbloqueado no se vuelve a registrar.
//
// Para anadir uno: un valor en AchievementId, su fila en kAchievements (achievements.cpp)
// y, si hace falta un contador nuevo, un valor en Stat que la aplicacion incremente.
#pragma once

#include <psptypes.h>

#include "core/i18n.h"

namespace app {

// Contadores que se acumulan entre partidas.
enum class Stat : u8 {
    Matches, Wins, Kills, Deaths, Headshots, MostKills, BestPlaceTop5, Seconds,
    ChestsOpened, AirdropsOpened, LlamasOpened, PiecesBuilt, MaterialsHarvested, TrapKills,
    ExplosiveKills, ItemsUsed,
    // v0.14: modo zombi (ZombieNights es el record de noches seguidas, como MostKills).
    ZombieNights, ZombieKills, SurvivorsRescued, BlueprintsDone, ItemsCrafted,
    Count
};

enum class AchievementId : u8 {
    FirstWin, FiveWins, FirstKill, Hunter, Exterminator, Sharpshooter, KillStreak, Looter,
    TreasureHunter, Llama, Airdrop, Architect, Lumberjack, Trapper, Demolition, TopFive,
    Veteran, Medic,
    // v0.14: modo zombi.
    ZFirstNight, ZThreeNights, ZWeek, ZHunter, ZRescuer, ZFortress, ZCrafter,
    Count
};

struct AchievementDef {
    const char* key;         // clave en perfil.txt ("logro_<key>")
    i18n::T name;            // textos (core/i18n_texts.h)
    i18n::T description;
    Stat stat;
    int goal;
};

const AchievementDef& achievementDef(AchievementId id);
constexpr int kAchievementCount = static_cast<int>(AchievementId::Count);

// Estado de un logro: desbloqueado y la fecha (AAAAMMDD; 0 si no se sabe), y si ya se ha
// recogido su recompensa de pavos (v0.13; los logros de antes se pueden recoger).
struct AchievementState {
    bool unlocked = false;
    int date = 0;
    bool claimed = false;
};

// Pavos (v0.13; docs/TIENDA.md): por cada victoria magistral y por cada logro completado.
constexpr int kVbucksPerWin = 50;
constexpr int kVbucksPerAchievement = 50;
// v0.14: en el modo zombi, por cada noche sobrevivida (y por cada plano completado).
constexpr int kVbucksPerNight = 25;
constexpr int kVbucksPerBlueprint = 15;

}  // namespace app
