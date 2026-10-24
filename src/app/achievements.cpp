#include "app/achievements.h"

namespace app {
namespace {

// Solo mecanicas que existen en el juego. Las metas son acumuladas entre partidas salvo
// MostKills (en una sola partida) y BestPlaceTop5 (cuantas veces se ha quedado entre los 5
// primeros).
const AchievementDef kAchievements[] = {
    {"primera_victoria", "Victoria Magistral", "Gana una partida", Stat::Wins, 1},
    {"cinco_victorias", "Imparable", "Gana 5 partidas", Stat::Wins, 5},
    {"primera_baja", "Primera sangre", "Elimina a un rival", Stat::Kills, 1},
    {"cazador", "Cazador", "Elimina a 25 rivales", Stat::Kills, 25},
    {"exterminador", "Exterminador", "Elimina a 100 rivales", Stat::Kills, 100},
    {"punteria", "Punteria", "Acierta 10 disparos a la cabeza", Stat::Headshots, 10},
    {"racha", "En racha", "Elimina a 5 rivales en una partida", Stat::MostKills, 5},
    {"saqueador", "Saqueador", "Abre 10 cofres", Stat::ChestsOpened, 10},
    {"buscatesoros", "Buscatesoros", "Abre 50 cofres", Stat::ChestsOpened, 50},
    {"llama", "Llama a la vista", "Abre o rompe una llama de suministros", Stat::LlamasOpened, 1},
    {"suministro", "Correo aereo", "Abre un suministro aereo", Stat::AirdropsOpened, 1},
    {"arquitecto", "Arquitecto", "Construye 100 piezas", Stat::PiecesBuilt, 100},
    {"lenador", "Lenador", "Consigue 500 materiales con el pico", Stat::MaterialsHarvested, 500},
    {"tramposo", "Trampero", "Elimina a un rival con una trampa", Stat::TrapKills, 1},
    {"demoledor", "Demoledor", "Elimina a un rival con un explosivo", Stat::ExplosiveKills, 1},
    {"top5", "Entre los mejores", "Termina una partida entre los 5 primeros", Stat::BestPlaceTop5, 1},
    {"veterano", "Veterano", "Juega 20 partidas", Stat::Matches, 20},
    {"medico", "Botiquin andante", "Usa 25 curativos", Stat::ItemsUsed, 25},
};
static_assert(sizeof(kAchievements) / sizeof(kAchievements[0]) == kAchievementCount, "falta un logro");

}  // namespace

const AchievementDef& achievementDef(AchievementId id) {
    const int i = static_cast<int>(id);
    return kAchievements[i >= 0 && i < kAchievementCount ? i : 0];
}

}  // namespace app
