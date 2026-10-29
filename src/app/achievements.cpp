#include "app/achievements.h"

namespace app {
namespace {

// Solo mecanicas que existen en el juego. Las metas son acumuladas entre partidas salvo
// MostKills (en una sola partida) y BestPlaceTop5 (cuantas veces se ha quedado entre los 5
// primeros).
const AchievementDef kAchievements[] = {
    {"primera_victoria", i18n::T::AchFirstWin, i18n::T::AchFirstWinDesc, Stat::Wins, 1},
    {"cinco_victorias", i18n::T::AchFiveWins, i18n::T::AchFiveWinsDesc, Stat::Wins, 5},
    {"primera_baja", i18n::T::AchFirstKill, i18n::T::AchFirstKillDesc, Stat::Kills, 1},
    {"cazador", i18n::T::AchHunter, i18n::T::AchHunterDesc, Stat::Kills, 25},
    {"exterminador", i18n::T::AchExterminator, i18n::T::AchExterminatorDesc, Stat::Kills, 100},
    {"punteria", i18n::T::AchSharpshooter, i18n::T::AchSharpshooterDesc, Stat::Headshots, 10},
    {"racha", i18n::T::AchStreak, i18n::T::AchStreakDesc, Stat::MostKills, 5},
    {"saqueador", i18n::T::AchLooter, i18n::T::AchLooterDesc, Stat::ChestsOpened, 10},
    {"buscatesoros", i18n::T::AchTreasure, i18n::T::AchTreasureDesc, Stat::ChestsOpened, 50},
    {"llama", i18n::T::AchLlama, i18n::T::AchLlamaDesc, Stat::LlamasOpened, 1},
    {"suministro", i18n::T::AchAirdrop, i18n::T::AchAirdropDesc, Stat::AirdropsOpened, 1},
    {"arquitecto", i18n::T::AchArchitect, i18n::T::AchArchitectDesc, Stat::PiecesBuilt, 100},
    {"lenador", i18n::T::AchLumberjack, i18n::T::AchLumberjackDesc, Stat::MaterialsHarvested, 500},
    {"tramposo", i18n::T::AchTrapper, i18n::T::AchTrapperDesc, Stat::TrapKills, 1},
    {"demoledor", i18n::T::AchDemolition, i18n::T::AchDemolitionDesc, Stat::ExplosiveKills, 1},
    {"top5", i18n::T::AchTopFive, i18n::T::AchTopFiveDesc, Stat::BestPlaceTop5, 1},
    {"veterano", i18n::T::AchVeteran, i18n::T::AchVeteranDesc, Stat::Matches, 20},
    {"medico", i18n::T::AchMedic, i18n::T::AchMedicDesc, Stat::ItemsUsed, 25},
    // v0.14: modo zombi.
    {"zombi_primera_noche", i18n::T::AchZFirstNight, i18n::T::AchZFirstNightDesc, Stat::ZombieNights, 1},
    {"zombi_tres_noches", i18n::T::AchZThreeNights, i18n::T::AchZThreeNightsDesc, Stat::ZombieNights, 3},
    {"zombi_semana", i18n::T::AchZWeek, i18n::T::AchZWeekDesc, Stat::ZombieNights, 7},
    {"cazazombis", i18n::T::AchZHunter, i18n::T::AchZHunterDesc, Stat::ZombieKills, 100},
    {"rescatador", i18n::T::AchZRescuer, i18n::T::AchZRescuerDesc, Stat::SurvivorsRescued, 5},
    {"fortaleza", i18n::T::AchZFortress, i18n::T::AchZFortressDesc, Stat::BlueprintsDone, 4},
    {"artesano", i18n::T::AchZCrafter, i18n::T::AchZCrafterDesc, Stat::ItemsCrafted, 10},
};
static_assert(sizeof(kAchievements) / sizeof(kAchievements[0]) == kAchievementCount, "falta un logro");

}  // namespace

const AchievementDef& achievementDef(AchievementId id) {
    const int i = static_cast<int>(id);
    return kAchievements[i >= 0 && i < kAchievementCount ? i : 0];
}

}  // namespace app
