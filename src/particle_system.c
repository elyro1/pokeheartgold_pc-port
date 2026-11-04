#include "particle_system.h"

#include "global.h"

#include "library/spl_internal.h"
#include "nnsys/gfd_internal.h"

#include "camera.h"
#include "filesystem.h"
#include "heap.h"
#include "particle_system_internal.h"
#include "sys_task.h"
#include "sys_task_api.h"

typedef void *(*ParticleAllocFunc)(u32 size);

enum ParticleFieldType {
    PARTICLE_FIELD_GRAVITY,
    PARTICLE_FIELD_RANDOM,
    PARTICLE_FIELD_MAGNET,
    PARTICLE_FIELD_SPIN,
    PARTICLE_FIELD_SCFIELD,
    PARTICLE_FIELD_CONVERGENCE,
};

static const VecFx32 sParticleCameraUp = { 0, FX32_ONE, 0 };

static const VecFx32 sParticleCameraPos = { 0, 0, 4 * FX32_ONE };

static const VecFx32 sParticleCameraTarget = { 0, 0, 0 };

static struct {
    ParticleSystem *loadingSystem; // system whose resource is being loaded
    void *emitterParam;            // arg of the running ParticleSystem_CreateEmitterWithParam
} sParticleSystemCallbackState;
static ParticleSystem *sParticleSystems[PARTICLE_SYSTEM_MAX];

static void ParticleSystem_LoadResource(ParticleSystem *system);
static void ParticleSystem_LoadResourceTask(SysTask *task, void *data);
static void *ParticleEmitter_FindField(SPLEmitter *emitter, enum ParticleFieldType type);

static void *ParticleSystem_AllocSlot0(u32 size);
static void *ParticleSystem_AllocSlot1(u32 size);
static void *ParticleSystem_AllocSlot2(u32 size);
static void *ParticleSystem_AllocSlot3(u32 size);
static void *ParticleSystem_AllocSlot4(u32 size);
static void *ParticleSystem_AllocSlot5(u32 size);
static void *ParticleSystem_AllocSlot6(u32 size);
static void *ParticleSystem_AllocSlot7(u32 size);
static void *ParticleSystem_AllocSlot8(u32 size);
static void *ParticleSystem_AllocSlot9(u32 size);
static void *ParticleSystem_AllocSlot10(u32 size);
static void *ParticleSystem_AllocSlot11(u32 size);
static void *ParticleSystem_AllocSlot12(u32 size);
static void *ParticleSystem_AllocSlot13(u32 size);
static void *ParticleSystem_AllocSlot14(u32 size);
static void *ParticleSystem_AllocSlot15(u32 size);

static const ParticleAllocFunc sParticleSystemAllocFuncs[PARTICLE_SYSTEM_MAX] = {
    ParticleSystem_AllocSlot0,
    ParticleSystem_AllocSlot1,
    ParticleSystem_AllocSlot2,
    ParticleSystem_AllocSlot3,
    ParticleSystem_AllocSlot4,
    ParticleSystem_AllocSlot5,
    ParticleSystem_AllocSlot6,
    ParticleSystem_AllocSlot7,
    ParticleSystem_AllocSlot8,
    ParticleSystem_AllocSlot9,
    ParticleSystem_AllocSlot10,
    ParticleSystem_AllocSlot11,
    ParticleSystem_AllocSlot12,
    ParticleSystem_AllocSlot13,
    ParticleSystem_AllocSlot14,
    ParticleSystem_AllocSlot15,
};

void ParticleSystem_ZeroAll(void) {
    int i;
    for (i = 0; i < PARTICLE_SYSTEM_MAX; i++) {
        sParticleSystems[i] = NULL;
    }
}

SPLEmitter *ParticleSystem_New(texAllocFun texAlloc, plttAllocFun plttAlloc, void *particleHeap, u32 workSize, BOOL makeCamera, enum HeapID heapID) {
    ParticleSystem *system;
    int slot;

    for (slot = 0; slot < PARTICLE_SYSTEM_MAX; slot++) {
        if (sParticleSystems[slot] == NULL) {
            break;
        }
    }
    if (slot >= PARTICLE_SYSTEM_MAX) {
        return NULL;
    }
    system = Heap_Alloc(heapID, sizeof(ParticleSystem));
    GF_ASSERT(system != NULL);
    memset(system, 0, sizeof(ParticleSystem));
#if defined(SDK_PORT)
    system->timingDomain = BattleTiming_GetDomain();
    if (system->timingDomain == BATTLE_TIMING_NONE && heapID == HEAP_ID_BATTLE && BattleTiming_IsActive())
        system->timingDomain = BATTLE_TIMING_ANIMATION;
#endif
    system->texAlloc = texAlloc;
    system->plttAlloc = plttAlloc;
    system->pos = sParticleCameraPos;
    system->up = sParticleCameraUp;
    system->target = sParticleCameraTarget;
    memset(particleHeap, 0, workSize);
    system->heapStart = particleHeap;
    system->heapCurrent = particleHeap;
    system->heapEnd = (u8 *)particleHeap + workSize;
    system->slot = slot;
    sParticleSystems[slot] = system;
    if (makeCamera == TRUE) {
        system->camera = Camera_New(heapID);
        system->unk_24 = 0;
        system->unk_28 = 0;
        system->unk_2C = 0;
        system->perspective = 2 * FX32_ONE;
        Camera_Init_FromTargetAndPos(&sParticleCameraTarget, &sParticleCameraPos, system->perspective, 0, FALSE, system->camera);
        system->perspectiveType = FALSE;
        Camera_SetStaticPtr(system->camera);
    }
    system->manager = SPL_Init(sParticleSystemAllocFuncs[slot], 20, 200, 5, 6, 63);
    ParticleSystem_SetCameraUp(system, &sParticleCameraUp);
    return (SPLEmitter *)system;
}

void ParticleSystem_Free(SPLEmitter *emitter) {
    ParticleSystem *system = (ParticleSystem *)emitter;
    int i;
    int j;

    ParticleSystem_DeleteAllEmitters(system);
    if (system->flags & PARTICLE_TEX_VRAM_FRAME) {
#if defined(SDK_PORT)
        NNS_GfdSetFrmTexVramState(&system->texState);
#else
        NNS_GfdSetFrmTexVramState(system->texKeys);
#endif
    } else if (system->flags & PARTICLE_TEX_VRAM_LINKED) {
        for (i = 0; i < PARTICLE_VRAM_KEY_MAX; i++) {
            if (system->texKeys[i] != 0) {
                NNS_GfdFreeLnkTexVram(system->texKeys[i]);
                system->texKeys[i] = 0;
            }
        }
    }
    if (system->flags & PARTICLE_PLTT_VRAM_FRAME) {
#if defined(SDK_PORT)
        NNS_GfdSetFrmPlttVramState(&system->plttState);
#else
        NNS_GfdSetFrmPlttVramState(system->plttKeys);
#endif
    } else if (system->flags & PARTICLE_PLTT_VRAM_LINKED) {
        for (i = 0; i < PARTICLE_VRAM_KEY_MAX; i++) {
            if (system->plttKeys[i] != 0) {
                NNS_GfdFreeLnkPlttVram(system->plttKeys[i]);
                system->plttKeys[i] = 0;
            }
        }
    }
    system->flags = 0;
    system->emitter = NULL;
    if (system->resource != NULL) {
        Heap_Free(system->resource);
        system->resource = NULL;
    }
    for (j = 0; j < PARTICLE_SYSTEM_MAX; j++) {
        if (sParticleSystems[j] == system) {
            sParticleSystems[j] = NULL;
            break;
        }
    }
    if (system->camera != NULL) {
        Camera_Delete(system->camera);
    }
    Heap_Free(system);
}

void ParticleSystem_FreeAll(void) {
    int i;
    for (i = 0; i < PARTICLE_SYSTEM_MAX; i++) {
        if (sParticleSystems[i] != NULL) {
            ParticleSystem_Free((SPLEmitter *)sParticleSystems[i]);
        }
    }
}

// SPL allocator of one slot: 4-aligned bump allocation in the system's heap
#if defined(SDK_PORT)
static void *ParticleSystem_AllocNativeSlot(int slot, u32 size) {
    ParticleSystem *system = sParticleSystems[slot];
    const uintptr_t alignment = _Alignof(max_align_t);
    uintptr_t start = ((uintptr_t)system->heapCurrent + alignment - 1) & ~(alignment - 1);
    uintptr_t next = (start + size + alignment - 1) & ~(alignment - 1);
    if (next > (uintptr_t)system->heapEnd || next < start) return NULL;
    system->heapCurrent = (void *)next;
    return (void *)start;
}

#define DEFINE_SLOT_ALLOC(name, slot) \
    static void *name(u32 size) { return ParticleSystem_AllocNativeSlot(slot, size); }
#else
#define DEFINE_SLOT_ALLOC(name, slot)                    \
    static void *name(u32 size) {                        \
        ParticleSystem *system = sParticleSystems[slot]; \
        u8 *ret = system->heapCurrent;                   \
        u8 *next = ret + size;                           \
        if ((u32)next & 3) {                             \
            next += 4 - ((u32)next & 3);                 \
        }                                                \
        system->heapCurrent = next;                      \
        GF_ASSERT(next < (u8 *)system->heapEnd);         \
        return ret;                                      \
    }
#endif

DEFINE_SLOT_ALLOC(ParticleSystem_AllocSlot0, 0)
DEFINE_SLOT_ALLOC(ParticleSystem_AllocSlot1, 1)
DEFINE_SLOT_ALLOC(ParticleSystem_AllocSlot2, 2)
DEFINE_SLOT_ALLOC(ParticleSystem_AllocSlot3, 3)
DEFINE_SLOT_ALLOC(ParticleSystem_AllocSlot4, 4)
DEFINE_SLOT_ALLOC(ParticleSystem_AllocSlot5, 5)
DEFINE_SLOT_ALLOC(ParticleSystem_AllocSlot6, 6)
DEFINE_SLOT_ALLOC(ParticleSystem_AllocSlot7, 7)
DEFINE_SLOT_ALLOC(ParticleSystem_AllocSlot8, 8)
DEFINE_SLOT_ALLOC(ParticleSystem_AllocSlot9, 9)
DEFINE_SLOT_ALLOC(ParticleSystem_AllocSlot10, 10)
DEFINE_SLOT_ALLOC(ParticleSystem_AllocSlot11, 11)
DEFINE_SLOT_ALLOC(ParticleSystem_AllocSlot12, 12)
DEFINE_SLOT_ALLOC(ParticleSystem_AllocSlot13, 13)
DEFINE_SLOT_ALLOC(ParticleSystem_AllocSlot14, 14)
DEFINE_SLOT_ALLOC(ParticleSystem_AllocSlot15, 15)

void *ParticleSystem_LoadResourceFile(NarcId narcId, int fileId, enum HeapID heapID) {
    return AllocAndReadWholeNarcMemberByIdPair(narcId, fileId, heapID);
}

void ParticleSystem_SetResource(SPLEmitter *emitter, void *data, u32 flag, BOOL loadNow) {
    ParticleSystem *system = (ParticleSystem *)emitter;
    int i;
    int j;

    GF_ASSERT(system->manager != NULL);
    GF_ASSERT(system->resource == NULL);
    system->flags = flag;
    if (flag & PARTICLE_TEX_VRAM_FRAME) {
#if defined(SDK_PORT)
        NNS_GfdGetFrmTexVramState(&system->texState);
#else
        NNS_GfdGetFrmTexVramState(system->texKeys);
#endif
    } else if (flag & PARTICLE_TEX_VRAM_LINKED) {
        for (i = 0; i < PARTICLE_VRAM_KEY_MAX; i++) {
            system->texKeys[i] = 0;
        }
    }
    if (flag & PARTICLE_PLTT_VRAM_FRAME) {
#if defined(SDK_PORT)
        NNS_GfdGetFrmPlttVramState(&system->plttState);
#else
        NNS_GfdGetFrmPlttVramState(system->plttKeys);
#endif
    } else if (flag & PARTICLE_PLTT_VRAM_LINKED) {
        for (j = 0; j < PARTICLE_VRAM_KEY_MAX; j++) {
            system->plttKeys[j] = 0;
        }
    }
    system->resource = data;
    if (loadNow == TRUE) {
        ParticleSystem_LoadResource(system);
        return;
    }
    SysTask_CreateOnVWaitQueue(ParticleSystem_LoadResourceTask, system, 5);
}

static void ParticleSystem_LoadResource(ParticleSystem *system) {
    texAllocFun texAlloc;
    plttAllocFun plttAlloc;
    SPLManager *manager;

    SPL_Load(system->manager, system->resource);
    sParticleSystemCallbackState.loadingSystem = system;
    texAlloc = system->texAlloc;
    manager = system->manager;
    if (texAlloc == NULL) {
        SPL_LoadTexByVRAMManager(manager);
    } else {
        SPL_LoadTexByCallbackFunction(manager, texAlloc);
    }
    plttAlloc = system->plttAlloc;
    manager = system->manager;
    if (plttAlloc == NULL) {
        SPL_LoadTexPlttByVRAMManager(manager);
    } else {
        SPL_LoadTexPlttByCallbackFunction(manager, plttAlloc);
    }
    sParticleSystemCallbackState.loadingSystem = NULL;
}

static void ParticleSystem_LoadResourceTask(SysTask *task, void *data) {
    ParticleSystem_LoadResource(data);
    SysTask_Destroy(task);
}

void ParticleSystem_RegisterTexKey(NNSGfdTexKey texKey) {
    int i;
    ParticleSystem *system;

    GF_ASSERT(texKey != 0);
    GF_ASSERT(sParticleSystemCallbackState.loadingSystem != NULL);
    system = sParticleSystemCallbackState.loadingSystem;
    for (i = 0; i < PARTICLE_VRAM_KEY_MAX; i++) {
        if (system->texKeys[i] == 0) {
            system->texKeys[i] = texKey;
            return;
        }
    }
    GF_ASSERT(FALSE);
}

void ParticleSystem_RegisterPlttKey(NNSGfdPlttKey plttKey) {
    int i;
    ParticleSystem *system;

    GF_ASSERT(plttKey != 0);
    GF_ASSERT(sParticleSystemCallbackState.loadingSystem != NULL);
    system = sParticleSystemCallbackState.loadingSystem;
    for (i = 0; i < PARTICLE_VRAM_KEY_MAX; i++) {
        if (system->plttKeys[i] == 0) {
            system->plttKeys[i] = plttKey;
            return;
        }
    }
    GF_ASSERT(FALSE);
}

void ParticleSystem_Draw(ParticleSystem *system) {
    if (system->camera != NULL) {
        Camera_ApplyPerspectiveType(system->perspectiveType, system->camera);
        Camera_SetStaticPtr(system->camera);
        Camera_PushLookAtToNNSGlb();
    }
    NNS_G3dGlbFlushP();
    SPL_Draw(system->manager, &NNS_G3dGlb.cameraMtx);
    if (system->camera != NULL) {
        Camera_UnsetStaticPtr();
    }
    NNS_G3dGlbFlushP();
}

void ParticleSystem_Calc(ParticleSystem *system) {
#if defined(SDK_PORT)
    if (!BattleTiming_ShouldTick(system->timingDomain)) return;
#endif
    SPL_Calc(system->manager);
}

int ParticleSystem_CountActive(void) {
    int count = 0, i;
    for (i = 0; i < PARTICLE_SYSTEM_MAX; i++) {
        if (sParticleSystems[i] != NULL) {
            count++;
        }
    }
    return count;
}

int ParticleSystem_DrawAll(void) {
    int count = 0, i;
    for (i = 0; i < PARTICLE_SYSTEM_MAX; i++) {
        if (sParticleSystems[i] != NULL) {
            ParticleSystem_Draw(sParticleSystems[i]);
            count++;
        }
    }
    return count;
}

int ParticleSystem_CalcAll(void) {
    int count = 0, i;
    for (i = 0; i < PARTICLE_SYSTEM_MAX; i++) {
        if (sParticleSystems[i] != NULL) {
            ParticleSystem_Calc(sParticleSystems[i]);
            count++;
        }
    }
    return count;
}

SPLEmitter *ParticleSystem_CreateEmitter(SPLEmitter *emitter, int resNo, const VecFx32 *position) {
    ParticleSystem *system = (ParticleSystem *)emitter;
    return system->emitter = SPL_Create(system->manager, resNo, position);
}

SPLEmitter *ParticleSystem_CreateEmitterWithParam(SPLEmitter *emitter, int resNo, void (*callback)(SPLEmitter *), void *arg) {
    ParticleSystem *system = (ParticleSystem *)emitter;
    SPLEmitter *result;

    sParticleSystemCallbackState.emitterParam = arg;
    result = SPL_CreateWithInitialize(system->manager, resNo, callback);
    sParticleSystemCallbackState.emitterParam = NULL;
    system->emitter = result;
    return result;
}

int ParticleSystem_GetActiveEmitterCount(SPLEmitter *emitter) {
    return ((ParticleSystem *)emitter)->manager->act_emtr_list.node_num;
}

void ParticleSystem_DeleteAllEmitters(ParticleSystem *system) {
    SPL_DeleteAll(system->manager);
}

void ParticleSystem_DeleteEmitter(ParticleSystem *system, SPLEmitter *emitter) {
    SPL_Delete(system->manager, emitter);
}

void *ParticleSystem_GetHeap(SPLEmitter *emitter) {
    return ((ParticleSystem *)emitter)->heapStart;
}

void ParticleSystem_GetCameraUp(ParticleSystem *system, VecFx32 *out) {
    *out = system->up;
}

void ParticleSystem_SetCameraUp(ParticleSystem *system, const VecFx32 *up) {
    system->up = *up;
    Camera_SetLookAtCamUp((VecFx32 *)up, system->camera);
}

void *ParticleSystem_GetEmitterParam(void) {
    return sParticleSystemCallbackState.emitterParam;
}

void ParticleSystem_GetDefaultCameraUp(VecFx32 *out) {
    *out = sParticleCameraUp;
}

Camera *ParticleSystem_GetCamera(SPLEmitter *emitter) {
    return ((ParticleSystem *)emitter)->camera;
}

void ParticleSystem_SetPerspectiveType(SPLEmitter *emitter, int type) {
    ((ParticleSystem *)emitter)->perspectiveType = type;
}

int ParticleSystem_GetPerspectiveType(ParticleSystem *system) {
    return system->perspectiveType;
}

void ParticleEmitter_GetAxis(SPLEmitter *emitter, VecFx16 *axis) {
    axis->x = *(const u16 *)&emitter->axis.x;
    axis->y = *(const u16 *)&emitter->axis.y;
    axis->z = *(const u16 *)&emitter->axis.z;
}

static void *ParticleEmitter_FindField(SPLEmitter *emitter, enum ParticleFieldType type) {
    SPLResource *resource = emitter->p_res;
    int j;
    int count = resource->fld_num;
    SPLField *field;

    if (count == 0) {
        return NULL;
    }
    for (j = 0; j < count; j++) {
        field = &resource->fld_ary[j];
        if (field == NULL) {
            continue;
        }
        switch (type) {
        case PARTICLE_FIELD_GRAVITY:
            if (field->p_exec == spl_calc_gravity) {
                return (void *)field->p_obj;
            }
            break;
        case PARTICLE_FIELD_RANDOM:
            if (field->p_exec == spl_calc_random) {
                return (void *)field->p_obj;
            }
            break;
        case PARTICLE_FIELD_MAGNET:
            if (field->p_exec == spl_calc_magnet) {
                return (void *)field->p_obj;
            }
            break;
        case PARTICLE_FIELD_SPIN:
            if (field->p_exec == spl_calc_spin) {
                return (void *)field->p_obj;
            }
            break;
        case PARTICLE_FIELD_SCFIELD:
            if (field->p_exec == spl_calc_scfield) {
                return (void *)field->p_obj;
            }
            break;
        case PARTICLE_FIELD_CONVERGENCE:
            if (field->p_exec == spl_calc_convergence) {
                return (void *)field->p_obj;
            }
            break;
        default:
            return NULL;
        }
    }
    return NULL;
}

void ParticleEmitter_SetGravity(SPLEmitter *emitter, VecFx16 *value) {
    SplGravityField *gravity = ParticleEmitter_FindField(emitter, PARTICLE_FIELD_GRAVITY);
    if (gravity != NULL) {
        gravity->mag = *value;
    }
}

void ParticleEmitter_SetMagnetPos(SPLEmitter *emitter, VecFx32 *value) {
    SplMagnetField *magnet = ParticleEmitter_FindField(emitter, PARTICLE_FIELD_MAGNET);
    if (magnet != NULL) {
        magnet->pos = *value;
    }
}

void ParticleEmitter_GetMagnetPos(SPLEmitter *emitter, VecFx32 *value) {
    SplMagnetField *magnet = ParticleEmitter_FindField(emitter, PARTICLE_FIELD_MAGNET);
    if (magnet == NULL) {
        VecFx32 zero = { 0, 0, 0 };
        *value = zero;
        return;
    }
    *value = magnet->pos;
}

void ParticleEmitter_SetMagnetForce(SPLEmitter *emitter, s16 *value) {
    SplMagnetField *magnet = ParticleEmitter_FindField(emitter, PARTICLE_FIELD_MAGNET);
    if (magnet != NULL) {
        magnet->mag = *value;
    }
}

void ParticleEmitter_GetMagnetForce(SPLEmitter *emitter, s16 *value) {
    SplMagnetField *magnet = ParticleEmitter_FindField(emitter, PARTICLE_FIELD_MAGNET);
    if (magnet == NULL) {
        *value = 0;
    } else {
        *value = magnet->mag;
    }
}

void ParticleEmitter_SetSpinAngle(SPLEmitter *emitter, u16 *value) {
    SplSpinField *spin = ParticleEmitter_FindField(emitter, PARTICLE_FIELD_SPIN);
    if (spin != NULL) {
        spin->rotation = *value;
    }
}

void ParticleEmitter_GetSpinAngle(SPLEmitter *emitter, u16 *value) {
    SplSpinField *spin = ParticleEmitter_FindField(emitter, PARTICLE_FIELD_SPIN);
    if (spin == NULL) {
        *value = 0;
    } else {
        *value = spin->rotation;
    }
}

void ParticleEmitter_SetSpinAxis(SPLEmitter *emitter, u16 *value) {
    SplSpinField *spin = ParticleEmitter_FindField(emitter, PARTICLE_FIELD_SPIN);
    if (spin != NULL) {
        spin->axis_type = *value;
    }
}

void ParticleEmitter_GetSpinAxis(SPLEmitter *emitter, u16 *value) {
    SplSpinField *spin = ParticleEmitter_FindField(emitter, PARTICLE_FIELD_SPIN);
    if (spin == NULL) {
        *value = 0;
    } else {
        *value = spin->axis_type;
    }
}

void ParticleEmitter_SetConvergencePos(SPLEmitter *emitter, VecFx32 *value) {
    SplConvergenceField *convergence = ParticleEmitter_FindField(emitter, PARTICLE_FIELD_CONVERGENCE);
    if (convergence != NULL) {
        convergence->pos = *value;
    }
}

void ParticleEmitter_GetConvergencePos(SPLEmitter *emitter, VecFx32 *value) {
    SplConvergenceField *convergence = ParticleEmitter_FindField(emitter, PARTICLE_FIELD_CONVERGENCE);
    if (convergence == NULL) {
        VecFx32 zero = { 0, 0, 0 };
        *value = zero;
        return;
    }
    *value = convergence->pos;
}

void ParticleEmitter_SetConvergenceForce(SPLEmitter *emitter, s16 *value) {
    SplConvergenceField *convergence = ParticleEmitter_FindField(emitter, PARTICLE_FIELD_CONVERGENCE);
    if (convergence != NULL) {
        convergence->ratio = *value;
    }
}

void ParticleEmitter_GetConvergenceForce(SPLEmitter *emitter, s16 *value) {
    SplConvergenceField *convergence = ParticleEmitter_FindField(emitter, PARTICLE_FIELD_CONVERGENCE);
    if (convergence == NULL) {
        *value = 0;
    } else {
        *value = convergence->ratio;
    }
}
