// Textos visibles del juego en espanol e ingles (parche, fase 8). Ver core/i18n.h.
//
// Cada fila: X(identificador, "espanol", "ingles"). La fuente es la 8x8 de pspdebug (ASCII):
// sin tildes ni enes en ninguno de los dos idiomas. Los textos con %d / %s son formatos
// de printf: los dos idiomas deben tener los mismos, en el mismo orden.
//
// Para anadir un idioma: otra columna aqui, otro valor en i18n::Lang y su tabla en i18n.cpp.
#pragma once

#define FNPSP_TEXTS(X)                                                                                        \
    X(None, "", "")                                                                                           \
    /* --- Inicio de sesion, nombre, carga ---------------------------------------------------------- */      \
    X(BootLoading, "Cargando...", "Loading...")                                                               \
    X(BootBuilding, "Construyendo el escenario...", "Building the map...")                                    \
    X(LoginConnecting, "Conectando...", "Connecting...")                                                      \
    X(LoginChecking, "Comprobando la version...", "Checking version...")                                      \
    X(LoginProfile, "Cargando el perfil...", "Loading profile...")                                            \
    X(LoginPress, "Pulsa ", "Press ")                                                                         \
    X(LoginToStart, "para iniciar", "to start")                                                               \
    X(NickTitle, "ELIGE TU NOMBRE DE JUGADOR", "CHOOSE YOUR PLAYER NAME")                                     \
    X(NickErase, "BORRAR", "DELETE")                                                                          \
    X(NickAccept, "ACEPTAR", "ACCEPT")                                                                        \
    X(NickSaved, "Se guarda tu nombre y tus estadisticas", "Your name and stats are saved")                   \
    X(HintType, "escribir", "type")                                                                           \
    X(HintErase, "borrar", "delete")                                                                          \
    X(HintCase, "mayus/minus", "upper/lower")                                                                 \
    X(HintAccept, "aceptar", "accept")                                                                        \
    X(LoadingMap, "Polvorin Polvoriento - %d bots", "Dusty Depot - %d bots")                                  \
    X(TipBuild, "construir: una pared te cubre al instante.", "build: a wall covers you instantly.")          \
    X(TipAim, "mantenido: fija al enemigo mas cercano.", "hold: lock on to the nearest enemy.")               \
    X(TipZone, "La zona se cierra: no te quedes fuera.", "The storm is closing: stay inside the zone.")       \
    X(TipReload, "recarga: el arma vuelve a estar lista.", "reload: your weapon is ready again.")             \
    X(TipWalls, "Las paredes paran las balas, pero se pueden romper.", "Walls stop bullets, but they break.") \
    X(TipPickaxe, "saca el pico: los arboles dan madera.", "pickaxe: trees give you wood.")                   \
    X(TipTraps, "Las trampas van en la cuarta casilla de construir.", "Traps are the fourth build slot.")     \
    /* --- Lobby ------------------------------------------------------------------------------------ */      \
    X(HintSelect, "elegir", "select")                                                                         \
    X(HintRotate, "girar", "rotate")                                                                          \
    X(HintSections, "secciones", "tabs")                                                                      \
    X(HintSettings, "ajustes", "settings")                                                                    \
    X(HintScroll, "recorrer", "scroll")                                                                       \
    X(HintOpen, "abrir", "open")                                                                              \
    X(HintEquip, "equipar", "equip")                                                                          \
    X(HintBack, "volver", "back")                                                                             \
    X(HintExit, "salir", "exit")                                                                              \
    X(HintCancel, "cancelar", "cancel")                                                                       \
    X(StatWins, "Victorias", "Wins")                                                                          \
    X(StatMatches, "Partidas", "Matches")                                                                     \
    X(StatKills, "Bajas", "Kills")                                                                            \
    X(StatHeadshots, "A la cabeza", "Headshots")                                                              \
    X(StatMostKills, "Max. bajas", "Most kills")                                                              \
    X(StatBestPlace, "Mejor puesto", "Best place")                                                            \
    X(BotsSelector, "%d BOTS", "%d BOTS")                                                       \
    X(BotsWarning, "(menos de 30: sin pavos ni logros)", "(under 30: no V-Bucks or achievements)")                                           \
    X(NoMapError, "ERROR: sin escenario", "ERROR: no map")                                                    \
    X(ExitQuestion, "Salir de FortnitePSP?", "Quit FortnitePSP?")                                             \
    X(AchievementsTitle, "LOGROS  %d / %d", "ACHIEVEMENTS  %d / %d")                                          \
    X(AchievementDone, "CONSEGUIDO", "UNLOCKED")                                                              \
    X(AchievementUnlocked, "LOGRO DESBLOQUEADO", "ACHIEVEMENT UNLOCKED")                                      \
    X(LockerGear, "EQUIPO", "GEAR")                                                                           \
    X(LockerDefault, "Por defecto", "Default")                                                                \
    X(LockerEmptyLine1, "Aun no hay objetos aqui.", "Nothing here yet.")                                      \
    X(LockerEmptyLine2, "Llegaran en una actualizacion.", "Coming in a future update.")                       \
    X(LockerEmptyList1, "Vacio: aun no hay objetos.", "Empty: nothing here yet.")                             \
    X(LockerEmptyList2, "Llegaran en una", "Coming in a")                                                     \
    X(LockerEmptyList3, "actualizacion.", "future update.")                                                   \
    X(SlotOutfit, "Traje", "Outfit")                                                                          \
    X(SlotBackBling, "Mochila", "Backpack")                                                                 \
    X(SlotPickaxe, "Pico", "Pickaxe")                                                                         \
    X(SlotGlider, "Ala delta", "Glider")                                                                      \
    X(SlotEmote, "Baile", "Emote")                                                                            \
    X(SlotWrap, "Envoltorio", "Wrap")                                                                         \
    X(CosmeticRecruit, "Recluta", "Recruit")                                                                  \
    X(CosmeticPickaxe, "Pico por defecto", "Default pickaxe")                                                 \
    X(CosmeticGlider, "Ala delta por defecto", "Default glider")                                              \
    X(CosmeticEmote, "Baile por defecto", "Default dance")                                                    \
    /* --- Logros ------------------------------------------------------------------------------------ */     \
    X(AchFirstWin, "Victoria Magistral", "Victory Royale")                                                    \
    X(AchFirstWinDesc, "Gana una partida", "Win a match")                                                     \
    X(AchFiveWins, "Imparable", "Unstoppable")                                                                \
    X(AchFiveWinsDesc, "Gana 5 partidas", "Win 5 matches")                                                    \
    X(AchFirstKill, "Primera sangre", "First blood")                                                          \
    X(AchFirstKillDesc, "Elimina a un rival", "Eliminate an opponent")                                        \
    X(AchHunter, "Cazador", "Hunter")                                                                         \
    X(AchHunterDesc, "Elimina a 25 rivales", "Eliminate 25 opponents")                                        \
    X(AchExterminator, "Exterminador", "Exterminator")                                                        \
    X(AchExterminatorDesc, "Elimina a 100 rivales", "Eliminate 100 opponents")                                \
    X(AchSharpshooter, "Punteria", "Sharpshooter")                                                            \
    X(AchSharpshooterDesc, "Acierta 10 disparos a la cabeza", "Land 10 headshots")                            \
    X(AchStreak, "En racha", "On a roll")                                                                     \
    X(AchStreakDesc, "5 bajas en una sola partida", "5 eliminations in one match")              \
    X(AchLooter, "Saqueador", "Looter")                                                                       \
    X(AchLooterDesc, "Abre 10 cofres", "Open 10 chests")                                                      \
    X(AchTreasure, "Buscatesoros", "Treasure hunter")                                                         \
    X(AchTreasureDesc, "Abre 50 cofres", "Open 50 chests")                                                    \
    X(AchLlama, "Llama a la vista", "Llama spotted")                                                          \
    X(AchLlamaDesc, "Abre o rompe una llama", "Open or break a supply llama")                  \
    X(AchAirdrop, "Correo aereo", "Air mail")                                                                 \
    X(AchAirdropDesc, "Abre un suministro aereo", "Open a supply drop")                                       \
    X(AchArchitect, "Arquitecto", "Architect")                                                                \
    X(AchArchitectDesc, "Construye 100 piezas", "Build 100 pieces")                                           \
    X(AchLumberjack, "Lenador", "Lumberjack")                                                                 \
    X(AchLumberjackDesc, "500 materiales con el pico", "Harvest 500 materials")                      \
    X(AchTrapper, "Trampero", "Trapper")                                                                      \
    X(AchTrapperDesc, "Elimina a un rival con una trampa", "Eliminate an opponent with a trap")               \
    X(AchDemolition, "Demoledor", "Demolition")                                                               \
    X(AchDemolitionDesc, "Elimina con un explosivo", "Eliminate with an explosive")      \
    X(AchTopFive, "Entre los mejores", "Top 5")                                                               \
    X(AchTopFiveDesc, "Acaba entre los 5 primeros", "Finish a match in the top 5")              \
    X(AchVeteran, "Veterano", "Veteran")                                                                      \
    X(AchVeteranDesc, "Juega 20 partidas", "Play 20 matches")                                                 \
    X(AchMedic, "Botiquin andante", "Field medic")                                                            \
    X(AchMedicDesc, "Usa 25 curativos", "Use 25 healing items")                                               \
    /* --- Pausa y resultados ---------------------------------------------------------------------- */      \
    X(Pause, "PAUSA", "PAUSED")                                                                               \
    X(PauseContinue, "CONTINUAR", "RESUME")                                                                   \
    X(PauseLobby, "VOLVER AL LOBBY", "BACK TO LOBBY")                                                         \
    X(HintContinue, "continuar", "resume")                                                                    \
    X(HintLobby, "lobby", "lobby")                                                                            \
    X(HintAnotherMatch, "otra partida", "play again")                                                         \
    X(HintSpectate, "seguir mirando", "spectate")                                                             \
    X(ResultPlace, "PUESTO", "PLACE")                                                                         \
    X(ResultKills, "BAJAS", "KILLS")                                                                          \
    X(ResultTime, "TIEMPO", "TIME")                                                                           \
    X(ResultOfPlayers, "de %d jugadores", "of %d players")                                                    \
    X(ResultSurvived, "Has sobrevivido a todos!", "You outlasted everyone!")                                  \
    X(ResultKilledBy, "Te ha eliminado %s", "Eliminated by %s")                                               \
    X(ResultKilledByStorm, "Te ha eliminado la tormenta", "Eliminated by the storm")                          \
    X(ResultFell, "Has caido", "You were eliminated")                                                         \
    X(ResultWinner, "Ganador: %s", "Winner: %s")                                                              \
    X(ResultNobody, "nadie", "nobody")                                                                        \
    X(ResultAlive, "Quedan %d con vida", "%d players left")                                                   \
    /* --- HUD -------------------------------------------------------------------------------------- */      \
    X(OpenChest, "Abrir cofre", "Open chest")                                                                 \
    X(OpenAmmoBox, "Abrir caja de municion", "Open ammo box")                                                 \
    X(OpenAirdrop, "Abrir suministro", "Open supply drop")                                                    \
    X(OpenLlama, "Abrir llama", "Open llama")                                                                 \
    X(SwapFor, "Cambiar por %s (%s)", "Swap for %s (%s)")                                                     \
    X(PickUpCount, "Recoger %s x%d", "Pick up %s x%d")                                                        \
    X(PickUpRarity, "Recoger %s (%s)", "Pick up %s (%s)")                                                     \
    X(SwapChoose, "Recoger %s y soltar %s", "Take %s, drop %s")                                               \
    X(SwapEmptySlot, "(vacia)", "(empty)")                                                                    \
    X(HintSwap, "cambiar", "swap")                                                                            \
    X(BusJump, "Saltar del autobus", "Jump from the bus")                                                     \
    X(GliderOpen, "Abrir el ala delta", "Deploy glider")                                                      \
    X(AutoJump, "Salto automatico en %.0f s", "Auto jump in %.0f s")                                          \
    X(Altitude, "Altura %.0f m", "Altitude %.0f m")                                                           \
    X(StormClosingIn, "La tormenta se cierra:", "Storm closing:")                                             \
    X(StormNextIn, "Siguiente cierre en", "Next storm in")                                                    \
    X(MapAliveKills, "Vivos %d   Bajas %d", "Alive %d   Kills %d")                                             \
    X(MapClose, "SELECT: cerrar", "SELECT: close")                                                            \
    X(StormClosingNow, "La tormenta se esta cerrando!", "The storm is closing!")                              \
    X(AirdropIncoming, "Suministro aereo en camino!", "Supply drop incoming!")                                \
    X(BuildBanner, "%s de %s  -  %d/%d", "%s (%s)  -  %d/%d")                                                 \
    X(TrapBanner, "%s  -  te quedan %d", "%s  -  %d left")                                                    \
    X(MaterialWood, "madera", "wood")                                                                         \
    X(MaterialStone, "piedra", "stone")                                                                       \
    X(MaterialMetal, "metal", "metal")                                                                        \
    X(PieceWall, "Pared", "Wall")                                                                             \
    X(PieceFloor, "Suelo", "Floor")                                                                           \
    X(PieceRamp, "Rampa", "Ramp")                                                                             \
    X(SpectatingName, "ESPECTANDO A %s", "SPECTATING %s")                                                     \
    X(StormDamage, "TORMENTA: -%.0f DE VIDA POR SEGUNDO", "STORM: -%.0f HEALTH PER SECOND")                   \
    X(StormBack, "Vuelve a la zona segura", "Get back to the safe zone")                                      \
    X(StormEdge, "Limite del mapa: tormenta a %.0f m", "Map edge: storm %.0f m away")                         \
    X(FeedEliminated, "%s elimino a %s", "%s eliminated %s")                                                  \
    X(FeedStorm, "%s cayo en la tormenta", "%s died in the storm")                                            \
    X(FeedFell, "%s ha caido", "%s was eliminated")                                                           \
    X(Reloading, "RECARGANDO", "RELOADING")                                                                   \
    X(NoAmmo, "SIN BALAS", "NO AMMO")                                                                         \
    X(Using, "Usando %s  %.1f s", "Using %s  %.1f s")                                                         \
    X(HintReload, "recargar", "reload")                                                                       \
    X(HintPlace, "colocar", "place")                                                                          \
    X(HintMaterial, "material", "material")                                                                   \
    X(HintPlaceTrap, "colocar trampa", "place trap")                                                          \
    X(HintFreeCamera, "camara libre", "free camera")                                                          \
    X(HintNextHudPage, "siguiente pagina del HUD", "next HUD page")                                           \
    X(HintBackToLobby, "volver al lobby", "back to lobby")                                                    \
    X(PlayerDefaultName, "Jugador", "Player")                                                                 \
    X(BotName, "Bot %d", "Bot %d")                                                                            \
    /* --- Pagina de controles --------------------------------------------------------------------- */      \
    X(ControlsCombat, "COMBATE", "COMBAT")                                                                    \
    X(ControlsBuilding, "CONSTRUCCION", "BUILDING")                                                           \
    X(ControlsMatch, "PARTIDA", "MATCH")                                                                      \
    X(CtlMove, "moverse", "move")                                                                             \
    X(CtlJump, "saltar", "jump")                                                                              \
    X(CtlFire, "disparar / usar", "fire / use")                                                               \
    X(CtlReload, "recargar / recoger", "reload / pick up")                                                    \
    X(CtlAim, "apuntar / fijar", "aim / lock on")                                                             \
    X(CtlPickaxe, "pico", "pickaxe")                                                                          \
    X(CtlItem, "objeto", "item")                                                                              \
    X(CtlLook, "mira", "look")                                                                                \
    X(CtlCrouch, "agacharse", "crouch")                                                                       \
    X(CtlEmote, "bailar", "dance")                                                                            \
    X(CtlBuild, "entrar / salir", "enter / exit")                                                             \
    X(CtlPlacePiece, "colocar pieza", "place piece")                                                          \
    X(CtlPiece, "pieza / trampa", "piece / trap")                                                             \
    X(CtlMaterial, "material", "material")                                                                    \
    X(CtlPause, "pausa", "pause")                                                                             \
    X(CtlMap, "mapa", "map")                                                                                  \
    X(CtlHudPages, "paginas del HUD", "HUD pages")                                                            \
    X(CtlQuit, "1 s: salir del juego", "1 s: quit the game")                                                  \
    X(PadTest, "PRUEBA DEL MANDO", "CONTROLLER TEST")                                                         \
    X(PadPressed, "Pulsado: %s", "Pressed: %s")                                                               \
    X(PadNone, "(ninguno)", "(none)")                                                                         \
    X(PadAnalog, "ANALOGICO", "ANALOG")                                                                       \
    /* --- Pagina de depuracion -------------------------------------------------------------------- */      \
    X(DebugPlayer, "JUGADOR", "PLAYER")                                                                       \
    X(DebugPerformance, "RENDIMIENTO (%d bots)", "PERFORMANCE (%d bots)")                                     \
    X(DebugMatch, "PARTIDA %d", "MATCH %d")                                                                   \
    X(DebugBotHeader, "BOT ESTADO       VIDA OBJ", "BOT STATE        HP   TGT")                               \
    X(DebugMore, "... (%d mas)", "... (%d more)")                                                             \
    X(DebugAlive, "Vivos %d/%d  zona %2.0f m", "Alive %d/%d  zone %2.0f m")                                   \
    X(DebugPaths, "Caminos %d (fallos %d)", "Paths %d (failed %d)")                                           \
    X(DebugNodes, "Max nodos A* %d  atascos %d", "Max A* nodes %d  stuck %d")                                 \
    X(DebugShots, "Disparos %d imp %d pared %d", "Shots %d hit %d wall %d")                                   \
    X(DebugWalls, "Paredes +%d -%d rotas %d", "Walls +%d -%d broken %d")                                      \
    X(DebugAudio, "Audio: %d voces, %u perdidas", "Audio: %d voices, %u lost")                                \
    X(DebugNoAudio, "Audio: sin canal", "Audio: no channel")                                                  \
    X(DebugDecisions, "Decisiones %3d/s", "Decisions %3d/s")                                                  \
    X(BotExplore, "Explorar", "Explore")                                                                      \
    X(BotTravel, "Desplazarse", "Travel")                                                                     \
    X(BotSearch, "Buscar obj.", "Search")                                                                     \
    X(BotAttack, "Atacar", "Attack")                                                                          \
    X(BotLoot, "Botin", "Loot")                                                                               \
    X(BotHarvest, "Picar", "Harvest")                                                                         \
    X(BotFlee, "Huir", "Flee")                                                                                \
    X(BotEliminated, "Eliminado", "Eliminated")                                                               \
    X(JumpGrounded, "En suelo", "Grounded")                                                                   \
    X(JumpRising, "Saltando", "Jumping")                                                                      \
    X(JumpFalling, "Cayendo", "Falling")                                                                      \
    X(MoveIdle, "Quieto", "Idle")                                                                             \
    X(MoveWalk, "Andando", "Walking")                                                                         \
    X(MoveRun, "Corriendo", "Running")                                                                        \
    /* --- Objetos, armas y rarezas ---------------------------------------------------------------- */      \
    X(ItemAssaultRifle, "Fusil de asalto", "Assault Rifle")                                                   \
    X(ItemPumpShotgun, "Escopeta de corredera", "Pump Shotgun")                                               \
    X(ItemSuppressedSmg, "Subfusil con silenciador", "Suppressed SMG")                                        \
    X(ItemBoltSniper, "Francotirador de cerrojo", "Bolt-Action Sniper")                                       \
    X(ItemBandage, "Vendas", "Bandages")                                                                      \
    X(ItemMedkit, "Botiquin", "Med Kit")                                                                      \
    X(ItemSmallShield, "Minipocion", "Small Shield Potion")                                                   \
    X(ItemShieldPotion, "Pocion de escudo", "Shield Potion")                                                  \
    X(ItemBurstRifle, "Fusil de rafagas", "Burst Assault Rifle")                                              \
    X(ItemScopedRifle, "Fusil con mira", "Scoped Assault Rifle")                                              \
    X(ItemSmg, "Subfusil", "Submachine Gun")                                                                  \
    X(ItemTacticalShotgun, "Escopeta tactica", "Tactical Shotgun")                                            \
    X(ItemPistol, "Pistola", "Pistol")                                                                        \
    X(ItemRevolver, "Revolver", "Revolver")                                                                   \
    X(ItemSemiAutoSniper, "Francotirador semiautomatico", "Semi-Auto Sniper")                                 \
    X(ItemRocketLauncher, "Lanzacohetes", "Rocket Launcher")                                                  \
    X(ItemGrenadeLauncher, "Lanzagranadas", "Grenade Launcher")                                               \
    X(ItemGrenade, "Granada", "Grenade")                                                                      \
    X(ItemSpikeTrap, "Trampa de pinchos", "Spike Trap")                                                       \
    X(WeaponScar, "SCAR", "SCAR")                                                                             \
    X(WeaponShotgun, "Escopeta", "Shotgun")                                                                   \
    X(WeaponSniper, "Francotirador", "Sniper Rifle")                                                          \
    X(WeaponPickaxe, "Pico", "Pickaxe")                                                                       \
    X(RarityCommon, "Comun", "Common")                                                                        \
    X(RarityUncommon, "Poco comun", "Uncommon")                                                               \
    X(RarityRare, "Rara", "Rare")                                                                             \
    X(RarityEpic, "Epica", "Epic")                                                                            \
    X(RarityLegendary, "Legendaria", "Legendary")                                                             \
    X(AmmoLight, "Ligera", "Light")                                                                           \
    X(AmmoMedium, "Media", "Medium")                                                                          \
    X(AmmoHeavy, "Pesada", "Heavy")                                                                           \
    X(AmmoShells, "Cartuchos", "Shells")                                                                      \
    X(AmmoRockets, "Cohetes", "Rockets")                                                                      \
    X(UseFullHealth, "Vida al maximo", "Health is full")                                                      \
    X(UseFullShield, "Escudo al maximo", "Shield is full")                                                    \
    /* --- Construccion y trampas ------------------------------------------------------------------ */      \
    X(BuildLimit, "Limite de construcciones alcanzado", "Build limit reached")                                \
    X(BuildOverPlayer, "Estas en medio", "You are in the way")                                                \
    X(BuildOverBot, "Hay alguien en medio", "Someone is in the way")                                          \
    X(BuildOverlap, "Choca con otra estructura", "Blocked by another structure")                              \
    X(BuildBlocked, "Hay un obstaculo en medio", "Something is in the way")                                   \
    X(BuildOutOfBounds, "Fuera del mapa", "Out of bounds")                                                    \
    X(BuildNoMaterials, "Faltan materiales (10 por pieza)", "Not enough materials (10 each)")                 \
    X(TrapNoTraps, "No te quedan trampas", "You have no traps")                                               \
    X(TrapNoSurface, "Apunta a un suelo, una pared o un techo", "Aim at a floor, wall or ceiling")            \
    X(TrapRamp, "En una rampa no se puede", "Not on a ramp")                                                  \
    X(TrapTooSmall, "No cabe en esa superficie", "Surface too small")                                         \
    X(TrapOccupied, "Ya hay una trampa ahi", "There is a trap already")                                       \
    X(TrapObstructed, "Hay algo en medio", "Something is in the way")                                         \
    X(TrapLimit, "Limite de trampas alcanzado", "Trap limit reached")                                         \
    /* --- Ajustes --------------------------------------------------------------------------------- */      \
    X(SettingsTitle, "AJUSTES", "SETTINGS")                                                                   \
    X(SetGraphics, "GRAFICOS", "GRAPHICS")                                                                    \
    X(SetSensitivity, "SENSIBILIDAD", "SENSITIVITY")                                                          \
    X(SetControls, "CONTROLES", "CONTROLS")                                                                   \
    X(SetLanguage, "IDIOMA", "LANGUAGE")                                                                      \
    X(SetData, "DATOS", "DATA")                                                                               \
    X(SetDrawDistance, "Distancia de dibujado", "Draw distance")                                              \
    X(SetCharacterDetail, "Detalle de personajes", "Character detail")                                        \
    X(SetDamageNumbers, "Numeros de dano", "Damage numbers")                                                  \
    X(SetTracers, "Trazadoras de disparos", "Bullet tracers")                                                 \
    X(SetFps, "Contador de FPS", "FPS counter")                                                               \
    X(SetVsync, "Sincronia vertical (VSync)", "Vertical sync (VSync)")                                        \
    X(SetNear, "Cerca", "Near")                                                                               \
    X(SetMedium, "Media", "Medium")                                                                           \
    X(SetFar, "Lejos", "Far")                                                                                 \
    X(SetLow, "Bajo", "Low")                                                                                  \
    X(SetMid, "Medio", "Medium")                                                                              \
    X(SetHigh, "Alto", "High")                                                                                \
    X(SetOn, "Si", "On")                                                                                      \
    X(SetOff, "No", "Off")                                                                                    \
    X(SetCamera, "Camara", "Camera")                                                                          \
    X(SetScope, "Apuntado con mirilla", "Scoped aim")                                                         \
    X(SetDeadzone, "Zona muerta del stick", "Stick dead zone")                                                \
    X(SetInvertY, "Invertir eje vertical", "Invert vertical axis")                                            \
    X(SetRestoreControls, "Restaurar predeterminados", "Restore defaults")                                    \
    X(SetPressButton, "Pulsa el boton para: %s", "Press a button for: %s")                                    \
    X(SetPressCancel, "(START: cancelar)", "(START: cancel)")                                                 \
    X(SetFixedButtons, "Fijos: cruceta izq./dcha. objeto, SELECT mapa, START pausa", "Fixed: left/right item, SELECT map, START pause") \
    X(SetSpanish, "Espanol", "Espanol")                                                                       \
    X(SetEnglish, "English", "English")                                                                       \
    X(SetDeleteData, "Borrar datos del jugador", "Delete player data")                                        \
    X(SetDeleteInfo1, "Borra el nombre, las estadisticas,", "Deletes your name, stats,")                      \
    X(SetDeleteInfo2, "los logros y la taquilla (perfil.txt).", "achievements and locker (perfil.txt).")     \
    X(SetDeleteInfo3, "Ajustes, juego y otras apps: intactos.", "Settings, game and other apps: untouched.")  \
    X(SetDeleteConfirm, "Borrar los datos? No se puede deshacer.", "Delete data? This cannot be undone.")     \
    X(SetDeleteDone, "Datos borrados", "Data deleted")                                                        \
    X(SetDeleteFailed, "No se pudo borrar (Memory Stick?)", "Could not delete (Memory Stick?)")               \
    X(SetSaveFailed, "No se pudieron guardar los ajustes", "Could not save settings")                         \
    X(SetAppliesNow, "Se aplica al momento", "Applies immediately")                                           \
    X(HintChange, "cambiar", "change")                                                                        \
    X(HintClose, "cerrar", "close")                                                                           \
    X(HintRemap, "reasignar", "remap")                                                                        \
    X(HintDelete, "borrar", "delete")                                                                         \
    X(ActJump, "Saltar / abrir ala delta", "Jump / glider")                                                   \
    X(ActBuild, "Construir", "Build mode")                                                                    \
    X(ActInteract, "Recargar / recoger / material", "Reload / pick up / material")                            \
    X(ActPickaxe, "Pico", "Pickaxe")                                                                          \
    X(ActFire, "Disparar / usar / colocar", "Fire / use / place")                                             \
    X(ActAim, "Apuntar / fijar", "Aim / lock on")                                                             \
    X(ActCrouch, "Agacharse", "Crouch")                                                                       \
    X(ActEmote, "Bailar", "Dance")                                                                            \
    /* --- v0.13: modo de juego, pavos, tienda, inventario, admin y trucos ---------------------- */ \
    X(HintBuy, "comprar / equipar", "buy / equip")                                                            \
    X(HintClaim, "recoger", "claim")                                                                          \
    X(HintToggle, "activar", "toggle")                                                                        \
    X(HintPage, "pagina", "page")                                                                             \
    X(HintMove, "mover", "move")                                                                              \
    X(HintDrop, "soltar", "drop")                                                                             \
    X(HintHand, "a la mano", "equip")                                                                         \
    X(ModeTitle, "MODO DE JUEGO", "GAME MODE")                                                                \
    X(ModeBattleRoyale, "BATTLE ROYALE", "BATTLE ROYALE")                                                     \
    X(ModeOneShot, "MONODISPARO", "ONE SHOT")                                                                 \
    X(ModeBattleRoyaleDesc1, "Todas las armas y objetos.", "Every weapon and item.")                          \
    X(ModeBattleRoyaleDesc2, "El ultimo en pie gana.", "Last one standing wins.")                             \
    X(ModeOneShotDesc1, "Solo francotiradores, armas", "Only snipers, scoped weapons")                        \
    X(ModeOneShotDesc2, "con mira y granadas.", "and grenades.")                                              \
    X(ModeBots, "BOTS  < %d >", "BOTS  < %d >")                                                               \
    X(RewardClaimed, "+50 pavos recogidos", "+50 V-Bucks claimed")                                            \
    X(RewardsPending, "Recompensas por recoger: %d", "Rewards to claim: %d")                                  \
    X(ResultVbucks, "+%d pavos", "+%d V-Bucks")                                                               \
    X(ShopTitle, "TIENDA DE OBJETOS", "ITEM SHOP")                                                            \
    X(ShopOwned, "COMPRADO", "OWNED")                                                                         \
    X(ShopEquippedTag, "EQUIPADO", "EQUIPPED")                                                                \
    X(ShopBought, "Comprado! Equipalo con X o en la TAQUILLA", "Bought! Equip it with X or in the LOCKER")    \
    X(ShopEquipped, "Traje equipado", "Outfit equipped")                                                      \
    X(ShopNoVbucks, "No tienes pavos suficientes", "Not enough V-Bucks")                                      \
    X(ShopConfirm, "Comprar %s?", "Buy %s?")                                                                  \
    X(ShopPrice, "%d pavos (te quedan %d)", "%d V-Bucks (%d left)")                                           \
    X(LockerBought, "Comprado en la tienda", "Bought in the shop")                                            \
    X(SkinCommando, "Comando Sombra", "Shadow Commando")                                                      \
    X(SkinOrange, "Mono Naranja", "Orange Jumpsuit")                                                          \
    X(SkinRanger, "Guardabosques", "Ranger")                                                                  \
    X(PauseModMenu, "MOD MENU", "MOD MENU")                                                                   \
    X(InventoryTitle, "INVENTARIO", "INVENTORY")                                                              \
    X(InventoryEmpty, "Ranura vacia", "Empty slot")                                                           \
    X(InventoryWeaponInfo, "Cargador %d/%d - reserva %d (%s)", "Magazine %d/%d - reserve %d (%s)")            \
    X(InventoryCount, "%d de %d", "%d of %d")                                                                 \
    X(InventoryMoving, "Elige donde dejarlo y pulsa X", "Choose where to put it and press X")                 \
    X(InventoryTraps, "Trampas %d", "Traps %d")                                                               \
    X(SetAdmin, "ADMIN", "ADMIN")                                                                             \
    X(SetAdminPassword, "Contrasena", "Password")                                                             \
    X(SetAdminPasswordTitle, "CONTRASENA DE ADMINISTRADOR", "ADMIN PASSWORD")                                 \
    X(SetAdminOpen, "Abrir MOD MENU", "Open MOD MENU")                                                        \
    X(SetAdminLock, "Bloquear (quita los trucos)", "Lock (disables cheats)")                                  \
    X(SetAdminInfo1, "Introduce la contrasena para desbloquear", "Enter the password to unlock")              \
    X(SetAdminInfo2, "el menu de trucos (MOD MENU).", "the cheat menu (MOD MENU).")                           \
    X(SetAdminInfoOn, "Tambien en partida: START > MOD MENU", "Also in a match: START > MOD MENU")            \
    X(SetAdminWrong, "Contrasena incorrecta", "Wrong password")                                               \
    X(CheatGod, "Modo dios", "God mode")                                                                      \
    X(CheatGodDesc, "No recibes dano de nada.", "You take no damage.")                                        \
    X(CheatAimbot, "Aimbot", "Aimbot")                                                                        \
    X(CheatAimbotDesc, "Cada disparo va a la cabeza del rival visible.", "Every shot hits the head of a visible enemy.") \
    X(CheatWallhack, "Wallhack", "Wallhack")                                                                  \
    X(CheatWallhackDesc, "Ves a los rivales y su vida a traves de todo.", "See enemies and their health through walls.") \
    X(CheatAmmo, "Municion infinita", "Infinite ammo")                                                        \
    X(CheatAmmoDesc, "El cargador y las granadas no se gastan.", "Magazines and grenades never run out.")     \
    X(CheatNoReload, "Recarga instantanea", "Instant reload")                                                 \
    X(CheatNoReloadDesc, "Recargar no lleva tiempo.", "Reloading takes no time.")                             \
    X(CheatRapidFire, "Cadencia x3", "Rapid fire x3")                                                         \
    X(CheatRapidFireDesc, "Las armas disparan tres veces mas rapido.", "Weapons fire three times faster.")    \
    X(CheatNoSpread, "Sin dispersion", "No spread")                                                           \
    X(CheatNoSpreadDesc, "Precision perfecta y sin retroceso.", "Perfect accuracy, no recoil.")               \
    X(CheatOneShot, "Un tiro", "One shot")                                                                    \
    X(CheatOneShotDesc, "Tu dano x10: casi todo elimina de un tiro.", "Your damage x10: almost everything one-shots.") \
    X(CheatExplosive, "Balas explosivas", "Explosive bullets")                                                \
    X(CheatExplosiveDesc, "Cada disparo explota donde da. Cuidado!", "Every shot explodes on impact. Careful!") \
    X(CheatWallbang, "Atravesar paredes", "Shoot through walls")                                              \
    X(CheatWallbangDesc, "Tus balas atraviesan paredes y objetos.", "Your bullets go through walls and objects.") \
    X(CheatFly, "Volar", "Fly")                                                                               \
    X(CheatFlyDesc, "Saltar (mantenido) sube; agacharse baja.", "Hold jump to rise, hold crouch to descend.") \
    X(CheatSpeed, "Supervelocidad", "Super speed")                                                            \
    X(CheatSpeedDesc, "Corres al doble de velocidad.", "You run twice as fast.")                              \
    X(CheatJump, "Supersalto", "Super jump")                                                                  \
    X(CheatJumpDesc, "Saltas tres veces mas alto.", "You jump three times higher.")                           \
    X(CheatGravity, "Gravedad lunar", "Moon gravity")                                                         \
    X(CheatGravityDesc, "Caes despacio, como en la Luna.", "You fall slowly, like on the Moon.")              \
    X(CheatMaterials, "Materiales infinitos", "Infinite materials")                                           \
    X(CheatMaterialsDesc, "Construir y poner trampas no gasta nada.", "Building and traps cost nothing.")     \
    X(CheatInvisible, "Invisible", "Invisible")                                                               \
    X(CheatInvisibleDesc, "Los bots no te ven ni te oyen.", "Bots can't see or hear you.")                    \
    X(CheatNoStorm, "Inmune a la tormenta", "Storm immunity")                                                 \
    X(CheatNoStormDesc, "La tormenta no te hace dano.", "The storm doesn't hurt you.")                        \
    X(CheatFreeze, "Bots congelados", "Frozen bots")                                                          \
    X(CheatFreezeDesc, "Los bots se quedan quietos y no disparan.", "Bots stand still and don't shoot.")      \
    X(CheatParty, "Fiesta", "Party")                                                                          \
    X(CheatPartyDesc, "Los bots se ponen a bailar (y no pelean).", "Bots start dancing (and don't fight).")   \
    X(CheatRadar, "Radar de botin", "Loot radar")                                                             \
    X(CheatRadarDesc, "Cofres y llamas en el minimapa.", "Chests and llamas on the minimap.")                 \
    X(CheatSlowMo, "Tiempo bala", "Bullet time")                                                              \
    X(CheatSlowMoDesc, "Todo va a la mitad de velocidad.", "Everything runs at half speed.")                  \
    X(CheatLlama, "Llama siempre", "Always a llama")                                                          \
    X(CheatLlamaDesc, "Hay llama en todas las partidas.", "Every match has a supply llama.")                  \
    X(CheatKit, "Kit legendario", "Legendary kit")                                                            \
    X(CheatKitDesc, "Empiezas con armas doradas, curas y material.", "Start with gold weapons, heals and mats.") \
    X(CheatActionHeal, "Vida y escudo al maximo", "Max health and shield")                                    \
    X(CheatActionRefill, "Llenar municion, material y trampas", "Refill ammo, materials and traps")           \
    X(CheatActionKillBots, "Eliminar a todos los bots", "Eliminate every bot")                                \
    X(CheatActionInfo, "Accion: se aplica a la partida al pulsar X.", "Action: applied to the match with X.") \
    X(CheatAppliesNow, "Se aplica al momento a esta partida.", "Applies to this match immediately.")          \
    X(CheatAppliesNext, "Se aplica desde la siguiente partida.", "Applies from the next match on.") \
    /* --- v0.14: modo zombi, tacticas de los bots y partidas de practica ------------------- */ \
    X(BotCamp, "Acampar", "Camp") \
    X(BotIdle, "Esperar", "Wait") \
    X(ModeZombies, "SUPERVIVENCIA ZOMBI", "ZOMBIE SURVIVAL") \
    X(ModeZombiesDesc1, "Sobrevive a los zombis,", "Survive the zombies,") \
    X(ModeZombiesDesc2, "rescata gente y fortifica.", "rescue people, fortify.") \
    X(ModeBotsZombies, "BOTS  -  (solo zombis)", "BOTS  -  (zombies only)") \
    X(BotsZombies, "ZOMBIS", "ZOMBIES") \
    X(PracticeMatch, "Partida de practica: sin pavos ni logros", "Practice match: no V-Bucks or achievements") \
    X(PracticeShort, "PRACTICA (menos de 30 bots)", "PRACTICE (under 30 bots)") \
    X(LoadingZombies, "Polvorin en ruinas - supervivencia zombi", "Ruined Depot - zombie survival") \
    X(TipZombies, "de noche hay mas zombis: fortifica la base.", "at night there are more zombies: fortify the base.") \
    X(ZombieWalker, "Zombi", "Zombie") \
    X(ZombieRunner, "Corredor", "Runner") \
    X(ZombieBrute, "Bruto", "Brute") \
    X(ZDay, "DIA %d", "DAY %d") \
    X(ZNight, "NOCHE %d", "NIGHT %d") \
    X(ZUntilNight, "anochece en %d:%02d", "nightfall in %d:%02d") \
    X(ZUntilDawn, "amanece en %d:%02d", "dawn in %d:%02d") \
    X(ZZombies, "Zombis cerca: %d", "Zombies around: %d") \
    X(ZKills, "Abatidos: %d", "Killed: %d") \
    X(ZObjSurvivors, "Supervivientes %d/%d", "Survivors %d/%d") \
    X(ZObjBlueprints, "Planos %d/%d (%d hechos)", "Blueprints %d/%d (%d built)") \
    X(ZObjBenches, "Centros de crafteo %d/%d", "Crafting benches %d/%d") \
    X(ZNightFall, "ANOCHECE: viene una horda", "NIGHTFALL: a horde is coming") \
    X(ZDawn, "AMANECE: DIA %d", "DAWN: DAY %d") \
    X(ZWake1, "...donde estoy?", "...where am I?") \
    X(ZWake2, "Despiertas de un coma en una base abandonada.", "You wake from a coma in an abandoned base.") \
    X(ZWake3, "Busca armas, gente y planos. Sobrevive.", "Find weapons, people and blueprints. Survive.") \
    X(ZSurvivorFound, "%s necesita ayuda", "%s needs help") \
    X(ZSurvivorRescued, "%s se une a ti", "%s joins you") \
    X(ZSurvivorDown, "%s ha caido", "%s is down") \
    X(ZNeedsWeapon, "Necesita un arma", "Needs a weapon") \
    X(ZNeedsHeal, "Necesita una curacion", "Needs a heal") \
    X(ZNeedsBoth, "Necesita un arma y una curacion", "Needs a weapon and a heal") \
    X(ZNeedsHint, "Llevalo en la mano y pulsa", "Hold it and press") \
    X(ZBlueprintFound, "Plano: %s", "Blueprint: %s") \
    X(ZBlueprintDone, "Base fortificada: %s", "Base fortified: %s") \
    X(ZPieceInfo, "Pieza del plano (%d de material)", "Blueprint piece (%d materials)") \
    X(ZPieceNoMats, "Faltan materiales para la pieza", "Not enough materials for the piece") \
    X(ZWorkbenchFound, "Centro de crafteo encontrado", "Crafting bench found") \
    X(ZBase, "BASE", "BASE") \
    X(HintGive, "dar", "give") \
    X(HintCraft, "fabricar", "craft") \
    X(HintBuildPiece, "construir pieza", "build piece") \
    X(CraftTitle, "CENTRO DE CRAFTEO", "CRAFTING BENCH") \
    X(CraftHave, "Tienes  M %d  P %d  Me %d", "You have  W %d  S %d  M %d") \
    X(CraftCost, "M %d  P %d  Me %d", "W %d  S %d  M %d") \
    X(CraftNoMats, "Faltan materiales", "Not enough materials") \
    X(CraftDone, "Fabricado: %s", "Crafted: %s") \
    X(CraftNoBench, "Acercate a un centro de crafteo", "Get closer to a crafting bench") \
    X(CraftAmmoNote, "(municion del arma en la mano)", "(ammo for the weapon in hand)") \
    X(RecipeRifle, "Fusil de asalto", "Assault rifle") \
    X(RecipeSmg, "Subfusil", "SMG") \
    X(RecipeSniper, "Francotirador", "Sniper rifle") \
    X(RecipeRocket, "Lanzacohetes", "Rocket launcher") \
    X(RecipeGrenades, "Granadas x3", "Grenades x3") \
    X(RecipeTraps, "Trampas de pinchos x2", "Spike traps x2") \
    X(RecipeBandages, "Vendas x5", "Bandages x5") \
    X(RecipeMedkit, "Botiquin", "Medkit") \
    X(RecipeAmmo, "Municion x2", "Ammo x2") \
    X(BlueprintNorthWest, "Muralla norte y oeste", "North and west wall") \
    X(BlueprintNorthWestDesc, "6 paredes al norte y al oeste de la base.", "6 walls on the north and west of the base.") \
    X(BlueprintEastSouth, "Muralla este y sur", "East and south wall") \
    X(BlueprintEastSouthDesc, "5 paredes; la puerta sur queda abierta.", "5 walls; the south gate stays open.") \
    X(BlueprintTower, "Torre de vigilancia", "Watchtower") \
    X(BlueprintTowerDesc, "Rampa, plataforma y parapetos a 3 m.", "Ramp, platform and parapets at 3 m.") \
    X(BlueprintShelter, "Refugio techado", "Roofed shelter") \
    X(BlueprintShelterDesc, "4 suelos de tejado sobre la base.", "4 roof floors over the base.") \
    X(PickupBlueprint, "Plano", "Blueprint") \
    X(ResultZDays, "DIAS", "DAYS") \
    X(ResultZKills, "ZOMBIS", "ZOMBIES") \
    X(ResultZRescued, "Rescatados %d   Planos %d   Fabricados %d", "Rescued %d   Blueprints %d   Crafted %d") \
    X(ResultZDied, "Los zombis te han atrapado", "The zombies got you") \
    X(ResultZDayN, "Caiste el dia %d", "You fell on day %d") \
    X(ResultZBest, "Record: %d dias", "Best: %d days") \
    X(StatZBest, "Zombis: record", "Zombies: best") \
    X(AchZFirstNight, "Primera noche", "First night") \
    X(AchZFirstNightDesc, "Sobrevive a una noche (zombis).", "Survive a night (zombie mode).") \
    X(AchZThreeNights, "Tres noches", "Three nights") \
    X(AchZThreeNightsDesc, "Sobrevive a 3 noches seguidas.", "Survive 3 nights in a row.") \
    X(AchZWeek, "Una semana", "One week") \
    X(AchZWeekDesc, "Sobrevive a 7 noches seguidas.", "Survive 7 nights in a row.") \
    X(AchZHunter, "Cazazombis", "Zombie hunter") \
    X(AchZHunterDesc, "Abate 100 zombis.", "Kill 100 zombies.") \
    X(AchZRescuer, "Rescatador", "Rescuer") \
    X(AchZRescuerDesc, "Rescata a 5 supervivientes.", "Rescue 5 survivors.") \
    X(AchZFortress, "Fortaleza", "Fortress") \
    X(AchZFortressDesc, "Completa 4 planos de la base.", "Complete 4 base blueprints.") \
    X(AchZCrafter, "Artesano", "Crafter") \
    X(AchZCrafterDesc, "Fabrica 10 objetos.", "Craft 10 items.") \
    X(AchZVbucks, "+%d pavos por dias sobrevividos", "+%d V-Bucks for days survived")
