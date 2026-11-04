#ifndef POKEHEARTGOLD_PARTICLE_SYSTEM_INTERNAL_H
#define POKEHEARTGOLD_PARTICLE_SYSTEM_INTERNAL_H

#include "particle_system.h"
#if defined(SDK_PORT)
#include "port/battle_timing.h"

typedef void *(*ParticleAllocFunc)(u32 size);
#endif

#define PARTICLE_SYSTEM_MAX 16
#define PARTICLE_VRAM_KEY_MAX 16

// The public API (particle_system.h) passes a ParticleSystem * as SPLEmitter *.
typedef struct ParticleSystem {
    SPLManager *manager;
    void *resource;
    SPLEmitter *emitter;
    void *heapStart;
    void *heapCurrent; // bump pointer of the slot's SPL allocator
    void *heapEnd;
    texAllocFun texAlloc; // NULL: SPL_LoadTexByVRAMManager
    plttAllocFun plttAlloc;
    Camera *camera; // only when created with makeCamera
    u32 unk_24;
    u32 unk_28;
    u32 unk_2C;
    u16 perspective;
    u8 pad_32[2];
    VecFx32 pos;
    VecFx32 up;
    VecFx32 target;
#if defined(SDK_PORT)
    union { u32 texKeys[PARTICLE_VRAM_KEY_MAX]; NNSGfdFrmTexVramState texState; };
    union { u32 plttKeys[PARTICLE_VRAM_KEY_MAX]; NNSGfdFrmPlttVramState plttState; };
#else
    u32 texKeys[PARTICLE_VRAM_KEY_MAX]; // frame VRAM state or linked VRAM keys
    u32 plttKeys[PARTICLE_VRAM_KEY_MAX];
#endif
    u8 flags; // enum ParticleVramFlags
    u8 unk_D9;
    u8 slot;
    u8 perspectiveType;
#if defined(SDK_PORT)
    BattleTimingDomain timingDomain;
#endif
} ParticleSystem;
#if defined(SDK_PORT)

extern ParticleSystem *gParticleSystems[16];
extern void *gParticleSystemCallbackState[2];
extern const ParticleAllocFunc gParticleSystemAllocFuncs[16];
#endif

void ParticleSystem_Draw(ParticleSystem *system);
void ParticleSystem_Calc(ParticleSystem *system);
void ParticleSystem_DeleteAllEmitters(ParticleSystem *system);
void ParticleSystem_DeleteEmitter(ParticleSystem *system, SPLEmitter *emitter);
void ParticleSystem_GetCameraUp(ParticleSystem *system, VecFx32 *out);
void ParticleSystem_SetCameraUp(ParticleSystem *system, const VecFx32 *up);
int ParticleSystem_GetPerspectiveType(ParticleSystem *system);
void ParticleSystem_GetDefaultCameraUp(VecFx32 *out);
void ParticleSystem_FreeAll(void);

#endif // POKEHEARTGOLD_PARTICLE_SYSTEM_INTERNAL_H
