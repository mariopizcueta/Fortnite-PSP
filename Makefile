# FortnitePSP - Makefile basado en build.mak del PSPSDK.
#
#   make            -> compila y genera EBOOT.PBP en la raiz del proyecto
#   make release    -> copia el EBOOT a build/PSP/GAME/FortnitePSP/ (listo para la Memory Stick)
#   make assets     -> regenera assets/ desde Recursos/ (requiere Python 3 + Pillow)
#   make preview    -> capturas de la escena 3D renderizadas en el Mac (build/preview/)
#   make physics-test -> pruebas de movimiento del jugador en el Mac
#   make match-sim  -> partidas simuladas con bots en el Mac (colisiones, disparos, IA)
#   make app-sim    -> la aplicacion completa simulada en el Mac (menus, partidas, audio)
#   make clean      -> borra los productos de compilacion

# --- Localizacion del toolchain -----------------------------------------------
# Usa PSPDEV si esta definido; si no, psp-config del PATH; si no, ~/pspdev.
ifeq ($(PSPDEV),)
PSPDEV := $(shell psp-config --pspdev-path 2>/dev/null)
endif
ifeq ($(PSPDEV),)
PSPDEV := $(HOME)/pspdev
endif
export PSPDEV
export PATH := $(PSPDEV)/bin:$(PATH)
PSPSDK := $(PSPDEV)/psp/sdk

ifeq ($(wildcard $(PSPSDK)/lib/build.mak),)
$(error No se encuentra PSPSDK en "$(PSPSDK)". Define PSPDEV con la ruta de instalacion)
endif

# --- Proyecto -------------------------------------------------------------------
TARGET = FortnitePSP

OBJS = src/main.o \
       src/app/app.o \
       src/app/app_hud.o \
       src/app/app_menus.o \
       src/app/build_visuals.o \
       src/app/lobby_scene.o \
       src/app/ui.o \
       src/audio/audio_system.o \
       src/audio/mixer.o \
       src/audio/sound_data.o \
       src/camera/orbit_camera.o \
       src/camera/third_person_camera.o \
       src/core/timer.o \
       src/game/character.o \
       src/game/character_model.o \
       src/game/inventory.o \
       src/game/match.o \
       src/game/nav_grid.o \
       src/game/player.o \
       src/game/structures.o \
       src/game/weapon.o \
       src/gfx/graphics.o \
       src/gfx/mesh.o \
       src/gfx/texture.o \
       src/gfx/debug_text.o \
       src/input/input.o \
       src/math/math3d.o \
       src/platform/callbacks.o \
       src/platform/log.o \
       src/world/collision.o \
       src/world/scene_builder.o \
       src/world/terrain.o \
       src/world/textures.o \
       src/world/world.o \
       src/world/world_layout.o \
       src/third_party/stb_image_impl.o \
       src/assets/embedded_assets.o

INCDIR = src
CFLAGS = -O2 -G0 -Wall -Wextra -MMD -MP
CXXFLAGS = -std=gnu++17 -fno-exceptions -fno-rtti

LIBS = -lpspgu -lpspaudiolib -lpspaudio -lpsppower -lstdc++ -lm

BUILD_PRX = 1
PSP_FW_VERSION = 600

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = FortnitePSP
PSP_EBOOT_ICON = assets/pbp/ICON0.PNG
PSP_EBOOT_PIC1 = assets/pbp/PIC1.PNG
# ID propio: el de mksfoex por defecto (UCJS10041) pertenece a un juego comercial y
# compartiria carpeta de partidas guardadas con el.
SFOFLAGS = -s DISC_ID=FNPS00001

DEPS = $(OBJS:.o=.d)
EXTRA_CLEAN = $(DEPS)

include $(PSPSDK)/lib/build.mak

# GNU Make 3.81 (el de macOS) ignora el PATH exportado al lanzar comandos sin shell,
# asi que las herramientas se fijan con ruta absoluta.
PSPBIN := $(PSPDEV)/bin
CC       := $(PSPBIN)/psp-gcc
CXX      := $(PSPBIN)/psp-g++
AS       := $(PSPBIN)/psp-gcc
LD       := $(PSPBIN)/psp-gcc
AR       := $(PSPBIN)/psp-gcc-ar
RANLIB   := $(PSPBIN)/psp-gcc-ranlib
STRIP    := $(PSPBIN)/psp-strip
MKSFO    := $(PSPBIN)/mksfoex
PACK_PBP := $(PSPBIN)/pack-pbp
FIXUP    := $(PSPBIN)/psp-fixup-imports
ENC      := $(PSPBIN)/PrxEncrypter

# Sustituye a la regla %.prx de build.mak, que invoca psp-prxgen sin ruta.
$(TARGET).prx: $(TARGET).elf
	$(PSPBIN)/psp-prxgen $< $@

# --- Reglas propias ---------------------------------------------------------------
# .incbin no genera dependencias: se declaran a mano para recompilar al cambiar el PNG.
src/assets/embedded_assets.o: $(wildcard assets/images/*.png) $(wildcard assets/sounds/*)

# El EBOOT tambien depende de los iconos del XMB.
EBOOT.PBP: $(PSP_EBOOT_ICON) $(PSP_EBOOT_PIC1)

# PARAM.SFO no tiene dependencias en build.mak: se regenera si cambia este Makefile
# (por ejemplo, el titulo).
PARAM.SFO: Makefile

RELEASE_DIR = build/PSP/GAME/$(TARGET)

release: EBOOT.PBP
	mkdir -p "$(RELEASE_DIR)"
	cp EBOOT.PBP "$(RELEASE_DIR)/EBOOT.PBP"
	@echo "Listo: $(RELEASE_DIR)/EBOOT.PBP"

assets:
	python3 tools/prepare_assets.py

# Previsualizador de host (no PSP): renderiza la escena por software en build/preview/.
preview:
	sh tools/scene_preview/build_and_run.sh

# Pruebas de host (no PSP) del movimiento del jugador y de la camara.
physics-test:
	sh tools/physics_test/build_and_run.sh

# Simulador de host (no PSP): colisiones, disparos y partidas completas con bots.
match-sim:
	sh tools/match_sim/build_and_run.sh

distclean: clean
	rm -rf build

# Simulacion de host de la aplicacion completa con capturas en build/app_sim/.
app-sim:
	sh tools/app_sim/build_and_run.sh

.PHONY: release assets preview physics-test match-sim app-sim distclean

-include $(DEPS)
