#ifndef POKEHEARTGOLD_PARTICLE_SYSTEM_H
#define POKEHEARTGOLD_PARTICLE_SYSTEM_H

#if defined(SDK_PORT)
#include "particle_work_size.h"
#endif
#include "camera.h"
#include "filesystem_files_def.h"
#include "spl.h"

typedef u32 (*texAllocFun)(u32 szByte, BOOL is4x4comp);
typedef u32 (*plttAllocFun)(u32 szByte, BOOL is4pltt);

// VRAM handling of a resource's textures and palettes (ParticleSystem_SetResource flags)
enum ParticleVramFlags {
    PARTICLE_TEX_VRAM_FRAME = 1 << 0,  // frame VRAM: the state is saved on load and restored on free
    PARTICLE_TEX_VRAM_LINKED = 1 << 1, // linked VRAM: the keys are registered on load and freed on free
    PARTICLE_PLTT_VRAM_FRAME = 1 << 2,
    PARTICLE_PLTT_VRAM_LINKED = 1 << 3,
};

// The SPLEmitter * handle below is a particle system (ParticleSystem in particle_system_internal.h).
void ParticleSystem_ZeroAll(void);
SPLEmitter *ParticleSystem_New(texAllocFun texAlloc, plttAllocFun plttAlloc, void *particleHeap, u32 workSize, BOOL makeCamera, enum HeapID heapID);
void ParticleSystem_Free(SPLEmitter *emitter);
Camera *ParticleSystem_GetCamera(SPLEmitter *emitter);
void *ParticleSystem_LoadResourceFile(NarcId narcId, int fileId, enum HeapID heapID);
void ParticleSystem_SetResource(SPLEmitter *emitter, void *data, u32 flag, BOOL loadNow);
void ParticleSystem_RegisterTexKey(NNSGfdTexKey texKey);
void ParticleSystem_RegisterPlttKey(NNSGfdPlttKey plttKey);
SPLEmitter *ParticleSystem_CreateEmitter(SPLEmitter *emitter, int resNo, const VecFx32 *position);
SPLEmitter *ParticleSystem_CreateEmitterWithParam(SPLEmitter *emitter, int res_no, void (*fp_callback)(struct SPLEmitter *), void *arg);
int ParticleSystem_GetActiveEmitterCount(SPLEmitter *emitter);
void *ParticleSystem_GetEmitterParam(void);
int ParticleSystem_DrawAll(void);
int ParticleSystem_CalcAll(void);
void *ParticleSystem_GetHeap(SPLEmitter *emitter);
int ParticleSystem_CountActive(void);
void ParticleEmitter_GetAxis(SPLEmitter *emitter, VecFx16 *axis);
void ParticleEmitter_SetGravity(SPLEmitter *emitter, VecFx16 *value);
void ParticleEmitter_SetMagnetPos(SPLEmitter *emitter, VecFx32 *value);
void ParticleEmitter_GetMagnetPos(SPLEmitter *emitter, VecFx32 *value);
void ParticleEmitter_SetMagnetForce(SPLEmitter *emitter, s16 *value);
void ParticleEmitter_GetMagnetForce(SPLEmitter *emitter, s16 *value);
void ParticleEmitter_SetSpinAngle(SPLEmitter *emitter, u16 *value);
void ParticleEmitter_GetSpinAngle(SPLEmitter *emitter, u16 *value);
void ParticleEmitter_SetSpinAxis(SPLEmitter *emitter, u16 *value);
void ParticleEmitter_GetSpinAxis(SPLEmitter *emitter, u16 *value);
void ParticleEmitter_SetConvergencePos(SPLEmitter *emitter, VecFx32 *value);
void ParticleEmitter_GetConvergencePos(SPLEmitter *emitter, VecFx32 *value);
void ParticleEmitter_SetConvergenceForce(SPLEmitter *emitter, s16 *value);
void ParticleEmitter_GetConvergenceForce(SPLEmitter *emitter, s16 *value);
void ParticleSystem_SetPerspectiveType(SPLEmitter *emitter, int type);

#endif // POKEHEARTGOLD_PARTICLE_SYSTEM_H
