// Stub de host: pspaudiolib (lo define tools/app_sim, que llama a la funcion de relleno).
#pragma once
typedef void (*pspAudioCallback_t)(void* buf, unsigned int reqn, void* pdata);
int pspAudioInit(void);
void pspAudioEndPre(void);
void pspAudioEnd(void);
void pspAudioSetChannelCallback(int channel, pspAudioCallback_t callback, void* pdata);
