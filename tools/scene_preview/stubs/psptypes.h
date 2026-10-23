// Stub de host para tools/scene_preview: tipos basicos de PSPSDK.
#pragma once
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t s8;
typedef struct ScePspFVector4 { float x, y, z, w; } __attribute__((aligned(16))) ScePspFVector4;
typedef struct ScePspFMatrix4 { ScePspFVector4 x, y, z, w; } __attribute__((aligned(16))) ScePspFMatrix4;
