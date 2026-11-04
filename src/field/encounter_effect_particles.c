#include "global.h"

#include "field/encounter_effect.h"
#include "field/fieldmap.h"

#include "bg_window.h"
#include "camera.h"
#include "gf_gfx_loader.h"
#include "gf_gfx_planes.h"
#include "heap.h"
#include "particle_system.h"
#include "particle_system_internal.h"
#include "scanline_effect.h"
#include "sys_task.h"
#include "sys_task_api.h"

// Particle mode of the leader effects: the main screen switches to 3D over a captured image of the field so particle
// emitters can be drawn over it. Also the HBlank BG offset wave used by several effects.

#if defined(SDK_PORT)
#define PARTICLE_HEAP_SIZE PARTICLE_WORK_SIZE(0x4800)
#else
#define PARTICLE_HEAP_SIZE 0x4800
#endif

enum EncounterEffectParticlesState {
    PARTICLES_STATE_WAIT_FRAME,
    PARTICLES_STATE_WAIT_REQUEST,
    PARTICLES_STATE_READY,
};

struct EncounterEffectParticles {
    u16 state;
    u8 setupDone;      // set once the 3D setup is complete
    u8 setupRequested; // set by EncounterEffect_RequestParticleSetup
    FieldSystem *fieldSystem;
    SPLEmitter *system;
    void *heap;
    u32 texOffset;  // next free texture VRAM offset
    u32 plttOffset; // next free palette VRAM offset
};

void MTX_Identity22_(MtxFx22 *mtx);

static void Task_ParticleSetup_VWait(SysTask *task, void *data);
static void Task_ParticleSetup_Main(SysTask *task, void *data);
static void EncounterEffect_SetupParticleGraphics(FieldSystem *fieldSystem);
static void EncounterEffect_FreeParticleGraphics(FieldSystem *fieldSystem);
static u32 EncounterEffectParticles_AllocTexVram(u32 size, BOOL is4x4Comp);
static u32 EncounterEffectParticles_AllocPlttVram(u32 size, BOOL is4Pltt);
static SysTask *EncounterEffectHBlankWave_StartTask(EncounterEffectHBlankWave *wave);
static void Task_EncounterEffectHBlankWave(SysTask *task, void *data);

void EncounterEffect_StartParticleMode(FieldSystem *fieldSystem) {
    GF_ASSERT(!gEncounterEffectState.particles);
    gEncounterEffectState.particles = Heap_Alloc(HEAP_ID_FIELD1, sizeof(EncounterEffectParticles));
    memset(gEncounterEffectState.particles, 0, sizeof(EncounterEffectParticles));
    gEncounterEffectState.particles->state = PARTICLES_STATE_WAIT_FRAME;
    gEncounterEffectState.particles->setupDone = FALSE;
    gEncounterEffectState.particles->fieldSystem = fieldSystem;
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_BG1, GF_PLANE_TOGGLE_OFF);
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_BG2, GF_PLANE_TOGGLE_OFF);
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_BG3, GF_PLANE_TOGGLE_OFF);
    GX_ResetBankForBG();
#if defined(SDK_PORT)
    MI_CpuClearFast((void *)HW_LCDC_VRAM_C, 0x20000); // VRAM bank C (LCDC)
#else
    MI_CpuClearFast((void *)0x06840000, 0x20000); // VRAM bank D (LCDC)
#endif
    // capture the main screen into VRAM bank D
    reg_GX_DISPCAPCNT = 0xC0320C04;
    SysTask_CreateOnVWaitQueue(Task_ParticleSetup_VWait, gEncounterEffectState.particles, 1024);
    SysTask_CreateOnMainQueue(Task_ParticleSetup_Main, gEncounterEffectState.particles, 1024);
}

void EncounterEffect_RequestParticleSetup(void) {
    EncounterEffectParticles *particles = gEncounterEffectState.particles;
    GF_ASSERT(particles);
    gEncounterEffectState.particles->setupRequested = TRUE;
}

u8 EncounterEffect_IsParticleSetupDone(void) {
    EncounterEffectParticles *particles = gEncounterEffectState.particles;
    GF_ASSERT(particles);
    return gEncounterEffectState.particles->setupDone;
}

void EncounterEffect_DrawParticles(void) {
    EncounterEffectParticles *particles = gEncounterEffectState.particles;
    if (particles) {
        ParticleSystem_DrawAll();
    }
}

void EncounterEffect_EndParticleMode(void) {
    EncounterEffectParticles *particles = gEncounterEffectState.particles;
    GF_ASSERT(particles);
    ParticleSystem_FreeAll();
    EncounterEffect_FreeParticleGraphics(gEncounterEffectState.particles->fieldSystem);
    FieldSystem_SetFieldMapRenderEnabled(gEncounterEffectState.particles->fieldSystem, TRUE);
    FieldSystem_SetFieldMapTextureFreeEnabled(gEncounterEffectState.particles->fieldSystem, TRUE);
    Heap_Free(gEncounterEffectState.particles);
    gEncounterEffectState.particles = NULL;
}

void EncounterEffect_LoadParticles(NARC *narc, int member) {
    GF_ASSERT(gEncounterEffectState.particles != NULL);
    GF_ASSERT(gEncounterEffectState.particles->system == NULL);
    gEncounterEffectState.particles->heap = Heap_Alloc(HEAP_ID_FIELD1, PARTICLE_HEAP_SIZE);
    gEncounterEffectState.particles->system = ParticleSystem_New(EncounterEffectParticles_AllocTexVram, EncounterEffectParticles_AllocPlttVram, gEncounterEffectState.particles->heap, PARTICLE_HEAP_SIZE, TRUE, HEAP_ID_FIELD1);
    GF_ASSERT(gEncounterEffectState.particles->system != NULL);
    ParticleSystem_SetPerspectiveType(gEncounterEffectState.particles->system, 1);
    Camera_SetPerspectiveClippingPlane(FX32_ONE, FX32_CONST(900), ParticleSystem_GetCamera(gEncounterEffectState.particles->system));
    {
        void *resource = GfGfxLoader_LoadFromOpenNarc(narc, member, FALSE, HEAP_ID_FIELD1, FALSE);
        ParticleSystem_SetResource(gEncounterEffectState.particles->system, resource, 0, FALSE);
    }
}

void EncounterEffect_CreateEmitters(u32 count) {
    VecFx32 position = { 0, 0, 0 };
    u32 i;
    GF_ASSERT(gEncounterEffectState.particles);
    GF_ASSERT(gEncounterEffectState.particles->system);
    for (i = 0; i < count; i++) {
        ParticleSystem_CreateEmitter(gEncounterEffectState.particles->system, i, &position);
    }
}

BOOL EncounterEffect_UpdateParticles(void) {
    EncounterEffectParticles *particles = gEncounterEffectState.particles;
    SPLEmitter *system;
    GF_ASSERT(particles);
    system = gEncounterEffectState.particles->system;
    if (!system) {
        return TRUE;
    }
    ParticleSystem_CalcAll();
    return ParticleSystem_GetActiveEmitterCount(gEncounterEffectState.particles->system) == 0;
}

BOOL EncounterEffect_AreParticlesDone(void) {
    return ParticleSystem_GetActiveEmitterCount(gEncounterEffectState.particles->system) == 0;
}

void EncounterEffect_FreeParticles(void) {
    GF_ASSERT(gEncounterEffectState.particles);
    ParticleSystem_Free(gEncounterEffectState.particles->system);
    Heap_Free(gEncounterEffectState.particles->heap);
    gEncounterEffectState.particles->system = NULL;
    gEncounterEffectState.particles->heap = NULL;
}

// Waits one frame for the capture, then sets up the 3D and bitmap BGs once the effect asks for it.
static void Task_ParticleSetup_VWait(SysTask *task, void *data) {
    EncounterEffectParticles *particles = data;
    switch (particles->state) {
    case PARTICLES_STATE_WAIT_FRAME:
        particles->state++;
        break;
    case PARTICLES_STATE_WAIT_REQUEST:
        if (particles->setupRequested == TRUE) {
            EncounterEffect_SetupParticleGraphics(particles->fieldSystem);
            SysTask_Destroy(task);
            particles->state++;
        }
        break;
    }
}

static void Task_ParticleSetup_Main(SysTask *task, void *data) {
    EncounterEffectParticles *particles = data;
    if (particles->state == PARTICLES_STATE_READY) {
        ParticleSystem_ZeroAll();
        VecFx32 position = { 0, 0, 0 };
        VecFx32 scale = gParticleBaseScale;
        MtxFx33 rotation;
        MTX_Identity33_(&rotation);
        NNS_G3dGlbSetBaseTrans(&position);
        NNS_G3dGlbSetBaseScale(&scale);
        NNS_G3dGlbSetBaseRot(&rotation);
        FieldSystem_SetFieldMapRenderEnabled(particles->fieldSystem, FALSE);
        FieldSystem_SetFieldMapTextureFreeEnabled(particles->fieldSystem, FALSE);
        SetBgPriority(GF_BG_LYR_MAIN_0, 0);
        particles->setupDone = TRUE;
        SysTask_Destroy(task);
    }
}

// 3D on BG0, the captured field image as a 256-colour bitmap on BG3 behind it; BG2 is cleared.
static void EncounterEffect_SetupParticleGraphics(FieldSystem *fieldSystem) {
    GraphicsBanks banks = gParticleGraphicsBanks;
    GraphicsModes modes;
    MtxFx22 matrix;
    BgTemplate template;
    GfGfx_SetBanks(&banks);
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_BG0, GF_PLANE_TOGGLE_OFF);
    modes = gParticleGraphicsModes;
    SetBothScreensModesAndDisable(&modes);
    FieldMap_ClearBgLayer3(fieldSystem->bgConfig);
    reg_G2_BG3CNT = (reg_G2_BG3CNT & 0x43) | 0x4884;
    SetBgPriority(GF_BG_LYR_MAIN_3, 3);
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_BG3, GF_PLANE_TOGGLE_ON);
    MTX_Identity22_(&matrix);
    G2_SetBG3Affine(&matrix, 0, 0, 0, 0);
    template = gParticleCaptureBgTemplate;
    InitBgFromTemplate(fieldSystem->bgConfig, GF_BG_LYR_MAIN_2, &template, GF_BG_TYPE_TEXT);
    BG_ClearCharDataRange(GF_BG_LYR_MAIN_2, 32, 0, HEAP_ID_FIELD1);
    BgClearTilemapBufferAndCommit(fieldSystem->bgConfig, GF_BG_LYR_MAIN_2);
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_OBJ, GF_PLANE_TOGGLE_ON);
}

static void EncounterEffect_FreeParticleGraphics(FieldSystem *fieldSystem) {
    FreeBgTilemapBuffer(fieldSystem->bgConfig, GF_BG_LYR_MAIN_2);
    Thunk_BgConfig_Init(fieldSystem->bgConfig);
}

static u32 EncounterEffectParticles_AllocTexVram(u32 size, BOOL is4x4Comp) {
    EncounterEffectParticles *particles;
    u32 offset, next;
    GF_ASSERT(gEncounterEffectState.particles);
    GF_ASSERT(!is4x4Comp);
    particles = gEncounterEffectState.particles;
    offset = particles->texOffset;
    next = offset + size;
    particles->texOffset = next;
    return offset;
}

static u32 EncounterEffectParticles_AllocPlttVram(u32 size, BOOL is4Pltt) {
    EncounterEffectParticles *particles;
    u32 offset, next;
    GF_ASSERT(gEncounterEffectState.particles);
    GF_ASSERT(!is4Pltt);
    particles = gEncounterEffectState.particles;
    offset = particles->plttOffset;
    next = offset + size;
    particles->plttOffset = next;
    return offset;
}

static SysTask *EncounterEffectHBlankWave_StartTask(EncounterEffectHBlankWave *wave) {
    return SysTask_CreateOnVBlankQueue(Task_EncounterEffectHBlankWave, wave, 1024);
}

static void Task_EncounterEffectHBlankWave(SysTask *task, void *data) {
    EncounterEffectHBlankWave *wave = data;
    if (wave->frames >= 2) {
        ScanlineEffect_SwapBuffers(wave->effect);
        wave->frames = 0;
    }
    ScanlineEffect_RestartDma(wave->effect);
    wave->frames++;
}

void EncounterEffectHBlankWave_Init(EncounterEffectHBlankWave *wave, enum HeapID heapID) {
    wave->effect = ScanlineEffect_New(heapID);
    wave->frames = 0;
    wave->task = EncounterEffectHBlankWave_StartTask(wave);
}

void EncounterEffectHBlankWave_Free(EncounterEffectHBlankWave *wave) {
    SysTask_Destroy(wave->task);
    ScanlineEffect_Stop(wave->effect);
    ScanlineEffect_Delete(wave->effect);
}

void EncounterEffectHBlankWave_Start(EncounterEffectHBlankWave *wave, u8 firstLine, u8 lastLine, u32 step, fx32 amplitude, s16 speed, void *destination, u32 fill, u32 priority) {
    ScanlineEffect_StartWave(wave->effect, firstLine, lastLine, step, amplitude, speed, destination, fill, priority);
}
