#include "safari_area_customizer.h"

#include "global.h"

#include "constants/sndseq.h"

#include "msgdata/msg.naix"
#include "msgdata/msg/msg_0429.h"

#include "bg_window.h"
#include "filesystem.h"
#include "font.h"
#include "font_sprite.h"
#include "gf_gfx_loader.h"
#include "gf_gfx_planes.h"
#include "heap.h"
#include "list_menu_items.h"
#include "message_format.h"
#include "msgdata.h"
#include "oam_manager.h"
#include "obj_char_transfer.h"
#include "options.h"
#include "player_data.h"
#include "pm_string.h"
#include "safari_zone.h"
#include "safari_zone_encounters.h"
#include "save_vars_flags.h"
#include "screen_fade.h"
#include "sound_playback.h"
#include "sound_system.h"
#include "sprite.h"
#include "sprite_resource_header.h"
#include "sprite_system.h"
#include "sprite_transfer.h"
#include "sys_flags.h"
#include "system.h"
#include "text.h"
#include "touchpad.h"
#include "touchscreen.h"
#include "touchscreen_list_menu.h"
#include "ui_util.h"
#include "vram_transfer_manager.h"
#include "yes_no_prompt.h"

#define NUM_AREA_SLOTS 6 // areas in a set (2 rows of 3)

// Sprite resources of the area picture sprites (12 = the 6 slots of the set and the 6 areas of a page).
typedef struct SafariAreaCustomizerSpriteRes {
    enum HeapID heapId;
    int transferMode;
    u16 unk_08;
    u16 unk_0A;
    u16 maxSprites;
    u16 vram;
    SpriteList *spriteList;
    SpriteList *externalSpriteList;
    G2dRenderer renderer;
    u8 padding_140[4];
    GF_2DGfxResMan *resMan[4];
    GF_2DGfxResObjList *resObjList[4];
} SafariAreaCustomizerSpriteRes;

enum SafariAreaCustomizerState {
    SAC_STATE_SETUP,
    SAC_STATE_TEARDOWN,
    SAC_STATE_SELECT_SLOT,
    SAC_STATE_SELECT_AREA,
    SAC_STATE_MOVE_AREA,
    SAC_STATE_FADE_IN,
    SAC_STATE_FADE_OUT,
    SAC_STATE_EXIT,
};

enum SafariAreaCustomizerInputState {
    SAC_INPUT_STATE_WAIT,
    SAC_INPUT_STATE_MENU,
    SAC_INPUT_STATE_SCROLL,
    SAC_INPUT_STATE_CHANGE_PAGE,
    SAC_INPUT_STATE_SWAP_AREAS = SAC_INPUT_STATE_CHANGE_PAGE,
    SAC_INPUT_STATE_EXIT,
};

enum SafariAreaCustomizerMode {
    SAC_MODE_SLOTS,
    SAC_MODE_AREAS,
    SAC_MODE_MOVE,
};

enum SafariAreaCustomizerSetupState {
    SAC_SETUP_RESET_DISPLAY,
    SAC_SETUP_LOAD,
    SAC_SETUP_INIT_SCREEN,
};

enum SafariAreaCustomizerTeardownState {
    SAC_TEARDOWN_FREE,
    SAC_TEARDOWN_WAIT,
    SAC_TEARDOWN_RESET_DISPLAY,
};

enum SafariAreaCustomizerAnimState {
    SAC_ANIM_START,
    SAC_ANIM_RUN,
    SAC_ANIM_DONE,
};

enum SafariAreaCustomizerSwapState {
    SAC_SWAP_START,
    SAC_SWAP_MOVE_OUT,
    SAC_SWAP_MOVE_BACK,
    SAC_SWAP_DONE,
};

enum SafariAreaCustomizerFadeState {
    SAC_FADE_START,
    SAC_FADE_WAIT,
};

typedef struct SafariAreaCustomizer {
    enum HeapID heapId;
    int state;
    int subState;
    int nextState;
    u32 touchMode;
    SafariAreaCustomizerArgs *args;
    SafariZone *safariZone;
    SafariZoneAreaSet areaSet;
    MsgData *msgData;
    MessageFormat *msgFormat;
    String *string;
    String *labelStrings[5];
    String *menuStrings[4];
    String *pageTemplate;
    String *countTemplate;
    u8 padding_33C[4];
    BgConfig *bgConfig;
    TouchscreenListMenuSpawner *listMenuSpawner;
    SafariAreaCustomizerSpriteRes *spriteRes;
    SpriteSystem *spriteSystem;
    SpriteManager *spriteManager;
    Sprite *sprites[6];                             // 0 slot cursor, 1 area cursor, 4/5 page arrows
    ManagedSprite *areaSprites[2 * NUM_AREA_SLOTS]; // 2 banks of 6, swapped when the page changes
    FontSystem *fontSystem;
    TextOBJLayout *fontLayout;
    TextOBJ *textObj;
    ObjCharSpaceReservation charTransfer;
    Window windows[15];
    Window textObjWindow;
    ListMenuItem *areaMenuItems;
    ListMenuItem *confirmMenuItems;
    TouchscreenListMenu *listMenu;
    YesNoPrompt *yesNoPrompt;
    void (*onSwitchToButtons)(struct SafariAreaCustomizer *);
    void (*onSwitchToTouch)(struct SafariAreaCustomizer *);
    void *selectFrameScreenFile;
    void *pageScreenFile;
    NNSG2dScreenData *selectFrameScreen;
    NNSG2dScreenData *pageScreen;
    u8 padding_4DC[0x18000];
    u8 textSpeed;
    u8 frame;
    u8 page;
    u8 selectedSlot;
    u8 cursorPos;
    u8 animFrame;
    u8 scrollPending : 1;
    u8 pageDir : 1;
    u8 spriteBank : 1;
    u8 mode : 5;
    u8 objectsUnlocked;
    s16 mainBgY;
    s16 subBgY;
    BOOL modified;
} SafariAreaCustomizer;

typedef struct SafariAreaCustomizerBgTemplates {
    BgTemplate templates[8];
} SafariAreaCustomizerBgTemplates;
#if defined(SDK_PORT)
_Static_assert(sizeof(SafariAreaCustomizerBgTemplates) == 224, "literal data: SafariAreaCustomizerBgTemplates");
#endif

typedef struct SafariAreaCustomizerResCounts {
    u8 counts[4];
} SafariAreaCustomizerResCounts;
#if defined(SDK_PORT)
_Static_assert(sizeof(SafariAreaCustomizerResCounts) == 4, "literal data: SafariAreaCustomizerResCounts");
#endif

// The per-row scroll directions of the slot swap, the label text colours and the list menu template.
typedef struct SafariAreaCustomizerConsts {
    s8 swapDir[2][2];
    u32 labelColors[2];
    TouchscreenListMenuTemplate menuTemplate;
} SafariAreaCustomizerConsts;
#if defined(SDK_PORT)
_Static_assert(sizeof(SafariAreaCustomizerConsts) == 24, "literal data: SafariAreaCustomizerConsts");
#endif

extern const SafariAreaCustomizerConsts gSafariAreaCustomizerConsts;
extern const u32 gSafariAreaCustomizerLabelColors[2];
extern const GraphicsModes gSafariAreaCustomizerGraphicsModes;
typedef struct SafariAreaCustomizerMenuPos {
    u16 x;
    u16 y;
} SafariAreaCustomizerMenuPos;

extern const SafariAreaCustomizerMenuPos gSafariAreaCustomizerSlotMenuPositions[];
extern const TouchscreenHitbox gSafariAreaCustomizerSlotHitboxes[];
extern const GraphicsBanks gSafariAreaCustomizerGraphicsBanks;
extern const WindowTemplate gSafariAreaCustomizerWindowTemplates[5];
extern const TouchscreenHitbox gSafariAreaCustomizerAreaHitboxes[];
extern const UnmanagedSpriteTemplate gSafariAreaCustomizerSprite0Template;
extern const UnmanagedSpriteTemplate gSafariAreaCustomizerSprite2Template;
extern const UnmanagedSpriteTemplate gSafariAreaCustomizerSprite4Template;
extern const UnmanagedSpriteTemplate gSafariAreaCustomizerSprite5Template;
extern const SafariAreaCustomizerBgTemplates gSafariAreaCustomizerBgTemplates;
extern const SafariAreaCustomizerResCounts gSafariAreaCustomizerResCounts;
extern const ResdatIdList gSafariAreaCustomizerResdatIds;
extern const OamCharTransferParam gSafariAreaCustomizerOamCharTransferParam;
extern const OamManagerParam gSafariAreaCustomizerOamManagerParam;

static void SafariAreaCustomizer_InitFromArgs(SafariAreaCustomizer *customizer, SafariAreaCustomizerArgs *args);
static void SafariAreaCustomizer_SaveResults(SafariAreaCustomizer *customizer);
static enum SafariAreaCustomizerState SafariAreaCustomizer_State_Setup(SafariAreaCustomizer *customizer);
static enum SafariAreaCustomizerState SafariAreaCustomizer_State_Teardown(SafariAreaCustomizer *customizer);
static int SafariAreaCustomizer_State_SelectSlot(SafariAreaCustomizer *customizer);
static int SafariAreaCustomizer_State_SelectArea(SafariAreaCustomizer *customizer);
static int SafariAreaCustomizer_State_MoveArea(SafariAreaCustomizer *customizer);
static enum SafariAreaCustomizerState SafariAreaCustomizer_State_FadeIn(SafariAreaCustomizer *customizer);
static enum SafariAreaCustomizerState SafariAreaCustomizer_State_FadeOut(SafariAreaCustomizer *customizer);
static BOOL SafariAreaCustomizer_Setup(SafariAreaCustomizer *customizer);
static BOOL SafariAreaCustomizer_Teardown(SafariAreaCustomizer *customizer);
static enum SafariAreaCustomizerInputState SafariAreaCustomizer_HandleSlotInput(SafariAreaCustomizer *customizer);
static enum SafariAreaCustomizerInputState SafariAreaCustomizer_HandleSlotMenu(SafariAreaCustomizer *customizer);
static enum SafariAreaCustomizerInputState SafariAreaCustomizer_ScrollPages(SafariAreaCustomizer *customizer);
static enum SafariAreaCustomizerInputState SafariAreaCustomizer_HandleAreaInput(SafariAreaCustomizer *customizer);
static enum SafariAreaCustomizerInputState SafariAreaCustomizer_HandleConfirmArea(SafariAreaCustomizer *customizer);
static enum SafariAreaCustomizerInputState SafariAreaCustomizer_ChangePage(SafariAreaCustomizer *customizer);
static enum SafariAreaCustomizerInputState SafariAreaCustomizer_HandleMoveInput(SafariAreaCustomizer *customizer);
static enum SafariAreaCustomizerInputState SafariAreaCustomizer_SwapAreas(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_ShowCursors(SafariAreaCustomizer *customizer);
static void thunk_SafariAreaCustomizer_ShowCursors(SafariAreaCustomizer *customizer);
static BOOL SafariAreaCustomizer_CheckInputModeSwitch(SafariAreaCustomizer *customizer);
static enum SafariAreaCustomizerInputState SafariAreaCustomizer_HandleSlotButtons(SafariAreaCustomizer *customizer);
static enum SafariAreaCustomizerInputState SafariAreaCustomizer_HandleSlotTouch(SafariAreaCustomizer *customizer);
static enum SafariAreaCustomizerInputState SafariAreaCustomizer_ChooseArea(SafariAreaCustomizer *customizer);
static enum SafariAreaCustomizerInputState SafariAreaCustomizer_HandleAreaButtons(SafariAreaCustomizer *customizer);
static enum SafariAreaCustomizerInputState SafariAreaCustomizer_HandleAreaTouch(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_StartMove(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_EndMove(SafariAreaCustomizer *customizer);
static enum SafariAreaCustomizerInputState SafariAreaCustomizer_HandleMoveButtons(SafariAreaCustomizer *customizer);
static enum SafariAreaCustomizerInputState SafariAreaCustomizer_HandleMoveTouch(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_OpenSlotMenu(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_AskConfirmArea(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_VBlankCB(void *cbArg);
static void SafariAreaCustomizer_SetGfxBanks(void);
static void SafariAreaCustomizer_InitBgLayers(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_FreeBgLayers(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_LoadBgGraphics(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_FreeScreenData(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_CreateStrings(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_DeleteStrings(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_AddWindows(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_RemoveWindows(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_CreateMenuItems(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_DeleteMenuItems(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_InitSprites(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_FreeSprites(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_CreateSprites(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_DeleteSprites(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_CreateCancelTextObj(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_DeleteCancelTextObj(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_DrawPageScreenPart(SafariAreaCustomizer *customizer, int value);
static void SafariAreaCustomizer_PrintWindowText(SafariAreaCustomizer *customizer, u8 windowId, u8 msg, BOOL unused);
static void SafariAreaCustomizer_PrintPageNumber(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_SetCursorAnim(SafariAreaCustomizer *customizer, u32 index, u32 anim, BOOL draw);
static void SafariAreaCustomizer_MoveCursor(SafariAreaCustomizer *customizer, u32 index, u32 pos);
static void SafariAreaCustomizer_DrawSelectFrame(SafariAreaCustomizer *customizer, u8 layer, int pos, BOOL show);
static void SafariAreaCustomizer_InitSlotTilemaps(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_LoadAreaPictureFromNarc(SafariAreaCustomizer *customizer, NARC *narc, u32 slot, int area);
static void SafariAreaCustomizer_LoadAreaPicture(SafariAreaCustomizer *customizer, u32 slot, int area);
static void SafariAreaCustomizer_InitScreen(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_PrintObjectTypeLabels(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_PrintSlotInfo(SafariAreaCustomizer *customizer, u32 slot);
static void SafariAreaCustomizer_PrintAreaNames(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_StartPageScroll(SafariAreaCustomizer *customizer);
static BOOL SafariAreaCustomizer_RunPageScroll(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_StartPageChange(SafariAreaCustomizer *customizer);
static BOOL SafariAreaCustomizer_RunPageChange(SafariAreaCustomizer *customizer);
static BOOL SafariAreaCustomizer_RunSwapAnim(SafariAreaCustomizer *customizer, int dir);
static void SafariAreaCustomizer_InitSpriteSystem(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_FreeSpriteSystem(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_CreateSpriteManager(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_DestroySpriteManager(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_DrawSprites(SafariAreaCustomizer *customizer);
static void SafariAreaCustomizer_TransferOam(SafariAreaCustomizer *customizer);
static SafariAreaCustomizerSpriteRes *SafariAreaCustomizerSpriteRes_New(enum HeapID heapId, u16 maxSprites, u16 vram, int transferMode);
static void SafariAreaCustomizerSpriteRes_Delete(SafariAreaCustomizerSpriteRes *res);
static void SafariAreaCustomizerSpriteRes_SetAreaPicture(SafariAreaCustomizerSpriteRes *res, u8 index, u8 area);
static void SafariAreaCustomizerSpriteRes_DrawSprites(SafariAreaCustomizerSpriteRes *res);
static void SafariAreaCustomizerSpriteRes_SetSpriteList(SafariAreaCustomizerSpriteRes *res, SpriteList *spriteList);
static ManagedSprite *SafariAreaCustomizerSpriteRes_CreateAreaSprite(SafariAreaCustomizerSpriteRes *res, u16 x, u16 y, u8 transfer, u8 priority, u8 index, u8 area, int vram);
static void SafariAreaCustomizer_DeleteAreaSprite(ManagedSprite *sprite);
static void SafariAreaCustomizerSpriteRes_Init(SafariAreaCustomizerSpriteRes *res);
static void SafariAreaCustomizerSpriteRes_Free(SafariAreaCustomizerSpriteRes *res);
static void SafariAreaCustomizerSpriteRes_LoadResources(SafariAreaCustomizerSpriteRes *res);

BOOL SafariAreaCustomizer_Init(OverlayManager *man, int *state) {
    SafariAreaCustomizerArgs *args = OverlayManager_GetArgs(man);
    SafariAreaCustomizer *customizer;

#pragma unused(state)
    Heap_Create(HEAP_ID_3, HEAP_ID_95, 0x50000);
    customizer = OverlayManager_CreateAndGetData(man, sizeof(SafariAreaCustomizer), HEAP_ID_95);
    MI_CpuFill8(customizer, 0, sizeof(SafariAreaCustomizer));
    customizer->heapId = HEAP_ID_95;
    SafariAreaCustomizer_InitFromArgs(customizer, args);
    Sound_SetSceneAndPlayBGM(0x43, 0, 0);
    return TRUE;
}

BOOL SafariAreaCustomizer_Exit(OverlayManager *man, int *state) {
    SafariAreaCustomizer *customizer = OverlayManager_GetData(man);
    enum HeapID heapId;

#pragma unused(state)
    SafariAreaCustomizer_SaveResults(customizer);
    heapId = customizer->heapId;
    MI_CpuFill8(customizer, 0, sizeof(SafariAreaCustomizer));
    OverlayManager_FreeData(man);
    Heap_Destroy(heapId);
    return TRUE;
}

BOOL SafariAreaCustomizer_Main(OverlayManager *man, int *state) {
    SafariAreaCustomizer *customizer = OverlayManager_GetData(man);

    switch (*state) {
    case SAC_STATE_SETUP:
        *state = SafariAreaCustomizer_State_Setup(customizer);
        break;
    case SAC_STATE_TEARDOWN:
        *state = SafariAreaCustomizer_State_Teardown(customizer);
        break;
    case SAC_STATE_SELECT_SLOT:
        *state = SafariAreaCustomizer_State_SelectSlot(customizer);
        break;
    case SAC_STATE_SELECT_AREA:
        *state = SafariAreaCustomizer_State_SelectArea(customizer);
        break;
    case SAC_STATE_MOVE_AREA:
        *state = SafariAreaCustomizer_State_MoveArea(customizer);
        break;
    case SAC_STATE_FADE_IN:
        *state = SafariAreaCustomizer_State_FadeIn(customizer);
        break;
    case SAC_STATE_FADE_OUT:
        *state = SafariAreaCustomizer_State_FadeOut(customizer);
        break;
    case SAC_STATE_EXIT:
        return TRUE;
    }
    return FALSE;
}

static void SafariAreaCustomizer_InitFromArgs(SafariAreaCustomizer *customizer, SafariAreaCustomizerArgs *args) {
    Options *options;

    customizer->safariZone = Save_SafariZone_Get(args->saveData);
    SafariZone_CopyAreaSet(customizer->safariZone, 0, &customizer->areaSet);
    options = Save_PlayerData_GetOptionsAddr(args->saveData);
    customizer->textSpeed = Options_GetTextSpeed(options);
    customizer->frame = Options_GetFrame(options);
    customizer->touchMode = MenuInputStateMgr_GetState(args->menuInputStatePtr);
    customizer->modified = FALSE;
    customizer->args = args;
    if (SafariZone_GetObjectUnlockLevel(customizer->safariZone)) {
        customizer->objectsUnlocked = TRUE;
    }
}

static void SafariAreaCustomizer_SaveResults(SafariAreaCustomizer *customizer) {
    SafariZone_SetAreaSet(customizer->safariZone, 0, &customizer->areaSet);
    ChangeFlag99D(Save_VarsFlags_Get(customizer->args->saveData), customizer->modified);
    MenuInputStateMgr_SetState(customizer->args->menuInputStatePtr, (MenuInputState)customizer->touchMode);
}

static enum SafariAreaCustomizerState SafariAreaCustomizer_State_Setup(SafariAreaCustomizer *customizer) {
    if (SafariAreaCustomizer_Setup(customizer)) {
        return SAC_STATE_FADE_IN;
    }
    return SAC_STATE_SETUP;
}

static enum SafariAreaCustomizerState SafariAreaCustomizer_State_Teardown(SafariAreaCustomizer *customizer) {
    if (SafariAreaCustomizer_Teardown(customizer)) {
        return SAC_STATE_EXIT;
    }
    return SAC_STATE_TEARDOWN;
}

static int SafariAreaCustomizer_State_SelectSlot(SafariAreaCustomizer *customizer) {
    switch (customizer->state) {
    case SAC_INPUT_STATE_WAIT:
        customizer->state = SafariAreaCustomizer_HandleSlotInput(customizer);
        break;
    case SAC_INPUT_STATE_MENU:
        customizer->state = SafariAreaCustomizer_HandleSlotMenu(customizer);
        break;
    case SAC_INPUT_STATE_SCROLL:
        customizer->state = SafariAreaCustomizer_ScrollPages(customizer);
        break;
    case SAC_INPUT_STATE_CHANGE_PAGE:
        break;
    case SAC_INPUT_STATE_EXIT:
        customizer->state = SAC_INPUT_STATE_WAIT;
        SafariAreaCustomizer_DrawSprites(customizer);
        return customizer->nextState;
    }
    SafariAreaCustomizer_DrawSprites(customizer);
    return SAC_STATE_SELECT_SLOT;
}

static int SafariAreaCustomizer_State_SelectArea(SafariAreaCustomizer *customizer) {
    switch (customizer->state) {
    case SAC_INPUT_STATE_WAIT:
        customizer->state = SafariAreaCustomizer_HandleAreaInput(customizer);
        break;
    case SAC_INPUT_STATE_MENU:
        customizer->state = SafariAreaCustomizer_HandleConfirmArea(customizer);
        break;
    case SAC_INPUT_STATE_SCROLL:
        customizer->state = SafariAreaCustomizer_ScrollPages(customizer);
        break;
    case SAC_INPUT_STATE_CHANGE_PAGE:
        customizer->state = SafariAreaCustomizer_ChangePage(customizer);
        break;
    case SAC_INPUT_STATE_EXIT:
        customizer->state = SAC_INPUT_STATE_WAIT;
        SafariAreaCustomizer_DrawSprites(customizer);
        return customizer->nextState;
    }
    SafariAreaCustomizer_DrawSprites(customizer);
    return SAC_STATE_SELECT_AREA;
}

static int SafariAreaCustomizer_State_MoveArea(SafariAreaCustomizer *customizer) {
    switch (customizer->state) {
    case SAC_INPUT_STATE_WAIT:
        customizer->state = SafariAreaCustomizer_HandleMoveInput(customizer);
        break;
    case SAC_INPUT_STATE_MENU:
        break;
    case SAC_INPUT_STATE_SCROLL:
        customizer->state = SafariAreaCustomizer_ScrollPages(customizer);
        break;
    case SAC_INPUT_STATE_SWAP_AREAS:
        customizer->state = SafariAreaCustomizer_SwapAreas(customizer);
        break;
    case SAC_INPUT_STATE_EXIT:
        customizer->state = SAC_INPUT_STATE_WAIT;
        SafariAreaCustomizer_DrawSprites(customizer);
        return customizer->nextState;
    }
    SafariAreaCustomizer_DrawSprites(customizer);
    return SAC_STATE_MOVE_AREA;
}

static enum SafariAreaCustomizerState SafariAreaCustomizer_State_FadeIn(SafariAreaCustomizer *customizer) {
    switch (customizer->state) {
    case SAC_FADE_START:
        BeginNormalPaletteFade(FADE_BOTH_SCREENS, FADE_TYPE_BRIGHTNESS_IN, FADE_TYPE_BRIGHTNESS_IN, RGB_BLACK, 6, 1, customizer->heapId);
        GfGfx_EngineATogglePlanes(GX_PLANEMASK_OBJ, GF_PLANE_TOGGLE_ON);
        GfGfx_EngineBTogglePlanes(GX_PLANEMASK_OBJ, GF_PLANE_TOGGLE_ON);
        customizer->state++;
        break;
    case SAC_FADE_WAIT:
        if (IsPaletteFadeFinished()) {
            customizer->state = SAC_FADE_START;
            SafariAreaCustomizer_DrawSprites(customizer);
            return SAC_STATE_SELECT_SLOT;
        }
        break;
    }
    SafariAreaCustomizer_DrawSprites(customizer);
    return SAC_STATE_FADE_IN;
}

static enum SafariAreaCustomizerState SafariAreaCustomizer_State_FadeOut(SafariAreaCustomizer *customizer) {
    switch (customizer->state) {
    case SAC_FADE_START:
        BeginNormalPaletteFade(FADE_BOTH_SCREENS, FADE_TYPE_BRIGHTNESS_OUT, FADE_TYPE_BRIGHTNESS_OUT, RGB_BLACK, 6, 1, customizer->heapId);
        customizer->state++;
        break;
    case SAC_FADE_WAIT:
        if (IsPaletteFadeFinished()) {
            customizer->state = SAC_FADE_START;
            SafariAreaCustomizer_DrawSprites(customizer);
            return SAC_STATE_TEARDOWN;
        }
        break;
    }
    SafariAreaCustomizer_DrawSprites(customizer);
    return SAC_STATE_FADE_OUT;
}

static BOOL SafariAreaCustomizer_Setup(SafariAreaCustomizer *customizer) {
    switch (customizer->subState) {
    case SAC_SETUP_RESET_DISPLAY:
        Main_SetVBlankIntrCB(NULL, NULL);
        HBlankInterruptDisable();
        GfGfx_DisableEngineAPlanes();
        GfGfx_DisableEngineBPlanes();
        GX_SetVisiblePlane(0);
        GXS_SetVisiblePlane(0);
        SetMasterBrightnessByColor(PM_LCD_TOP, RGB_BLACK);
        SetMasterBrightnessByColor(PM_LCD_BOTTOM, RGB_BLACK);
        ResetVisibleHardwareWindows(PM_LCD_TOP);
        ResetVisibleHardwareWindows(PM_LCD_BOTTOM);
        GF_TouchpadResetSampling();
        GF_TouchpadStartSampling(4);
        break;
    case SAC_SETUP_LOAD:
        SafariAreaCustomizer_InitBgLayers(customizer);
        SafariAreaCustomizer_LoadBgGraphics(customizer);
        SafariAreaCustomizer_CreateStrings(customizer);
        SafariAreaCustomizer_AddWindows(customizer);
        SafariAreaCustomizer_CreateMenuItems(customizer);
        SafariAreaCustomizer_InitSprites(customizer);
        SafariAreaCustomizer_CreateSprites(customizer);
        break;
    case SAC_SETUP_INIT_SCREEN:
        SafariAreaCustomizer_InitScreen(customizer);
        Main_SetVBlankIntrCB(SafariAreaCustomizer_VBlankCB, customizer);
        customizer->subState = SAC_SETUP_RESET_DISPLAY;
        return TRUE;
    }
    customizer->subState++;
    return FALSE;
}

static BOOL SafariAreaCustomizer_Teardown(SafariAreaCustomizer *customizer) {
    switch (customizer->subState) {
    case SAC_TEARDOWN_FREE:
        Main_SetVBlankIntrCB(NULL, NULL);
        HBlankInterruptDisable();
        SafariAreaCustomizer_DeleteSprites(customizer);
        SafariAreaCustomizer_FreeSprites(customizer);
        SafariAreaCustomizer_DeleteMenuItems(customizer);
        SafariAreaCustomizer_RemoveWindows(customizer);
        SafariAreaCustomizer_DeleteStrings(customizer);
        SafariAreaCustomizer_FreeScreenData(customizer);
        SafariAreaCustomizer_FreeBgLayers(customizer);
        break;
    case SAC_TEARDOWN_RESET_DISPLAY:
        GF_TouchpadStopSampling();
        GX_SetVisiblePlane(0);
        GXS_SetVisiblePlane(0);
        GfGfx_DisableEngineAPlanes();
        GfGfx_DisableEngineBPlanes();
        customizer->subState = SAC_TEARDOWN_FREE;
        return TRUE;
    }
    customizer->subState++;
    return FALSE;
}

static enum SafariAreaCustomizerInputState SafariAreaCustomizer_HandleSlotInput(SafariAreaCustomizer *customizer) {
    if (SafariAreaCustomizer_CheckInputModeSwitch(customizer)) {
        return SAC_INPUT_STATE_WAIT;
    }
    if (customizer->touchMode == MENU_INPUT_STATE_BUTTONS) {
        return SafariAreaCustomizer_HandleSlotButtons(customizer);
    }
    return SafariAreaCustomizer_HandleSlotTouch(customizer);
}

static enum SafariAreaCustomizerInputState SafariAreaCustomizer_HandleSlotMenu(SafariAreaCustomizer *customizer) {
    s32 input = TouchscreenListMenu_HandleInput(customizer->listMenu);
    u32 touchMode;

    if (input == -1) {
        return SAC_INPUT_STATE_MENU;
    }
    touchMode = customizer->listMenu->isTouch;
    TouchscreenListMenu_Destroy(customizer->listMenu);
    if (touchMode != customizer->touchMode) {
        if (touchMode == MENU_INPUT_STATE_BUTTONS) {
            SafariAreaCustomizer_ShowCursors(customizer);
        } else {
            thunk_SafariAreaCustomizer_ShowCursors(customizer);
        }
    }
    customizer->touchMode = touchMode;
    switch (input) {
    case 0:
        customizer->cursorPos = 0;
        return SAC_INPUT_STATE_SCROLL;
    case 1:
        SafariAreaCustomizer_DrawSelectFrame(customizer, GF_BG_LYR_MAIN_1, 0, FALSE);
        SafariAreaCustomizer_DrawSelectFrame(customizer, GF_BG_LYR_SUB_0, 0, FALSE);
        customizer->cursorPos = customizer->selectedSlot;
        SafariAreaCustomizer_StartMove(customizer);
        customizer->nextState = SAC_STATE_MOVE_AREA;
        return SAC_INPUT_STATE_EXIT;
    case 2:
    default:
        SafariAreaCustomizer_DrawSelectFrame(customizer, GF_BG_LYR_MAIN_1, 0, FALSE);
        SafariAreaCustomizer_DrawSelectFrame(customizer, GF_BG_LYR_SUB_0, 0, FALSE);
        return SAC_INPUT_STATE_WAIT;
    }
}

static enum SafariAreaCustomizerInputState SafariAreaCustomizer_ScrollPages(SafariAreaCustomizer *customizer) {
    switch (customizer->subState) {
    case SAC_ANIM_START:
        SafariAreaCustomizer_StartPageScroll(customizer);
        customizer->subState++;
        break;
    case SAC_ANIM_RUN:
        if (SafariAreaCustomizer_RunPageScroll(customizer)) {
            customizer->subState++;
        }
        break;
    default:
        GXS_SetVisibleWnd(GX_WNDMASK_NONE);
        customizer->animFrame = 0;
        customizer->subState = SAC_ANIM_START;
        if (customizer->mode == SAC_MODE_SLOTS) {
            customizer->nextState = SAC_STATE_SELECT_AREA;
            SafariAreaCustomizer_PrintWindowText(customizer, 0, 1, TRUE);
            SafariAreaCustomizer_PrintAreaNames(customizer);
            customizer->mode = SAC_MODE_AREAS;
        } else {
            customizer->nextState = SAC_STATE_SELECT_SLOT;
            SafariAreaCustomizer_PrintWindowText(customizer, 0, 0, TRUE);
            SafariAreaCustomizer_PrintSlotInfo(customizer, customizer->selectedSlot);
            customizer->mode = SAC_MODE_SLOTS;
        }
        return SAC_INPUT_STATE_EXIT;
    }
    return SAC_INPUT_STATE_SCROLL;
}

static enum SafariAreaCustomizerInputState SafariAreaCustomizer_HandleAreaInput(SafariAreaCustomizer *customizer) {
    if (SafariAreaCustomizer_CheckInputModeSwitch(customizer)) {
        return SAC_INPUT_STATE_WAIT;
    }
    if (customizer->touchMode == MENU_INPUT_STATE_BUTTONS) {
        return SafariAreaCustomizer_HandleAreaButtons(customizer);
    }
    return SafariAreaCustomizer_HandleAreaTouch(customizer);
}

static enum SafariAreaCustomizerInputState SafariAreaCustomizer_HandleConfirmArea(SafariAreaCustomizer *customizer) {
    BOOL yes;
    int touchMode;

    switch (YesNoPrompt_HandleInput(customizer->yesNoPrompt)) {
    case YESNORESPONSE_YES: {
        u8 chosen;
        u8 current;

        yes = TRUE;
        chosen = (u8)(customizer->page * NUM_AREA_SLOTS + customizer->cursorPos);
        current = customizer->areaSet.areas[customizer->selectedSlot].area_no;
        if (chosen != current) {
            customizer->modified = TRUE;
            SafariZone_InitAreaInSet(&customizer->areaSet, customizer->selectedSlot, chosen);
        }
        break;
    }
    case YESNORESPONSE_NO:
        SafariAreaCustomizer_LoadAreaPicture(customizer, customizer->selectedSlot, customizer->areaSet.areas[customizer->selectedSlot].area_no);
        yes = FALSE;
        break;
    default:
        return SAC_INPUT_STATE_MENU;
    }
    touchMode = YesNoPrompt_IsInTouchMode(customizer->yesNoPrompt);
    if (touchMode != customizer->touchMode) {
        if (!touchMode) {
            SafariAreaCustomizer_ShowCursors(customizer);
        } else {
            thunk_SafariAreaCustomizer_ShowCursors(customizer);
        }
    }
    customizer->touchMode = touchMode;
    YesNoPrompt_Reset(customizer->yesNoPrompt);
    ClearWindowTilemapAndCopyToVram(&customizer->windows[1]);
    SafariAreaCustomizer_DrawPageScreenPart(customizer, 0);
    SafariAreaCustomizer_PrintWindowText(customizer, 0, 1, TRUE);
    SafariAreaCustomizer_DrawSelectFrame(customizer, GF_BG_LYR_MAIN_1, 0, FALSE);
    if (yes) {
        return SAC_INPUT_STATE_SCROLL;
    }
    return SAC_INPUT_STATE_WAIT;
}

static enum SafariAreaCustomizerInputState SafariAreaCustomizer_ChangePage(SafariAreaCustomizer *customizer) {
    switch (customizer->subState) {
    case SAC_ANIM_START:
        SafariAreaCustomizer_StartPageChange(customizer);
        customizer->subState++;
        break;
    case SAC_ANIM_RUN:
        if (SafariAreaCustomizer_RunPageChange(customizer)) {
            customizer->subState++;
        }
        break;
    default:
        customizer->spriteBank ^= 1;
        customizer->subState = SAC_ANIM_START;
        return SAC_INPUT_STATE_WAIT;
    }
    return SAC_INPUT_STATE_CHANGE_PAGE;
}

static enum SafariAreaCustomizerInputState SafariAreaCustomizer_HandleMoveInput(SafariAreaCustomizer *customizer) {
    if (SafariAreaCustomizer_CheckInputModeSwitch(customizer)) {
        return SAC_INPUT_STATE_WAIT;
    }
    if (customizer->touchMode == MENU_INPUT_STATE_BUTTONS) {
        return SafariAreaCustomizer_HandleMoveButtons(customizer);
    }
    return SafariAreaCustomizer_HandleMoveTouch(customizer);
}

static enum SafariAreaCustomizerInputState SafariAreaCustomizer_SwapAreas(SafariAreaCustomizer *customizer) {
    u32 bank = customizer->spriteBank * NUM_AREA_SLOTS;

    switch (customizer->subState) {
    case SAC_SWAP_START:
        ManagedSprite_SetPriority(customizer->areaSprites[customizer->selectedSlot + bank], 0);
        ManagedSprite_SetPriority(customizer->areaSprites[customizer->cursorPos + bank], 0);
        GfGfx_EngineBTogglePlanes(GX_PLANEMASK_OBJ, GF_PLANE_TOGGLE_OFF);
        customizer->subState++;
        break;
    case SAC_SWAP_MOVE_OUT:
        if (SafariAreaCustomizer_RunSwapAnim(customizer, 0)) {
            customizer->modified = TRUE;
            SafariZone_SwapAreasInSet(&customizer->areaSet, customizer->selectedSlot, customizer->cursorPos);
            SafariAreaCustomizerSpriteRes_SetAreaPicture(customizer->spriteRes, customizer->selectedSlot + bank, customizer->areaSet.areas[customizer->selectedSlot].area_no);
            SafariAreaCustomizer_LoadAreaPicture(customizer, customizer->selectedSlot, customizer->areaSet.areas[customizer->selectedSlot].area_no);
            SafariAreaCustomizerSpriteRes_SetAreaPicture(customizer->spriteRes, customizer->cursorPos + bank, customizer->areaSet.areas[customizer->cursorPos].area_no);
            SafariAreaCustomizer_LoadAreaPicture(customizer, customizer->cursorPos, customizer->areaSet.areas[customizer->cursorPos].area_no);
            customizer->subState++;
        }
        break;
    case SAC_SWAP_MOVE_BACK:
        if (SafariAreaCustomizer_RunSwapAnim(customizer, 1)) {
            ManagedSprite_SetPriority(customizer->areaSprites[customizer->selectedSlot + bank], 2);
            ManagedSprite_SetPriority(customizer->areaSprites[customizer->cursorPos + bank], 2);
            GfGfx_EngineBTogglePlanes(GX_PLANEMASK_OBJ, GF_PLANE_TOGGLE_ON);
            customizer->subState++;
        }
        break;
    default:
        customizer->selectedSlot = customizer->cursorPos;
        SafariAreaCustomizer_EndMove(customizer);
        customizer->subState = SAC_SWAP_START;
        customizer->nextState = SAC_STATE_SELECT_SLOT;
        return SAC_INPUT_STATE_EXIT;
    }
    return SAC_INPUT_STATE_SWAP_AREAS;
}

static void SafariAreaCustomizer_ShowCursors(SafariAreaCustomizer *customizer) {
    switch (customizer->mode) {
    case SAC_MODE_SLOTS:
        SafariAreaCustomizer_MoveCursor(customizer, 0, customizer->selectedSlot);
        break;
    case SAC_MODE_AREAS:
        SafariAreaCustomizer_MoveCursor(customizer, 1, customizer->cursorPos);
        SafariAreaCustomizer_MoveCursor(customizer, 2, customizer->selectedSlot);
        break;
    case SAC_MODE_MOVE:
        SafariAreaCustomizer_MoveCursor(customizer, 0, customizer->cursorPos);
        break;
    }
}

static void thunk_SafariAreaCustomizer_ShowCursors(SafariAreaCustomizer *customizer) {
    SafariAreaCustomizer_ShowCursors(customizer);
}

static BOOL SafariAreaCustomizer_CheckInputModeSwitch(SafariAreaCustomizer *customizer) {
    if (customizer->touchMode == MENU_INPUT_STATE_TOUCH) {
        if (System_GetTouchHeld()) {
            return FALSE;
        }
        if (gSystem.heldKeys) {
            if (customizer->onSwitchToButtons != NULL) {
                customizer->onSwitchToButtons(customizer);
            }
            SafariAreaCustomizer_ShowCursors(customizer);
            customizer->touchMode = MENU_INPUT_STATE_BUTTONS;
            return FALSE;
        }
    } else {
        if (gSystem.heldKeys) {
            return FALSE;
        }
        if (System_GetTouchHeld()) {
            if (customizer->onSwitchToTouch != NULL) {
                customizer->onSwitchToTouch(customizer);
            }
            thunk_SafariAreaCustomizer_ShowCursors(customizer);
            customizer->touchMode = MENU_INPUT_STATE_TOUCH;
            return FALSE;
        }
    }
    return FALSE;
}

static enum SafariAreaCustomizerInputState SafariAreaCustomizer_HandleSlotButtons(SafariAreaCustomizer *customizer) {
    u32 keys = gSystem.newKeys;
    u8 old;
    u8 column;
    u8 row;

    if (keys & PAD_BUTTON_B) {
        PlaySE(SEQ_SE_DP_SELECT);
        customizer->nextState = SAC_STATE_FADE_OUT;
        return SAC_INPUT_STATE_EXIT;
    }
    if (keys & PAD_BUTTON_A) {
        if (customizer->selectedSlot >= NUM_AREA_SLOTS) {
            PlaySE(SEQ_SE_DP_SELECT);
            customizer->nextState = SAC_STATE_FADE_OUT;
            return SAC_INPUT_STATE_EXIT;
        }
        PlaySE(SEQ_SE_DP_SELECT);
        SafariAreaCustomizer_OpenSlotMenu(customizer);
        return SAC_INPUT_STATE_MENU;
    }
    if (!(keys & PAD_PLUS_KEY_MASK)) {
        return SAC_INPUT_STATE_WAIT;
    }
    old = customizer->selectedSlot;
    column = old % 3;
    row = old / 3;
    if (row < 2) {
        if (keys & PAD_KEY_RIGHT) {
            column = (column + 1) % 3;
        } else if (keys & PAD_KEY_LEFT) {
            column = (column + 2) % 3;
        }
    }
    if (keys & PAD_KEY_UP) {
        row = (row + 2) % 3;
    } else if (keys & PAD_KEY_DOWN) {
        row = (row + 1) % 3;
    }
    customizer->selectedSlot = column + row * 3;
    if (row < 2 || (gSystem.newKeys & (PAD_KEY_UP | PAD_KEY_DOWN))) {
        PlaySE(SEQ_SE_DP_BUTTON3);
    }
    SafariAreaCustomizer_MoveCursor(customizer, 0, customizer->selectedSlot);
    SafariAreaCustomizer_PrintSlotInfo(customizer, customizer->selectedSlot);
    return SAC_INPUT_STATE_WAIT;
}

static enum SafariAreaCustomizerInputState SafariAreaCustomizer_HandleSlotTouch(SafariAreaCustomizer *customizer) {
    s32 hit = TouchscreenHitbox_FindRectAtTouchNew(gSafariAreaCustomizerSlotHitboxes);

    if (hit == TOUCH_MENU_NO_INPUT) {
        return SAC_INPUT_STATE_WAIT;
    }
    if (hit == 6) {
        u16 transparent = 0xFFFE;
        if (!DoesPixelAtScreenXYMatchPtrVal(customizer->bgConfig, GF_BG_LYR_MAIN_3, gSystem.touchX, gSystem.touchY, &transparent)) {
            return SAC_INPUT_STATE_WAIT;
        }
        PlaySE(SEQ_SE_DP_SELECT);
        customizer->nextState = SAC_STATE_FADE_OUT;
        return SAC_INPUT_STATE_EXIT;
    }
    customizer->selectedSlot = hit;
    customizer->cursorPos = 0;
    SafariAreaCustomizer_MoveCursor(customizer, 0, customizer->selectedSlot);
    SafariAreaCustomizer_PrintSlotInfo(customizer, customizer->selectedSlot);
    PlaySE(SEQ_SE_DP_SELECT);
    SafariAreaCustomizer_OpenSlotMenu(customizer);
    return SAC_INPUT_STATE_MENU;
}

static enum SafariAreaCustomizerInputState SafariAreaCustomizer_ChooseArea(SafariAreaCustomizer *customizer) {
    u8 area;

    PlaySE(SEQ_SE_DP_SELECT);
    if (customizer->objectsUnlocked) {
        SafariAreaCustomizer_AskConfirmArea(customizer);
        return SAC_INPUT_STATE_MENU;
    }
    area = customizer->cursorPos + customizer->page * NUM_AREA_SLOTS;
    customizer->modified = TRUE;
    SafariZone_InitAreaInSet(&customizer->areaSet, customizer->selectedSlot, area);
    SafariAreaCustomizer_LoadAreaPicture(customizer, customizer->selectedSlot, customizer->areaSet.areas[customizer->selectedSlot].area_no);
    SafariAreaCustomizer_DrawSelectFrame(customizer, GF_BG_LYR_MAIN_1, 0, FALSE);
    return SAC_INPUT_STATE_SCROLL;
}

static enum SafariAreaCustomizerInputState SafariAreaCustomizer_HandleAreaButtons(SafariAreaCustomizer *customizer) {
    u8 old = customizer->cursorPos;
    u32 keys = gSystem.newKeys;
    u8 column;
    u8 row;
    u32 changePage;

    if (keys & PAD_BUTTON_B) {
        PlaySE(SEQ_SE_DP_SELECT);
        return SAC_INPUT_STATE_SCROLL;
    }
    if (keys & PAD_BUTTON_A) {
        if (old >= NUM_AREA_SLOTS) {
            PlaySE(SEQ_SE_DP_SELECT);
            return SAC_INPUT_STATE_SCROLL;
        }
        if (old + customizer->page * NUM_AREA_SLOTS == customizer->areaSet.areas[customizer->selectedSlot].area_no) {
            return SAC_INPUT_STATE_WAIT;
        }
        return SafariAreaCustomizer_ChooseArea(customizer);
    }
    if (!(keys & PAD_PLUS_KEY_MASK)) {
        return SAC_INPUT_STATE_WAIT;
    }
    column = old % 3;
    row = old / 3;
    changePage = FALSE;
    if ((keys & PAD_KEY_RIGHT) && row < 2) {
        if (column == 2) {
            if (row == 0) {
                row ^= 1;
                column = (column + 1) % 3;
            } else if (row < 2 && customizer->page < 1) {
                changePage = TRUE;
                customizer->pageDir = 0;
                row ^= changePage;
                column = (column + 1) % 3;
            }
        } else {
            column = (column + 1) % 3;
        }
    } else if ((keys & PAD_KEY_LEFT) && row < 2) {
        if (column == 0) {
            if (row == 1) {
                row ^= 1;
                column = (column + 2) % 3;
            } else if (row < 2 && customizer->page != 0) {
                changePage = TRUE;
                customizer->pageDir = 1;
                row ^= changePage;
                column = (column + 2) % 3;
            }
        } else {
            column = (column + 2) % 3;
        }
    } else if (keys & PAD_KEY_UP) {
        row = (row + 2) % 3;
    } else if (keys & PAD_KEY_DOWN) {
        row = (row + 1) % 3;
    }
    customizer->cursorPos = column + row * 3;
    SafariAreaCustomizer_MoveCursor(customizer, 1, customizer->cursorPos);
    if (changePage) {
        PlaySE(SEQ_SE_DP_SELECT5);
        SafariAreaCustomizer_SetCursorAnim(customizer, 1, 0, FALSE);
        return SAC_INPUT_STATE_CHANGE_PAGE;
    }
    if (old != customizer->cursorPos) {
        PlaySE(SEQ_SE_DP_BUTTON3);
    }
    SafariAreaCustomizer_PrintAreaNames(customizer);
    return SAC_INPUT_STATE_WAIT;
}

static enum SafariAreaCustomizerInputState SafariAreaCustomizer_HandleAreaTouch(SafariAreaCustomizer *customizer) {
    s32 hit = TouchscreenHitbox_FindRectAtTouchNew(gSafariAreaCustomizerAreaHitboxes);

    if (hit == TOUCH_MENU_NO_INPUT) {
        return SAC_INPUT_STATE_WAIT;
    }
    switch (hit) {
    case 6: {
        u16 transparent = 0xFFFE;
        if (!DoesPixelAtScreenXYMatchPtrVal(customizer->bgConfig, GF_BG_LYR_MAIN_3, gSystem.touchX, gSystem.touchY, &transparent)) {
            return SAC_INPUT_STATE_WAIT;
        }
        customizer->cursorPos = 6;
        PlaySE(SEQ_SE_DP_SELECT);
        return SAC_INPUT_STATE_SCROLL;
    }
    case 7:
        if (customizer->page == 0) {
            return SAC_INPUT_STATE_WAIT;
        }
        customizer->pageDir = 1;
        customizer->cursorPos = 5;
        SafariAreaCustomizer_SetCursorAnim(customizer, 1, 0, FALSE);
        Sprite_SetAnimActiveFlag(customizer->sprites[4], TRUE);
        Sprite_ResetAnimCtrlState(customizer->sprites[4]);
        PlaySE(SEQ_SE_DP_SELECT5);
        return SAC_INPUT_STATE_CHANGE_PAGE;
    case 8:
        if (customizer->page == 1) {
            return SAC_INPUT_STATE_WAIT;
        }
        customizer->pageDir = 0;
        customizer->cursorPos = 0;
        SafariAreaCustomizer_SetCursorAnim(customizer, 1, 0, FALSE);
        Sprite_SetAnimActiveFlag(customizer->sprites[5], TRUE);
        Sprite_ResetAnimCtrlState(customizer->sprites[5]);
        PlaySE(SEQ_SE_DP_SELECT5);
        return SAC_INPUT_STATE_CHANGE_PAGE;
    default:
        customizer->cursorPos = hit;
        SafariAreaCustomizer_MoveCursor(customizer, 1, customizer->cursorPos);
        SafariAreaCustomizer_PrintAreaNames(customizer);
        if (customizer->cursorPos + customizer->page * NUM_AREA_SLOTS == customizer->areaSet.areas[customizer->selectedSlot].area_no) {
            return SAC_INPUT_STATE_WAIT;
        }
        return SafariAreaCustomizer_ChooseArea(customizer);
    }
}

static void SafariAreaCustomizer_StartMove(SafariAreaCustomizer *customizer) {
    customizer->mode = SAC_MODE_MOVE;
    SafariAreaCustomizer_MoveCursor(customizer, 0, customizer->cursorPos);
    thunk_Sprite_SetPriority(customizer->sprites[0], 0);
    SafariAreaCustomizer_MoveCursor(customizer, 3, customizer->selectedSlot);
    SafariAreaCustomizer_PrintWindowText(customizer, 0, 2, TRUE);
}

static void SafariAreaCustomizer_EndMove(SafariAreaCustomizer *customizer) {
    customizer->mode = SAC_MODE_SLOTS;
    SafariAreaCustomizer_MoveCursor(customizer, 0, customizer->selectedSlot);
    thunk_Sprite_SetPriority(customizer->sprites[0], 2);
    SafariAreaCustomizer_SetCursorAnim(customizer, 3, 0, FALSE);
    SafariAreaCustomizer_PrintWindowText(customizer, 0, 0, TRUE);
}

static enum SafariAreaCustomizerInputState SafariAreaCustomizer_HandleMoveButtons(SafariAreaCustomizer *customizer) {
    u32 keys = gSystem.newKeys;
    u8 column;
    u8 row;

    if (keys & PAD_BUTTON_B) {
        SafariAreaCustomizer_EndMove(customizer);
        customizer->nextState = SAC_STATE_SELECT_SLOT;
        PlaySE(SEQ_SE_DP_SELECT);
        return SAC_INPUT_STATE_EXIT;
    }
    if (keys & PAD_BUTTON_A) {
        if (customizer->cursorPos >= NUM_AREA_SLOTS || customizer->selectedSlot == customizer->cursorPos) {
            SafariAreaCustomizer_EndMove(customizer);
            customizer->nextState = SAC_STATE_SELECT_SLOT;
            PlaySE(SEQ_SE_DP_SELECT);
            return SAC_INPUT_STATE_EXIT;
        }
        PlaySE(SEQ_SE_DP_SYU02);
        return SAC_INPUT_STATE_SWAP_AREAS;
    }
    if (!(keys & PAD_PLUS_KEY_MASK)) {
        return SAC_INPUT_STATE_WAIT;
    }
    column = (s32)customizer->cursorPos % 3;
    row = (s32)customizer->cursorPos / 3;
    if (row < 2) {
        if (keys & PAD_KEY_RIGHT) {
            column = (column + 1) % 3;
        } else if (keys & PAD_KEY_LEFT) {
            column = (column + 2) % 3;
        }
    }
    if (keys & PAD_KEY_UP) {
        row = (row + 2) % 3;
    } else if (keys & PAD_KEY_DOWN) {
        row = (row + 1) % 3;
    }
    customizer->cursorPos = row * 3 + column;
    if (row < 2 || (gSystem.newKeys & (PAD_KEY_UP | PAD_KEY_DOWN))) {
        PlaySE(SEQ_SE_DP_BUTTON3);
    }
    SafariAreaCustomizer_MoveCursor(customizer, 0, customizer->cursorPos);
    return SAC_INPUT_STATE_WAIT;
}

static enum SafariAreaCustomizerInputState SafariAreaCustomizer_HandleMoveTouch(SafariAreaCustomizer *customizer) {
    int hit = TouchscreenHitbox_FindRectAtTouchNew(gSafariAreaCustomizerSlotHitboxes);

    if (hit == TOUCH_MENU_NO_INPUT) {
        return SAC_INPUT_STATE_WAIT;
    }
    if (hit == 6) {
        u16 transparent = 0xFFFE;
        if (!DoesPixelAtScreenXYMatchPtrVal(customizer->bgConfig, GF_BG_LYR_MAIN_3, gSystem.touchX, gSystem.touchY, &transparent)) {
            return SAC_INPUT_STATE_WAIT;
        }
        customizer->cursorPos = 6;
        SafariAreaCustomizer_EndMove(customizer);
        customizer->nextState = SAC_STATE_SELECT_SLOT;
        PlaySE(SEQ_SE_DP_SELECT);
        return SAC_INPUT_STATE_EXIT;
    }
    if (customizer->selectedSlot == hit) {
        SafariAreaCustomizer_EndMove(customizer);
        customizer->nextState = SAC_STATE_SELECT_SLOT;
        PlaySE(SEQ_SE_DP_SELECT);
        return SAC_INPUT_STATE_EXIT;
    }
    customizer->cursorPos = hit;
    SafariAreaCustomizer_MoveCursor(customizer, 0, customizer->cursorPos);
    PlaySE(SEQ_SE_DP_SYU02);
    return SAC_INPUT_STATE_SWAP_AREAS;
}

static void SafariAreaCustomizer_OpenSlotMenu(SafariAreaCustomizer *customizer) {
    TouchscreenListMenuHeader header;
    u16 x;
    u16 y;

    MI_CpuFill8(&header, 0, sizeof(header));
    header.template = gSafariAreaCustomizerConsts.menuTemplate;
    header.listMenuItems = customizer->areaMenuItems;
    header.bgConfig = customizer->bgConfig;
    header.numWindows = 3;
    y = gSafariAreaCustomizerSlotMenuPositions[customizer->selectedSlot].y;
    x = gSafariAreaCustomizerSlotMenuPositions[customizer->selectedSlot].x;
    customizer->listMenu = TouchscreenListMenu_Create(customizer->listMenuSpawner, &header, customizer->touchMode, x, y, 0, 0);
    SafariAreaCustomizer_DrawSelectFrame(customizer, GF_BG_LYR_MAIN_1, customizer->selectedSlot, TRUE);
    SafariAreaCustomizer_DrawSelectFrame(customizer, GF_BG_LYR_SUB_0, customizer->selectedSlot, TRUE);
}

static void SafariAreaCustomizer_AskConfirmArea(SafariAreaCustomizer *customizer) {
    YesNoPromptTemplate template;

    MI_CpuFill8(&template, 0, sizeof(template));
    template.bgConfig = customizer->bgConfig;
    template.bgId = GF_BG_LYR_MAIN_0;
    template.tileStart = 0x379;
    template.plttSlot = 13;
    template.x = 26;
    template.y = 16;
    template.ignoreTouchFlag = (u8)customizer->touchMode;
    template.initialCursorPos = 0;
    YesNoPrompt_InitFromTemplate(customizer->yesNoPrompt, &template);
    SafariAreaCustomizer_DrawSelectFrame(customizer, GF_BG_LYR_MAIN_1, customizer->cursorPos, TRUE);
    SafariAreaCustomizer_LoadAreaPicture(customizer, customizer->selectedSlot, (u8)(customizer->cursorPos + NUM_AREA_SLOTS * customizer->page));
    SafariAreaCustomizer_DrawPageScreenPart(customizer, 1);
    SafariAreaCustomizer_PrintWindowText(customizer, 1, 3, TRUE);
}

static void SafariAreaCustomizer_VBlankCB(void *cbArg) {
    SafariAreaCustomizer *customizer = cbArg;

    SafariAreaCustomizer_TransferOam(customizer);
    if (customizer->scrollPending) {
        BgSetPosTextAndCommit(customizer->bgConfig, GF_BG_LYR_MAIN_1, BG_POS_OP_SET_Y, customizer->mainBgY);
        BgSetPosTextAndCommit(customizer->bgConfig, GF_BG_LYR_MAIN_2, BG_POS_OP_SET_Y, customizer->mainBgY);
        BgSetPosTextAndCommit(customizer->bgConfig, GF_BG_LYR_SUB_0, BG_POS_OP_SET_Y, customizer->subBgY);
        BgSetPosTextAndCommit(customizer->bgConfig, GF_BG_LYR_SUB_2, BG_POS_OP_SET_Y, customizer->subBgY);
        BgSetPosTextAndCommit(customizer->bgConfig, GF_BG_LYR_SUB_1, BG_POS_OP_SET_Y, customizer->subBgY);
        customizer->scrollPending = FALSE;
    }
    GF_RunVramTransferTasks();
    DoScheduledBgGpuUpdates(customizer->bgConfig);
    OS_SetIrqCheckFlag(OS_IE_V_BLANK);
}

static void SafariAreaCustomizer_SetGfxBanks(void) {
    GraphicsBanks banks = gSafariAreaCustomizerGraphicsBanks;
    GfGfx_SetBanks(&banks);
}

static void SafariAreaCustomizer_InitBgLayers(SafariAreaCustomizer *customizer) {
    GraphicsModes modes;
    SafariAreaCustomizerBgTemplates templates;
    int i;
    int count;
    BgTemplate *template;

    SafariAreaCustomizer_SetGfxBanks();
    GX_SetDispSelect(GX_DISP_SELECT_SUB_MAIN);
    customizer->bgConfig = BgConfig_Alloc(customizer->heapId);
    modes = gSafariAreaCustomizerGraphicsModes;
    SetBothScreensModesAndDisable(&modes);
    templates = gSafariAreaCustomizerBgTemplates;
    i = 0;
    count = 0;
    template = templates.templates;
    while (count < 8) {
        InitBgFromTemplate(customizer->bgConfig, i, template, GF_BG_TYPE_TEXT);
        BgClearTilemapBufferAndCommit(customizer->bgConfig, i);
        BG_ClearCharDataRange(i, 0x40, 0, customizer->heapId);
        count++;
        i++;
        template ++;
    }
    G2_SetBlendAlpha(GX_BLEND_PLANEMASK_BG1, GX_BLEND_PLANEMASK_BG2 | GX_BLEND_PLANEMASK_BG3 | GX_BLEND_PLANEMASK_OBJ, 28, 4);
    G2S_SetBlendAlpha(GX_BLEND_PLANEMASK_BG0, GX_BLEND_PLANEMASK_BG1 | GX_BLEND_PLANEMASK_BG2 | GX_BLEND_PLANEMASK_BG3 | GX_BLEND_PLANEMASK_OBJ, 28, 4);
}

static void SafariAreaCustomizer_FreeBgLayers(SafariAreaCustomizer *customizer) {
    int i;

    G2_SetBlendAlpha(GX_BLEND_PLANEMASK_NONE, GX_BLEND_PLANEMASK_NONE, 31, 0);
    G2S_SetBlendAlpha(GX_BLEND_PLANEMASK_NONE, GX_BLEND_PLANEMASK_NONE, 31, 0);
    for (i = 0; i < 8; i++) {
        FreeBgTilemapBuffer(customizer->bgConfig, i);
    }
    Heap_Free(customizer->bgConfig);
    GX_SetDispSelect(GX_DISP_SELECT_MAIN_SUB);
}

static void SafariAreaCustomizer_LoadBgGraphics(SafariAreaCustomizer *customizer) {
    NARC *narc = NARC_New(NARC_a_1_6_6, customizer->heapId);

    BgConfig_LoadAssetFromOpenNarc(customizer->bgConfig, customizer->heapId, narc, NARC_a_1_6_6, 6, GF_BG_LYR_MAIN_3, GF_BG_GFX_TYPE_CHAR, 0, 0);
    BgConfig_LoadAssetFromOpenNarc(customizer->bgConfig, customizer->heapId, narc, NARC_a_1_6_6, 6, GF_BG_LYR_SUB_0, GF_BG_GFX_TYPE_CHAR, 0, 0);
    BgConfig_LoadAssetFromOpenNarc(customizer->bgConfig, customizer->heapId, narc, NARC_a_1_6_6, 1, GF_BG_LYR_SUB_3, GF_BG_GFX_TYPE_CHAR, 0, 0);
    BgConfig_LoadAssetFromOpenNarc(customizer->bgConfig, customizer->heapId, narc, NARC_a_1_6_6, 5, GF_BG_LYR_MAIN_3, GF_BG_GFX_TYPE_PLTT, 0, 0);
    BgConfig_LoadAssetFromOpenNarc(customizer->bgConfig, customizer->heapId, narc, NARC_a_1_6_6, 0, GF_BG_LYR_SUB_3, GF_BG_GFX_TYPE_PLTT, 0, 0);
    BgConfig_LoadAssetFromOpenNarc(customizer->bgConfig, customizer->heapId, narc, NARC_a_1_6_6, 10, GF_BG_LYR_MAIN_2, GF_BG_GFX_TYPE_SCRN, 0, 0);
    BgConfig_LoadAssetFromOpenNarc(customizer->bgConfig, customizer->heapId, narc, NARC_a_1_6_6, 7, GF_BG_LYR_MAIN_3, GF_BG_GFX_TYPE_SCRN, 0, 0);
    BgConfig_LoadAssetFromOpenNarc(customizer->bgConfig, customizer->heapId, narc, NARC_a_1_6_6, 4, GF_BG_LYR_SUB_2, GF_BG_GFX_TYPE_SCRN, 0, 0);
    BgConfig_LoadAssetFromOpenNarc(customizer->bgConfig, customizer->heapId, narc, NARC_a_1_6_6, (!customizer->objectsUnlocked) + 2, GF_BG_LYR_SUB_3, GF_BG_GFX_TYPE_SCRN, 0, 0);
    customizer->pageScreenFile = GfGfxLoader_GetScrnDataFromOpenNarc(narc, 8, FALSE, &customizer->pageScreen, customizer->heapId);
    customizer->selectFrameScreenFile = GfGfxLoader_GetScrnDataFromOpenNarc(narc, 9, FALSE, &customizer->selectFrameScreen, customizer->heapId);
    NARC_Delete(narc);
    SafariAreaCustomizer_InitSlotTilemaps(customizer);
    SafariAreaCustomizer_LoadAreaPicture(customizer, 0xFF, 0);
    ScheduleBgTilemapBufferTransfer(customizer->bgConfig, GF_BG_LYR_MAIN_0);
    ScheduleBgTilemapBufferTransfer(customizer->bgConfig, GF_BG_LYR_MAIN_2);
    ScheduleBgTilemapBufferTransfer(customizer->bgConfig, GF_BG_LYR_MAIN_3);
    ScheduleBgTilemapBufferTransfer(customizer->bgConfig, GF_BG_LYR_SUB_2);
    ScheduleBgTilemapBufferTransfer(customizer->bgConfig, GF_BG_LYR_SUB_3);
}

static void SafariAreaCustomizer_FreeScreenData(SafariAreaCustomizer *customizer) {
    Heap_Free(customizer->pageScreenFile);
    Heap_Free(customizer->selectFrameScreenFile);
}

static void SafariAreaCustomizer_CreateStrings(SafariAreaCustomizer *customizer) {
    int i;

    FontID_Alloc(4, customizer->heapId);
    customizer->msgData = NewMsgDataFromNarc(MSGDATA_LOAD_DIRECT, NARC_msgdata_msg, NARC_msg_msg_0429_bin, customizer->heapId);
    customizer->msgFormat = MessageFormat_New_Custom(1, 17, customizer->heapId);
    customizer->string = String_New(81, customizer->heapId);
    customizer->pageTemplate = NewString_ReadMsgData(customizer->msgData, msg_0429_00009);
    customizer->countTemplate = NewString_ReadMsgData(customizer->msgData, msg_0429_00015);
    for (i = 0; i < 5; i++) {
        customizer->labelStrings[i] = NewString_ReadMsgData(customizer->msgData, i + msg_0429_00004);
    }
    for (i = 0; i < 4; i++) {
        customizer->menuStrings[i] = NewString_ReadMsgData(customizer->msgData, i);
    }
}

static void SafariAreaCustomizer_DeleteStrings(SafariAreaCustomizer *customizer) {
    int i;

    for (i = 0; i < 4; i++) {
        String_Delete(customizer->menuStrings[i]);
    }
    for (i = 0; i < 5; i++) {
        String_Delete(customizer->labelStrings[i]);
    }
    String_Delete(customizer->countTemplate);
    String_Delete(customizer->pageTemplate);
    String_Delete(customizer->string);
    MessageFormat_Delete(customizer->msgFormat);
    DestroyMsgData(customizer->msgData);
    FontID_Release(4);
}

static void SafariAreaCustomizer_AddWindows(SafariAreaCustomizer *customizer) {
    int i;
    u8 y;
    int windowId;
    u8 height;
    int baseTile;

    for (i = 0; i <= 4; i++) {
        AddWindow(customizer->bgConfig, &customizer->windows[i], &gSafariAreaCustomizerWindowTemplates[i]);
        FillWindowPixelBuffer(&customizer->windows[i], 0);
    }
    baseTile = 0x3A6;
    for (i = 0; i < 5; i++) {
        windowId = i * 2 + 5;
        y = i * 4 + 1;
        height = (u8)(1 ^ (i / 4)) * 2 + 2;
        baseTile -= 22 * height;
        AddWindowParameterized(customizer->bgConfig, &customizer->windows[windowId], GF_BG_LYR_SUB_3, 1, y, 22, height, 5, baseTile);
        FillWindowPixelBuffer(&customizer->windows[windowId], (gSafariAreaCustomizerLabelColors[i % 2] & 0xFF));
        baseTile -= 22 * height;
        AddWindowParameterized(customizer->bgConfig, &customizer->windows[windowId + 1], GF_BG_LYR_SUB_3, 26, y + (u8)(1 ^ (i / 4)), 5, 2, 5, baseTile);
        FillWindowPixelBuffer(&customizer->windows[windowId + 1], (gSafariAreaCustomizerLabelColors[i % 2] & 0xFF));
        baseTile -= 10;
    }
    InitWindow(&customizer->textObjWindow);
    AddTextWindowTopLeftCorner(customizer->bgConfig, &customizer->textObjWindow, 6, 2, 0x2E1, 0);
    FillWindowPixelBufferText_AssumeTileSize32(&customizer->textObjWindow, 0);
    customizer->yesNoPrompt = YesNoPrompt_Create(customizer->heapId);
}

static void SafariAreaCustomizer_RemoveWindows(SafariAreaCustomizer *customizer) {
    int i;

    YesNoPrompt_Destroy(customizer->yesNoPrompt);
    RemoveWindow(&customizer->textObjWindow);
    for (i = 0; i < 15; i++) {
        ClearWindowTilemapAndCopyToVram(&customizer->windows[i]);
        RemoveWindow(&customizer->windows[i]);
    }
}

static void SafariAreaCustomizer_CreateMenuItems(SafariAreaCustomizer *customizer) {
    customizer->areaMenuItems = ListMenuItems_New(3, customizer->heapId);
    customizer->confirmMenuItems = ListMenuItems_New(2, customizer->heapId);
    ListMenuItems_AppendFromMsgData(customizer->areaMenuItems, customizer->msgData, msg_0429_00007, 0);
    ListMenuItems_AppendFromMsgData(customizer->areaMenuItems, customizer->msgData, msg_0429_00008, 1);
    ListMenuItems_AppendFromMsgData(customizer->areaMenuItems, customizer->msgData, msg_0429_00006, 2);
    ListMenuItems_AppendFromMsgData(customizer->confirmMenuItems, customizer->msgData, msg_0429_00005, 0);
    ListMenuItems_AppendFromMsgData(customizer->confirmMenuItems, customizer->msgData, msg_0429_00006, 1);
}

static void SafariAreaCustomizer_DeleteMenuItems(SafariAreaCustomizer *customizer) {
    ListMenuItems_Delete(customizer->confirmMenuItems);
    ListMenuItems_Delete(customizer->areaMenuItems);
    customizer->areaMenuItems = NULL;
    customizer->confirmMenuItems = NULL;
}

static void SafariAreaCustomizer_InitSprites(SafariAreaCustomizer *customizer) {
    SafariAreaCustomizer_InitSpriteSystem(customizer);
    customizer->spriteRes = SafariAreaCustomizerSpriteRes_New(customizer->heapId, 12, NNS_G2D_VRAM_TYPE_2DMAIN, 2);
    SafariAreaCustomizer_CreateSpriteManager(customizer);
    customizer->listMenuSpawner = TouchscreenListMenuSpawner_Create(customizer->heapId, NULL);
    customizer->fontSystem = FontSystem_NewInit(1, customizer->heapId);
}

static void SafariAreaCustomizer_FreeSprites(SafariAreaCustomizer *customizer) {
    FontSystem_Delete(customizer->fontSystem);
    TouchscreenListMenuSpawner_Destroy(customizer->listMenuSpawner);
    SafariAreaCustomizer_DestroySpriteManager(customizer);
    SafariAreaCustomizerSpriteRes_Delete(customizer->spriteRes);
    SafariAreaCustomizer_FreeSpriteSystem(customizer);
}

static void SafariAreaCustomizer_CreateSprites(SafariAreaCustomizer *customizer) {
    int i = 0;

    for (; i < NUM_AREA_SLOTS; i++) {
        customizer->areaSprites[i] = SafariAreaCustomizerSpriteRes_CreateAreaSprite(customizer->spriteRes, (i % 3) * 72 + 56, (i / 3) * 72 + 56, 2, 128, i, customizer->areaSet.areas[i].area_no, NNS_G2D_VRAM_TYPE_2DMAIN);
    }
    for (i = 0; i < NUM_AREA_SLOTS; i++) {
        customizer->areaSprites[i + 6] = SafariAreaCustomizerSpriteRes_CreateAreaSprite(customizer->spriteRes, (i % 3) * 80 + 48, (i / 3) * 72 + 56, 3, 128, i + 6, i + 6 * customizer->page, NNS_G2D_VRAM_TYPE_2DMAIN);
    }
    customizer->sprites[0] = SpriteSystem_CreateSpriteFromResourceHeader(customizer->spriteSystem, customizer->spriteManager, &gSafariAreaCustomizerSprite0Template);
    customizer->sprites[1] = SpriteSystem_CreateSpriteFromResourceHeader(customizer->spriteSystem, customizer->spriteManager, &gSafariAreaCustomizerSprite0Template);
    customizer->sprites[3] = SpriteSystem_CreateSpriteFromResourceHeader(customizer->spriteSystem, customizer->spriteManager, &gSafariAreaCustomizerSprite0Template);
    customizer->sprites[2] = SpriteSystem_CreateSpriteFromResourceHeader(customizer->spriteSystem, customizer->spriteManager, &gSafariAreaCustomizerSprite2Template);
    customizer->sprites[4] = SpriteSystem_CreateSpriteFromResourceHeader(customizer->spriteSystem, customizer->spriteManager, &gSafariAreaCustomizerSprite4Template);
    customizer->sprites[5] = SpriteSystem_CreateSpriteFromResourceHeader(customizer->spriteSystem, customizer->spriteManager, &gSafariAreaCustomizerSprite5Template);
    thunk_Sprite_SetPriority(customizer->sprites[1], 3);
    thunk_Sprite_SetPriority(customizer->sprites[4], 3);
    thunk_Sprite_SetPriority(customizer->sprites[5], 3);
    thunk_Sprite_SetPriority(customizer->sprites[3], 0);
    thunk_Sprite_SetDrawPriority(customizer->sprites[3], 5);
    thunk_Sprite_SetDrawFlag(customizer->sprites[0], FALSE);
    thunk_Sprite_SetDrawFlag(customizer->sprites[1], FALSE);
    thunk_Sprite_SetDrawFlag(customizer->sprites[3], FALSE);
    thunk_Sprite_SetDrawFlag(customizer->sprites[2], FALSE);
    SafariAreaCustomizer_CreateCancelTextObj(customizer);
}

static void SafariAreaCustomizer_DeleteSprites(SafariAreaCustomizer *customizer) {
    int i;

    SafariAreaCustomizer_DeleteCancelTextObj(customizer);
    thunk_Sprite_Delete(customizer->sprites[0]);
    thunk_Sprite_Delete(customizer->sprites[1]);
    thunk_Sprite_Delete(customizer->sprites[2]);
    thunk_Sprite_Delete(customizer->sprites[3]);
    thunk_Sprite_Delete(customizer->sprites[4]);
    thunk_Sprite_Delete(customizer->sprites[5]);
    for (i = 0; i < 12; i++) {
        SafariAreaCustomizer_DeleteAreaSprite(customizer->areaSprites[i]);
    }
}

// The "CANCEL" text object of the main screen.
static void SafariAreaCustomizer_CreateCancelTextObj(SafariAreaCustomizer *customizer) {
    TextOBJTemplate template;
    u32 size;
    u32 width;
    u32 x;

    customizer->fontLayout = TextOBJLayout_New(&customizer->textObjWindow, customizer->heapId);
    size = TextOBJLayout_GetCharSize(customizer->fontLayout, NNS_G2D_VRAM_TYPE_2DMAIN);
    width = GetWindowWidth(&customizer->textObjWindow);
    x = (width * 8 - FontID_String_GetWidth(0, customizer->labelStrings[0], 0)) / 2;
    AddTextPrinterParameterizedWithColor(&customizer->textObjWindow, 0, customizer->labelStrings[0], x, 0, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(11, 6, 0), NULL);
    template.fontSystem = customizer->fontSystem;
    template.window = &customizer->textObjWindow;
    template.spriteList = SpriteManager_GetSpriteList(customizer->spriteManager);
    template.plttResourceProxy = SpriteManager_FindPlttResourceProxy(customizer->spriteManager, 0);
    template.bgPriority = 3;
    template.drawPriority = 128;
    template.x = 200;
    template.y = 172;
    template.vram = NNS_G2D_VRAM_TYPE_2DMAIN;
    template.heapID = customizer->heapId;
    ObjCharTransfer_ReserveCharSpace(size, FALSE, NNS_G2D_VRAM_TYPE_2DMAIN, &customizer->charTransfer);
    template.offset = customizer->charTransfer.offset;
    template.sprite = NULL;
    customizer->textObj = TextOBJ_Create(&template, customizer->fontLayout);
    TextOBJ_SetSpritesDrawFlag(customizer->textObj, TRUE);
    TextOBJ_SetPaletteNum(customizer->textObj, 4);
}

static void SafariAreaCustomizer_DeleteCancelTextObj(SafariAreaCustomizer *customizer) {
    TextOBJ_Destroy(customizer->textObj);
    ObjCharTransfer_FreeCharSpace(&customizer->charTransfer);
    customizer->textObj = NULL;
    TextOBJLayout_Delete(customizer->fontLayout);
}

static void SafariAreaCustomizer_DrawPageScreenPart(SafariAreaCustomizer *customizer, int value) {
    NNSG2dScreenData *screen;

    FillBgTilemapRect(customizer->bgConfig, GF_BG_LYR_MAIN_0, 0, 0, 19, 24, 5, TILEMAP_FILL_OVWT_PAL);
    screen = customizer->pageScreen;
    CopyToBgTilemapRect(customizer->bgConfig, GF_BG_LYR_MAIN_0, 0, (u8)(21u - (u32)value * 2), 24, (u8)((u32)value * 2 + 3), screen->rawData, 0, (u8)((u32)value * 3), (u8)(screen->screenWidth >> 3), (u8)(screen->screenHeight >> 3));
    ScheduleBgTilemapBufferTransfer(customizer->bgConfig, GF_BG_LYR_MAIN_0);
}

static void SafariAreaCustomizer_PrintWindowText(SafariAreaCustomizer *customizer, u8 windowId, u8 msg, BOOL unused) {
    String **strings;
    u32 x;
    u8 width;
    u8 height;
    u32 color;
    int y;

    width = GetWindowWidth(&customizer->windows[windowId]) * 8;
    height = GetWindowHeight(&customizer->windows[windowId]) * 8;
    color = MAKE_TEXT_COLOR(3, 1, 2);
    if (windowId != 2) {
        strings = customizer->menuStrings;
        x = 0;
        y = 4;
        if (windowId <= 1) {
            color = MAKE_TEXT_COLOR(13, 12, 14);
            FillWindowPixelBuffer(&customizer->windows[windowId], 14);
            FillWindowPixelRect(&customizer->windows[windowId], 6, 0, 0, width, 2);
            FillWindowPixelRect(&customizer->windows[windowId], 6, 0, height - 2, width, 2);
        } else {
            FillWindowPixelBuffer(&customizer->windows[windowId], 2);
        }
    } else {
        strings = customizer->labelStrings;
        x = (u8)((width - FontID_String_GetWidth(0, strings[msg], 0)) / 2);
        y = 4;
    }
    AddTextPrinterParameterizedWithColor(&customizer->windows[windowId], 0, strings[msg], x, y, TEXT_SPEED_INSTANT, color, NULL);
}

static void SafariAreaCustomizer_PrintPageNumber(SafariAreaCustomizer *customizer) {
    u32 x;

    FillWindowPixelBuffer(&customizer->windows[3], 2);
    BufferIntegerAsString(customizer->msgFormat, 0, customizer->page + 1, 1, PRINTING_MODE_RIGHT_ALIGN, TRUE);
    StringExpandPlaceholders(customizer->msgFormat, customizer->string, customizer->pageTemplate);
    x = (GetWindowWidth(&customizer->windows[3]) * 8 - FontID_String_GetWidth(0, customizer->string, 0)) / 2;
    AddTextPrinterParameterizedWithColor(&customizer->windows[3], 0, customizer->string, x, 0, TEXT_SPEED_INSTANT, MAKE_TEXT_COLOR(3, 1, 2), NULL);
    ScheduleBgTilemapBufferTransfer(customizer->bgConfig, GF_BG_LYR_MAIN_3);
    Sprite_SetAnimActiveFlag(customizer->sprites[4], FALSE);
    if (customizer->page == 0) {
        Sprite_SetAnimCtrlSeq(customizer->sprites[4], 6);
    } else {
        Sprite_SetAnimCtrlSeq(customizer->sprites[4], 4);
    }
    Sprite_SetAnimActiveFlag(customizer->sprites[5], FALSE);
    if (customizer->page == 1) {
        Sprite_SetAnimCtrlSeq(customizer->sprites[5], 7);
    } else {
        Sprite_SetAnimCtrlSeq(customizer->sprites[5], 5);
    }
}

static void SafariAreaCustomizer_SetCursorAnim(SafariAreaCustomizer *customizer, u32 index, u32 anim, BOOL draw) {
    if (!draw) {
        Sprite_SetDrawFlag(customizer->sprites[index], FALSE);
    } else {
        Sprite_SetDrawFlag(customizer->sprites[index], TRUE);
        Sprite_SetAnimCtrlSeq(customizer->sprites[index], anim);
    }
}

// Moves cursor sprite `index` to grid position `pos` (6 = the CANCEL button).
static void SafariAreaCustomizer_MoveCursor(SafariAreaCustomizer *customizer, u32 index, u32 pos) {
    s16 x;
    s16 y;
    int anim = 0;

    if (pos >= 6) {
        x = 224;
        y = 180;
        anim = 1;
    } else if (index != 1) {
        x = ((s32)pos % 3) * 72 + 56;
        y = ((s32)pos / 3) * 72 + 56;
        if (index == 2) {
            y += 192;
        } else if (index == 3) {
            anim = 2;
        } else if (index == 0 && customizer->mode == SAC_MODE_MOVE) {
            anim = 3;
        }
    } else {
        x = ((s32)pos % 3) * 80 + 48;
        y = ((s32)pos / 3) * 72 + 56;
    }
    Sprite_SetPositionXY(customizer->sprites[index], x, y);
    SafariAreaCustomizer_SetCursorAnim(customizer, index, anim, TRUE);
}

static void SafariAreaCustomizer_DrawSelectFrame(SafariAreaCustomizer *customizer, u8 layer, int pos, BOOL show) {
    struct {
        u8 x;
        u8 y;
    } frame;

    if (!show) {
        BgClearTilemapBufferAndCommit(customizer->bgConfig, layer);
        ScheduleBgTilemapBufferTransfer(customizer->bgConfig, layer);
        return;
    }
    {
        NNSG2dScreenData *screen = customizer->selectFrameScreen;
        CopyToBgTilemapRect(customizer->bgConfig, layer, 0, (u8)(24 * customizer->mode), 32, 24, screen->rawData, 0, 0, screen->screenWidth >> 3, screen->screenHeight >> 3);
    }
    if (customizer->mode == SAC_MODE_SLOTS) {
        frame.x = (u8)((u32)(pos % 3) * 9 + 3);
        frame.y = (u8)((u32)(pos / 3) * 9 + 3);
        if (layer == GF_BG_LYR_SUB_0) {
            NNSG2dScreenData *screen = customizer->selectFrameScreen;
            CopyToBgTilemapRect(customizer->bgConfig, layer, 0, 21, 32, 3, screen->rawData, 0, 24, screen->screenWidth >> 3, screen->screenHeight >> 3);
        }
    } else {
        frame.x = (u8)((u32)(pos % 3) * 10 + 2);
        frame.y = (u8)((u32)(pos / 3) * 9 + 27);
    }
    FillBgTilemapRect(customizer->bgConfig, layer, 0, frame.x, frame.y, 8, 8, TILEMAP_FILL_OVWT_PAL);
    ScheduleBgTilemapBufferTransfer(customizer->bgConfig, layer);
}

static void SafariAreaCustomizer_InitSlotTilemaps(SafariAreaCustomizer *customizer) {
    int slot;
    int row;
    int column;
    int tile = 1;
    int x;
    u8 y;

    slot = 0;
    while (slot < NUM_AREA_SLOTS) {
        row = 0;
        x = (u8)((slot / 3) * 9 + 3);
        y = (slot % 3) * 9 + 3;
        while (row < 8) {
            u8 xPos = x;
            column = 0;
            while (column < 8) {
                FillBgTilemapRect(customizer->bgConfig, GF_BG_LYR_SUB_1, tile++, (u8)(y + column), xPos, 1, 1, TILEMAP_FILL_OVWT_PAL);
                column++;
            }
            x++;
            row++;
        }
        slot++;
    }
    ScheduleBgTilemapBufferTransfer(customizer->bgConfig, GF_BG_LYR_SUB_1);
}

static void SafariAreaCustomizer_LoadAreaPictureFromNarc(SafariAreaCustomizer *customizer, NARC *narc, u32 slot, int area) {
    NNSG2dCharacterData *charData;
    void *charFile = GfGfxLoader_GetCharDataFromOpenNarc(narc, area + 18, FALSE, &charData, customizer->heapId);

    BG_LoadCharTilesData(customizer->bgConfig, GF_BG_LYR_SUB_1, charData->pRawData, 0x1000, slot * 0x40 + 1);
    Heap_Free(charFile);
}

// Loads the picture of `area` into slot `slot`, or of every slot when `slot` >= 6.
static void SafariAreaCustomizer_LoadAreaPicture(SafariAreaCustomizer *customizer, u32 slot, int area) {
    NARC *narc = NARC_New(NARC_a_1_6_6, customizer->heapId);

    if (slot >= NUM_AREA_SLOTS) {
        int i;
        for (i = 0; i < NUM_AREA_SLOTS; i++) {
            SafariAreaCustomizer_LoadAreaPictureFromNarc(customizer, narc, (u8)i, customizer->areaSet.areas[i].area_no);
        }
    } else {
        SafariAreaCustomizer_LoadAreaPictureFromNarc(customizer, narc, slot, area);
    }
    NARC_Delete(narc);
}

static void SafariAreaCustomizer_InitScreen(SafariAreaCustomizer *customizer) {
    SafariAreaCustomizer_PrintObjectTypeLabels(customizer);
    SafariAreaCustomizer_PrintSlotInfo(customizer, customizer->selectedSlot);
    SafariAreaCustomizer_DrawPageScreenPart(customizer, 0);
    SafariAreaCustomizer_PrintWindowText(customizer, 0, 0, TRUE);
    if (customizer->touchMode == MENU_INPUT_STATE_BUTTONS) {
        SafariAreaCustomizer_ShowCursors(customizer);
    } else {
        thunk_SafariAreaCustomizer_ShowCursors(customizer);
    }
    customizer->mainBgY = 0;
    customizer->subBgY = -192;
    customizer->scrollPending = TRUE;
}

static void SafariAreaCustomizer_PrintObjectTypeLabels(SafariAreaCustomizer *customizer) {
    int i;

    if (customizer->objectsUnlocked) {
        for (i = 0; i < 5; i++) {
            Window *window = &customizer->windows[5 + i * 2];
            ReadMsgDataIntoString(customizer->msgData, i + msg_0429_00010, customizer->string);
            AddTextPrinterParameterizedWithColor(window, 0, customizer->string, 0, 0, TEXT_SPEED_INSTANT, gSafariAreaCustomizerLabelColors[i % 2], NULL);
            CopyWindowToVram(window);
        }
    }
}

// Prints the name of the area in `slot` and, once objects are unlocked, its object counts per type.
static void SafariAreaCustomizer_PrintSlotInfo(SafariAreaCustomizer *customizer, u32 slot) {
    int i;
    SAFARIZONE_AREA *area;
    u8 x;
    u8 counts[5];
    SafariObjectConfig config;

    if (slot >= NUM_AREA_SLOTS) {
        if (customizer->objectsUnlocked) {
            for (i = 0; i < 5; i++) {
                FillWindowPixelBuffer(&customizer->windows[i * 2 + 6], gSafariAreaCustomizerLabelColors[i % 2]);
                ScheduleWindowCopyToVram(&customizer->windows[i * 2 + 6]);
            }
        }
        FillWindowPixelBuffer(&customizer->windows[4], 12);
        ScheduleWindowCopyToVram(&customizer->windows[4]);
        return;
    }
    area = &customizer->areaSet.areas[slot];
    {
        Window *window = &customizer->windows[4];
        ReadMsgDataIntoString(customizer->msgData, area->area_no + msg_0429_00016, customizer->string);
        x = (GetWindowWidth(window) * 8 - FontID_String_GetWidth(0, customizer->string, 0)) / 2;
        FillWindowPixelBuffer(window, 12);
        AddTextPrinterParameterizedWithColor(window, 0, customizer->string, x, 4, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(8, 11, 12), NULL);
        ScheduleWindowCopyToVram(window);
    }
    if (customizer->objectsUnlocked) {
        MI_CpuFill8(counts, 0, 5);
        for (i = 0; i < area->active_object_count; i++) {
            GetSafariObjectConfig(&config, area->objects[i].id, 2);
            if (config.objectType == 0) {
                counts[4]++;
            } else {
                counts[config.objectType - 1]++;
            }
        }
        for (i = 0; i < 5; i++) {
            Window *window = &customizer->windows[i * 2 + 6];
            BufferIntegerAsString(customizer->msgFormat, 0, counts[i], 2, PRINTING_MODE_RIGHT_ALIGN, TRUE);
            StringExpandPlaceholders(customizer->msgFormat, customizer->string, customizer->countTemplate);
            FillWindowPixelBuffer(window, gSafariAreaCustomizerLabelColors[i % 2]);
            AddTextPrinterParameterizedWithColor(window, 0, customizer->string, 0, 0, TEXT_SPEED_NOTRANSFER, gSafariAreaCustomizerLabelColors[i % 2], NULL);
            ScheduleWindowCopyToVram(window);
        }
    }
}

static void SafariAreaCustomizer_PrintAreaNames(SafariAreaCustomizer *customizer) {
    u8 x;
    SAFARIZONE_AREA *area;
    Window *window = &customizer->windows[4];

    FillWindowPixelBuffer(window, 12);
    area = &customizer->areaSet.areas[customizer->selectedSlot];
    ReadMsgDataIntoString(customizer->msgData, area->area_no + msg_0429_00016, customizer->string);
    x = (96 - FontID_String_GetWidth(0, customizer->string, 0)) / 2;
    AddTextPrinterParameterizedWithColor(window, 0, customizer->string, x, 4, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(8, 11, 12), NULL);
    if (customizer->cursorPos < NUM_AREA_SLOTS) {
        ReadMsgDataIntoString(customizer->msgData, customizer->cursorPos + msg_0429_00016 + customizer->page * NUM_AREA_SLOTS, customizer->string);
        x = (96 - FontID_String_GetWidth(0, customizer->string, 0)) / 2;
        AddTextPrinterParameterizedWithColor(window, 0, customizer->string, x + 144, 4, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(8, 11, 12), NULL);
    }
    ScheduleWindowCopyToVram(window);
}

static void SafariAreaCustomizer_StartPageScroll(SafariAreaCustomizer *customizer) {
    int i;
    int bank;
    u32 ctrl;

    if (customizer->mode == SAC_MODE_SLOTS) {
        FillBgTilemapRect(customizer->bgConfig, GF_BG_LYR_MAIN_1, 0, 0, 21, 32, 3, TILEMAP_FILL_OVWT_PAL);
        ScheduleBgTilemapBufferTransfer(customizer->bgConfig, GF_BG_LYR_MAIN_1);
        customizer->subBgY = -192;
        customizer->scrollPending = TRUE;
        customizer->cursorPos = 0;
        SafariAreaCustomizer_MoveCursor(customizer, 1, customizer->cursorPos);
        SafariAreaCustomizer_MoveCursor(customizer, 2, customizer->selectedSlot);
        SafariAreaCustomizer_PrintPageNumber(customizer);
    } else {
        SafariAreaCustomizer_DrawSelectFrame(customizer, GF_BG_LYR_MAIN_1, 0xFF, FALSE);
        SafariAreaCustomizer_SetCursorAnim(customizer, 1, 0, FALSE);
        i = 0;
        bank = customizer->spriteBank * NUM_AREA_SLOTS;
        do {
            ManagedSprite_SetPositionXY(customizer->areaSprites[bank + i], 56 + 72 * (i % 3), -136 + 72 * (i / 3));
            ManagedSprite_SetPriority(customizer->areaSprites[bank + i], 2);
            SafariAreaCustomizerSpriteRes_SetAreaPicture(customizer->spriteRes, i + bank, customizer->areaSet.areas[i].area_no);
            i++;
        } while (i < NUM_AREA_SLOTS);
    }
    customizer->animFrame = 0;
    FillWindowPixelBuffer(&customizer->windows[4], 12);
    ScheduleWindowCopyToVram(&customizer->windows[4]);
    reg_G2S_DB_WIN0H = 0xF0;
    reg_G2S_DB_WIN0V = 0x10;
    ctrl = (reg_G2S_DB_WININ & ~0x3F) | 0xF;
    ctrl |= 0x20;
    reg_G2S_DB_WININ = ctrl;
    ctrl = (reg_G2S_DB_WINOUT & ~0x3F) | 0x1F;
    ctrl |= 0x20;
    reg_G2S_DB_WINOUT = ctrl;
    GXS_SetVisibleWnd(GX_WNDMASK_W0);
}

// Scrolls the area pages between the set view and the page view, 8 frames.
static BOOL SafariAreaCustomizer_RunPageScroll(SafariAreaCustomizer *customizer) {
    int i;
    int delta;

    if (customizer->mode == SAC_MODE_SLOTS) {
        customizer->mainBgY += 24;
        customizer->subBgY += 24;
        delta = -24;
    } else {
        customizer->mainBgY -= 24;
        customizer->subBgY -= 24;
        delta = 24;
    }
    customizer->scrollPending = TRUE;
    for (i = 0; i < NUM_AREA_SLOTS; i++) {
        ManagedSprite_OffsetPositionXY(customizer->areaSprites[customizer->spriteBank * NUM_AREA_SLOTS + i], 0, delta);
    }
    Sprite_OffsetPositionXY(customizer->sprites[0], 0, delta);
    Sprite_OffsetPositionXY(customizer->sprites[2], 0, delta);
    customizer->animFrame++;
    return customizer->animFrame >= 8;
}

static void SafariAreaCustomizer_StartPageChange(SafariAreaCustomizer *customizer) {
    int bank;
    int dx;
    int i;

    if (customizer->pageDir == 0) {
        customizer->page++;
        dx = 256;
    } else {
        customizer->page--;
        dx = -256;
    }
    bank = customizer->spriteBank * NUM_AREA_SLOTS;
    for (i = 0; i < NUM_AREA_SLOTS; i++) {
        ManagedSprite_SetPositionXY(customizer->areaSprites[bank + i], 48 + 80 * (i % 3) + dx, 56 + 72 * (i / 3));
        SafariAreaCustomizerSpriteRes_SetAreaPicture(customizer->spriteRes, (u8)(i + bank), (u8)(customizer->page * NUM_AREA_SLOTS + i));
        ManagedSprite_SetPriority(customizer->areaSprites[bank + i], 3);
    }
    customizer->animFrame = 0;
}

// Slides the area page by 32 pixels per frame, 8 frames.
static BOOL SafariAreaCustomizer_RunPageChange(SafariAreaCustomizer *customizer) {
    int i;
    int dx = customizer->pageDir == 0 ? -32 : 32;

    for (i = 0; i < 12; i++) {
        ManagedSprite_OffsetPositionXY(customizer->areaSprites[i], dx, 0);
    }
    customizer->animFrame++;
    if (customizer->animFrame >= 8) {
        customizer->animFrame = 0;
        SafariAreaCustomizer_MoveCursor(customizer, 1, customizer->cursorPos);
        SafariAreaCustomizer_PrintAreaNames(customizer);
        SafariAreaCustomizer_PrintPageNumber(customizer);
        return TRUE;
    }
    return FALSE;
}

// Moves the two areas being swapped out of (dir 0) or back into (dir 1) their slots, 8 frames.
static BOOL SafariAreaCustomizer_RunSwapAnim(SafariAreaCustomizer *customizer, int dir) {
    u8 sprites[2];
    u8 rows[2];
    int bank = NUM_AREA_SLOTS * customizer->spriteBank;
    int i;

    sprites[0] = customizer->selectedSlot + bank;
    sprites[1] = customizer->cursorPos + bank;
    rows[0] = customizer->selectedSlot / 3;
    rows[1] = customizer->cursorPos / 3;
    for (i = 0; i < 2; i++) {
        ManagedSprite_OffsetPositionXY(customizer->areaSprites[sprites[i]], 0, 12 * gSafariAreaCustomizerConsts.swapDir[dir][rows[i]]);
    }
    customizer->animFrame++;
    if (customizer->animFrame >= 8) {
        customizer->animFrame = 0;
        return TRUE;
    }
    return FALSE;
}

static void SafariAreaCustomizer_InitSpriteSystem(SafariAreaCustomizer *customizer) {
    GF_CreateVramTransferManager(32, customizer->heapId);
    customizer->spriteSystem = SpriteSystem_Alloc(customizer->heapId);
    SpriteSystem_Init(customizer->spriteSystem, &gSafariAreaCustomizerOamManagerParam, &gSafariAreaCustomizerOamCharTransferParam, 4);
    thunk_ClearMainOAM(customizer->heapId);
    thunk_ClearSubOAM(customizer->heapId);
}

static void SafariAreaCustomizer_FreeSpriteSystem(SafariAreaCustomizer *customizer) {
    SpriteSystem_Free(customizer->spriteSystem);
    customizer->spriteSystem = NULL;
    GF_DestroyVramTransferManager();
    thunk_ClearMainOAM(customizer->heapId);
    thunk_ClearSubOAM(customizer->heapId);
}

static void SafariAreaCustomizer_CreateSpriteManager(SafariAreaCustomizer *customizer) {
    if (customizer->spriteSystem != NULL) {
        customizer->spriteManager = SpriteManager_New(customizer->spriteSystem);
        SpriteSystem_InitSprites(customizer->spriteSystem, customizer->spriteManager, 140);
        SpriteSystem_LoadResourceDataFromFileIdListEx(customizer->spriteSystem, customizer->spriteManager, &gSafariAreaCustomizerResdatIds, 2, 0);
        SafariAreaCustomizerSpriteRes_SetSpriteList(customizer->spriteRes, SpriteManager_GetSpriteList(customizer->spriteManager));
    }
}

static void SafariAreaCustomizer_DestroySpriteManager(SafariAreaCustomizer *customizer) {
    if (customizer->spriteManager != NULL) {
        SpriteSystem_DestroySpriteManager(customizer->spriteSystem, customizer->spriteManager);
        customizer->spriteManager = NULL;
    }
}

static void SafariAreaCustomizer_DrawSprites(SafariAreaCustomizer *customizer) {
    if (customizer->spriteRes != NULL) {
        SafariAreaCustomizerSpriteRes_DrawSprites(customizer->spriteRes);
    }
    if (customizer->spriteManager != NULL) {
        SpriteSystem_DrawSprites(customizer->spriteManager);
    }
}

static void SafariAreaCustomizer_TransferOam(SafariAreaCustomizer *customizer) {
    if (customizer->spriteSystem != NULL) {
        SpriteSystem_TransferOam();
    }
}

static SafariAreaCustomizerSpriteRes *SafariAreaCustomizerSpriteRes_New(enum HeapID heapId, u16 maxSprites, u16 vram, int transferMode) {
    SafariAreaCustomizerSpriteRes *res = Heap_Alloc(heapId, sizeof(SafariAreaCustomizerSpriteRes));

    MI_CpuFill8(res, 0, sizeof(SafariAreaCustomizerSpriteRes));
    res->heapId = heapId;
    res->unk_0A = 4;
    res->maxSprites = maxSprites;
    res->vram = vram;
    res->transferMode = transferMode;
    SafariAreaCustomizerSpriteRes_Init(res);
    return res;
}

static void SafariAreaCustomizerSpriteRes_Delete(SafariAreaCustomizerSpriteRes *res) {
    SafariAreaCustomizerSpriteRes_Free(res);
    MI_CpuFill8(res, 0, sizeof(SafariAreaCustomizerSpriteRes));
    Heap_Free(res);
}

static void SafariAreaCustomizerSpriteRes_SetAreaPicture(SafariAreaCustomizerSpriteRes *res, u8 index, u8 area) {
    SpriteResource *charRes = res->resObjList[GF_GFX_RES_TYPE_CHAR]->obj[index];

    ReplaceCharResObjFromNarc(res->resMan[GF_GFX_RES_TYPE_CHAR], charRes, NARC_a_1_6_6, area + 18, FALSE, res->heapId);
    SpriteTransfer_ReplaceCharData(charRes);
}

static void SafariAreaCustomizerSpriteRes_DrawSprites(SafariAreaCustomizerSpriteRes *res) {
    if (res->spriteList != NULL) {
        SpriteList_RenderAndAnimateSprites(res->spriteList);
    }
}

static void SafariAreaCustomizerSpriteRes_SetSpriteList(SafariAreaCustomizerSpriteRes *res, SpriteList *spriteList) {
    res->externalSpriteList = spriteList;
}

static ManagedSprite *SafariAreaCustomizerSpriteRes_CreateAreaSprite(SafariAreaCustomizerSpriteRes *res, u16 x, u16 y, u8 transfer, u8 priority, u8 index, u8 area, int vram) {
    ManagedSprite *sprite;
    SpriteTemplate template;

    sprite = Heap_Alloc(res->heapId, sizeof(ManagedSprite));
    MI_CpuFill8(sprite, 0, sizeof(ManagedSprite));
    sprite->spriteResourceHeaderList = Heap_Alloc(res->heapId, sizeof(SpriteResourceHeaderList));
    sprite->spriteResourceHeaderList->headers = Heap_Alloc(res->heapId, sizeof(SpriteResourcesHeader));
    sprite->spriteResourcesHeader = sprite->spriteResourceHeaderList->headers;
    CreateSpriteResourcesHeader(sprite->spriteResourcesHeader, 0xE000 + index, 0xE000, 0xE000, 0xE000, -1, -1, 0, transfer, res->resMan[0], res->resMan[1], res->resMan[2], res->resMan[3], NULL, NULL);
    if (res->externalSpriteList != NULL) {
        template.spriteList = res->externalSpriteList;
    } else {
        template.spriteList = res->spriteList;
    }
    template.header = sprite->spriteResourcesHeader;
    template.position.x = FX32_CONST(x);
    template.position.y = FX32_CONST(y);
    template.position.z = 0;
    if (vram == NNS_G2D_VRAM_TYPE_2DSUB) {
        template.position.y += FX32_CONST(192);
    }
    template.scale.x = FX32_ONE;
    template.scale.y = FX32_ONE;
    template.scale.z = FX32_ONE;
    template.rotation = 0;
    template.drawPriority = priority;
    template.whichScreen = (NNS_G2D_VRAM_TYPE)vram;
    template.heapID = res->heapId;
    SafariAreaCustomizerSpriteRes_SetAreaPicture(res, index, area);
    sprite->sprite = Sprite_CreateAffine(&template);
    GF_ASSERT(sprite->sprite != NULL);
    return sprite;
}

static void SafariAreaCustomizer_DeleteAreaSprite(ManagedSprite *sprite) {
    Sprite_DeleteAndFreeResources(sprite);
}

static void SafariAreaCustomizerSpriteRes_Init(SafariAreaCustomizerSpriteRes *res) {
    SafariAreaCustomizerResCounts counts = gSafariAreaCustomizerResCounts;
    u32 i;

    res->spriteList = G2dRenderer_Init(res->maxSprites, &res->renderer, res->heapId);
    for (i = 0; i < 4; i++) {
        u32 j;
        res->resMan[i] = Create2DGfxResObjMan(counts.counts[i], (GfGfxResType)i, res->heapId);
        res->resObjList[i] = Create2DGfxResObjList(counts.counts[i], res->heapId);
        for (j = 0; j < res->resObjList[i]->max; j++) {
            res->resObjList[i]->obj[j] = NULL;
        }
    }
    SafariAreaCustomizerSpriteRes_LoadResources(res);
}

static void SafariAreaCustomizerSpriteRes_Free(SafariAreaCustomizerSpriteRes *res) {
    u32 i;

    SpriteList_Delete(res->spriteList);
    SpriteTransfer_DeleteAllCharTransferTasks(res->resObjList[GF_GFX_RES_TYPE_CHAR]);
    SpriteTransfer_DeleteAllPlttTransferTasks(res->resObjList[GF_GFX_RES_TYPE_PLTT]);
    for (i = 0; i < 4; i++) {
        Delete2DGfxResObjList(res->resObjList[i]);
        Destroy2DGfxResObjMan(res->resMan[i]);
    }
}

static void SafariAreaCustomizerSpriteRes_LoadResources(SafariAreaCustomizerSpriteRes *res) {
    int i;
    GF_2DGfxResObjList *list = res->resObjList[GF_GFX_RES_TYPE_CHAR];

    for (i = 0; i < 12; i++) {
        list->obj[i] = AddCharResObjFromNarc(res->resMan[GF_GFX_RES_TYPE_CHAR], NARC_a_1_6_6, i + 18, FALSE, i + 0xE000, res->vram, res->heapId);
        GF_ASSERT(list->obj[i] != NULL);
        switch (res->transferMode) {
        case 0:
            SpriteTransfer_CreateCharTransferTask_AllocAtEnd(list->obj[i]);
            break;
        case 1:
            SpriteTransfer_CreateCharTransferTask_UpdateMappingTypeFromHW_AllocAtEnd(list->obj[i]);
            break;
        case 2:
        default:
            SpriteTransfer_CreateCharTransferTask_UpdateMappingTypeFromHW(list->obj[i]);
            break;
        }
    }
    for (i = 0; i < 2; i++) {
        GF_2DGfxResObjList *cellList = res->resObjList[GF_GFX_RES_TYPE_CELL + i];
        cellList->obj[0] = AddCellOrAnimResObjFromNarc(res->resMan[GF_GFX_RES_TYPE_CELL + i], NARC_a_1_6_6, i + 16, FALSE, 0xE000, (GfGfxResType)(GF_GFX_RES_TYPE_CELL + i), res->heapId);
        GF_ASSERT(cellList->obj[0] != NULL);
    }
    list = res->resObjList[GF_GFX_RES_TYPE_PLTT];
    list->obj[0] = AddPlttResObjFromNarc(res->resMan[GF_GFX_RES_TYPE_PLTT], NARC_a_1_6_6, 15, FALSE, 0xE000, res->vram, 4, res->heapId);
    GF_ASSERT(list->obj[0] != NULL);
    SpriteTransfer_CreatePlttTransferTask(list->obj[0]);
}
