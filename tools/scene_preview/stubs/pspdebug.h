// Stub de host: texto de pspDebugScreen (lo dibuja tools/host/soft_gfx.cpp).
#pragma once
#include <psptypes.h>
void pspDebugScreenInitEx(void* vramBase, int mode, int setup);
void pspDebugScreenEnableBackColor(int enable);
void pspDebugScreenSetBase(u32* base);
void pspDebugScreenPutChar(int x, int y, u32 color, u8 ch);
