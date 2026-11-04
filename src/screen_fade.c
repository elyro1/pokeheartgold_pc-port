#if defined(SDK_PORT)
#include "port/battle_timing.h"
#endif
#include "screen_fade.h"

#include "global.h"

#include "constants/gx.h"

#include "bg_window.h"
#include "heap.h"
#include "screen_fade_effects.h"
#include "screen_fade_internal.h"
#include "screen_window_wipe.h"
#include "sys_task_api.h"
#include "system.h"

// The palette fade: BeginNormalPaletteFade starts a fade of one or both screens and HandleFadeUpdateFrame advances
// it every frame. Each screen runs one ScreenFade whose effect is the FadeFunc of its enum FadeType.

typedef void (*FadeHBlankCallback)(void *data);

// One HBlank callback per screen, called from a single HBlank interrupt handler
struct FadeBlankPair {
    void *data[2];
    FadeHBlankCallback callbacks[2];
    BOOL active[2];
};

typedef struct FadeBlankStartArgs {
    FadeBlankPair *blankPair;
    void *data;
    FadeHBlankCallback callback;
    int screen;
} FadeBlankStartArgs;

typedef struct FadeBlankStopArgs {
    FadeBlankPair *blankPair;
    int screen;
} FadeBlankStopArgs;

enum PaletteFadeOrder {
    PALETTE_FADE_BOTH_SCREENS,
    PALETTE_FADE_MAIN_FIRST,
    PALETTE_FADE_SUB_FIRST,
};

typedef struct PaletteFade {
    enum PaletteFadeOrder order;
    BOOL mainActive;
    BOOL subActive;
    BOOL mainEnabled; // the screens taking part in the fade
    BOOL subEnabled;
    ScreenFade main;
    ScreenFade sub;
    FadeBlankPair blankPair;
    WindowRegisterQueue regQueue;
    u16 active;
    u8 mainFaded;
    u8 subFaded;
#if defined(SDK_PORT)
    BattleTimingDomain timingDomain;
#endif
    u16 color; // the color of the last fade, used when a fade passes 0xFFFF
} PaletteFade;

typedef BOOL (*FadeFunc)(ScreenFade *fade);

static void HandleEndFade(PaletteFade *fade);
static BOOL DoFadeUpdateFrame(PaletteFade *fade, ScreenFade *main, ScreenFade *sub);
static void FadeWork_UpdateFrame(BOOL *active, ScreenFade *fade);
static BOOL CallFadeFunc(ScreenFade *fade);
static void PaletteFade_SetMode(enum FadeMode mode, PaletteFade *fade);
static void PaletteFade_SetScreenOrder(PaletteFade *fade, enum PaletteFadeOrder order, BOOL main, BOOL sub);
static void ScreenFade_Init(ScreenFade *fade, u32 type, int steps, int framesPerStep, enum ScreenFadeState state, void *effect, int screen, WindowRegisterQueue *regQueue, FadeBlankPair *blankPair, enum HeapID heapId, u16 color);
static void FadeBlankPair_Init(FadeBlankPair *blankPair);
static void FadeBlankPair_HBlankCallback(void *data);
static void FadeBlankPair_Start(FadeBlankPair *blankPair, void *data, FadeHBlankCallback callback, int screen);
static void FadeBlankPair_Stop(FadeBlankPair *blankPair, int screen);
static void FadeBlankPair_StartTask(SysTask *task, void *data);
static void FadeBlankPair_StopTask(SysTask *task, void *data);
static void FadeBlankPair_DummyCallback(void *data);
static u16 PaletteFade_ResolveColor(PaletteFade *fade, u16 color);
static u16 PaletteFade_GetEndColor(PaletteFade *fade);
static void ScreenFade_ResetBrightnessTask(SysTask *task, void *data);
static void ScreenFade_QueueBrightnessReset(ScreenFade *fade);
static void ScreenFade_HoldFadeColor(ScreenFade *fade);
static void PaletteFade_Clear(PaletteFade *fade);

void GXx_SetMasterBrightness_(vu16 *reg, s32 brightness);

static const FadeFunc sFadeFuncPtrs[] = {
    FadeFunc_00,
    FadeFunc_01,
    FadeFunc_02,
    FadeFunc_03,
    FadeFunc_04,
    FadeFunc_05,
    FadeFunc_06,
    FadeFunc_07,
    FadeFunc_08,
    FadeFunc_09,
    FadeFunc_10,
    FadeFunc_11,
    FadeFunc_12,
    FadeFunc_13,
    FadeFunc_14,
    FadeFunc_15,
    FadeFunc_16,
    FadeFunc_17,
    FadeFunc_18,
    FadeFunc_19,
    FadeFunc_20,
    FadeFunc_21,
    FadeFunc_22,
    FadeFunc_23,
    FadeFunc_24,
    FadeFunc_25,
    FadeFunc_26,
    FadeFunc_27,
    FadeFunc_28,
    FadeFunc_29,
    FadeFunc_30,
    FadeFunc_31,
    FadeFunc_32,
    FadeFunc_33,
    FadeFunc_34,
    FadeFunc_35,
    FadeFunc_36,
    FadeFunc_37,
    FadeFunc_38,
    FadeFunc_39,
    FadeFunc_40,
    FadeFunc_41,
    FadeFunc_42,
};

static PaletteFade sPaletteFade;

void BeginNormalPaletteFade(enum FadeMode mode, enum FadeType mainType, enum FadeType subType, u16 color, int steps, int framesPerStep, enum HeapID heapId) {
    GF_ASSERT(steps != 0);
    GF_ASSERT(framesPerStep != 0);
    GF_ASSERT(sPaletteFade.active == 0);
    {
        PaletteFade *fade = &sPaletteFade;
        PaletteFade_Clear(&sPaletteFade);
#if defined(SDK_PORT)
        sPaletteFade.timingDomain = BattleTiming_GetDomain();
#endif
        PaletteFade_SetMode(mode, &sPaletteFade);
        FadeBlankPair_Init(&sPaletteFade.blankPair);
        color = PaletteFade_ResolveColor(&sPaletteFade, color);
        ScreenFade_Init(&sPaletteFade.main, mainType, steps, framesPerStep, SCREEN_FADE_STATE_START, NULL, SCREEN_MAIN, &sPaletteFade.regQueue, &sPaletteFade.blankPair, heapId, color);
        ScreenFade_Init(&sPaletteFade.sub, subType, steps, framesPerStep, SCREEN_FADE_STATE_START, NULL, SCREEN_SUB, &sPaletteFade.regQueue, &sPaletteFade.blankPair, heapId, color);
        sPaletteFade.active = 1;
        FadeWork_UpdateFrame(&sPaletteFade.mainActive, &sPaletteFade.main);
        FadeWork_UpdateFrame(&sPaletteFade.subActive, &sPaletteFade.sub);
        if (sPaletteFade.mainEnabled) {
            ScreenFade_QueueBrightnessReset(&fade->main);
            fade->mainFaded = 1;
        }
        if (fade->subEnabled) {
            ScreenFade_QueueBrightnessReset(&fade->sub);
            fade->subFaded = 1;
        }
    }
}

void HandleFadeUpdateFrame(void) {
    PaletteFade *fade = &sPaletteFade;
#if defined(SDK_PORT)
    if (sPaletteFade.active != 0 && BattleTiming_ShouldTick(
        fade->timingDomain
    )) {
#else
    if (sPaletteFade.active != 0) {
#endif
        if (DoFadeUpdateFrame(fade, &fade->main, &fade->sub) == TRUE) {
            HandleEndFade(fade);
        }
    }
}

BOOL IsPaletteFadeFinished(void) {
    return sPaletteFade.active == 0;
}

// Ends the fade at once: both screens skip to their effect's clean-up.
void StopPaletteFade(void) {
    FadeBlankPair_Stop(&sPaletteFade.blankPair, SCREEN_MAIN);
    FadeBlankPair_Stop(&sPaletteFade.blankPair, SCREEN_SUB);
    if (sPaletteFade.mainActive) {
        sPaletteFade.main.state = SCREEN_FADE_STATE_CLEANUP;
    }
    if (sPaletteFade.subActive) {
        sPaletteFade.sub.state = SCREEN_FADE_STATE_CLEANUP;
    }
    FadeWork_UpdateFrame(&sPaletteFade.mainActive, &sPaletteFade.main);
    FadeWork_UpdateFrame(&sPaletteFade.subActive, &sPaletteFade.sub);
    sPaletteFade.active = 0;
    sPaletteFade.mainFaded = 0;
    sPaletteFade.subFaded = 0;
    PaletteFade_Clear(&sPaletteFade);
}

void ResetVisibleHardwareWindows(PMLCDTarget screen) {
    ScreenWindow_SetVisible(0, screen);
}

void SetMasterBrightnessNeutral(PMLCDTarget screen) {
    SetMasterBrightness(screen, 0);
}

void SetMasterBrightnessByColor(PMLCDTarget screen, u16 color) {
    if (color == 0xFFFF) {
        color = sPaletteFade.color;
    }
    SetMasterBrightness(screen, color == RGB_WHITE ? 16 : -16);
}

void SetBothScreensBrightnessByColor(u16 color) {
    int brightness;
    if (color == 0xFFFF) {
        color = sPaletteFade.color;
    }
    brightness = color == RGB_WHITE ? 16 : -16;
    SetMasterBrightness(PM_LCD_TOP, brightness);
    SetMasterBrightness(PM_LCD_BOTTOM, brightness);
    sPaletteFade.color = color;
}

// Covers the screen with the backdrop, set to the color, through a window that shows no layer.
void FillScreenWithColor(int screen, u16 color) {
    if (color == 0xFFFF) {
        color = sPaletteFade.color;
    }
    if (screen == SCREEN_MAIN) {
        GX_LoadBGPltt(&color, 0, sizeof(u16));
    } else {
        GXS_LoadBGPltt(&color, 0, sizeof(u16));
    }
    ScreenWindow_QueueSetVisible(&sPaletteFade.regQueue, GX_WNDMASK_W0, screen);
    ScreenWindow_QueueSetInside(&sPaletteFade.regQueue, 0x3F, FALSE, 0, screen);
    ScreenWindow_QueueSetRect(&sPaletteFade.regQueue, 0, 0, 0, 0, 0, screen);
    ScreenWindow_QueueSetOutside(&sPaletteFade.regQueue, 0x20, FALSE, screen);
}

void SetBothBackdropColors(u16 color) {
    GX_LoadBGPltt(&color, 0, sizeof(u16));
    GXS_LoadBGPltt(&color, 0, sizeof(u16));
}

void SetMasterBrightness(int screen, int brightness) {
    if (screen == PM_LCD_TOP) {
        GXx_SetMasterBrightness_(&reg_GX_MASTER_BRIGHT, brightness);
    } else {
        GXx_SetMasterBrightness_(&reg_GXS_DB_MASTER_BRIGHT, brightness);
    }
}

static void HandleEndFade(PaletteFade *fade) {
    fade->active = 0;
    fade->color = PaletteFade_GetEndColor(fade);
    if (fade->mainEnabled) {
        ScreenFade_HoldFadeColor(&fade->main);
        if (!fade->main.fadeOut) {
            sPaletteFade.mainFaded = 0;
        }
    }
    if (fade->subEnabled) {
        ScreenFade_HoldFadeColor(&fade->sub);
        if (!fade->main.fadeOut) { // the main screen's, even for the sub screen
            sPaletteFade.subFaded = 0;
        }
    }
    PaletteFade_Clear(fade);
}

static BOOL DoFadeUpdateFrame(PaletteFade *fade, ScreenFade *main, ScreenFade *sub) {
    switch (fade->order) {
    case PALETTE_FADE_BOTH_SCREENS:
        FadeWork_UpdateFrame(&fade->mainActive, main);
        FadeWork_UpdateFrame(&fade->subActive, sub);
        break;
    case PALETTE_FADE_MAIN_FIRST:
        if (fade->mainActive) {
            FadeWork_UpdateFrame(&fade->mainActive, main);
        } else {
            FadeWork_UpdateFrame(&fade->subActive, sub);
        }
        break;
    case PALETTE_FADE_SUB_FIRST:
        if (fade->subActive) {
            FadeWork_UpdateFrame(&fade->subActive, sub);
        } else {
            FadeWork_UpdateFrame(&fade->mainActive, main);
        }
        break;
    }
    if (!fade->mainActive && !fade->subActive) {
        return TRUE;
    }
    return FALSE;
}

static void FadeWork_UpdateFrame(BOOL *active, ScreenFade *fade) {
    if (*active && CallFadeFunc(fade) == TRUE) {
        *active = FALSE;
    }
}

static BOOL CallFadeFunc(ScreenFade *fade) {
    return sFadeFuncPtrs[fade->type](fade);
}

static void PaletteFade_SetMode(enum FadeMode mode, PaletteFade *fade) {
    switch (mode) {
    case FADE_BOTH_SCREENS:
        PaletteFade_SetScreenOrder(fade, PALETTE_FADE_BOTH_SCREENS, TRUE, TRUE);
        break;
    case FADE_MAIN_THEN_SUB:
        PaletteFade_SetScreenOrder(fade, PALETTE_FADE_MAIN_FIRST, TRUE, TRUE);
        break;
    case FADE_SUB_THEN_MAIN:
        PaletteFade_SetScreenOrder(fade, PALETTE_FADE_SUB_FIRST, TRUE, TRUE);
        break;
    case FADE_MAIN_ONLY:
        PaletteFade_SetScreenOrder(fade, PALETTE_FADE_MAIN_FIRST, TRUE, FALSE);
        break;
    case FADE_SUB_ONLY:
        PaletteFade_SetScreenOrder(fade, PALETTE_FADE_SUB_FIRST, FALSE, TRUE);
        break;
    }
}

static void PaletteFade_SetScreenOrder(PaletteFade *fade, enum PaletteFadeOrder order, BOOL main, BOOL sub) {
    fade->order = order;
    fade->mainActive = main;
    fade->subActive = sub;
    fade->mainEnabled = main;
    fade->subEnabled = sub;
}

static void ScreenFade_Init(ScreenFade *fade, u32 type, int steps, int framesPerStep, enum ScreenFadeState state, void *effect, int screen, WindowRegisterQueue *regQueue, FadeBlankPair *blankPair, enum HeapID heapId, u16 color) {
    fade->type = type;
    fade->steps = steps;
    fade->framesPerStep = framesPerStep;
    fade->state = state;
    fade->effect = effect;
    fade->screen = screen;
    fade->regQueue = regQueue;
    fade->blankPair = blankPair;
    fade->heapId = heapId;
    fade->color = color;
}

static void FadeBlankPair_Init(FadeBlankPair *blankPair) {
    int i;
    for (i = 0; i < 2; i++) {
        blankPair->data[i] = NULL;
        blankPair->callbacks[i] = FadeBlankPair_DummyCallback;
        blankPair->active[i] = FALSE;
    }
}

static void FadeBlankPair_HBlankCallback(void *data) {
    FadeBlankPair *blankPair = data;
    int i;
    for (i = 0; i < 2; i++) {
        blankPair->callbacks[i](blankPair->data[i]);
    }
}

static void FadeBlankPair_Start(FadeBlankPair *blankPair, void *data, FadeHBlankCallback callback, int screen) {
    u8 installed = TRUE;
    BOOL *active = &blankPair->active[screen];

    GF_ASSERT(!*active);
    GF_ASSERT(blankPair->callbacks[screen] != NULL);
    if (!blankPair->active[SCREEN_MAIN] && !blankPair->active[SCREEN_SUB]) {
        installed = Main_SetHBlankIntrCB(FadeBlankPair_HBlankCallback, blankPair);
    }
    GF_ASSERT(installed == TRUE);
    blankPair->data[screen] = data;
    if (callback != NULL) {
        blankPair->callbacks[screen] = callback;
    } else {
        blankPair->callbacks[screen] = FadeBlankPair_DummyCallback;
    }
    *active = TRUE;
}

static void FadeBlankPair_Stop(FadeBlankPair *blankPair, int screen) {
    blankPair->active[screen] = FALSE;
    if (!blankPair->active[SCREEN_MAIN] && !blankPair->active[SCREEN_SUB]) {
        HBlankInterruptDisable();
    }
    blankPair->callbacks[screen] = FadeBlankPair_DummyCallback;
    blankPair->data[screen] = NULL;
}

void FadeBlankPair_StartOnVBlank(FadeBlankPair *blankPair, void *data, void (*callback)(void *data), int screen, enum HeapID heapId) {
    FadeBlankStartArgs *args = Heap_AllocAtEnd(heapId, sizeof(FadeBlankStartArgs));
    args->blankPair = blankPair;
    args->data = data;
    args->callback = callback;
    args->screen = screen;
    SysTask_CreateOnVWaitQueue(FadeBlankPair_StartTask, args, 0x400);
}

void FadeBlankPair_StopOnVBlank(FadeBlankPair *blankPair, int screen, enum HeapID heapId) {
    FadeBlankStopArgs *args = Heap_AllocAtEnd(heapId, sizeof(FadeBlankStopArgs));
    args->blankPair = blankPair;
    args->screen = screen;
    SysTask_CreateOnVWaitQueue(FadeBlankPair_StopTask, args, 0x400);
}

static void FadeBlankPair_StartTask(SysTask *task, void *data) {
    FadeBlankStartArgs *args = data;
    FadeBlankPair_Start(args->blankPair, args->data, args->callback, args->screen);
    SysTask_Destroy(task);
    Heap_Free(args);
}

static void FadeBlankPair_StopTask(SysTask *task, void *data) {
    FadeBlankStopArgs *args = data;
    FadeBlankPair_Stop(args->blankPair, args->screen);
    SysTask_Destroy(task);
    Heap_Free(args);
}

static void FadeBlankPair_DummyCallback(void *data) {
}

static u16 PaletteFade_ResolveColor(PaletteFade *fade, u16 color) {
    if (color == 0xFFFF) {
        color = fade->color;
    }
    return color;
}

// The color the screens are left in: the main screen's fade color if it faded out, else the previous color.
static u16 PaletteFade_GetEndColor(PaletteFade *fade) {
    ScreenFade *screenFade;
    if (fade->mainEnabled == TRUE) {
        screenFade = &fade->main;
    } else {
        screenFade = &fade->sub;
    }
    if (screenFade->fadeOut == TRUE) {
        return screenFade->color;
    }
    return fade->color;
}

static void ScreenFade_ResetBrightnessTask(SysTask *task, void *data) {
    ScreenFade *fade = data;
    SetMasterBrightness(fade->screen, 0);
    SysTask_Destroy(task);
}

// A fade in from black or white done by a window wipe: the master brightness goes back to neutral on the next VBlank.
static void ScreenFade_QueueBrightnessReset(ScreenFade *fade) {
    if (!fade->fadeOut && (fade->color == RGB_WHITE || fade->color == RGB_BLACK) && !fade->isBrightnessFade) {
        SysTask_CreateOnVWaitQueue(ScreenFade_ResetBrightnessTask, fade, 0x400);
    }
}

// A fade out to black or white done by a window wipe: the screen is left at full brightness of that color with
// the windows off.
static void ScreenFade_HoldFadeColor(ScreenFade *fade) {
    if (fade->fadeOut == TRUE && (fade->color == RGB_WHITE || fade->color == RGB_BLACK) && !fade->isBrightnessFade) {
        SetMasterBrightnessByColor((PMLCDTarget)fade->screen, fade->color);
        ResetVisibleHardwareWindows((PMLCDTarget)fade->screen);
    }
}

static void PaletteFade_Clear(PaletteFade *fade) {
    memset(fade, 0, offsetof(PaletteFade, main));
    memset(&fade->main, 0, sizeof(ScreenFade));
    memset(&fade->sub, 0, sizeof(ScreenFade));
    memset(&fade->blankPair, 0, sizeof(FadeBlankPair));
    memset(&fade->regQueue, 0, sizeof(WindowRegisterQueue));
}
