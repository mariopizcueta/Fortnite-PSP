// Stub de host: reloj del sistema (lo define tools/app_sim con un reloj simulado).
#pragma once
long long sceKernelGetSystemTimeWide(void);
unsigned int sceKernelGetSystemTimeLow(void);
