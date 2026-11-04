#include "battle/battle_anim_util.h"

#include "global.h"

#include "battle/battle_anim_emitter.h"
#include "battle/battle_anim_internal.h"

#include "geometry_math.h"
#include "heap.h"
#include "palette.h"
#include "pokepic.h"
#include "scanline_effect.h"
#include "sprite_system.h"
#include "sys_task.h"
#include "sys_task_api.h"

void BattleAnimUtil_InitSinMotionFromStep(BattleAnimSinMotion *motion, u32 startX, u32 endX, u32 startY, u16 endY, fx32 ampX, fx32 ampY, u16 step) {
    s16 signedStep;

    GF_ASSERT(motion != NULL);
    if (startX > endX) {
        step = -step;
    }
    signedStep = step;
    motion->duration = BattleAnimUtil_CalcStepCount(startX << FX32_SHIFT, endX << FX32_SHIFT, signedStep << FX32_SHIFT);
    motion->angleX = startX;
    motion->ampX = ampX;
    motion->angleY = startY;
    motion->ampY = ampY;
    motion->stepX = signedStep;
    motion->stepY = ((int)endY - (int)startY) / motion->duration;
}

BOOL BattleAnimUtil_UpdateSinMotion(BattleAnimSinMotion *motion) {
    GF_ASSERT(motion != NULL);
    if (motion->duration != 0) {
        motion->angleX += motion->stepX;
        motion->angleY += motion->stepY;
        motion->angleX = (u16)motion->angleX;
        motion->angleY = (u16)motion->angleY;
        motion->duration--;
        motion->x = FX_Mul(FX_SinIdx(motion->angleX), motion->ampX) >> FX32_SHIFT;
        motion->y = FX_Mul(FX_CosIdx(motion->angleY), motion->ampY) >> FX32_SHIFT;
        return TRUE;
    }
    return FALSE;
}

BOOL BattleAnimUtil_UpdateSinMotionSprite(BattleAnimSinMotion *motion, s16 x, s16 y, ManagedSprite *sprite) {
    if (BattleAnimUtil_UpdateSinMotion(motion)) {
        BattleAnimUtil_SetSpritePosWithOffset(motion, sprite, x, y);
        return TRUE;
    }
    return FALSE;
}

BOOL BattleAnimUtil_UpdateSinMotionPokepic(BattleAnimSinMotion *motion, int x, int y, Pokepic *pokepic) {
    if (BattleAnimUtil_UpdateSinMotion(motion)) {
        BattleAnimUtil_SetPokepicPosWithOffset(motion, pokepic, x, y);
        return TRUE;
    }
    return FALSE;
}

void BattleAnimUtil_InitLinearMotion(BattleAnimLinearMotion *motion, s16 startX, s16 endX, s16 startY, s16 endY, u16 duration) {
    GF_ASSERT(motion != NULL);
    motion->x = startX;
    motion->y = startY;
    motion->duration = duration;
    motion->stepX = BattleAnimUtil_CalcStep(startX << FX32_SHIFT, endX << FX32_SHIFT, duration);
    motion->stepY = BattleAnimUtil_CalcStep(startY << FX32_SHIFT, endY << FX32_SHIFT, duration);
    motion->curX = startX << FX32_SHIFT;
    motion->curY = startY << FX32_SHIFT;
}

BOOL BattleAnimUtil_UpdateLinearMotion(BattleAnimLinearMotion *motion) {
    GF_ASSERT(motion != NULL);
    if (motion->duration != 0) {
        motion->curX += motion->stepX;
        motion->curY += motion->stepY;
        motion->x = motion->curX >> FX32_SHIFT;
        motion->y = motion->curY >> FX32_SHIFT;
        motion->duration--;
        return TRUE;
    }
    return FALSE;
}

BOOL BattleAnimUtil_UpdateLinearMotionSprite(BattleAnimLinearMotion *motion, ManagedSprite *sprite) {
    if (BattleAnimUtil_UpdateLinearMotion(motion)) {
        BattleAnimUtil_SetSpritePosWithOffset(motion, sprite, 0, 0);
        return TRUE;
    }
    return FALSE;
}

BOOL BattleAnimUtil_UpdateLinearMotionPokepic(BattleAnimLinearMotion *motion, Pokepic *pokepic) {
    if (BattleAnimUtil_UpdateLinearMotion(motion)) {
        BattleAnimUtil_SetPokepicPosWithOffset(motion, pokepic, 0, 0);
        return TRUE;
    }
    return FALSE;
}

// Linear motion plus a vertical arc (half a cosine period) of the given amplitude
void BattleAnimUtil_InitLinearSinMotion(BattleAnimLinearMotion *linear, BattleAnimSinMotion *sin, s16 startX, s16 endX, s16 startY, s16 endY, u16 duration, fx32 amplitude) {
    BattleAnimUtil_InitLinearMotion(linear, startX, endX, startY, endY, duration);
    sin->x = 0;
    sin->y = 0;
    BattleAnimUtil_InitSinMotion(sin, 0, 0, 0x3FFF, 0xBFFF, 0, amplitude, duration);
}

BOOL BattleAnimUtil_UpdateLinearSinMotion(BattleAnimLinearMotion *linear, BattleAnimSinMotion *sin) {
    BOOL linearActive;
    BOOL sinActive;

    GF_ASSERT(linear != NULL);
    GF_ASSERT(sin != NULL);
    linearActive = BattleAnimUtil_UpdateLinearMotion(linear);
    sinActive = BattleAnimUtil_UpdateSinMotion(sin);
    linear->x += sin->x;
    linear->y += sin->y;
    if (linearActive == sinActive) {
        if (!linearActive) {
            return FALSE;
        }
    }
    return TRUE;
}

BOOL BattleAnimUtil_UpdateLinearSinMotionSprite(BattleAnimLinearMotion *linear, BattleAnimSinMotion *sin, ManagedSprite *sprite) {
    if (BattleAnimUtil_UpdateLinearSinMotion(linear, sin)) {
        BattleAnimUtil_SetSpritePosWithOffset(linear, sprite, 0, 0);
        return TRUE;
    }
    return FALSE;
}

void BattleAnimUtil_InitValueMotion(BattleAnimValueMotion *motion, int start, int end, int duration) {
    GF_ASSERT(motion != NULL);
    motion->value = start;
    motion->duration = duration;
    motion->step = BattleAnimUtil_CalcStep(start << FX32_SHIFT, end << FX32_SHIFT, duration) >> FX32_SHIFT;
}

void BattleAnimUtil_InitValueMotionFx(BattleAnimValueMotion *motion, int start, int end, int duration) {
    GF_ASSERT(motion != NULL);
    motion->value = start;
    motion->duration = duration;
    motion->step = BattleAnimUtil_CalcStep(start << FX32_SHIFT, end << FX32_SHIFT, duration);
    motion->current = start << FX32_SHIFT;
}

BOOL BattleAnimUtil_UpdateValueMotion(BattleAnimValueMotion *motion) {
    GF_ASSERT(motion != NULL);
    if (motion->duration != 0) {
        motion->value += motion->step;
        motion->duration--;
        return TRUE;
    }
    return FALSE;
}

BOOL BattleAnimUtil_UpdateValueMotionFx(BattleAnimValueMotion *motion) {
    GF_ASSERT(motion != NULL);
    if (motion->duration != 0) {
        motion->current += motion->step;
        motion->value = motion->current >> FX32_SHIFT;
        motion->duration--;
        return TRUE;
    }
    return FALSE;
}

void BattleAnimUtil_InitCosMotion(BattleAnimCosMotion *motion, u32 startAngle, u32 endAngle, u32 amplitude, u32 duration) {
    motion->duration = duration;
    motion->angle = startAngle;
    motion->amplitude = amplitude;
    motion->angleStep = (endAngle - startAngle) / duration;
}

BOOL BattleAnimUtil_UpdateCosMotion(BattleAnimCosMotion *motion) {
    GF_ASSERT(motion != NULL);
    if (motion->duration != 0) {
        motion->angle = (u16)(motion->angle + motion->angleStep);
        motion->duration--;
        motion->value = FX_Mul(FX_CosIdx(motion->angle), motion->amplitude) >> FX32_SHIFT;
        return TRUE;
    }
    return FALSE;
}

// Uniform scale from start / divisor to end / divisor (x and y in 1/256 units)
void BattleAnimUtil_InitScaleMotion(BattleAnimLinearMotion *motion, int start, int divisor, int end, int duration) {
    GF_ASSERT(motion != NULL);
    motion->duration = duration;
    int startScale = (start << 8) / divisor;
    int endScale = (end << 8) / divisor;
    motion->stepX = BattleAnimUtil_CalcStep(startScale << FX32_SHIFT, endScale << FX32_SHIFT, duration);
    motion->x = startScale;
    motion->y = startScale;
    motion->curX = motion->x << FX32_SHIFT;
    motion->curY = motion->y << FX32_SHIFT;
}

BOOL BattleAnimUtil_UpdateScaleMotion(BattleAnimLinearMotion *motion) {
    GF_ASSERT(motion != NULL);
    if (motion->duration != 0) {
        motion->duration--;
        motion->curX += motion->stepX;
        motion->curY += motion->stepX;
        motion->x = motion->curX >> FX32_SHIFT;
        motion->y = motion->curY >> FX32_SHIFT;
        return TRUE;
    }
    return FALSE;
}

void BattleAnimUtil_InitScaleMotionXY(BattleAnimLinearMotion *motion, int startX, int endX, int startY, s16 endY, s16 divisor, int duration) {
    GF_ASSERT(motion != NULL);
    motion->duration = duration;
    int startScaleX = (startX << 8) / divisor;
    int endScaleX = (endX << 8) / divisor;
    motion->stepX = BattleAnimUtil_CalcStep(startScaleX << FX32_SHIFT, endScaleX << FX32_SHIFT, duration);
    int startScaleY = (startY << 8) / divisor;
    int endScaleY = (endY << 8) / divisor;
    motion->stepY = BattleAnimUtil_CalcStep(startScaleY << FX32_SHIFT, endScaleY << FX32_SHIFT, duration);
    motion->x = startScaleX;
    motion->y = startScaleY;
    motion->curX = motion->x << FX32_SHIFT;
    motion->curY = motion->y << FX32_SHIFT;
}

BOOL BattleAnimUtil_UpdateScaleMotionXY(BattleAnimLinearMotion *motion) {
    GF_ASSERT(motion != NULL);
    if (motion->duration != 0) {
        motion->duration--;
        motion->curX += motion->stepX;
        motion->curY += motion->stepY;
        motion->x = motion->curX >> FX32_SHIFT;
        motion->y = motion->curY >> FX32_SHIFT;
        return TRUE;
    }
    return FALSE;
}

void BattleAnimUtil_GetScaleFloat(BattleAnimLinearMotion *motion, f32 *x, f32 *y) {
    *x = (f32)motion->x / 256.0f;
    *y = (f32)motion->y / 256.0f;
}

// Half the height an 80 pixel tall picture (less 2 * inset) loses at the given scale (1/256 units)
s16 BattleAnimUtil_CalcScaleYOffset(int y, int inset, fx32 scale) {
    fx32 whole;
    fx32 height = (80 - (inset << 1)) << FX32_SHIFT;
    fx32 scaled = FX_Mul(height, scale);
    fx32 offset = height - FX_Div(scaled, 256 << FX32_SHIFT);
    fx32 frac = FX_Modf(offset, &whole);
    if (frac != 0) {
        frac += FX32_ONE / 2;
    }
    int pixels = (whole + frac) >> FX32_SHIFT;
    return pixels / 2;
}

void BattleAnimUtil_AlignPokepicToScale(Pokepic *pokepic, int y, int inset, fx32 scale, int mode) {
    int offset = BattleAnimUtil_CalcScaleYOffset(y, inset, scale);
    if (mode == 1) {
        offset = (s16)(offset * ~0u);
        y = (s16)(y - inset);
    }
    Pokepic_SetAttr(pokepic, POKEPIC_Y, y + offset);
}

void BattleAnimUtil_AlignSpriteToScale(ManagedSprite *sprite, int y, int inset, fx32 scale, int mode) {
    s16 spriteX;
    s16 spriteY;
    int offset = BattleAnimUtil_CalcScaleYOffset(y, inset, scale);
    if (mode == 1) {
        offset = (s16)(offset * ~0u);
        y = (s16)(y - inset);
    }
    ManagedSprite_GetPositionXY(sprite, &spriteX, &spriteY);
    ManagedSprite_SetPositionXY(sprite, spriteX, y + offset);
}

BOOL BattleAnimUtil_UpdateScaleMotionSprite(BattleAnimLinearMotion *motion, ManagedSprite *sprite) {
    if (BattleAnimUtil_UpdateScaleMotion(motion)) {
        BattleAnimUtil_SetSpriteScale(motion, sprite);
        return TRUE;
    }
    return FALSE;
}

BOOL BattleAnimUtil_UpdateScaleMotionPokepic(BattleAnimLinearMotion *motion, Pokepic *pokepic) {
    if (BattleAnimUtil_UpdateScaleMotion(motion)) {
        BattleAnimUtil_SetPokepicScale(motion, pokepic);
        return TRUE;
    }
    return FALSE;
}

BOOL BattleAnimUtil_UpdateScaleMotionXYPokepic(BattleAnimLinearMotion *motion, Pokepic *pokepic) {
    if (BattleAnimUtil_UpdateScaleMotionXY(motion)) {
        BattleAnimUtil_SetPokepicScale(motion, pokepic);
        return TRUE;
    }
    return FALSE;
}

void BattleAnimUtil_InitShake(BattleAnimShake *shake, int ampX, int ampY, int interval, int count) {
    GF_ASSERT(shake != NULL);
    shake->timer = interval;
    shake->interval = interval;
    shake->count = count;
    shake->phase = 0;
    shake->ampX = ampX;
    shake->ampY = ampY;
    shake->prevX = -ampX;
    shake->prevY = -ampY;
    shake->x = 0;
    shake->y = 0;
}

BOOL BattleAnimUtil_UpdateShake(BattleAnimShake *shake) {
    GF_ASSERT(shake != NULL);
    if (shake->count != 0) {
        shake->timer++;
        if (shake->timer >= shake->interval) {
            shake->timer = 0;
            int prevX = shake->prevX;
            shake->prevX = shake->x;
            if (prevX == 0) {
                shake->x = 0;
            } else {
                shake->x = -prevX;
            }
            int prevY = shake->prevY;
            shake->prevY = shake->y;
            if (prevY == 0) {
                shake->y = 0;
            } else {
                shake->y = -prevY;
            }
            shake->phase++;
            if (shake->phase >= 4) {
                shake->phase = 0;
                shake->count--;
            }
        }
        return TRUE;
    }
    return FALSE;
}

BOOL BattleAnimUtil_UpdateShakePokepic(BattleAnimShake *shake, int x, int y, Pokepic *pokepic) {
    if (BattleAnimUtil_UpdateShake(shake)) {
        BattleAnimUtil_SetPokepicPosWithOffset(shake, pokepic, x, y);
        return TRUE;
    }
    return FALSE;
}

void BattleAnimUtil_InitSpriteSequence(BattleAnimSpriteSequence *sequence, const BattleAnimSinMotion *motion, BattleAnimSinMotionFunc update, s16 x, s16 y, u16 interval, u8 count, u8 scale, ManagedSprite *sprite0, ManagedSprite *sprite1, ManagedSprite *sprite2, ManagedSprite *sprite3) {
    int i;

    sequence->x = x;
    sequence->y = y;
    sequence->update = update;
    sequence->interval = interval;
    sequence->timer = 0;
    sequence->count = count;
    sequence->started = 0;
    sequence->scale = scale;
    for (i = 0; i < count; i++) {
        sequence->motions[i] = *motion;
    }
    sequence->sprites[0] = sprite0;
    sequence->sprites[1] = sprite1;
    sequence->sprites[2] = sprite2;
    sequence->sprites[3] = sprite3;
    for (i = 0; i < sequence->count; i++) {
        ManagedSprite_SetDrawFlag(sequence->sprites[i], FALSE);
    }
}

BOOL BattleAnimUtil_UpdateSpriteSequence(BattleAnimSpriteSequence *sequence) {
    BOOL active[4];
    int i;

    for (i = 0; i < 4; i++) {
        active[i] = TRUE;
    }

    if (sequence->interval <= sequence->timer) {
        ManagedSprite_SetDrawFlag(sequence->sprites[sequence->started], TRUE);
        sequence->started++;
        sequence->timer = 0;
    }
    if (sequence->started < sequence->count) {
        sequence->timer++;
    }
    for (i = 0; i < sequence->started; i++) {
        active[i] = sequence->update(&sequence->motions[i]);
        if (active[i]) {
            if (!sequence->scale) {
                ManagedSprite_SetPositionXY(sequence->sprites[i], sequence->x + sequence->motions[i].x, sequence->y + sequence->motions[i].y);
            } else {
                f32 x;
                f32 y;
                BattleAnimUtil_GetScaleFloat((BattleAnimLinearMotion *)&sequence->motions[i], &x, &y);
                ManagedSprite_SetAffineScale(sequence->sprites[i], x, y);
            }
        } else {
            ManagedSprite_SetDrawFlag(sequence->sprites[i], FALSE);
        }
    }
    for (i = 0; i < sequence->count; i++) {
        if (active[i] == TRUE) {
            return TRUE;
        }
    }
    return FALSE;
}

void BattleAnimUtil_InitCircleMotion(BattleAnimSinMotion *motion, int multiplier, int duration) {
    BattleAnimUtil_InitSinMotion(motion, 0, 0xFFFF, 0, 0xFFFF, 2 << 16, -0x8000, duration);
    motion->duration *= multiplier;
}

void BattleAnimTask_BlendAlphaMotion(SysTask *task, void *data) {
    BattleAnimBlendAlphaMotion *blend = data;
    BattleAnimLinearMotion *motion = &blend->motion;

    if (!BattleAnimUtil_UpdateLinearMotion(motion)) {
        blend->done = TRUE;
        SysTask_Destroy(task);
        return;
    }
    if (motion->x < 0) {
        motion->x = 0;
    }
    if (motion->y < 0) {
        motion->y = 0;
    }
    reg_G2_BLDALPHA = motion->x | (motion->y << 8);
}

void BattleAnimUtil_StartBlendAlphaMotion(BattleAnimBlendAlphaMotion *blend, s16 startEva, s16 endEva, s16 startEvb, s16 endEvb, int duration) {
    BattleAnimUtil_InitLinearMotion(&blend->motion, startEva, endEva, startEvb, endEvb, duration);
    blend->done = FALSE;
    SysTask_CreateOnMainQueue(BattleAnimTask_BlendAlphaMotion, blend, 0);
}

BOOL BattleAnimUtil_IsBlendAlphaMotionDone(BattleAnimBlendAlphaMotion *blend) {
    return blend->done;
}

static void BattleAnimScanline_VWaitTask(SysTask *task, void *data) {
    BattleAnimScanlineTasks *tasks = data;

    if (tasks->active) {
        tasks->pending = TRUE;
        tasks->vwaitCallback(tasks->callbackArg);
    } else {
        HBlankDma_Stop();
    }
}

static void BattleAnimScanline_VBlankTask(SysTask *task, void *data) {
    BattleAnimScanlineTasks *tasks = data;

    if (tasks->active && tasks->pending) {
        tasks->vblankCallback(tasks->callbackArg);
        tasks->pending = FALSE;
    }
}

static void BattleAnimScanline_StartTasks(BattleAnimScanlineTasks *tasks, void *arg, BattleAnimScanlineCallback vwaitCallback, BattleAnimScanlineCallback vblankCallback) {
    GF_ASSERT(tasks != NULL);
    tasks->active = TRUE;
    tasks->pending = FALSE;
    tasks->callbackArg = arg;
    tasks->vwaitCallback = vwaitCallback;
    tasks->vblankCallback = vblankCallback;
    tasks->vwaitTask = SysTask_CreateOnVWaitQueue(BattleAnimScanline_VWaitTask, tasks, 0);
    tasks->vblankTask = SysTask_CreateOnVBlankQueue(BattleAnimScanline_VBlankTask, tasks, 0);
}

static void BattleAnimScanline_StopTasks(BattleAnimScanlineTasks *tasks) {
    GF_ASSERT(tasks != NULL);
    if (tasks->vwaitTask != NULL) {
        SysTask_Destroy(tasks->vwaitTask);
    }
    if (tasks->vblankTask != NULL) {
        SysTask_Destroy(tasks->vblankTask);
    }
    HBlankDma_Stop();
}

static void BattleAnimScanline_Deactivate(BattleAnimScanlineTasks *tasks) {
    GF_ASSERT(tasks != NULL);
    tasks->active = FALSE;
}

static void BattleAnimScanline_StartHBlankDma(BattleAnimScanline *scanline) {
    void *buffer = DoubleBuffer_GetOther(scanline->doubleBuffer);
    HBlankDma_Stop();
    HBlankDma_Start(buffer, scanline->destination, 4, TRUE);
}

static void BattleAnimScanline_SwapAndStartDma(BattleAnimScanline *scanline) {
    DoubleBuffer_Swap(scanline->doubleBuffer);
    BattleAnimScanline_StartHBlankDma(scanline);
}

static void BattleAnimScanline_VBlankCallback(BattleAnimScanline *scanline) {
    BattleAnimScanline_StartHBlankDma(scanline);
}

BattleAnimScanline *BattleAnimScanline_New(void *destination, u32 fill, enum HeapID heapId) {
    BattleAnimScanline *scanline = Heap_Alloc(heapId, sizeof(BattleAnimScanline));
    memset(scanline, 0, sizeof(BattleAnimScanline));
    GF_ASSERT(scanline != NULL);
    scanline->doubleBuffer = DoubleBuffer_New(heapId, scanline->buffers[0], scanline->buffers[1]);
    GF_ASSERT(scanline->doubleBuffer != NULL);
    scanline->destination = destination;
    MI_CpuFill32(scanline->buffers[0], fill, sizeof(scanline->buffers[0]));
    MI_CpuFill32(scanline->buffers[1], fill, sizeof(scanline->buffers[1]));
    BattleAnimScanline_StartTasks(&scanline->tasks, scanline, (BattleAnimScanlineCallback)BattleAnimScanline_SwapAndStartDma, (BattleAnimScanlineCallback)BattleAnimScanline_VBlankCallback);
    return scanline;
}

void BattleAnimScanline_Free(BattleAnimScanline *scanline) {
    GF_ASSERT(scanline != NULL);
    BattleAnimScanline_StopTasks(&scanline->tasks);
    if (scanline->doubleBuffer != NULL) {
        DoubleBuffer_Delete(scanline->doubleBuffer);
    }
    Heap_Free(scanline);
}

u32 *BattleAnimScanline_GetWriteBuffer(BattleAnimScanline *scanline) {
    GF_ASSERT(scanline != NULL);
    return DoubleBuffer_GetCurrent(scanline->doubleBuffer);
}

void BattleAnimScanline_Disable(BattleAnimScanline *scanline) {
    GF_ASSERT(scanline != NULL);
    BattleAnimScanline_Deactivate(&scanline->tasks);
}

static void BattleAnimScanline_WaveVWaitCallback(BattleAnimScanlineWave *wave) {
    ScanlineEffect_SwapBuffers(wave->effect);
    ScanlineEffect_RestartDma(wave->effect);
}

static void BattleAnimScanline_WaveVBlankCallback(BattleAnimScanlineWave *wave) {
    ScanlineEffect_RestartDma(wave->effect);
}

BattleAnimScanlineWave *BattleAnimScanline_NewWave(u8 firstLine, u8 lastLine, u8 step, fx32 amplitude, s16 speed, int bgLayer, u32 priority, u32 fill, enum HeapID heapId) {
    BattleAnimScanlineWave *wave = Heap_Alloc(heapId, sizeof(BattleAnimScanlineWave));
    void *destination;

    GF_ASSERT(wave != NULL);
    memset(wave, 0, sizeof(BattleAnimScanlineWave));
    destination = BattleAnimUtil_GetBgOffsetRegister(bgLayer);
    wave->effect = ScanlineEffect_New(heapId);
    ScanlineEffect_StartWave(wave->effect, firstLine, lastLine, step, amplitude, speed, destination, fill, priority);
    BattleAnimScanline_StartTasks(&wave->tasks, wave, (BattleAnimScanlineCallback)BattleAnimScanline_WaveVWaitCallback, (BattleAnimScanlineCallback)BattleAnimScanline_WaveVBlankCallback);
    return wave;
}

void BattleAnimScanline_FreeWave(BattleAnimScanlineWave *wave) {
    GF_ASSERT(wave != NULL);
    BattleAnimScanline_StopTasks(&wave->tasks);
    if (wave->effect != NULL) {
        ScanlineEffect_Delete(wave->effect);
    }
    Heap_Free(wave);
}

u32 *BattleAnimScanline_GetWaveBuffer(BattleAnimScanlineWave *wave) {
    GF_ASSERT(wave != NULL);
    return ScanlineEffect_GetBuffer(wave->effect);
}

void BattleAnimScanline_DisableWave(BattleAnimScanlineWave *wave) {
    GF_ASSERT(wave != NULL);
    BattleAnimScanline_Deactivate(&wave->tasks);
}

// Two 16-bit values in one scanline buffer word (BGnHOFS, BGnVOFS)
u32 BattleAnimUtil_PackXY(u16 x, u16 y) {
    return x | (y << 16);
}

void *BattleAnimUtil_GetBgOffsetRegister(int bgLayer) {
    switch (bgLayer) {
    case GF_BG_LYR_MAIN_0:
        return (void *)REG_BG0HOFS_ADDR;
    case GF_BG_LYR_MAIN_1:
        return (void *)REG_BG1HOFS_ADDR;
    case GF_BG_LYR_MAIN_2:
        return (void *)REG_BG2HOFS_ADDR;
    case GF_BG_LYR_MAIN_3:
        return (void *)REG_BG3HOFS_ADDR;
    }
#if defined(SDK_PORT)
    GF_ASSERT(FALSE);
    return NULL;
#else
    return (void *)bgLayer;
#endif
}

void BattleAnimUtil_CalcMidpoint(int x0, int y0, int x1, int y1, s16 *x, s16 *y) {
    *x = (x0 + x1) / 2;
    *y = (y0 + y1) / 2;
}

void BattleAnimUtil_CalcDistance(s16 x, s16 y, s16 targetX, s16 targetY, fx32 *distance) {
    s16 dy = -(y - targetY);
    s16 dx = x - targetX;
    *distance = FX_Sqrt((dy * dy + dx * dx) << FX32_SHIFT);
}

// Angle (0..0xFFFF) of the direction from (x, y) to (targetX, targetY), y up
void BattleAnimUtil_CalcAngle(s16 x, s16 y, s16 targetX, s16 targetY, u16 *angle) {
    s16 dy = -(y - targetY);
    s16 dx = x - targetX;
    *angle = FX_Atan2Idx(dy << FX32_SHIFT, dx << FX32_SHIFT);
    if (*angle && dy < 0) {
        *angle = (*angle - 0x7FFF) * 0xFFFF;
    }
}

// Moves *value by step; TRUE (value = target) once it reaches or passes target
BOOL BattleAnimUtil_StepTowards(int *value, int target, int step) {
    if (step < 0) {
        int next = *value + step;
        if (next > target) {
            *value = next;
            return FALSE;
        }
        *value = target;
        return TRUE;
    } else {
        int next = *value + step;
        if (next < target) {
            *value = next;
            return FALSE;
        }
        *value = target;
        return TRUE;
    }
}

static void BattleAnimTask_PaletteBlend(SysTask *task, void *data) {
    BattleAnimPaletteBlend *blend = data;

    if (blend->active) {
        blend->timer++;
        if (blend->timer >= blend->delay) {
            int next;
            s8 nextS8;
            blend->timer = 0;
            PaletteData_BlendPalette(blend->paletteData, blend->bufferId, blend->offset, blend->size, blend->current, blend->color);
            if (blend->current == blend->end) {
                blend->active = FALSE;
                return;
            }
            next = blend->current + blend->step;
            nextS8 = next;
            if (blend->step > 0) {
                if (nextS8 > blend->end) {
                    blend->current = blend->end;
                } else {
                    blend->current = next;
                }
            } else {
                if (nextS8 < blend->end) {
                    blend->current = blend->end;
                } else {
                    blend->current = next;
                }
            }
        }
    }
}

BOOL BattleAnimUtil_IsPaletteBlendActive(BattleAnimPaletteBlend *blend) {
    GF_ASSERT(blend != NULL);
    return blend->active;
}

void BattleAnimUtil_FreePaletteBlend(BattleAnimPaletteBlend *blend) {
    GF_ASSERT(blend != NULL);
    SysTask_Destroy(blend->task);
    Heap_Free(blend);
}

BattleAnimPaletteBlend *BattleAnimUtil_StartPaletteBlend(PaletteData *paletteData, enum HeapID heapId, PaletteBufferId bufferId, u16 offset, u16 size, s8 delay, s8 step, u8 start, u8 end, u16 color, u32 priority) {
    BattleAnimPaletteBlend *blend = Heap_Alloc(heapId, sizeof(BattleAnimPaletteBlend));
    GF_ASSERT(blend != NULL);
    blend->paletteData = paletteData;
    blend->bufferId = bufferId;
    blend->offset = offset;
    blend->size = size;
    blend->color = color;
    blend->start = start;
    blend->end = end;
    blend->current = start;
    blend->delay = delay;
    blend->timer = delay;
    if (blend->start < blend->end) {
        blend->step = step;
    } else {
        blend->step = -step;
    }
    blend->active = TRUE;
    blend->task = SysTask_CreateOnMainQueue(BattleAnimTask_PaletteBlend, blend, priority);
    return blend;
}

static void BattleAnimUtil_PaletteToGrayscale(u16 *palette, int count) {
    int i;
    for (i = 0; i < count; i++) {
        int color = *palette;
        int b = (color >> 10) & 0x1F;
        int r = color & 0x1F;
        int g = (color >> 5) & 0x1F;
        int level = (b * 29 + (r * 76 + g * 151)) >> 8;
        *palette = (level << 10) | (level << 5) | level;
        palette++;
    }
}

void BattleAnimSystem_ApplyGrayscale(BattleAnimSystem *system) {
    u16 *buffer = PaletteData_GetFadedBuf(BattleAnimSystem_GetPaletteData(system), PLTTBUF_MAIN_BG);
    if (BattleAnimSystem_IsContest(system) == TRUE) {
        BattleAnimUtil_PaletteToGrayscale(buffer, 48);
    } else {
        BattleAnimUtil_PaletteToGrayscale(buffer, 128);
    }
}

void BattleAnimSystem_RestoreFromGrayscale(BattleAnimSystem *system) {
    PaletteData *paletteData = BattleAnimSystem_GetPaletteData(system);
    if (BattleAnimSystem_IsContest(system) == TRUE) {
        PaletteData_CopyPalette(paletteData, PLTTBUF_MAIN_BG, 0, PLTTBUF_MAIN_BG, 0, 48 * sizeof(u16));
        return;
    }
    PaletteData_CopyPalette(paletteData, PLTTBUF_MAIN_BG, 0, PLTTBUF_MAIN_BG, 0, 128 * sizeof(u16));
}

BattleAnimFunc BattleAnimSystem_GetFunc(u32 id) {
    if (id >= 0x54) {
        return NULL;
    }
    return gBattleAnimFuncTable[id];
}

BattleAnimSpriteFunc BattleAnimSystem_GetSpriteFunc(u32 id) {
    if (id >= 0x21) {
        return NULL;
    }
    return gBattleAnimSpriteFuncTable[id];
}

void BattleAnimFunc_None(BattleAnimSystem *system) {
}
#if defined(SDK_PORT)

/* Native tables from asm/battle_anim_data.s; adapters give differing originals the table's signature. */
void BattleAnimSpriteFunc_SpreadSpriteCopies(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_BlinkScaleFadeSprites(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_OrbitingSpriteTrail(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_CrossingSprites(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_GrowRockShrinkSprite(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_StackedSpritesOnDefender(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_BoomerangSprite(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_RisingSpriteStretchAttacker(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_ZigzagSlash(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_ZigzagFlash(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_SpritePops(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_PulseSpriteBrightnessFlash(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_FanSpritesAroundAttacker(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_ShrinkAttackerIntoSprite(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_ScaleSpritesOverDefender(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_SpritesOrbitAttackerBobbing(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_FloatingSpriteStream(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_ThrowSpriteArc(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_FadeSpriteAnim(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_BlinkSpritesCenter(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_SlidingSpritePairs(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_PopAndScatterSprites(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_SpriteQuad(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_FlashingSpritesAroundAttacker(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_SpriteTrailToDefender(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_MoveSpriteAndAnimate(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_SpriteOrbit(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_SpriteAboveDefender(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_SpriteArc(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_RisingShrinkSprites(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_SpriteRowSequence(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_PaletteCycleSprite(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
void BattleAnimSpriteFunc_SpritesOrbitAttacker(BattleAnimSystem *system, SpriteSystem *spriteSystem, SpriteManager *spriteManager, ManagedSprite *sprite);
const BattleAnimSpriteFunc gBattleAnimSpriteFuncTable[] = {
    BattleAnimSpriteFunc_SpreadSpriteCopies,
    BattleAnimSpriteFunc_BlinkScaleFadeSprites,
    BattleAnimSpriteFunc_OrbitingSpriteTrail,
    BattleAnimSpriteFunc_CrossingSprites,
    BattleAnimSpriteFunc_GrowRockShrinkSprite,
    BattleAnimSpriteFunc_StackedSpritesOnDefender,
    BattleAnimSpriteFunc_BoomerangSprite,
    BattleAnimSpriteFunc_RisingSpriteStretchAttacker,
    BattleAnimSpriteFunc_ZigzagSlash,
    BattleAnimSpriteFunc_ZigzagFlash,
    BattleAnimSpriteFunc_SpritePops,
    BattleAnimSpriteFunc_PulseSpriteBrightnessFlash,
    BattleAnimSpriteFunc_FanSpritesAroundAttacker,
    BattleAnimSpriteFunc_ShrinkAttackerIntoSprite,
    BattleAnimSpriteFunc_ScaleSpritesOverDefender,
    BattleAnimSpriteFunc_SpritesOrbitAttackerBobbing,
    BattleAnimSpriteFunc_FloatingSpriteStream,
    BattleAnimSpriteFunc_ThrowSpriteArc,
    BattleAnimSpriteFunc_FadeSpriteAnim,
    BattleAnimSpriteFunc_BlinkSpritesCenter,
    BattleAnimSpriteFunc_SlidingSpritePairs,
    BattleAnimSpriteFunc_PopAndScatterSprites,
    BattleAnimSpriteFunc_SpriteQuad,
    BattleAnimSpriteFunc_FlashingSpritesAroundAttacker,
    BattleAnimSpriteFunc_SpriteTrailToDefender,
    BattleAnimSpriteFunc_MoveSpriteAndAnimate,
    BattleAnimSpriteFunc_SpriteOrbit,
    BattleAnimSpriteFunc_SpriteAboveDefender,
    BattleAnimSpriteFunc_SpriteArc,
    BattleAnimSpriteFunc_RisingShrinkSprites,
    BattleAnimSpriteFunc_SpriteRowSequence,
    BattleAnimSpriteFunc_PaletteCycleSprite,
    BattleAnimSpriteFunc_SpritesOrbitAttacker,
};
void BattleAnimFunc_OneFrameTask(BattleAnimSystem *system);
void BattleAnimFunc_OneFrameSoundTask(BattleAnimSystem *system);
void BattleAnimFunc_OneFrameUncountedTask(BattleAnimSystem *system);
void BattleAnimFunc_RotateBattler(BattleAnimSystem *system);
void BattleAnimFunc_AttackerGrowFlashRed(BattleAnimSystem *system);
void BattleAnimFunc_AttackerSquashStretch(BattleAnimSystem *system);
void BattleAnimFunc_AttackerAfterimages(BattleAnimSystem *system);
void BattleAnimFunc_PicOrbit(BattleAnimSystem *system);
void BattleAnimFunc_AttackerRecoilTilt(BattleAnimSystem *system);
void BattleAnimFunc_CircleBattler(BattleAnimSystem *system);
void BattleAnimFunc_ShakeDefenderWithOverlay(BattleAnimSystem *system);
void BattleAnimFunc_SkewSlideAttackerBg(BattleAnimSystem *system);
void BattleAnimFunc_AttackerPulseWhite(BattleAnimSystem *system);
void BattleAnimFunc_AttackerStretchSteps(BattleAnimSystem *system);
void BattleAnimFunc_AttackerLeapOffscreen(BattleAnimSystem *system);
void BattleAnimFunc_AttackerSilhouette(BattleAnimSystem *system);
void BattleAnimFunc_FadeInShrinkSprite(BattleAnimSystem *system);
void BattleAnimFunc_ShakeDefenderBlackFlash(BattleAnimSystem *system);
void BattleAnimFunc_AttackerHop(BattleAnimSystem *system);
void BattleAnimFunc_WaveDefenderOnBg(BattleAnimSystem *system);
void BattleAnimFunc_ShowAttackerObjWindowSprites(BattleAnimSystem *system);
void BattleAnimFunc_AttackerSquashAfterimages(BattleAnimSystem *system);
void BattleAnimFunc_AttackerCircleFade(BattleAnimSystem *system);
void BattleAnimFunc_ShakeBattlefieldFlashing(BattleAnimSystem *system);
void BattleAnimFunc_WaddlePic(BattleAnimSystem *system);
void BattleAnimFunc_SpriteDriftFade(BattleAnimSystem *system);
void BattleAnimFunc_RockOrShakeAttacker(BattleAnimSystem *system);
void BattleAnimFunc_ShakeBattlefieldByPower(BattleAnimSystem *system);
void BattleAnimFunc_AttackerDip(BattleAnimSystem *system);
void BattleAnimFunc_AttackerLungeKnockback(BattleAnimSystem *system);
void BattleAnimFunc_AttackerStretchUp(BattleAnimSystem *system);
void BattleAnimFunc_DarkSpriteJump(BattleAnimSystem *system);
void BattleAnimFunc_FadeBgPalette(BattleAnimSystem *system);
void BattleAnimFunc_BattlerPaletteFade(BattleAnimSystem *system);
void BattleAnimFunc_PulseBattlerSprite(BattleAnimSystem *system);
void BattleAnimFunc_ShakeTargets(BattleAnimSystem *system);
void BattleAnimFunc_WobbleDefenderOnBg(BattleAnimSystem *system);
void BattleAnimFunc_BlendBattlerSprites(BattleAnimSystem *system);
void BattleAnimFunc_ShakeSpritePair(BattleAnimSystem *system);
void BattleAnimFunc_SetBattlerVanished(BattleAnimSystem *system);
void BattleAnimFunc_WindowSweepFlashWhite(BattleAnimSystem *system);
void BattleAnimFunc_StretchBattler(BattleAnimSystem *system);
void BattleAnimFunc_StretchShakeDefender(BattleAnimSystem *system);
void BattleAnimFunc_SlidingBg(BattleAnimSystem *system);
void BattleAnimFunc_SlidingBg2(BattleAnimSystem *system);
void BattleAnimFunc_ShootSpriteFromAttacker(BattleAnimSystem *system);
void BattleAnimFunc_AttackerShakeLunge(BattleAnimSystem *system);
void BattleAnimFunc_DefenderKnockback(BattleAnimSystem *system);
void BattleAnimFunc_StretchFadeSprite(BattleAnimSystem *system);
void BattleAnimFunc_BlinkBattler(BattleAnimSystem *system);
void BattleAnimFunc_SlideBattler(BattleAnimSystem *system);
void BattleAnimFunc_SlideBattler2(BattleAnimSystem *system);
void BattleAnimFunc_ShakeAndScaleBattler(BattleAnimSystem *system);
void BattleAnimFunc_ShakeAndScaleBattlerXY(BattleAnimSystem *system);
void BattleAnimFunc_WaveAttackerOnBg(BattleAnimSystem *system);
void BattleAnimFunc_ShowBattleBgInObjWindow(BattleAnimSystem *system);
void BattleAnimFunc_MoveBattlerBy(BattleAnimSystem *system);
void BattleAnimFunc_ShrinkSpriteOnDefender(BattleAnimSystem *system);
void BattleAnimFunc_AttackerDashAfterimages(BattleAnimSystem *system);
void BattleAnimFunc_MoveBattlerSin(BattleAnimSystem *system);
void BattleAnimFunc_MoveBattlerLinear(BattleAnimSystem *system);
void BattleAnimFunc_ResetBattlerPosition(BattleAnimSystem *system);
void BattleAnimFunc_BlendSpritePalette(BattleAnimSystem *system);
void BattleAnimFunc_ChangeBattlerHeight(BattleAnimSystem *system);
void BattleAnimFunc_MoveEmitter(BattleAnimSystem *system);
void BattleAnimFunc_MoveEmitterArc(BattleAnimSystem *system);
void BattleAnimFunc_RevealBattler(BattleAnimSystem *system);
void BattleAnimFunc_ShakeBg(BattleAnimSystem *system);
void BattleAnimFunc_SpriteMosaic(BattleAnimSystem *system);
void BattleAnimFunc_SquashSprite(BattleAnimSystem *system);
void BattleAnimFunc_SlideOutAndTransform(BattleAnimSystem *system);
void BattleAnimFunc_EmitterSinMotion(BattleAnimSystem *system);
void BattleAnimFunc_MoveEmitterVertical(BattleAnimSystem *system);
void BattleAnimFunc_SetBgPaletteGrayscale(BattleAnimSystem *system);
void BattleAnimFunc_DropSpriteStepwise(BattleAnimSystem *system);
void BattleAnimFunc_BattleBgWave(BattleAnimSystem *system);
void BattleAnimFunc_MoveBattlerOffscreen(BattleAnimSystem *system);
void BattleAnimFunc_AnimateSprites(BattleAnimSystem *system);
void BattleAnimFunc_WipeOutDefender(BattleAnimSystem *system);
void BattleAnimFunc_ObjWindowBgScrollUp(BattleAnimSystem *system);
void BattleAnimFunc_ObjWindowBgScrollDown(BattleAnimSystem *system);
void BattleAnimFunc_ObjWindowBgScrollUpAlt(BattleAnimSystem *system);
void BattleAnimFunc_ObjWindowBgScrollDownFast(BattleAnimSystem *system);
const BattleAnimFunc gBattleAnimFuncTable[] = {
    BattleAnimFunc_None,
    BattleAnimFunc_OneFrameTask,
    BattleAnimFunc_OneFrameSoundTask,
    BattleAnimFunc_OneFrameUncountedTask,
    BattleAnimFunc_RotateBattler,
    BattleAnimFunc_AttackerGrowFlashRed,
    BattleAnimFunc_AttackerSquashStretch,
    BattleAnimFunc_AttackerAfterimages,
    BattleAnimFunc_PicOrbit,
    BattleAnimFunc_AttackerRecoilTilt,
    BattleAnimFunc_CircleBattler,
    BattleAnimFunc_ShakeDefenderWithOverlay,
    BattleAnimFunc_SkewSlideAttackerBg,
    BattleAnimFunc_AttackerPulseWhite,
    BattleAnimFunc_AttackerStretchSteps,
    BattleAnimFunc_AttackerLeapOffscreen,
    BattleAnimFunc_AttackerSilhouette,
    BattleAnimFunc_FadeInShrinkSprite,
    BattleAnimFunc_ShakeDefenderBlackFlash,
    BattleAnimFunc_AttackerHop,
    BattleAnimFunc_WaveDefenderOnBg,
    BattleAnimFunc_ShowAttackerObjWindowSprites,
    BattleAnimFunc_AttackerSquashAfterimages,
    BattleAnimFunc_AttackerCircleFade,
    BattleAnimFunc_ShakeBattlefieldFlashing,
    BattleAnimFunc_WaddlePic,
    BattleAnimFunc_SpriteDriftFade,
    BattleAnimFunc_RockOrShakeAttacker,
    BattleAnimFunc_ShakeBattlefieldByPower,
    BattleAnimFunc_AttackerDip,
    BattleAnimFunc_AttackerLungeKnockback,
    BattleAnimFunc_AttackerStretchUp,
    BattleAnimFunc_DarkSpriteJump,
    BattleAnimFunc_FadeBgPalette,
    BattleAnimFunc_BattlerPaletteFade,
    BattleAnimFunc_PulseBattlerSprite,
    BattleAnimFunc_ShakeTargets,
    BattleAnimFunc_WobbleDefenderOnBg,
    BattleAnimFunc_BlendBattlerSprites,
    BattleAnimFunc_ShakeSpritePair,
    BattleAnimFunc_SetBattlerVanished,
    BattleAnimFunc_WindowSweepFlashWhite,
    BattleAnimFunc_StretchBattler,
    BattleAnimFunc_StretchShakeDefender,
    BattleAnimFunc_SlidingBg,
    BattleAnimFunc_SlidingBg2,
    BattleAnimFunc_ShootSpriteFromAttacker,
    BattleAnimFunc_AttackerShakeLunge,
    BattleAnimFunc_DefenderKnockback,
    BattleAnimFunc_StretchFadeSprite,
    BattleAnimFunc_BlinkBattler,
    BattleAnimFunc_SlideBattler,
    BattleAnimFunc_SlideBattler2,
    BattleAnimFunc_ShakeAndScaleBattler,
    BattleAnimFunc_ShakeAndScaleBattlerXY,
    BattleAnimFunc_WaveAttackerOnBg,
    BattleAnimFunc_ShowBattleBgInObjWindow,
    BattleAnimFunc_MoveBattlerBy,
    BattleAnimFunc_ShrinkSpriteOnDefender,
    BattleAnimFunc_AttackerDashAfterimages,
    BattleAnimFunc_MoveBattlerSin,
    BattleAnimFunc_MoveBattlerLinear,
    BattleAnimFunc_ResetBattlerPosition,
    BattleAnimFunc_BlendSpritePalette,
    BattleAnimFunc_ChangeBattlerHeight,
    BattleAnimFunc_MoveEmitter,
    BattleAnimFunc_MoveEmitterArc,
    BattleAnimFunc_RevealBattler,
    BattleAnimFunc_ShakeBg,
    BattleAnimFunc_SpriteMosaic,
    BattleAnimFunc_SquashSprite,
    BattleAnimFunc_SlideOutAndTransform,
    BattleAnimFunc_EmitterSinMotion,
    BattleAnimFunc_MoveEmitterVertical,
    BattleAnimFunc_SetBgPaletteGrayscale,
    BattleAnimFunc_DropSpriteStepwise,
    BattleAnimFunc_BattleBgWave,
    BattleAnimFunc_MoveBattlerOffscreen,
    BattleAnimFunc_AnimateSprites,
    BattleAnimFunc_WipeOutDefender,
    BattleAnimFunc_ObjWindowBgScrollUp,
    BattleAnimFunc_ObjWindowBgScrollDown,
    BattleAnimFunc_ObjWindowBgScrollUpAlt,
    BattleAnimFunc_ObjWindowBgScrollDownFast,
};
#endif
