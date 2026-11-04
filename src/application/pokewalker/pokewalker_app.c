// Pokewalker connection app: init, main and exit, graphics and message setup, and the start of the IR link.

#include "application/pokewalker/pokewalker_app.h"

#include "global.h"

#include "application/pokewalker/pokewalker_app_internal.h"
#include "msgdata/msg.naix"

#include "font.h"
#include "gf_gfx_loader.h"
#include "gf_gfx_planes.h"
#include "heap.h"
#include "main.h"
#include "pm_string.h"
#include "render_text.h"
#include "render_window.h"
#include "screen_fade.h"
#include "system.h"
#include "title_screen.h"
#include "touchpad.h"
#include "vram_transfer_manager.h"

static void PokewalkerApp_BuildOwnRecord(PokewalkerApp *app);
static void PokewalkerApp_FreeBgs(BgConfig *bgConfig);
static void PokewalkerApp_FreeMessages(PokewalkerApp *app);
static void PokewalkerApp_GetSaveData(PokewalkerApp *app);
static void PokewalkerApp_InitBgs(BgConfig *bgConfig);
static void PokewalkerApp_InitMessages(PokewalkerApp *app);
static void PokewalkerApp_LoadBgGraphics(PokewalkerApp *app);
static void PokewalkerApp_SetGxBanks(void);
static void PokewalkerApp_VBlankCB(void *arg);

BOOL PokewalkerApp_Init(OverlayManager *manager, int *state) {
    PokewalkerApp *app;
    Main_SetVBlankIntrCB(NULL, NULL);
    HBlankInterruptDisable();
    GfGfx_DisableEngineAPlanes();
    GfGfx_DisableEngineBPlanes();
    GX_SetVisiblePlane(0);
    GXS_SetVisiblePlane(0);
    reg_G2_BLDCNT = 0;
    reg_G2S_DB_BLDCNT = 0;
    Sys_SetSoftResetDisableFlag(0x10);
    Heap_Create(HEAP_ID_3, HEAP_ID_154, 0x70000);
    app = OverlayManager_CreateAndGetData(manager, sizeof(PokewalkerApp), HEAP_ID_154);
    memset(app, 0, sizeof(PokewalkerApp));
    app->saveData = ((MainOverlayArgs *)OverlayManager_GetArgs(manager))->saveData;
    PokewalkerApp_GetSaveData(app);
    SetKeyRepeatTimers(3, 8);
    GF_TouchpadResetSampling();
    GF_TouchpadStartSampling(4);
    PokewalkerApp_Setup(app);
    TextFlags_SetCanABSpeedUpPrint(TRUE);
    return TRUE;
}

void PokewalkerApp_Setup(PokewalkerApp *app) {
    app->bgConfig = BgConfig_Alloc(HEAP_ID_154);
    FontID_Alloc(4, HEAP_ID_154);
    PokewalkerApp_InitMessages(app);
    PokewalkerApp_SetGxBanks();
    PokewalkerApp_InitBgs(app->bgConfig);
    PokewalkerApp_LoadBgGraphics(app);
    PokewalkerApp_AddWindowBank(app, 0);
    PokewalkerApp_AddWindowBank(app, 1);
    PokewalkerApp_InitSprites(app);
    PokewalkerApp_ResetCursor(app);
    TextFlags_SetCanTouchSpeedUpPrint(TRUE);
    PokewalkerApp_LoadStrings(app);
    Main_SetVBlankIntrCB(PokewalkerApp_VBlankCB, app);
    GX_SetDispSelect(GX_DISP_SELECT_SUB_MAIN);
    ToggleBgLayer(GF_BG_LYR_SUB_0, GF_PLANE_TOGGLE_ON);
}

void PokewalkerApp_Teardown(PokewalkerApp *app) {
    PokewalkerApp_FreeSprites(app);
    PokewalkerApp_RemoveWindowBank(app, 1);
    PokewalkerApp_RemoveWindowBank(app, 0);
    PokewalkerApp_FreeBgs(app->bgConfig);
    GF_TouchpadStopSampling();
    GF_DestroyVramTransferManager();
    PokewalkerApp_FreeStrings(app);
    PokewalkerApp_FreeMessages(app);
    FontID_Release(4);
    Main_SetVBlankIntrCB(NULL, NULL);
    app->spriteManager = NULL;
}

BOOL PokewalkerApp_Main(OverlayManager *manager, int *state) {
    PokewalkerApp *app = OverlayManager_GetData(manager);
    switch (*state) {
    case POKEWALKER_APP_STATE_FADE_IN:
        if (IsPaletteFadeFinished() == TRUE) {
            *state = POKEWALKER_APP_STATE_ENTER;
        }
        break;
    case POKEWALKER_APP_STATE_ENTER:
        *state = gPokewalkerAppModes[app->mode].enter(app);
        break;
    case POKEWALKER_APP_STATE_UPDATE:
        *state = gPokewalkerAppModes[app->mode].update(app);
        BgSetPosTextAndCommit(app->bgConfig, GF_BG_LYR_SUB_1, BG_POS_OP_SET_X, app->subBgScrollX);
        break;
    case POKEWALKER_APP_STATE_EXIT:
        *state = gPokewalkerAppModes[app->mode].exit(app);
        if (app->nextMode == POKEWALKER_APP_MODE_EXIT) {
            BeginNormalPaletteFade(FADE_BOTH_SCREENS, FADE_TYPE_BRIGHTNESS_OUT, FADE_TYPE_BRIGHTNESS_OUT, RGB_WHITE, 6, 1, HEAP_ID_154);
            *state = POKEWALKER_APP_STATE_FADE_OUT;
        } else {
            app->mode = app->nextMode;
            PokewalkerApp_OnModeChangeDummy(app);
        }
        break;
    case POKEWALKER_APP_STATE_FADE_OUT:
        if (IsPaletteFadeFinished() == TRUE) {
            return TRUE;
        }
        break;
    }
    if (app->spriteManager) {
        SpriteSystem_DrawSprites(app->spriteManager);
    }
    return FALSE;
}

BOOL PokewalkerApp_Exit(OverlayManager *manager, int *state) {
    PokewalkerApp *app = OverlayManager_GetData(manager);
    TextFlags_SetCanABSpeedUpPrint(FALSE);
    TextFlags_SetCanTouchSpeedUpPrint(FALSE);
    PokewalkerApp_Teardown(app);
    OverlayManager_FreeData(manager);
    Main_SetVBlankIntrCB(NULL, NULL);
    Heap_Destroy(HEAP_ID_154);
    Sys_ClearSoftResetDisableFlag(0x10);
    RegisterMainOverlay(FS_OVERLAY_ID(intro_title), &gApplication_TitleScreen);
    return TRUE;
}

static void PokewalkerApp_GetSaveData(PokewalkerApp *app) {
    app->bag = Save_Bag_Get(app->saveData);
    app->profile = Save_PlayerData_GetProfile(app->saveData);
    app->options = Save_PlayerData_GetOptionsAddr(app->saveData);
    app->pokewalker = Save_Pokewalker_Get(app->saveData);
    app->pokedex = Save_Pokedex_Get(app->saveData);
}

static inline void SetIrqCheckFlag(u32 *dtcm) {
#if defined(SDK_PORT)
    (void)dtcm;
    OS_SetIrqCheckFlag(OS_IE_V_BLANK);
#else
    dtcm[0xFFE] |= OS_IE_V_BLANK;
#endif
}

static void PokewalkerApp_VBlankCB(void *arg) {
    PokewalkerApp *app = arg;
    DoScheduledBgGpuUpdates(app->bgConfig);
    GF_RunVramTransferTasks();
    SpriteSystem_TransferOam();
    OS_SetIrqCheckFlag(OS_IE_V_BLANK);
}

static void PokewalkerApp_SetGxBanks(void) {
    GraphicsBanks banks = gPokewalkerAppGraphicsBanks;
    GfGfx_SetBanks(&banks);
}

static void PokewalkerApp_InitBgs(BgConfig *bgConfig) {
    GraphicsModes modes = gPokewalkerAppGraphicsModes;
    SetBothScreensModesAndDisable(&modes);
    InitBgFromTemplate(bgConfig, GF_BG_LYR_MAIN_1, &gPokewalkerAppBgTemplate_Main1, GF_BG_TYPE_TEXT);
    InitBgFromTemplate(bgConfig, GF_BG_LYR_MAIN_2, &gPokewalkerAppBgTemplate_Main2, GF_BG_TYPE_TEXT);
    InitBgFromTemplate(bgConfig, GF_BG_LYR_MAIN_3, &gPokewalkerAppBgTemplate_Main3, GF_BG_TYPE_TEXT);
    BgClearTilemapBufferAndCommit(bgConfig, GF_BG_LYR_MAIN_1);
    BgClearTilemapBufferAndCommit(bgConfig, GF_BG_LYR_MAIN_2);
    BgClearTilemapBufferAndCommit(bgConfig, GF_BG_LYR_MAIN_3);
    InitBgFromTemplate(bgConfig, GF_BG_LYR_SUB_0, &gPokewalkerAppBgTemplate_Sub0, GF_BG_TYPE_TEXT);
    InitBgFromTemplate(bgConfig, GF_BG_LYR_SUB_1, &gPokewalkerAppBgTemplate_Sub1, GF_BG_TYPE_TEXT);
    InitBgFromTemplate(bgConfig, GF_BG_LYR_SUB_2, &gPokewalkerAppBgTemplate_Sub2, GF_BG_TYPE_TEXT);
    BgClearTilemapBufferAndCommit(bgConfig, GF_BG_LYR_SUB_0);
    BgClearTilemapBufferAndCommit(bgConfig, GF_BG_LYR_SUB_1);
    BgClearTilemapBufferAndCommit(bgConfig, GF_BG_LYR_SUB_2);
    BG_ClearCharDataRange(GF_BG_LYR_SUB_0, 0x20, 0, HEAP_ID_154);
    BG_ClearCharDataRange(GF_BG_LYR_SUB_1, 0x20, 0, HEAP_ID_154);
    BG_ClearCharDataRange(GF_BG_LYR_SUB_2, 0x20, 0, HEAP_ID_154);
}

static void PokewalkerApp_FreeBgs(BgConfig *bgConfig) {
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_BG0 | GX_PLANEMASK_BG1 | GX_PLANEMASK_BG2 | GX_PLANEMASK_BG3 | GX_PLANEMASK_OBJ, GF_PLANE_TOGGLE_OFF);
    GfGfx_EngineBTogglePlanes(GX_PLANEMASK_BG0 | GX_PLANEMASK_BG1 | GX_PLANEMASK_BG3 | GX_PLANEMASK_OBJ, GF_PLANE_TOGGLE_OFF);
    FreeBgTilemapBuffer(bgConfig, GF_BG_LYR_SUB_2);
    FreeBgTilemapBuffer(bgConfig, GF_BG_LYR_SUB_1);
    FreeBgTilemapBuffer(bgConfig, GF_BG_LYR_SUB_0);
    FreeBgTilemapBuffer(bgConfig, GF_BG_LYR_MAIN_3);
    FreeBgTilemapBuffer(bgConfig, GF_BG_LYR_MAIN_2);
    FreeBgTilemapBuffer(bgConfig, GF_BG_LYR_MAIN_1);
    Heap_FreeExplicit(HEAP_ID_154, bgConfig);
}

static void PokewalkerApp_LoadBgGraphics(PokewalkerApp *app) {
    NARC *narc = NARC_New(NARC_a_2_4_7, HEAP_ID_154);
    GfGfxLoader_LoadCharDataFromOpenNarc(narc, 4, app->bgConfig, GF_BG_LYR_MAIN_3, 0, 0, TRUE, HEAP_ID_154);
    GfGfxLoader_LoadScrnDataFromOpenNarc(narc, 8, app->bgConfig, GF_BG_LYR_MAIN_3, 0, 0, TRUE, HEAP_ID_154);
    GfGfxLoader_GXLoadPalFromOpenNarc(narc, 0, GF_PAL_LOCATION_MAIN_BG, GF_PAL_SLOT_0_OFFSET, 0, HEAP_ID_154);
    LoadFontPal1(GF_PAL_LOCATION_MAIN_BG, GF_PAL_SLOT_14_OFFSET, HEAP_ID_154);
    LoadUserFrameGfx2(app->bgConfig, GF_BG_LYR_MAIN_1, 994, 13, (u8)Options_GetFrame(app->options), HEAP_ID_154);
    GfGfxLoader_GXLoadPalFromOpenNarc(narc, 1, GF_PAL_LOCATION_SUB_BG, GF_PAL_SLOT_0_OFFSET, 0, HEAP_ID_154);
    LoadFontPal1(GF_PAL_LOCATION_SUB_BG, GF_PAL_SLOT_14_OFFSET, HEAP_ID_154);
    GfGfxLoader_LoadCharDataFromOpenNarc(narc, 5, app->bgConfig, GF_BG_LYR_SUB_2, 0, 0, TRUE, HEAP_ID_154);
    GfGfxLoader_LoadScrnDataFromOpenNarc(narc, 9, app->bgConfig, GF_BG_LYR_SUB_2, 0, 0, TRUE, HEAP_ID_154);
    LoadUserFrameGfx2(app->bgConfig, GF_BG_LYR_SUB_0, 994, 13, (u8)Options_GetFrame(app->options), HEAP_ID_154);
    NARC_Delete(narc);
}

static void PokewalkerApp_InitMessages(PokewalkerApp *app) {
    app->msgData = NewMsgDataFromNarc(MSGDATA_LOAD_DIRECT, NARC_msgdata_msg, NARC_msg_msg_0274_bin, HEAP_ID_154);
    app->msgDataItemNames = NewMsgDataFromNarc(MSGDATA_LOAD_LAZY, NARC_msgdata_msg, NARC_msg_msg_0222_bin, HEAP_ID_154);
    app->msgFormat = MessageFormat_New_Custom(16, 32, HEAP_ID_154);
    app->string = String_New(400, HEAP_ID_154);
}

static void PokewalkerApp_FreeMessages(PokewalkerApp *app) {
    String_Delete(app->string);
    DestroyMsgData(app->msgDataItemNames);
    DestroyMsgData(app->msgData);
    MessageFormat_Delete(app->msgFormat);
}

void PokewalkerApp_LoadTilemap(PokewalkerApp *app, GFBgLayer bgId, int member) {
    GfGfxLoader_LoadScrnData(NARC_a_2_4_7, member, app->bgConfig, (GFBgLayer)bgId, 0, 0, TRUE, HEAP_ID_154);
}

void PokewalkerApp_StartLink(PokewalkerApp *app) {
    String *name = PlayerProfile_GetPlayerName_NewString(Save_PlayerData_GetProfile(app->saveData), HEAP_ID_154);
    app->identity.unk_00 = 1;
    app->identity.unk_08 = GAME_VERSION;
    app->identity.isRegistered = FALSE;
    app->identity.unk_5A = 0;
    app->identity.trainerId = PlayerProfile_GetTrainerID(app->profile);
    CopyStringToU16Array(name, app->identity.trainerName, PLAYER_NAME_LENGTH + 1);
    app->identity.unk_04 = 1;
    app->identity.unk_0A = GAME_VERSION;
    MI_CpuCopyFast(Pokewalker_GetIrClock(app->pokewalker), &app->identity.clock, sizeof(PokewalkerIrClock));
    PokewalkerIr_Start(app->irThreadStack, sizeof(app->irThreadStack), 15);
    PokewalkerIr_SetIdentities(&app->identity, &app->peerIdentity);
    PokewalkerIr_SetCompressBuffer(app->irCompressBuffer);
    String_Delete(name);
}

void PokewalkerApp_BuildUploadData(PokewalkerApp *app) {
    PokewalkerApp_BuildUploadImage(app, app->uploadImage);
    PokewalkerApp_BuildStrollData(&app->strollData, app);
    PokewalkerApp_BuildOwnRecord(app);
}

void PokewalkerApp_BuildSendStrollData(PokewalkerApp *app) {
    PokewalkerApp_BuildStrollData(&app->strollData, app);
    PokewalkerApp_BuildOwnRecord(app);
}

static void PokewalkerApp_BuildOwnRecord(PokewalkerApp *app) {
    void *clock;
    PokewalkerApp_BuildTrainerHouseData(app->saveData, &app->ownTrainerHouseData);
    app->unk_F1D0 = 1;
    app->ownRecordVersion = GAME_VERSION;
    clock = Pokewalker_GetIrClock(app->pokewalker);
    MI_CpuCopyFast(clock, &app->ownRecordClock, sizeof(PokewalkerIrClock));
}
