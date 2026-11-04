#include "field/encounter_effect.h"

#include <nitro/os/tick.h>

#include "global.h"

#include "brightness.h"
#include "filesystem.h"
#include "heap.h"
#include "overlay_manager.h"
#include "poke_overlay.h"
#include "sys_task.h"
#include "sys_task_api.h"
#include "systask_environment.h"

FS_EXTERN_OVERLAY(OVY_114);
FS_EXTERN_OVERLAY(OVY_115);
FS_EXTERN_OVERLAY(OVY_116);
FS_EXTERN_OVERLAY(OVY_117);
FS_EXTERN_OVERLAY(OVY_118);
FS_EXTERN_OVERLAY(OVY_119);
FS_EXTERN_OVERLAY(OVY_120);

enum EncounterEffectFlashState {
    ENCOUNTER_FLASH_STATE_FADE_OTHER,
    ENCOUNTER_FLASH_STATE_START,
    ENCOUNTER_FLASH_STATE_WAIT,
    ENCOUNTER_FLASH_STATE_START_BACK,
    ENCOUNTER_FLASH_STATE_WAIT_BACK,
    ENCOUNTER_FLASH_STATE_END,
};

// Master brightness fade of one screen (1: main, otherwise sub).
typedef struct BrightnessInterp {
    EncounterEffectInterp interp;
    int screen;
} BrightnessInterp;

// The screen flashes count times between 0 and brightness while the other screen fades to otherBrightness.
typedef struct EncounterEffectFlash {
    u32 state;
    int count;
    int flashes;
    int screen;
    int brightness;
    int otherBrightness;
    BrightnessInterp flash;
    BrightnessInterp otherFade;
    BOOL *done;
} EncounterEffectFlash;

void GXx_SetMasterBrightness_(vu16 *reg, s32 brightness);

static void Task_EncounterEffectFlash(SysTask *task, void *data);
static void EncounterEffect_SetMasterBrightness(int screen, s32 brightness);
static void BrightnessInterp_Init(BrightnessInterp *interp, s32 start, s32 end, int screen, s32 duration);
static BOOL BrightnessInterp_Update(BrightnessInterp *interp);
static void Task_ApplyBrightness(SysTask *task, void *data);

static void Task_RunEncounterEffect(SysTask *task, void *data);

static void Task_RunEncounterEffect(SysTask *task, void *data) {
    EncounterEffectTask *effect = data;
    gEncounterEffectFuncs[gEncounterEffectState.effect](task, effect);
    gEncounterEffectState.frameCount++;
    OS_GetTick();
    OS_SetTick(0);
}

void EncounterEffect_LoadOverlay(s32 effect) {
    if (effect >= 12 && effect <= 33) {
        HandleLoadOverlay(FS_OVERLAY_ID(OVY_115), OVY_LOAD_ASYNC);
        return;
    }
    if ((u32)(effect - 34) <= 2) {
        HandleLoadOverlay(FS_OVERLAY_ID(OVY_116), OVY_LOAD_ASYNC);
        return;
    }
    if (effect >= 39 && effect <= 44) {
        HandleLoadOverlay(FS_OVERLAY_ID(OVY_117), OVY_LOAD_ASYNC);
        return;
    }
    if (effect == 45) {
        HandleLoadOverlay(FS_OVERLAY_ID(OVY_118), OVY_LOAD_ASYNC);
        return;
    }
    if (effect >= 0 && effect <= 5) {
        HandleLoadOverlay(FS_OVERLAY_ID(OVY_120), OVY_LOAD_ASYNC);
        return;
    }
    if (effect >= 6 && effect <= 11) {
        HandleLoadOverlay(FS_OVERLAY_ID(OVY_119), OVY_LOAD_ASYNC);
        return;
    }
    if (effect == 46) {
        HandleLoadOverlay(FS_OVERLAY_ID(OVY_120), OVY_LOAD_ASYNC);
        return;
    }
    HandleLoadOverlay(FS_OVERLAY_ID(OVY_114), OVY_LOAD_ASYNC);
}

void EncounterEffect_UnloadOverlay(s32 effect) {
    if (effect >= 12 && effect <= 33) {
        UnloadOverlayByID(FS_OVERLAY_ID(OVY_115));
        return;
    }
    if ((u32)(effect - 34) <= 2) {
        UnloadOverlayByID(FS_OVERLAY_ID(OVY_116));
        return;
    }
    if (effect >= 39 && effect <= 44) {
        UnloadOverlayByID(FS_OVERLAY_ID(OVY_117));
        return;
    }
    if (effect == 45) {
        UnloadOverlayByID(FS_OVERLAY_ID(OVY_118));
        return;
    }
    if (effect >= 0 && effect <= 5) {
        UnloadOverlayByID(FS_OVERLAY_ID(OVY_120));
        return;
    }
    if (effect >= 6 && effect <= 11) {
        UnloadOverlayByID(FS_OVERLAY_ID(OVY_119));
        return;
    }
    if (effect == 46) {
        UnloadOverlayByID(FS_OVERLAY_ID(OVY_120));
        return;
    }
    UnloadOverlayByID(FS_OVERLAY_ID(OVY_114));
}

void EncounterEffect_Start(s32 effect, FieldSystem *fieldSystem, BOOL *done) {
    EncounterEffectTask *effectTask;
    SysTask *task;
    BOOL *taskDone;
    gEncounterEffectState.effect = effect;
    gEncounterEffectState.frameCount = 0;
    task = CreateSysTaskAndEnvironment(Task_RunEncounterEffect, sizeof(EncounterEffectTask), 5, HEAP_ID_FIELD1);
    effectTask = SysTask_GetData(task);
    effectTask->fieldSystem = fieldSystem;
    effectTask->done = done;
    effectTask->narc = NARC_New(NARC_a_1_0_9, HEAP_ID_FIELD1);
    taskDone = effectTask->done;
    if (taskDone) {
        *taskDone = FALSE;
    }
    effectTask->wipeDone = FALSE;
}

void EncounterEffect_Finish(EncounterEffectTask *effect, SysTask *task) {
    NARC_Delete(effect->narc);
    Heap_FreeExplicit(HEAP_ID_FIELD1, effect->work);
    DestroySysTaskAndEnvironment(task);
}

void EncounterEffect_StartFlash(int screen, int brightness, int otherBrightness, BOOL *done, int count) {
    EncounterEffectFlash *flash = Heap_Alloc(HEAP_ID_FIELD1, sizeof(EncounterEffectFlash));
    memset(flash, 0, sizeof(EncounterEffectFlash));
    SysTask_CreateOnMainQueue(Task_EncounterEffectFlash, flash, 5);
    flash->done = done;
    if (done) {
        *done = FALSE;
    }
    flash->screen = screen;
    flash->brightness = brightness;
    flash->otherBrightness = otherBrightness;
    flash->count = count;
}

static void Task_EncounterEffectFlash(SysTask *task, void *data) {
    EncounterEffectFlash *flash = data;
    switch (flash->state) {
    case ENCOUNTER_FLASH_STATE_FADE_OTHER:
        if (flash->screen == SCREEN_MASK_MAIN) {
            BrightnessInterp_Init(&flash->otherFade, 0, flash->otherBrightness, SCREEN_MASK_SUB, 8);
        } else if (flash->screen == SCREEN_MASK_SUB) {
            BrightnessInterp_Init(&flash->otherFade, 0, flash->otherBrightness, SCREEN_MASK_MAIN, 8);
        }
        ++flash->state;
        break;
    case ENCOUNTER_FLASH_STATE_START:
        BrightnessInterp_Init(&flash->flash, 0, flash->brightness, flash->screen, 3);
        ++flash->state;
        break;
    case ENCOUNTER_FLASH_STATE_WAIT:
        if (BrightnessInterp_Update(&flash->flash)) {
            ++flash->state;
        }
        break;
    case ENCOUNTER_FLASH_STATE_START_BACK:
        BrightnessInterp_Init(&flash->flash, flash->brightness, 0, flash->screen, 3);
        ++flash->state;
        break;
    case ENCOUNTER_FLASH_STATE_WAIT_BACK:
        if (BrightnessInterp_Update(&flash->flash)) {
            ++flash->flashes;
            if (flash->flashes == flash->count) {
                flash->state = ENCOUNTER_FLASH_STATE_END;
            } else {
                flash->state = ENCOUNTER_FLASH_STATE_START;
            }
        }
        break;
    case ENCOUNTER_FLASH_STATE_END:
        flash->state = ENCOUNTER_FLASH_STATE_FADE_OTHER;
        flash->flashes = 0;
        if (flash->done != NULL) {
            *flash->done = TRUE;
        }
        SysTask_Destroy(task);
        Heap_Free(flash);
        return;
    }
    BrightnessInterp_Update(&flash->otherFade);
}

BOOL EncounterEffect_IsWipeDone(EncounterEffectTask *effect) {
    return effect->wipeDone;
}

void EncounterEffectInterp_Init(EncounterEffectInterp *interp, s32 start, s32 end, s32 duration) {
    interp->value = start;
    interp->start = start;
    interp->delta = end - start;
    interp->duration = duration;
    interp->frame = 0;
}

BOOL EncounterEffectInterp_Update(EncounterEffectInterp *interp) {
    s32 value = interp->delta * interp->frame / interp->duration;
    value += interp->start;
    interp->value = value;
    if (interp->frame + 1 <= interp->duration) {
        interp->frame++;
        return FALSE;
    }
    interp->frame = interp->duration;
    return TRUE;
}

void EncounterEffectInterp_InitFx(EncounterEffectInterp *interp, s32 start, s32 end, s32 duration) {
    interp->value = start;
    interp->start = start;
    interp->delta = end - start;
    interp->duration = duration;
    interp->frame = 0;
}

BOOL EncounterEffectInterp_UpdateFx(EncounterEffectInterp *interp) {
    s64 product = (s64)interp->delta * (interp->frame << FX32_SHIFT);
    s32 value = FX_Div((product + FX32_HALF) >> FX32_SHIFT, interp->duration << FX32_SHIFT);
    value += interp->start;
    interp->value = value;
    if (interp->frame + 1 <= interp->duration) {
        interp->frame++;
        return FALSE;
    }
    interp->frame = interp->duration;
    return TRUE;
}

void EncounterEffectQuadInterp_Init(EncounterEffectQuadInterp *interp, s32 start, s32 end, s32 velocity, s32 duration) {
    s32 linearTotal = (s32)(((s64)velocity * (duration << FX32_SHIFT) + FX32_HALF) >> FX32_SHIFT);
    s32 remaining = end - start - linearTotal;
    s32 numerator = (s32)((((s64)remaining << 13) + FX32_HALF) >> FX32_SHIFT);
    s32 denominator = (duration * duration) << FX32_SHIFT;
    s32 acceleration = FX_Div(numerator, denominator);
    interp->value = start;
    interp->start = start;
    interp->velocity = velocity;
    interp->acceleration = acceleration;
    interp->frame = 0;
    interp->duration = duration;
}

BOOL EncounterEffectQuadInterp_Update(EncounterEffectQuadInterp *interp) {
    s32 frame = interp->frame;
    s32 linear = (s32)(((s64)interp->velocity * (frame << FX32_SHIFT) + FX32_HALF) >> FX32_SHIFT);
    s32 square = (frame * frame) << FX32_SHIFT;
    s32 quad = (s32)(((s64)interp->acceleration * square + FX32_HALF) >> FX32_SHIFT);
    interp->value = linear + FX_Div(quad, FX32_CONST(2)) + interp->start;
    if (interp->frame + 1 <= interp->duration) {
        interp->frame++;
        return FALSE;
    }
    interp->frame = interp->duration;
    return TRUE;
}

static void EncounterEffect_SetMasterBrightness(int screen, s32 brightness) {
    if (screen == SCREEN_MASK_MAIN) {
        GXx_SetMasterBrightness_(&reg_GX_MASTER_BRIGHT, brightness);
    } else {
        GXx_SetMasterBrightness_(&reg_GXS_DB_MASTER_BRIGHT, brightness);
    }
}

static void BrightnessInterp_Init(BrightnessInterp *interp, s32 start, s32 end, int screen, s32 duration) {
    interp->screen = screen;
    EncounterEffectInterp_Init(&interp->interp, start, end, duration);
}

static BOOL BrightnessInterp_Update(BrightnessInterp *interp) {
    BOOL finished = EncounterEffectInterp_Update(&interp->interp);
    SysTask_CreateOnVWaitQueue(Task_ApplyBrightness, interp, 10);
    return finished;
}

static void Task_ApplyBrightness(SysTask *task, void *data) {
    BrightnessInterp *interp = data;
    EncounterEffect_SetMasterBrightness(interp->screen, interp->interp.value);
    SysTask_Destroy(task);
}
#if defined(SDK_PORT)

/* Native tables from asm/encounter_effect_data.s; adapters give differing originals the table's signature. */
void EncounterEffect_WildRowsWhite(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_WildRowsBlack(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_WildWaveWhite(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_WildWaveBlack(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_WildDiagonalsWhite(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_WildDiagonalsBlack(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_TrainerSpinApart(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_TrainerSpinZoom(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_TrainerBallDrop(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_TrainerFadeSpin(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_TrainerBallFall(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_TrainerRisingBall(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_LeaderFalkner(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_LeaderBugsy(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_LeaderWhitney(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_LeaderMorty(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_LeaderJasmine(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_LeaderChuck(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_LeaderPryce(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_LeaderClair(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_LeaderBrock(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_LeaderMisty(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_LeaderLtSurge(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_LeaderErika(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_LeaderJanine(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_LeaderSabrina(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_LeaderBlaine(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_LeaderBlue(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_Rival(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_EliteFourWill(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_EliteFourKoga(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_EliteFourBruno(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_EliteFourKaren(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_Champion(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_Legendary(void *sysTask, void *task);
void EncounterEffect_HoOh(void *sysTask, void *task);
void EncounterEffect_Lugia(void *sysTask, void *task);
void EncounterEffect_FrontierOrLinkSingle(SysTask *task, EncounterEffectTask *ctx);
void EncounterEffect_Double(SysTask *task, EncounterEffectTask *ctx);
void EncounterEffect_TeamRocket(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_ExecutivePetrel(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_ExecutiveProton(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_ExecutiveArcher(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_ExecutiveAriana(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_RocketBoss(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_KimonoGirl(SysTask *task, EncounterEffectTask *effect);
void EncounterEffect_PkmnTrainerRed(SysTask *task, EncounterEffectTask *effect);
static void Native_EncounterEffect_Legendary(SysTask * a0, EncounterEffectTask * a1) { EncounterEffect_Legendary((void *)a0, (void *)a1); }
static void Native_EncounterEffect_HoOh(SysTask * a0, EncounterEffectTask * a1) { EncounterEffect_HoOh((void *)a0, (void *)a1); }
static void Native_EncounterEffect_Lugia(SysTask * a0, EncounterEffectTask * a1) { EncounterEffect_Lugia((void *)a0, (void *)a1); }
static void Native_EncounterEffect_FrontierOrLinkSingle(SysTask * a0, EncounterEffectTask * a1) { EncounterEffect_FrontierOrLinkSingle((void *)a0, (void *)a1); }
static void Native_EncounterEffect_Double(SysTask * a0, EncounterEffectTask * a1) { EncounterEffect_Double((void *)a0, (void *)a1); }
void (*const gEncounterEffectFuncs[])(SysTask *task, EncounterEffectTask *effect) = {
    EncounterEffect_WildRowsWhite,
    EncounterEffect_WildRowsBlack,
    EncounterEffect_WildWaveWhite,
    EncounterEffect_WildWaveBlack,
    EncounterEffect_WildDiagonalsWhite,
    EncounterEffect_WildDiagonalsBlack,
    EncounterEffect_TrainerSpinApart,
    EncounterEffect_TrainerSpinZoom,
    EncounterEffect_TrainerBallDrop,
    EncounterEffect_TrainerFadeSpin,
    EncounterEffect_TrainerBallFall,
    EncounterEffect_TrainerRisingBall,
    EncounterEffect_LeaderFalkner,
    EncounterEffect_LeaderBugsy,
    EncounterEffect_LeaderWhitney,
    EncounterEffect_LeaderMorty,
    EncounterEffect_LeaderJasmine,
    EncounterEffect_LeaderChuck,
    EncounterEffect_LeaderPryce,
    EncounterEffect_LeaderClair,
    EncounterEffect_LeaderBrock,
    EncounterEffect_LeaderMisty,
    EncounterEffect_LeaderLtSurge,
    EncounterEffect_LeaderErika,
    EncounterEffect_LeaderJanine,
    EncounterEffect_LeaderSabrina,
    EncounterEffect_LeaderBlaine,
    EncounterEffect_LeaderBlue,
    EncounterEffect_Rival,
    EncounterEffect_EliteFourWill,
    EncounterEffect_EliteFourKoga,
    EncounterEffect_EliteFourBruno,
    EncounterEffect_EliteFourKaren,
    EncounterEffect_Champion,
    Native_EncounterEffect_Legendary,
    Native_EncounterEffect_HoOh,
    Native_EncounterEffect_Lugia,
    Native_EncounterEffect_FrontierOrLinkSingle,
    Native_EncounterEffect_Double,
    EncounterEffect_TeamRocket,
    EncounterEffect_ExecutivePetrel,
    EncounterEffect_ExecutiveProton,
    EncounterEffect_ExecutiveArcher,
    EncounterEffect_ExecutiveAriana,
    EncounterEffect_RocketBoss,
    EncounterEffect_KimonoGirl,
    EncounterEffect_PkmnTrainerRed,
};

/* Native storage for the .bss labels of asm/encounter_effect_data.s. */
EncounterEffectState gEncounterEffectState;
#endif
