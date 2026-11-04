#include "application/plaza_game_mime_jr_top.h"

#include <string.h>

#include "global.h"

#include "application/plaza_game.h"
#include "msgdata/msg.naix"
#include "msgdata/msg/msg_0235.h"

#include "bg_window.h"
#include "camera.h"
#include "comm_state.h"
#include "communication_command.h"
#include "communication_core.h"
#include "communication_player_state.h"
#include "filesystem.h"
#include "font.h"
#include "geometry_math.h"
#include "gf_3d_vramman.h"
#include "gf_gfx_loader.h"
#include "gf_gfx_planes.h"
#include "heap.h"
#include "link_synchronization.h"
#include "math_util.h"
#include "model_animation.h"
#include "msgdata.h"
#include "obj_char_transfer.h"
#include "obj_pltt_transfer.h"
#include "overlay_manager.h"
#include "palette.h"
#include "player_data.h"
#include "render_window.h"
#include "screen_fade_internal.h"
#include "screen_fade_legacy.h"
#include "sound_playback.h"
#include "sprite.h"
#include "sprite_resource_header.h"
#include "sprite_system.h"
#include "swap_3d_buffers.h"
#include "sys_task_api.h"
#include "system.h"
#include "text.h"
#include "touchpad.h"
#include "touchscreen.h"
#include "vram_transfer_manager.h"
#include "wifi_connection.h"
#include "wireless_status_icon.h"

void MTX_MultVec43(const VecFx32 *vec, const MtxFx43 *mtx, VecFx32 *dst);
fx64c FX_SinFx64c(fx32 x);
fx64c FX_CosFx64c(fx32 x);
f32 _f_lltof(s64 value);

#define MIME_JR_TOP_GAME_FRAMES 1200
#define MIME_JR_TOP_MAX_LEVEL   8

enum MimeJrTopState {
    MIME_JR_TOP_STATE_CREATE_ENTRY,
    MIME_JR_TOP_STATE_WAIT_ENTRY,
    MIME_JR_TOP_STATE_INIT_GAME,
    MIME_JR_TOP_STATE_FADE_IN,
    MIME_JR_TOP_STATE_WAIT_FADE_IN,
    MIME_JR_TOP_STATE_START_DEMO,
    MIME_JR_TOP_STATE_RUN_DEMO,
    MIME_JR_TOP_STATE_WAIT_START,
    MIME_JR_TOP_STATE_START_COUNTDOWN,
    MIME_JR_TOP_STATE_WAIT_COUNTDOWN,
    MIME_JR_TOP_STATE_PLAY,
    MIME_JR_TOP_STATE_SHOW_FINISH,
    MIME_JR_TOP_STATE_WAIT_FINISH,
    MIME_JR_TOP_STATE_SEND_SCORE,
    MIME_JR_TOP_STATE_WAIT_SCORES,
    MIME_JR_TOP_STATE_FADE_OUT,
    MIME_JR_TOP_STATE_FREE_GAME,
    MIME_JR_TOP_STATE_SHOW_RESULTS,
    MIME_JR_TOP_STATE_WAIT_RESULTS,
    MIME_JR_TOP_STATE_START_SYNC,
    MIME_JR_TOP_STATE_WAIT_SYNC,
};

enum MimeJrTopExitState {
    MIME_JR_TOP_EXIT_STATE_FREE,
    MIME_JR_TOP_EXIT_STATE_WAIT_SYNC,
};

enum MimeJrTopConnection {
    MIME_JR_TOP_CONNECTED,
    MIME_JR_TOP_CONNECTION_LOST_WAIT,
    MIME_JR_TOP_CONNECTION_LOST_DONE,
};

enum MimeJrTopPlayerState {
    MIME_JR_TOP_PLAYER_SPINNING,
    MIME_JR_TOP_PLAYER_FALLEN,
    MIME_JR_TOP_PLAYER_GETTING_UP,
    MIME_JR_TOP_PLAYER_STEADYING,
};

enum MimeJrTopPose {
    MIME_JR_TOP_POSE_BALANCING,
    MIME_JR_TOP_POSE_FALLING,
    MIME_JR_TOP_POSE_DOWN,
};

enum MimeJrTopFallState {
    MIME_JR_TOP_FALL_START_HOP,
    MIME_JR_TOP_FALL_HOP,
    MIME_JR_TOP_FALL_LAND,
    MIME_JR_TOP_FALL_LIE,
    MIME_JR_TOP_FALL_WAIT,
};

enum MimeJrTopFallStep {
    MIME_JR_TOP_FALL_STEP_FALL_OFF,
    MIME_JR_TOP_FALL_STEP_CLIMB_BACK,
};

enum MimeJrTopRivalFallState {
    MIME_JR_TOP_RIVAL_FALL_START,
    MIME_JR_TOP_RIVAL_FALL_MOVE,
    MIME_JR_TOP_RIVAL_FALL_WAIT,
};

enum MimeJrTopRivalTierState {
    MIME_JR_TOP_RIVAL_TIER_START_GROW,
    MIME_JR_TOP_RIVAL_TIER_GROW,
    MIME_JR_TOP_RIVAL_TIER_START_SHRINK,
    MIME_JR_TOP_RIVAL_TIER_SHRINK,
    MIME_JR_TOP_RIVAL_TIER_START_PULSE,
    MIME_JR_TOP_RIVAL_TIER_PULSE,
    MIME_JR_TOP_RIVAL_TIER_HIDE = 255,
};

enum MimeJrTopTierModelsState {
    MIME_JR_TOP_TIER_MODELS_LOAD_TOP,
    MIME_JR_TOP_TIER_MODELS_LOAD_TOP_ANIMS,
    MIME_JR_TOP_TIER_MODELS_LOAD_CHANGE,
    MIME_JR_TOP_TIER_MODELS_LOAD_CHANGE_ANIMS,
    MIME_JR_TOP_TIER_MODELS_WAIT_CHANGE,
    MIME_JR_TOP_TIER_MODELS_WATCH_TIER,
    MIME_JR_TOP_TIER_MODELS_FREE = 255,
};

enum MimeJrTopStagePhase {
    MIME_JR_TOP_STAGE_OFF,
    MIME_JR_TOP_STAGE_LIGHTS,
    MIME_JR_TOP_STAGE_POPUPS,
    MIME_JR_TOP_STAGE_AUDIENCE,
    MIME_JR_TOP_STAGE_MORE_POPUPS,
    MIME_JR_TOP_STAGE_STARS,
};

enum MimeJrTopSlideState {
    MIME_JR_TOP_SLIDE_START,
    MIME_JR_TOP_SLIDE_MOVE,
    MIME_JR_TOP_SLIDE_DONE,
};

enum MimeJrTopDancerState {
    MIME_JR_TOP_DANCER_START,
    MIME_JR_TOP_DANCER_MOVE,
    MIME_JR_TOP_DANCER_DONE,
};

enum MimeJrTopCommandId {
    MIME_JR_TOP_CMD_START = COMM_BASE_COMMAND_COUNT,
    MIME_JR_TOP_CMD_FINISH,
    MIME_JR_TOP_CMD_PLAYER_STATE,
    MIME_JR_TOP_CMD_FINAL_SCORE,
};

enum MimeJrTopFlashState {
    MIME_JR_TOP_FLASH_BRIGHTEN,
    MIME_JR_TOP_FLASH_SHOW_STAGE,
    MIME_JR_TOP_FLASH_HOLD,
    MIME_JR_TOP_FLASH_DARKEN,
    MIME_JR_TOP_FLASH_END,
};

enum MimeJrTopPopupsState {
    MIME_JR_TOP_POPUPS_RISE,
    MIME_JR_TOP_POPUPS_HOLD,
    MIME_JR_TOP_POPUPS_SINK,
    MIME_JR_TOP_POPUPS_END,
};

enum MimeJrTopAudienceState {
    MIME_JR_TOP_AUDIENCE_SHOW,
    MIME_JR_TOP_AUDIENCE_HOLD,
    MIME_JR_TOP_AUDIENCE_HIDE,
};

enum MimeJrTopStarsState {
    MIME_JR_TOP_STARS_START,
    MIME_JR_TOP_STARS_HOLD,
    MIME_JR_TOP_STARS_WAIT_END,
};

typedef void (*MimeJrTopSetPaletteOffsetFunc)(ManagedSprite *sprite, int offset);
typedef void (*MimeJrTopInitTemplateFunc)(ManagedSpriteTemplate *template, u16 x, u16 y, u32 vram, int pal, int resId);
typedef void (*MimeJrTopStartBurstFunc)(MimeJrTopGame *game, s16 x, s16 y);
typedef void (*MimeJrTopRosterInitFunc)(PlazaGameRoster *roster, u32 fromLobby, SaveData *saveData, u32 voiceChat, PlazaGameArgs *args);

static void MimeJrTop_ReceiveStart(int netId, int size, void *data, MimeJrTopApp *app);
static void MimeJrTop_ReceiveFinish(int netId, int size, void *data, MimeJrTopApp *app);
static void MimeJrTop_ReceivePlayerState(int netId, int size, void *data, MimeJrTopApp *app);
static void MimeJrTop_ReceiveFinalScore(int netId, int size, void *data, MimeJrTopApp *app);
static void MimeJrTop_ReceiveCommand4(int netId, int size, void *data, MimeJrTopApp *app);
static u32 MimeJrTop_GetFinishSize(void);
static u32 MimeJrTop_GetStartSize(void);
static u32 MimeJrTop_GetPlayerStateSize(void);
static u32 MimeJrTop_GetFinalScoreSize(void);
static u32 MimeJrTop_GetCommand4Size(void);
static const MimeJrTopCommand *MimeJrTop_GetCommandTable(void);
static int MimeJrTop_GetCommandCount(void);
static void MimeJrTop_RegisterCommands(MimeJrTopApp *app);
static void MimeJrTop_InitGame(MimeJrTopApp *app);
static void MimeJrTopGame_RubDemo(MimeJrTopGame *game, int x, int y);
static BOOL MimeJrTopGame_RunDemo(MimeJrTopGame *game);
static void MimeJrTop_FreeGame(MimeJrTopApp *app);
static void MimeJrTop_ResetRound(MimeJrTopApp *app);
static void MimeJrTop_InitGraphics(MimeJrTopApp *app);
static void MimeJrTop_FreeGraphics(MimeJrTopApp *app);
static void MimeJrTop_InitBgs(BgConfig *bgConfig);
static void MimeJrTop_InitSprites(MimeJrTopApp *app);
static BOOL MimeJrTop_StartFade(int type);
static void MimeJrTop_SetStateIf(MimeJrTopApp *app, BOOL condition, u32 next, int *state);
static void MimeJrTop_VBlank(void *data);
static BOOL MimeJrTop_IsParent(MimeJrTopApp *app);
static void MimeJrTop_SetStartReceived(MimeJrTopApp *app);
static void MimeJrTop_SetFinishReceived(MimeJrTopApp *app);
static int MimeJrTop_HandleConnectionLost(MimeJrTopApp *app);
static void MimeJrTopModel_Draw(MimeJrTopModel *model, MtxFx43 *rotation, VecFx32 *position);
static void MimeJrTopModel_Load(MimeJrTopModel *model, NARC *narc, int fileId);
static void MimeJrTopModel_Init(MimeJrTopModel *model, int large);
static void MimeJrTopModel_Free(MimeJrTopModel *model);
static void MimeJrTop_DrawScene(MimeJrTopApp *app);
static void MimeJrTopGame_SetMimeJrAnimation(MimeJrTopGame *game, int animation);
static void MimeJrTop_InitCamera(MimeJrTopApp *app);
static void MimeJrTop_InitSpriteTemplate(ManagedSpriteTemplate *template, s16 x, s16 y, u32 vram, int pal, int resId);
static void MimeJrTopGame_CreateStars(MimeJrTopGame *game);
static void MimeJrTopGame_CreatePopups(MimeJrTopGame *game);
static void MimeJrTopGame_FreePopups(MimeJrTopGame *game);
static void MimeJrTopGame_SetStarsVisible(MimeJrTopGame *game, BOOL visible, int unused);
static void MimeJrTopGame_UpdateStars(MimeJrTopGame *game);
static void MimeJrTopGame_FreeStars(MimeJrTopGame *game);
static void MimeJrTopGame_SetRivalAnim(MimeJrTopGame *game, int player, int anim);
static int MimeJrTopGame_GetRivalAnim(MimeJrTopGame *game, int player);
static void MimeJrTopGame_UpdateRivalSprites(MimeJrTopGame *game);
static void MimeJrTopGame_FreeRivalSprites(MimeJrTopGame *game);
static void MimeJrTopGame_ShowInstructions(MimeJrTopGame *game);
static void MimeJrTopGame_HideInstructions(MimeJrTopGame *game);
static void MimeJrTop_LoadPlayerPalettes(MimeJrTopApp *app, int index);
static int MimeJrTop_GetCenteredX(Window *window, String *string);
static void MimeJrTopGame_ShowRivalNames(MimeJrTopApp *app, MimeJrTopGame *game);
static void MimeJrTopGame_HideRivalNames(MimeJrTopGame *game);
static void MimeJrTop_LoadSpriteResources(MimeJrTopApp *app);
static void MimeJrTopGame_CreateRivalSprites(MimeJrTopApp *app, MimeJrTopGame *game);
static ManagedSprite *MimeJrTopGame_NewSpark(MimeJrTopGame *game, u16 x, u16 y);
static void MimeJrTopGame_CreateDancerSprites(MimeJrTopGame *game);
static void MimeJrTopGame_FreeDancerSprites(MimeJrTopGame *game);
static void MimeJrTopGame_CreateDemoHand(MimeJrTopGame *game);
static void MimeJrTopGame_FreeDemoHand(MimeJrTopGame *game);
static void MimeJrTop_LoadBgGraphics(MimeJrTopApp *app);
static MimeJrTopClock *MimeJrTopClock_New(u32 playerCount, MimeJrTopResources *resources);
static void MimeJrTopClock_Free(MimeJrTopClock *clock);
static BOOL MimeJrTopClock_Tick(MimeJrTopClock *clock);
static MimeJrTopGame *MimeJrTopGame_New(u32 playerCount, u32 localIndex, MimeJrTopResources *resources);
static void MimeJrTopGame_ResetBalance(MimeJrTopGame *game);
static void MimeJrTopGame_Free(MimeJrTopGame *game);
static BOOL MimeJrTopGame_HasFallen(MimeJrTopGame *game, int unused);
static void MimeJrTopGame_UpdateMimeJrAnimation(MimeJrTopGame *game);
static SysTask *MimeJrTopGame_StartRivalFall(MimeJrTopGame *game, int player);
static void MimeJrTopGame_StartRivalTier(MimeJrTopGame *game, int player);
static void MimeJrTopGame_StartTierModels(MimeJrTopGame *game, int player);
static void MimeJrTopGame_StartFall(MimeJrTopGame *game);
static void MimeJrTopFall_Task(SysTask *sysTask, MimeJrTopFallTask *task);
static void MimeJrTopRivalFall_Task(SysTask *sysTask, void *data);
static void MimeJrTopRivalTier_Task(SysTask *sysTask, void *data);
static void MimeJrTopTierModels_Task(SysTask *sysTask, MimeJrTopTierModelsTask *task);
static void MimeJrTopGame_AddTierScore(MimeJrTopGame *game);
static void MimeJrTopGame_AddTierBonus(MimeJrTopGame *game);
static BOOL MimeJrTopGame_UpdateTier(MimeJrTopGame *game);
static BOOL MimeJrTop_GetTier(MimeJrTopGame *game, int spinFrames, u32 *tier);
static void MimeJrTopGame_UpdateRivals(MimeJrTopGame *game);
static void MimeJrTopGame_UpdateLocalPlayer(MimeJrTopGame *game);
static void MimeJrTopGame_Update(MimeJrTopGame *game);
static void MimeJrTopGame_HandleTouch(MimeJrTopGame *game);
static void MimeJrTopGame_ResetMotion(MimeJrTopGame *game);
static void MimeJrTopGame_UpdateLevel(MimeJrTopGame *game);
static void MimeJrTopGame_UpdateWobble(MimeJrTopGame *game);
static BOOL MimeJrTopModel_Turn(MimeJrTopModel *model, s32 x, s32 y, s32 targetX, s32 targetY, f32 scale, BOOL saveVelocity);
static BOOL MimeJrTopModel_ContinueSpin(MimeJrTopModel *model, f32 unused);
static int MimeJrTop_GetPlayerIndex(MimeJrTopApp *app, u32 netId);
static void MimeJrTopTween_InitAccel(MimeJrTopTween *tween, fx32 start, fx32 end, fx32 velocity, int frames);
static BOOL MimeJrTopTween_UpdateAccel(MimeJrTopTween *tween);
static void MimeJrTopTween_Init(MimeJrTopTween *tween, fx32 start, fx32 end, int frames);
static BOOL MimeJrTopTween_Update(MimeJrTopTween *tween);
static void MimeJrTopBurst_Task(SysTask *sysTask, MimeJrTopBurst *burst);
static void MimeJrTopGame_StartBurst(MimeJrTopGame *game, u16 x, u16 y);
static void MimeJrTopStars_Task(SysTask *sysTask, MimeJrTopStarsTask *task);
static void MimeJrTopGame_StartStars(MimeJrTopGame *game, s32 *stop);
static void MimeJrTopStage_UpdateFlash(MimeJrTopStageEffect *effect);
static void MimeJrTopStage_UpdatePopups(MimeJrTopStageEffect *effect);
static void MimeJrTopStage_UpdateAudience(MimeJrTopStageEffect *effect);
static void MimeJrTopStage_UpdateStars(MimeJrTopStageEffect *effect);
static void MimeJrTopStage_Task(SysTask *sysTask, void *data);
static void MimeJrTopGame_StartStage(MimeJrTopGame *game);
static void MimeJrTopDance_SlideIn(MimeJrTopDance *dance);
static void MimeJrTopDance_SlideOut(MimeJrTopDance *dance);
static int MimeJrTop_GetDanceStepFrames(int speed);
static void MimeJrTopDance_Task(SysTask *sysTask, void *data);
static void MimeJrTopGame_StartDance(MimeJrTopGame *game);
static void MimeJrTopDancer_Reset(MimeJrTopDancer *dancer);
static BOOL MimeJrTopDancer_Slide(MimeJrTopDancer *dancer, int speed, s32 xdir, s32 ydir);
static BOOL MimeJrTopDancer_EnterLow(MimeJrTopDancer *dancer, int speed, s32 xdir, s32 ydir);
static BOOL MimeJrTopDancer_EnterHigh(MimeJrTopDancer *dancer, int speed, s32 xdir, s32 ydir);
static BOOL MimeJrTopDancer_Circle(MimeJrTopDancer *dancer, int speed, s32 xdir, s32 cycles);
static void MimeJrTop_QuatIdentity(MimeJrTopQuat *quat);
static void MimeJrTop_QuatCopy(MimeJrTopQuat *dst, const MimeJrTopQuat *src);
static void MimeJrTop_QuatMul(f32 *dst, const f32 *a, const f32 *b);
static f32 MimeJrTop_QuatLengthSq(const MimeJrTopQuat *quat);
static f32 MimeJrTop_QuatLength(const MimeJrTopQuat *quat);
static void MimeJrTop_QuatDivide(f32 *dst, const f32 *src, f32 divisor);
static void MimeJrTop_QuatToMatrix(MimeJrTopFloatMatrix *dst, const MimeJrTopQuat *quat);
static void MimeJrTop_FloatMatrixToFx44(const MimeJrTopFloatMatrix *src, MtxFx44 *dst);
static void MimeJrTop_FloatMatrixToFx43(const MimeJrTopFloatMatrix *src, MtxFx43 *dst);

// Command 22: the selection made on the entry screen and the parent's random seed.
static void MimeJrTop_ReceiveStart(int netId, int size, void *data, MimeJrTopApp *app) {
    const MimeJrTopStartMessage *start = data;
    app->start = *start;
    SetLCRNGSeed(app->start.seed);
    MimeJrTop_SetStartReceived(app);
}

// Command 23: the parent's clock ran out.
static void MimeJrTop_ReceiveFinish(int netId, int size, void *data, MimeJrTopApp *app) {
    MimeJrTop_SetFinishReceived(app);
}

// Command 24: a player's state (0 spinning, 1 fallen, 2 back on the top).
static void MimeJrTop_ReceivePlayerState(int netId, int size, void *data, MimeJrTopApp *app) {
    app->game->playerStates[netId] = *(u32 *)data;
}

// Command 25: a player's final score.
static void MimeJrTop_ReceiveFinalScore(int netId, int size, void *data, MimeJrTopApp *app) {
    app->scores[netId].score = *(u32 *)data;
}

static void MimeJrTop_ReceiveCommand4(int netId, int size, void *data, MimeJrTopApp *app) {
}

static u32 MimeJrTop_GetFinishSize(void) {
    return 0;
}

static u32 MimeJrTop_GetStartSize(void) {
    return 8;
}

static u32 MimeJrTop_GetPlayerStateSize(void) {
    return sizeof(u32);
}

static u32 MimeJrTop_GetFinalScoreSize(void) {
    return 8;
}

static u32 MimeJrTop_GetCommand4Size(void) {
    return 0x20;
}

static const MimeJrTopCommand sMimeJrTopCommands[] = {
    { MimeJrTop_ReceiveStart,       MimeJrTop_GetStartSize,       NULL },
    { MimeJrTop_ReceiveFinish,      MimeJrTop_GetFinishSize,      NULL },
    { MimeJrTop_ReceivePlayerState, MimeJrTop_GetPlayerStateSize, NULL },
    { MimeJrTop_ReceiveFinalScore,  MimeJrTop_GetFinalScoreSize,  NULL },
    { MimeJrTop_ReceiveCommand4,    MimeJrTop_GetCommand4Size,    NULL },
};

static const MimeJrTopCommand *MimeJrTop_GetCommandTable(void) {
    return sMimeJrTopCommands;
}

static int MimeJrTop_GetCommandCount(void) {
    return NELEMS(sMimeJrTopCommands);
}

static void MimeJrTop_RegisterCommands(MimeJrTopApp *app) {
    const MimeJrTopCommand *table = MimeJrTop_GetCommandTable();
    int count = MimeJrTop_GetCommandCount();
    Comm_RegisterCommandTable(table, count, app);
}

// Builds the game screen once the entry screen is done.
#pragma opt_propagation off
#pragma opt_strength_reduction off

static void MimeJrTop_InitGame(MimeJrTopApp *app) {
    NNSG2dPaletteData *pal;
    MimeJrTopTopModels topModels;
    MimeJrTopModelFiles audienceFiles;
    int i;
    void *raw;
    int playerCount;
    int topFile;
    MimeJrTopModel *model;
    int offset;
    int shift;
    int fixed;
    int step;
    int(*files)[2];

    app->savedSeed = GetLCRNGSeed();
    MimeJrTop_InitGraphics(app);
    Main_SetVBlankIntrCB(MimeJrTop_VBlank, app);
    HBlankInterruptDisable();
    GF_CreateVramTransferManager(32, HEAP_ID_113);
    G2dRenderer_SetObjCharTransferReservedRegion(NNS_G2D_VRAM_TYPE_2DMAIN, GX_OBJVRAMMODE_CHAR_1D_128K);
    G2dRenderer_SetPlttTransferReservedRegion(NNS_G2D_VRAM_TYPE_2DMAIN);
    WirelessStatusIcon_Create();
    raw = WirelessStatusIcon_LoadPaletteFile(HEAP_ID_113);
    NNS_G2dGetUnpackedPaletteData(raw, &pal);
    PaletteData_LoadPalette(app->resources.palette, pal->pRawData, PLTTBUF_MAIN_OBJ, 0xE0, 32);
    Heap_Free(raw);
    playerCount = CommPlayerInfo_CountAccepted();
    app->netId = Comm_GetLocalNetId();
    MimeJrTop_RegisterCommands(app);
    if (MimeJrTop_IsParent(app) == TRUE) {
        app->clock = MimeJrTopClock_New(playerCount, &app->resources);
    }
    app->game = MimeJrTopGame_New(playerCount, app->netId, &app->resources);
    app->game->selection = app->start.selection;
    app->game->seed = app->savedSeed;
    GF_TouchpadResetSampling();
    GF_TouchpadStartSampling(4);
    MimeJrTop_LoadBgGraphics(app);
    MimeJrTop_LoadSpriteResources(app);
    MimeJrTopGame_CreateRivalSprites(app, app->game);
    MimeJrTopGame_CreatePopups(app->game);
    MimeJrTopGame_CreateStars(app->game);
    MimeJrTopGame_CreateDancerSprites(app->game);
    MimeJrTopGame_CreateDemoHand(app->game);
    MimeJrTop_LoadPlayerPalettes(app, MimeJrTop_GetPlayerIndex(app, app->netId));
    audienceFiles = gMimeJrTopAudienceModelFiles;
    step = 5;
    i = 0;
    files = audienceFiles.files;
    offset = i;
    shift = offset;
    fixed = offset;
    // The audience: eight models in two rows; offset walks the model array in bytes.
    do {
        MimeJrTopModel_Load((MimeJrTopModel *)((u8 *)app->game->audience + offset), app->resources.narc, (*files)[0]);
        MimeJrTopModel_Init((MimeJrTopModel *)((u8 *)app->game->audience + offset), app->start.selection);
        ModelAnimation_LoadFromNarc((ModelAnimation *)((u8 *)&app->game->audience[0].animations[0] + offset), (ModelResource *)((u8 *)&app->game->audience[0].resource + offset), app->resources.narc, (*files)[1], HEAP_ID_113, &app->resources.allocator);
        ModelInstance_AddAnimation((ModelInstance *)((u8 *)&app->game->audience[0].instance + offset), (ModelAnimation *)((u8 *)&app->game->audience[0].animations[0] + offset));
        ModelAnimation_SetFrame((ModelAnimation *)((u8 *)&app->game->audience[0].animations[0] + offset), 0);
        ModelInstance_SetVisible((ModelInstance *)((u8 *)app->game->audience + offset), FALSE);
        files = app->start.selection ? (int(*)[2])files : files;
        if (i % 2) {
            ((MimeJrTopGame *)((u8 *)app->game + offset))->audience[0].position.x += (fx32)(shift > 0 ? fixed + 0.5f : fixed - 0.5f);
        } else {
            ((MimeJrTopGame *)((u8 *)app->game + offset))->audience[0].position.x -= (fx32)(shift > 0 ? fixed + 0.5f : fixed - 0.5f);
        }
        if (i < 8) {
            ((MimeJrTopGame *)((u8 *)app->game + offset))->audience[0].position.z += FX32_CONST((i + 1) * step);
        } else {
            ((MimeJrTopGame *)((u8 *)app->game + offset))->audience[0].position.z -= (fx32)(shift > 0 ? fixed + 0.5f : fixed - 0.5f);
        }
        files++;
        shift += 5;
        offset += sizeof(MimeJrTopModel);
        fixed += 5 * FX32_ONE;
        i++;
    } while (i < 8);
    MimeJrTopModel_Load(&app->game->stage, app->resources.narc, 0x47);
    MimeJrTopModel_Init(&app->game->stage, app->start.selection);
    ModelInstance_SetVisible(&app->game->stage.instance, FALSE);
    topModels = gMimeJrTopTopModels;
    topFile = topModels.files[MimeJrTop_GetPlayerIndex(app, app->netId)];
    MimeJrTopModel_Load(&app->game->top, app->resources.narc, topFile);
    MimeJrTopModel_Init(&app->game->top, app->start.selection);
    MimeJrTopModel_Load(&app->game->mimeJr[0], app->resources.narc, 0x33);
    MimeJrTopModel_Load(&app->game->mimeJr[1], app->resources.narc, 0x39);
    MimeJrTopModel_Load(&app->game->mimeJr[2], app->resources.narc, 0x37);
    MimeJrTopModel_Init(&app->game->mimeJr[0], app->start.selection);
    MimeJrTopModel_Init(&app->game->mimeJr[1], app->start.selection);
    MimeJrTopModel_Init(&app->game->mimeJr[2], app->start.selection);
    model = &app->game->mimeJr[0];
    ModelAnimation_LoadFromNarc(&model->animations[0], &model->resource, app->resources.narc, 0x35, HEAP_ID_113, &app->resources.allocator);
    ModelInstance_AddAnimation(&model->instance, &model->animations[0]);
    ModelAnimation_SetFrame(&model->animations[0], 0);
    ModelAnimation_LoadFromNarc(&model->animations[1], &model->resource, app->resources.narc, 0x34, HEAP_ID_113, &app->resources.allocator);
    ModelAnimation_SetFrame(&model->animations[1], 0);
    ModelAnimation_LoadFromNarc(&model->animations[2], &model->resource, app->resources.narc, 0x3A, HEAP_ID_113, &app->resources.allocator);
    ModelAnimation_SetFrame(&model->animations[2], 0);
    model = &app->game->mimeJr[1];
    ModelAnimation_LoadFromNarc(&model->animations[0], &model->resource, app->resources.narc, 0x38, HEAP_ID_113, &app->resources.allocator);
    ModelInstance_AddAnimation(&model->instance, &model->animations[0]);
    ModelAnimation_SetFrame(&model->animations[0], 0);
    model = &app->game->mimeJr[2];
    ModelAnimation_LoadFromNarc(&model->animations[0], &model->resource, app->resources.narc, 0x36, HEAP_ID_113, &app->resources.allocator);
    ModelInstance_AddAnimation(&model->instance, &model->animations[0]);
    ModelAnimation_SetFrame(&model->animations[0], 0);
    if (app->start.selection) {
        app->game->top.position.y = FX32_CONST(-8);
        app->game->mimeJr[0].position.y = FX32_CONST(-8);
        app->game->mimeJr[1].position.y = FX32_CONST(-8);
        app->game->mimeJr[2].position.y = FX32_CONST(-8);
    }
    app->sign = PlazaGameSign_New(SpriteManager_GetSpriteList(app->resources.spriteManager), HEAP_ID_113);
    PaletteData_LoadPaletteSlotFromHardware(app->resources.palette, PLTTBUF_MAIN_OBJ, 0, 0x200);
    if (app->args->mimeJrTopVoiceChat) {
        WifiConnection_StartVoiceChat(HEAP_ID_113);
    }
}

#pragma opt_propagation reset
#pragma opt_strength_reduction reset

BOOL MimeJrTop_Init(OverlayManager *man, int *state) {
    MimeJrTopApp *app;
    PlazaGameArgs *args;
    int i;
    int count;

    Heap_Create(HEAP_ID_3, HEAP_ID_113, 0x65 << 12);
    app = OverlayManager_CreateAndGetData(man, sizeof(MimeJrTopApp), HEAP_ID_113);
    memset(app, 0, sizeof(MimeJrTopApp));
    args = OverlayManager_GetArgs(man);
    app->args = args;

    // netIds lists the connected net ids first.
    count = 0;
    for (i = 0; i < PLAZA_GAME_MAX_PLAYERS; i++) {
        app->netIds[i] = 0xFF;
        if (CommPlayerInfo_GetProfile(i) != NULL) {
            app->netIds[count] = i;
            count++;
        }
    }

    args = app->args;
    ((MimeJrTopRosterInitFunc)PlazaGameRoster_Init)(&app->roster, *(u32 *)&args->voiceChat, args->saveData, args->mimeJrTopVoiceChat, args);
    return TRUE;
}

// The demo shown before the start: a hand rubbing the screen at (x, y); (0, 0) lets go.
static void MimeJrTopGame_RubDemo(MimeJrTopGame *game, int x, int y) {
    VecFx32 spinAxis;
    VecFx32 turnAxis;
    f32 lean;
    int tiltY;
    if (x == 0 && y == 0) {
        game->lastTouchX = 0;
        game->lastTouchY = 0;
        if (game->top.spinFrames) {
            MimeJrTopModel_ContinueSpin(&game->top, 1.0f);
            spinAxis = gMimeJrTopDemoSpinTiltAxis;
            MTX_MultVec43(&spinAxis, &game->mimeJr[0].rotationFx, &game->tilt);
            return;
        }
        game->top.spinX = 0;
        game->top.spinY = 0;
        MimeJrTopModel_Turn(&game->top, game->lastTouchX, game->lastTouchY, game->lastTouchX, game->lastTouchY, 1.88f, FALSE);
        MimeJrTopModel_Turn(&game->mimeJr[0], game->lastTouchX, game->lastTouchY, game->lastTouchX, game->lastTouchY, 0.22f, FALSE);
        return;
    }
    tiltY = game->tilt.y >> FX32_SHIFT;
    lean = (100 - tiltY) / 40;
    if (tiltY >= 95) {
        lean = 0.1f;
    }
    if (game->lastTouchX == 0) {
        game->lastTouchX = x;
        game->lastTouchY = y;
    }
    MimeJrTopModel_Turn(&game->top, x, y, game->lastTouchX, game->lastTouchY, 2.0f, TRUE);
    turnAxis = gMimeJrTopDemoTurnTiltAxis;
    MTX_MultVec43(&turnAxis, &game->mimeJr[0].rotationFx, &game->tilt);
    if (!IsSEPlaying(SEQ_SE_PL_KIRAKIRA)) {
        PlaySE(SEQ_SE_PL_KIRAKIRA);
    }
    game->lastTouchX = x;
    game->lastTouchY = y;
}

static BOOL MimeJrTopGame_RunDemo(MimeJrTopGame *game) {
    MimeJrTopDemoPath path = gMimeJrTopDemoPath;
    s32 x;
    s32 frame;
    s32 y;
    s32 index;
    frame = ManagedSprite_GetAnimationFrame(game->demoHand);
    index = frame + game->demoStep * 12;
    x = path.points[index].x;
    y = path.points[index].y;
    if (x == 0 && y == 0) {
        MimeJrTopGame_RubDemo(game, 0, 0);
    } else {
        MimeJrTopGame_RubDemo(game, x + 0x80, y + 0x80);
        if (x != 0) {
            ((MimeJrTopStartBurstFunc)MimeJrTopGame_StartBurst)(game, x + 0x58, y + 0x80);
        } else {
            ((MimeJrTopStartBurstFunc)MimeJrTopGame_StartBurst)(game, x + 0x80, y + 0x80);
        }
    }
    if (frame == 0 && game->demoFrame != frame) {
        game->demoStep++;
    }
    game->demoFrame = frame;
    if (game->demoStep == 2 && ManagedSprite_GetActiveAnim(game->demoHand) != 2) {
        ManagedSprite_SetPositionXY(game->demoHand, 0x80, 0x64);
        ManagedSprite_SetAnimNoRestart(game->demoHand, 2);
        MimeJrTop_QuatIdentity(&game->top.rotation);
        MimeJrTop_QuatIdentity(&game->top.lastTurn);
        MimeJrTopModel_Turn(&game->top, 0, 0, 5, 5, 0.8f, FALSE);
        MimeJrTopModel_Turn(&game->top, 0, 0, -5, -5, 0.8f, FALSE);
    }
    if (game->demoStep == 3 && frame == 11) {
        game->demoFrame = 0;
        game->demoStep = 0;
        ManagedSprite_SetDrawFlag(game->demoHand, FALSE);
        MimeJrTop_QuatIdentity(&game->top.rotation);
        MimeJrTop_QuatIdentity(&game->top.lastTurn);
        MimeJrTopModel_Turn(&game->top, 0, 0, 5, 5, 0.8f, FALSE);
        MimeJrTopModel_Turn(&game->top, 0, 0, -5, -5, 0.8f, FALSE);
        return TRUE;
    }
    ManagedSprite_TickTwoFrames(game->demoHand);
    return FALSE;
}

BOOL MimeJrTop_Main(OverlayManager *man, int *state) {
    MimeJrTopApp *app = OverlayManager_GetData(man);
    s32 done;
    s32 keepPlaying;
    u32 score;
    s32 i;
    s32 index;

    switch (MimeJrTop_HandleConnectionLost(app)) {
    case MIME_JR_TOP_CONNECTION_LOST_WAIT:
        return FALSE;
    case MIME_JR_TOP_CONNECTION_LOST_DONE:
        return TRUE;
    default:
    case MIME_JR_TOP_CONNECTED:
        switch (*state) {
        case MIME_JR_TOP_STATE_CREATE_ENTRY:
            app->entry = PlazaGameEntry_NewMimeJrTop(&app->roster, HEAP_ID_113);
            MimeJrTop_SetStateIf(app, TRUE, MIME_JR_TOP_STATE_WAIT_ENTRY, state);
            break;
        case MIME_JR_TOP_STATE_WAIT_ENTRY:
            MimeJrTop_SetStateIf(app, PlazaGameEntry_IsDone(app->entry), MIME_JR_TOP_STATE_INIT_GAME, state);
            break;
        case MIME_JR_TOP_STATE_INIT_GAME:
            app->start.selection = PlazaGameEntry_GetSelection(app->entry);
            PlazaGameEntry_Free(app->entry);
            app->entry = NULL;
            MimeJrTop_InitGame(app);
            MimeJrTop_SetStateIf(app, TRUE, MIME_JR_TOP_STATE_FADE_IN, state);
            break;
        case MIME_JR_TOP_STATE_FADE_IN:
            done = MimeJrTop_StartFade(0x1B);
            MimeJrTopGame_ShowRivalNames(app, app->game);
            MimeJrTop_SetStateIf(app, done, MIME_JR_TOP_STATE_WAIT_FADE_IN, state);
            break;
        case MIME_JR_TOP_STATE_WAIT_FADE_IN:
            MimeJrTop_SetStateIf(app, IsPaletteFadeFinished(), MIME_JR_TOP_STATE_START_DEMO, state);
            break;
        case MIME_JR_TOP_STATE_START_DEMO:
            MimeJrTopGame_ShowInstructions(app->game);
            MimeJrTopGame_StartStage(app->game);
            MimeJrTopGame_StartDance(app->game);
            MimeJrTop_SetStateIf(app, TRUE, MIME_JR_TOP_STATE_RUN_DEMO, state);
            break;
        case MIME_JR_TOP_STATE_RUN_DEMO:
            done = MimeJrTopGame_RunDemo(app->game);
            if (done && MimeJrTop_IsParent(app) == TRUE) {
                app->start.seed = app->savedSeed;
                Comm_SendCommand(MIME_JR_TOP_CMD_START, &app->start, sizeof(MimeJrTopStartMessage));
            }
            MimeJrTop_SetStateIf(app, done, MIME_JR_TOP_STATE_WAIT_START, state);
            break;
        case MIME_JR_TOP_STATE_WAIT_START:
            MimeJrTop_SetStateIf(app, app->startReceived, MIME_JR_TOP_STATE_START_COUNTDOWN, state);
            break;
        case MIME_JR_TOP_STATE_START_COUNTDOWN:
            PlazaGameSign_StartCountdown(app->sign);
            MimeJrTop_SetStateIf(app, TRUE, MIME_JR_TOP_STATE_WAIT_COUNTDOWN, state);
            break;
        case MIME_JR_TOP_STATE_WAIT_COUNTDOWN:
            done = PlazaGameSign_IsDone(app->sign);
            if (done) {
                MimeJrTopGame_HideRivalNames(app->game);
                MimeJrTopGame_HideInstructions(app->game);
            }
            MimeJrTop_SetStateIf(app, done, MIME_JR_TOP_STATE_PLAY, state);
            break;
        case MIME_JR_TOP_STATE_PLAY:
            done = app->finishReceived;
            if (done && app->game->frame < MIME_JR_TOP_GAME_FRAMES) {
                done = FALSE;
            }
            if (!done) {
                if (MimeJrTop_IsParent(app) == TRUE && !MimeJrTopClock_Tick(app->clock)) {
                    StopSE(SEQ_SE_PL_HAND, 0);
                    Comm_SendCommand(MIME_JR_TOP_CMD_FINISH, NULL, 0);
                }
                MimeJrTopGame_Update(app->game);
            }
            MimeJrTop_SetStateIf(app, done, MIME_JR_TOP_STATE_SHOW_FINISH, state);
            break;
        case MIME_JR_TOP_STATE_SHOW_FINISH:
            StopSE(SEQ_SE_PL_140_2_2, 0);
            StopSE(SEQ_SE_PL_HAND, 0);
            PlazaGameSign_StartFinish(app->sign);
            MimeJrTop_SetStateIf(app, TRUE, MIME_JR_TOP_STATE_WAIT_FINISH, state);
            break;
        case MIME_JR_TOP_STATE_WAIT_FINISH:
            MimeJrTop_SetStateIf(app, PlazaGameSign_IsDone(app->sign), MIME_JR_TOP_STATE_SEND_SCORE, state);
            break;
        case MIME_JR_TOP_STATE_SEND_SCORE:
            score = 100 * ((int)app->game->score / 10);
            app->finalScore = score;
            if (score >= 100000) {
                app->finalScore = 100000;
            }
            if (!app->game->everTouched) {
                app->finalScore = 1;
            }
            Comm_SendCommand(MIME_JR_TOP_CMD_FINAL_SCORE, &app->finalScore, 8);
            MimeJrTop_SetStateIf(app, TRUE, MIME_JR_TOP_STATE_WAIT_SCORES, state);
            break;
        case MIME_JR_TOP_STATE_WAIT_SCORES: {
            s32 allReceived;
            s32 i;
            s32 received;
            allReceived = FALSE;
            received = allReceived;
            i = allReceived;
            do {
                if (app->scores[i].score != 0) {
                    received += 1;
                }
                i += 1;
            } while (i < 4);
            if (received == app->game->playerCount) {
                allReceived = TRUE;
            }
            MimeJrTop_SetStateIf(app, allReceived, MIME_JR_TOP_STATE_FADE_OUT, state);
            break;
        }
        case MIME_JR_TOP_STATE_FADE_OUT:
            MimeJrTop_SetStateIf(app, MimeJrTop_StartFade(0x1A), MIME_JR_TOP_STATE_FREE_GAME, state);
            break;
        case MIME_JR_TOP_STATE_FREE_GAME:
            done = IsPaletteFadeFinished();
            if (done) {
                MimeJrTop_FreeGame(app);
            }
            MimeJrTop_SetStateIf(app, done, MIME_JR_TOP_STATE_SHOW_RESULTS, state);
            break;
        case MIME_JR_TOP_STATE_SHOW_RESULTS: {
            i = 0;
            do {
                index = MimeJrTop_GetPlayerIndex(app, i);
                if (index != 0xFF) {
                    app->resultData.scores[index] = app->scores[i].score;
                    if (app->resultData.scores[index] <= 1) {
                        app->resultData.scores[index] = 0;
                    }
                }
                i += 1;
            } while (i < 4);
            PlazaGame_CalcRanks(&app->resultData, app->roster.count);
            app->results = PlazaGameResults_NewMimeJrTop(&app->roster, &app->resultData, HEAP_ID_113);
            MimeJrTop_SetStateIf(app, TRUE, MIME_JR_TOP_STATE_WAIT_RESULTS, state);
            break;
        }
        case MIME_JR_TOP_STATE_WAIT_RESULTS:
            done = PlazaGameResults_IsDone(app->results);
            if (done && (keepPlaying = PlazaGameResults_GetContinue(app->results), PlazaGameResults_Free(app->results), app->results = NULL, keepPlaying)) {
                app->keepPlaying = TRUE;
                MimeJrTop_ResetRound(app);
                MimeJrTop_SetStateIf(app, done, MIME_JR_TOP_STATE_CREATE_ENTRY, state);
                if (app->args->mimeJrTopVoiceChat) {
                    WifiConnection_StopVoiceChat();
                }
            } else {
                MimeJrTop_SetStateIf(app, done, MIME_JR_TOP_STATE_START_SYNC, state);
            }
            break;
        case MIME_JR_TOP_STATE_START_SYNC:
            LinkSync_StartSync(0x1B);
            if (app->args->mimeJrTopVoiceChat) {
                WifiConnection_StopVoiceChat();
            }
            MimeJrTop_SetStateIf(app, TRUE, MIME_JR_TOP_STATE_WAIT_SYNC, state);
            break;
        case MIME_JR_TOP_STATE_WAIT_SYNC:
        default:
            if (LinkSync_IsSynced(0x1B) == TRUE) {
                return TRUE;
            }
            return FALSE;
        }
        if ((int)*state >= MIME_JR_TOP_STATE_FADE_IN && (int)*state < MIME_JR_TOP_STATE_FREE_GAME) {
            MimeJrTopGame_UpdateRivalSprites(app->game);
            MimeJrTop_DrawScene(app);
        }
        return FALSE;
    }
}

static void MimeJrTop_FreeGame(MimeJrTopApp *app) {
    int i;

    Main_SetVBlankIntrCB(NULL, NULL);
    HBlankInterruptDisable();
    GF_DestroyVramTransferManager();
    GF_TouchpadStopSampling();
    SetLCRNGSeed(app->savedSeed);
    MimeJrTopGame_FreeRivalSprites(app->game);
    MimeJrTopGame_FreePopups(app->game);
    MimeJrTopGame_FreeStars(app->game);
    MimeJrTopGame_FreeDancerSprites(app->game);
    MimeJrTopGame_FreeDemoHand(app->game);
    MimeJrTopModel_Free(&app->game->top);
    MimeJrTopModel_Free(&app->game->mimeJr[0]);
    MimeJrTopModel_Free(&app->game->mimeJr[1]);
    MimeJrTopModel_Free(&app->game->mimeJr[2]);
    MimeJrTopModel_Free(&app->game->stage);
    ModelAnimation_Free(&app->game->mimeJr[0].animations[0], &app->resources.allocator);
    ModelAnimation_Free(&app->game->mimeJr[0].animations[1], &app->resources.allocator);
    ModelAnimation_Free(&app->game->mimeJr[0].animations[2], &app->resources.allocator);
    ModelAnimation_Free(&app->game->mimeJr[1].animations[0], &app->resources.allocator);
    ModelAnimation_Free(&app->game->mimeJr[2].animations[0], &app->resources.allocator);

    for (i = 0; i < 8; i++) {
        MimeJrTopModel_Free(&app->game->audience[i]);
        ModelAnimation_Free(&app->game->audience[i].animations[0], &app->resources.allocator);
        ModelAnimation_Free(&app->game->audience[i].animations[1], &app->resources.allocator);
    }

    PlazaGameSign_Free(app->sign);
    MimeJrTop_FreeGraphics(app);
    if (MimeJrTop_IsParent(app) == TRUE) {
        MimeJrTopClock_Free(app->clock);
    }
    MimeJrTopGame_Free(app->game);
    app->clock = NULL;
    app->game = NULL;
}

// Before "keep playing" starts the next round.
static void MimeJrTop_ResetRound(MimeJrTopApp *app) {
    u8 *bytes;
    app->finishReceived = FALSE;
    app->startReceived = FALSE;
    app->unk10 = 0;
    bytes = (u8 *)&app->finalScore;
    *(u8 *)&app->finalScore = 0;
    bytes[1] = 0;
    bytes[2] = 0;
    bytes[3] = 0;
    bytes[4] = 0;
    bytes[5] = 0;
    bytes[6] = 0;
    bytes[7] = 0;
    memset(app->scores, 0, sizeof(app->scores));
}

BOOL MimeJrTop_Exit(OverlayManager *man, int *state) {
    int result;
    if (*state == MIME_JR_TOP_EXIT_STATE_FREE) {
        result = MimeJrTop_HandleConnectionLost(OverlayManager_GetData(man));
        OverlayManager_FreeData(man);
        Heap_Destroy(HEAP_ID_113);
        CommState_SetErrorWatch(FALSE, TRUE);
        if (result != 0) {
            return TRUE;
        }
        LinkSync_StartSync(0xE7);
        (*state)++;
    } else {
        if (LinkSync_IsSynced(0xE7) == TRUE || Comm_CountConnected() < CommPlayerInfo_CountAccepted()) {
            return TRUE;
        }
    }
    return FALSE;
}

static void MimeJrTop_InitGraphics(MimeJrTopApp *app) {
    app->resources.narc = NARC_New(NARC_a_1_9_3, HEAP_ID_113);
    app->resources.unkNarc = NARC_New(NARC_a_1_8_9, HEAP_ID_113);
    app->resources.bgConfig = BgConfig_Alloc(HEAP_ID_113);
    app->resources.palette = PaletteData_Init(HEAP_ID_113);
    app->resources.vramMan = GF_3DVramMan_Create(HEAP_ID_113, 0, 1, 0, 4, NULL);
    app->resources.camera = Camera_New(HEAP_ID_113);
    MimeJrTop_InitCamera(app);
    PaletteData_SetAutoTransparent(app->resources.palette, TRUE);
    PaletteData_AllocBuffers(app->resources.palette, PLTTBUF_MAIN_BG, 0x200, HEAP_ID_113);
    PaletteData_AllocBuffers(app->resources.palette, PLTTBUF_SUB_BG, 0x200, HEAP_ID_113);
    PaletteData_AllocBuffers(app->resources.palette, PLTTBUF_MAIN_OBJ, 0x200, HEAP_ID_113);
    PaletteData_AllocBuffers(app->resources.palette, PLTTBUF_SUB_OBJ, 0x200, HEAP_ID_113);
    MimeJrTop_InitBgs(app->resources.bgConfig);
    MimeJrTop_InitSprites(app);
    reg_G3X_DISP3DCNT = (reg_G3X_DISP3DCNT & 0xFFFFCFFF) | 8;
    reg_G3X_DISP3DCNT = (reg_G3X_DISP3DCNT & 0xFFFFCFFF) | 0x20;
    G3X_SetEdgeColorTable(gMimeJrTopEdgeColors);
    HeapExp_FndInitAllocator(&app->resources.allocator, HEAP_ID_113, 0x20);
}

static void MimeJrTop_FreeGraphics(MimeJrTopApp *app) {
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_BG0, GF_PLANE_TOGGLE_OFF);
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_BG1, GF_PLANE_TOGGLE_OFF);
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_BG2, GF_PLANE_TOGGLE_OFF);
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_BG3, GF_PLANE_TOGGLE_OFF);
    GfGfx_EngineBTogglePlanes(GX_PLANEMASK_BG0, GF_PLANE_TOGGLE_OFF);
    GfGfx_EngineBTogglePlanes(GX_PLANEMASK_BG1, GF_PLANE_TOGGLE_OFF);
    GfGfx_EngineBTogglePlanes(GX_PLANEMASK_BG2, GF_PLANE_TOGGLE_OFF);
    GfGfx_EngineBTogglePlanes(GX_PLANEMASK_BG3, GF_PLANE_TOGGLE_OFF);
    FreeBgTilemapBuffer(app->resources.bgConfig, GF_BG_LYR_MAIN_0);
    FreeBgTilemapBuffer(app->resources.bgConfig, GF_BG_LYR_MAIN_1);
    FreeBgTilemapBuffer(app->resources.bgConfig, GF_BG_LYR_MAIN_2);
    FreeBgTilemapBuffer(app->resources.bgConfig, GF_BG_LYR_MAIN_3);
    FreeBgTilemapBuffer(app->resources.bgConfig, GF_BG_LYR_SUB_0);
    FreeBgTilemapBuffer(app->resources.bgConfig, GF_BG_LYR_SUB_1);
    FreeBgTilemapBuffer(app->resources.bgConfig, GF_BG_LYR_SUB_2);
    FreeBgTilemapBuffer(app->resources.bgConfig, GF_BG_LYR_SUB_3);
    Heap_Free(app->resources.bgConfig);
    PaletteData_FreeBuffers(app->resources.palette, PLTTBUF_MAIN_BG);
    PaletteData_FreeBuffers(app->resources.palette, PLTTBUF_SUB_BG);
    PaletteData_FreeBuffers(app->resources.palette, PLTTBUF_MAIN_OBJ);
    PaletteData_FreeBuffers(app->resources.palette, PLTTBUF_SUB_OBJ);
    PaletteData_Free(app->resources.palette);
    NARC_Delete(app->resources.narc);
    NARC_Delete(app->resources.unkNarc);
    GF_3DVramMan_Delete(app->resources.vramMan);
    Camera_Delete(app->resources.camera);
    SpriteSystem_FreeResourcesAndManager(app->resources.spriteSystem, app->resources.spriteManager);
    SpriteSystem_Free(app->resources.spriteSystem);
    gSystem.screensFlipped = FALSE;
    GfGfx_SwapDisplay();
    reg_G3X_DISP3DCNT &= 0xCFF7;
}

static void MimeJrTop_InitBgs(BgConfig *bgConfig) {
    MimeJrTopBgTemplates mainTemplates;
    MimeJrTopBgTemplates subTemplates;
    MimeJrTopGraphicsModes modes;
    MimeJrTopBanks banks;
    GfGfx_DisableEngineAPlanes();
    modes = gMimeJrTopGraphicsModes;
    SetBothScreensModesAndDisable((GraphicsModes *)&modes);
    banks = gMimeJrTopBanks;
    GfGfx_SetBanks((GraphicsBanks *)&banks);
    MIi_CpuClear32(0, (void *)HW_BG_VRAM, 0x80000);
    MIi_CpuClear32(0, (void *)HW_DB_BG_VRAM, 0x20000);
    MIi_CpuClear32(0, (void *)HW_OBJ_VRAM, 0x40000);
    MIi_CpuClear32(0, (void *)HW_DB_OBJ_VRAM, 0x20000);
    mainTemplates = gMimeJrTopMainBgTemplates;
    BG_ClearCharDataRange(GF_BG_LYR_MAIN_1, 32, 0, HEAP_ID_113);
    BG_ClearCharDataRange(GF_BG_LYR_MAIN_2, 32, 0, HEAP_ID_113);
    BG_ClearCharDataRange(GF_BG_LYR_MAIN_3, 32, 0, HEAP_ID_113);
    InitBgFromTemplate(bgConfig, GF_BG_LYR_MAIN_1, &mainTemplates.bgs[1], GF_BG_TYPE_TEXT);
    InitBgFromTemplate(bgConfig, GF_BG_LYR_MAIN_2, &mainTemplates.bgs[2], GF_BG_TYPE_TEXT);
    InitBgFromTemplate(bgConfig, GF_BG_LYR_MAIN_3, &mainTemplates.bgs[3], GF_BG_TYPE_TEXT);
    BgClearTilemapBufferAndCommit(bgConfig, GF_BG_LYR_MAIN_0);
    BgClearTilemapBufferAndCommit(bgConfig, GF_BG_LYR_MAIN_1);
    BgClearTilemapBufferAndCommit(bgConfig, GF_BG_LYR_MAIN_2);
    BgClearTilemapBufferAndCommit(bgConfig, GF_BG_LYR_MAIN_3);
    subTemplates = gMimeJrTopSubBgTemplates;
    BG_ClearCharDataRange(GF_BG_LYR_SUB_0, 32, 0, HEAP_ID_113);
    BG_ClearCharDataRange(GF_BG_LYR_SUB_1, 32, 0, HEAP_ID_113);
    BG_ClearCharDataRange(GF_BG_LYR_SUB_2, 32, 0, HEAP_ID_113);
    BG_ClearCharDataRange(GF_BG_LYR_SUB_3, 32, 0, HEAP_ID_113);
    InitBgFromTemplate(bgConfig, GF_BG_LYR_SUB_0, &subTemplates.bgs[0], GF_BG_TYPE_TEXT);
    InitBgFromTemplate(bgConfig, GF_BG_LYR_SUB_1, &subTemplates.bgs[1], GF_BG_TYPE_TEXT);
    InitBgFromTemplate(bgConfig, GF_BG_LYR_SUB_2, &subTemplates.bgs[2], GF_BG_TYPE_TEXT);
    InitBgFromTemplate(bgConfig, GF_BG_LYR_SUB_3, &subTemplates.bgs[3], GF_BG_TYPE_TEXT);
    BgClearTilemapBufferAndCommit(bgConfig, GF_BG_LYR_SUB_0);
    BgClearTilemapBufferAndCommit(bgConfig, GF_BG_LYR_SUB_1);
    BgClearTilemapBufferAndCommit(bgConfig, GF_BG_LYR_SUB_2);
    BgClearTilemapBufferAndCommit(bgConfig, GF_BG_LYR_SUB_3);
    BG_ClearCharDataRange(GF_BG_LYR_SUB_0, 32, 0, HEAP_ID_113);
    BG_ClearCharDataRange(GF_BG_LYR_SUB_1, 32, 0, HEAP_ID_113);
    BG_ClearCharDataRange(GF_BG_LYR_SUB_2, 32, 0, HEAP_ID_113);
    BG_ClearCharDataRange(GF_BG_LYR_SUB_3, 32, 0, HEAP_ID_113);
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_BG0, GF_PLANE_TOGGLE_ON);
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_BG1, GF_PLANE_TOGGLE_ON);
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_BG2, GF_PLANE_TOGGLE_ON);
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_BG3, GF_PLANE_TOGGLE_ON);
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_OBJ, GF_PLANE_TOGGLE_ON);
    GfGfx_EngineBTogglePlanes(GX_PLANEMASK_BG0, GF_PLANE_TOGGLE_ON);
    GfGfx_EngineBTogglePlanes(GX_PLANEMASK_BG1, GF_PLANE_TOGGLE_ON);
    GfGfx_EngineBTogglePlanes(GX_PLANEMASK_BG2, GF_PLANE_TOGGLE_ON);
    GfGfx_EngineBTogglePlanes(GX_PLANEMASK_BG3, GF_PLANE_TOGGLE_ON);
    GfGfx_EngineBTogglePlanes(GX_PLANEMASK_OBJ, GF_PLANE_TOGGLE_ON);
    gSystem.screensFlipped = TRUE;
    GfGfx_SwapDisplay();
#if defined(SDK_PORT)
    G2_SetBlendAlpha(0, 30, 7, 10);
    G2S_SetBlendAlpha(0, 17, 7, 10);
#else
    G2x_SetBlendAlpha_((u32)&reg_G2_BLDCNT, 0, 30, 7, 10);
    G2x_SetBlendAlpha_((u32)&reg_G2S_DB_BLDCNT, 0, 17, 7, 10);
#endif
}

static void MimeJrTop_InitSprites(MimeJrTopApp *app) {
    OamManagerParam oam;
    OamCharTransferParam transfer;
    SpriteResourceCountsListUnion counts;
    app->resources.spriteSystem = SpriteSystem_Alloc(HEAP_ID_113);
    oam = gMimeJrTopOamManagerParam;
    transfer = gMimeJrTopOamCharTransferParam;
    SpriteSystem_Init(app->resources.spriteSystem, &oam, &transfer, 32);
    counts = gMimeJrTopSpriteResourceCounts;
    app->resources.spriteManager = SpriteManager_New(app->resources.spriteSystem);
    if (!SpriteSystem_InitSprites(app->resources.spriteSystem, app->resources.spriteManager, 255)) {
        GF_ASSERT(FALSE);
    }
    if (!SpriteSystem_InitManagerWithCapacities(app->resources.spriteSystem, app->resources.spriteManager, &counts)) {
        GF_ASSERT(FALSE);
    }
    G2dRenderer_SetSubSurfaceCoords(SpriteSystem_GetRenderer(app->resources.spriteSystem), 0, 256 << 12);
}

static BOOL MimeJrTop_StartFade(int type) {
    BeginNormalPaletteFade(0, type, type, RGB_BLACK, 6, 1, HEAP_ID_113);
    return TRUE;
}

static void MimeJrTop_SetStateIf(MimeJrTopApp *app, BOOL condition, u32 next, int *state) {
    if (condition) {
        *state = next;
    }
}

static void MimeJrTop_VBlank(void *data) {
    MimeJrTopApp *app = data;
#if defined(SDK_PORT)
#else
    volatile u32 *irqCheck = (u32 *)0x027E0000; // DTCM
#endif
    GF_RunVramTransferTasks();
    SpriteSystem_TransferOam();
    PaletteData_PushTransparentBuffers(app->resources.palette);
    DoScheduledBgGpuUpdates(app->resources.bgConfig);
#if defined(SDK_PORT)
    OS_SetIrqCheckFlag(OS_IE_V_BLANK);
#else
    irqCheck[0x3FF8 / 4] |= OS_IE_V_BLANK; // HW_INTR_CHECK_BUF
#endif
}

static BOOL MimeJrTop_IsParent(MimeJrTopApp *app) {
    if (app->netId == 0) {
        return TRUE;
    }
    return FALSE;
}

static void MimeJrTop_SetStartReceived(MimeJrTopApp *app) {
    app->startReceived = TRUE;
}

static void MimeJrTop_SetFinishReceived(MimeJrTopApp *app) {
    app->finishReceived = TRUE;
}

// 0: connected, 1: showing the lost connection, 2: done (leave the overlay).
static int MimeJrTop_HandleConnectionLost(MimeJrTopApp *app) {
    if (PlazaGameRoster_CheckConnectionLost(&app->roster) == TRUE) {
        if (!app->resources.connectionLost) {
            if (IsPaletteFadeFinished() == TRUE) {
                StopPaletteFade();
            }
            SetBothScreensBrightnessByColor(0);
            app->resources.connectionLost = TRUE;
            return MIME_JR_TOP_CONNECTION_LOST_WAIT;
        } else {
            if (!PlazaGameRoster_ProcessConnectionLost(&app->roster)) {
                return MIME_JR_TOP_CONNECTION_LOST_WAIT;
            }
            if (app->game) {
                MimeJrTopGame_HideRivalNames(app->game);
                MimeJrTopGame_HideInstructions(app->game);
                MimeJrTop_FreeGame(app);
            }
            if (app->results) {
                PlazaGameResults_Free(app->results);
                app->results = NULL;
            }
            if (app->entry) {
                PlazaGameEntry_Free(app->entry);
                app->entry = NULL;
            }
            return MIME_JR_TOP_CONNECTION_LOST_DONE;
        }
    }
    return MIME_JR_TOP_CONNECTED;
}

static void MimeJrTopModel_Draw(MimeJrTopModel *model, MtxFx43 *rotation, VecFx32 *position) {
    if (model->instance.visible) {
        NNS_G3dGlbFlushP();
        NNS_G3dGeBufferOP_N(G3OP_MTX_TRANS, (u32 *)position, 3);
        NNS_G3dGeBufferOP_N(G3OP_MTX_MULT_4x3, (u32 *)rotation, 12);
        NNS_G3dGlbSetBaseScale(&model->instance.scale);
        NNS_G3dDraw(&model->instance.render);
    }
}

static void MimeJrTopModel_Load(MimeJrTopModel *model, NARC *narc, int fileId) {
    ModelResource_LoadFromNarc(&model->resource, narc, fileId, HEAP_ID_113);
}

static void MimeJrTopModel_Init(MimeJrTopModel *model, int large) {
    ModelInstance_Init(&model->instance, &model->resource);
    ModelInstance_SetPosition(&model->instance, 0, FX32_CONST(-20), 0);
    if (large == 0) {
        ModelInstance_SetScale(&model->instance, FX32_ONE, FX32_ONE, FX32_ONE);
    } else {
        ModelInstance_SetScale(&model->instance, FX32_CONST(1.5), FX32_CONST(1.5), FX32_CONST(1.5));
    }
    ModelInstance_SetVisible(&model->instance, TRUE);
    model->position.x = 0;
    model->position.y = FX32_CONST(-20);
    model->position.z = 0;
    model->spinX = 0;
    model->spinY = 0;
    model->animate = FALSE;
    model->loaded = TRUE;
}

static void MimeJrTopModel_Free(MimeJrTopModel *model) {
    model->loaded = FALSE;
    ModelResource_Free(&model->resource);
}

// Draws the 3D scene (audience, Mime Jr. in its current pose, the top, the stage and the tier
// models) and the sprites.
static void MimeJrTop_DrawScene(MimeJrTopApp *app) {
    VecFx32 light;
    int i;
    MimeJrTopModel *model;
    u32 p6, p5, p4, p3, p2, p1;
    int a, b;
    Thunk_G3X_Reset();
    Camera_PushLookAtToNNSGlb();
    light = gMimeJrTopLightVector;
    VEC_Normalize(&light, &light);
    NNS_G3dGlbLightVector(GX_LIGHTID_0, light.x, light.y, light.z);
    NNS_G3dGlbLightColor(GX_LIGHTID_0, GX_RGB(31, 31, 31));
    NNS_G3dGlbMaterialColorDiffAmb(GX_RGB(31, 31, 31), GX_RGB(31, 31, 31), FALSE);
    NNS_G3dGlbMaterialColorSpecEmi(GX_RGB(31, 31, 31), GX_RGB(31, 31, 31), FALSE);
    for (i = 0; i < 8; i++) {
        model = &app->game->audience[i];
        NNS_G3dGeBufferOP_N(G3OP_MTX_PUSH, NULL, 0);
        MimeJrTopModel_Draw(model, &model->rotationFx, &model->position);
        p6 = 1;
        NNS_G3dGeBufferOP_N(G3OP_MTX_POP, &p6, 1);
        ModelAnimation_AdvanceLooped(&app->game->audience[i].animations[0], FX32_ONE);
    }
    switch (app->game->mimeJrPose) {
    case MIME_JR_TOP_POSE_BALANCING:
        ModelInstance_SetVisible(&app->game->mimeJr[0].instance, TRUE);
        ModelInstance_SetVisible(&app->game->mimeJr[1].instance, FALSE);
        ModelInstance_SetVisible(&app->game->mimeJr[2].instance, FALSE);
        ModelAnimation_AdvanceLooped(&app->game->mimeJr[0].animations[app->game->mimeJr[0].animation], FX32_ONE);
        break;
    case MIME_JR_TOP_POSE_FALLING:
        ModelInstance_SetVisible(&app->game->mimeJr[0].instance, FALSE);
        ModelInstance_SetVisible(&app->game->mimeJr[1].instance, TRUE);
        ModelInstance_SetVisible(&app->game->mimeJr[2].instance, FALSE);
        ModelAnimation_Advance(&app->game->mimeJr[1].animations[0], FX32_ONE);
        break;
    case MIME_JR_TOP_POSE_DOWN:
        ModelInstance_SetVisible(&app->game->mimeJr[0].instance, FALSE);
        ModelInstance_SetVisible(&app->game->mimeJr[1].instance, FALSE);
        ModelInstance_SetVisible(&app->game->mimeJr[2].instance, TRUE);
        ModelAnimation_Advance(&app->game->mimeJr[2].animations[0], FX32_ONE);
        break;
    }
    // Every Mime Jr. model is drawn with the rotation and position of the first.
    model = app->game->mimeJr;
    i = app->game->mimeJrPose * sizeof(MimeJrTopModel);
    NNS_G3dGeBufferOP_N(G3OP_MTX_PUSH, NULL, 0);
    MimeJrTopModel_Draw((MimeJrTopModel *)((u8 *)model + i), &model->rotationFx, &model->position);
    p5 = 1;
    NNS_G3dGeBufferOP_N(G3OP_MTX_POP, &p5, 1);
    model = &app->game->top;
    NNS_G3dGeBufferOP_N(G3OP_MTX_PUSH, NULL, 0);
    MimeJrTopModel_Draw(model, &model->rotationFx, &model->position);
    p4 = 1;
    NNS_G3dGeBufferOP_N(G3OP_MTX_POP, &p4, 1);
    model = &app->game->stage;
    NNS_G3dGeBufferOP_N(G3OP_MTX_PUSH, NULL, 0);
    MimeJrTopModel_Draw(model, &model->rotationFx, &model->position);
    p3 = 1;
    NNS_G3dGeBufferOP_N(G3OP_MTX_POP, &p3, 1);
    if (app->game->tierModels[1].loaded) {
        model = &app->game->tierModels[1];
        NNS_G3dGeBufferOP_N(G3OP_MTX_PUSH, NULL, 0);
        MimeJrTopModel_Draw(model, &model->rotationFx, &model->position);
        p2 = 1;
        NNS_G3dGeBufferOP_N(G3OP_MTX_POP, &p2, 1);
        if (app->game->tierModels[1].animate) {
            a = ModelAnimation_Advance(&app->game->tierModels[1].animations[0], FX32_ONE);
            b = ModelAnimation_Advance(&app->game->tierModels[1].animations[1], FX32_ONE);
            if (a && b) {
                app->game->tierModels[1].animate = FALSE;
            }
        }
    }
    if (app->game->tierModels[0].loaded) {
        model = &app->game->tierModels[0];
        NNS_G3dGeBufferOP_N(G3OP_MTX_PUSH, NULL, 0);
        MimeJrTopModel_Draw(model, &model->rotationFx, &model->position);
        p1 = 1;
        NNS_G3dGeBufferOP_N(G3OP_MTX_POP, &p1, 1);
        if (app->game->tierModels[0].animate) {
            ModelAnimation_AdvanceLooped(&app->game->tierModels[0].animations[1], FX32_ONE);
        }
    }
    RequestSwap3DBuffers(GX_SORTMODE_AUTO, GX_BUFFERMODE_W);
    SpriteSystem_DrawSprites(app->resources.spriteManager);
}

static void MimeJrTopGame_SetMimeJrAnimation(MimeJrTopGame *game, int animation) {
    if (animation != game->mimeJr[0].animation) {
        ModelInstance_RemoveAnimation(&game->mimeJr[0].instance, &game->mimeJr[0].animations[game->mimeJr[0].animation]);
        game->mimeJr[0].animation = animation;
        ModelInstance_AddAnimation(&game->mimeJr[0].instance, &game->mimeJr[0].animations[animation]);
    }
}

static void MimeJrTop_InitCamera(MimeJrTopApp *app) {
    VecFx32 target = { 0, 0, 0 };
    VecFx32 pos = gMimeJrTopCameraPosition;
    Camera_Init_FromTargetAndPos(&target, &pos, 0xB60, 0, FALSE, app->resources.camera);
    Camera_SetPerspectiveClippingPlane(FX32_CONST(-1000), FX32_CONST(1000), app->resources.camera);
    Camera_SetStaticPtr(app->resources.camera);
}

static void MimeJrTop_InitSpriteTemplate(ManagedSpriteTemplate *template, s16 x, s16 y, u32 vram, int pal, int resId) {
    template->x = x;
    template->y = y;
    template->z = 0;
    template->animation = 0;
    template->drawPriority = 0;
    template->vram = (NNS_G2D_VRAM_TYPE)vram;
    template->bgPriority = 2;
    template->vramTransfer = 0;
    template->pal = pal;
    template->resIdList[GF_GFX_RES_TYPE_CHAR] = resId;
    template->resIdList[GF_GFX_RES_TYPE_PLTT] = resId;
    template->resIdList[GF_GFX_RES_TYPE_CELL] = resId;
    template->resIdList[GF_GFX_RES_TYPE_ANIM] = resId;
    template->resIdList[GF_GFX_RES_TYPE_MCEL] = -1;
    template->resIdList[GF_GFX_RES_TYPE_MANM] = -1;
}

static void MimeJrTopGame_CreateStars(MimeJrTopGame *game) {
    ManagedSpriteTemplate template;
    SpriteSystem *spriteSystem = game->resources->spriteSystem;
    SpriteManager *spriteManager = game->resources->spriteManager;
    int x, i;
    i = 0;
    x = i;
    do {
        MimeJrTop_InitSpriteTemplate(&template, x, 20, NNS_G2D_VRAM_TYPE_2DMAIN, 3, 9006);
        game->stars[i] = SpriteSystem_NewSprite(spriteSystem, spriteManager, &template);
        ManagedSprite_SetPaletteOverrideOffset(game->stars[i], 3);
        ManagedSprite_SetAnim(game->stars[i], i % 11 + 1);
        x += 8;
        i++;
    } while (i < 36);
    MimeJrTopGame_SetStarsVisible(game, FALSE, 0);
}

static void MimeJrTopGame_CreatePopups(MimeJrTopGame *game) {
    ManagedSpriteTemplate template;
    u16 positions[4];
    SpriteSystem *spriteSystem = game->resources->spriteSystem;
    SpriteManager *spriteManager = game->resources->spriteManager;
    int i;
    s16 *pos = (s16 *)positions;
    positions[0] = gMimeJrTopPopupPositions[0];
    positions[1] = gMimeJrTopPopupPositions[1];
    positions[2] = gMimeJrTopPopupPositions[2];
    positions[3] = gMimeJrTopPopupPositions[3];
    for (i = 0; i < 4; pos++, i++) {
        MimeJrTop_InitSpriteTemplate(&template, *pos, 224, NNS_G2D_VRAM_TYPE_2DMAIN, 1, 9004);
        template.bgPriority = 0;
        template.drawPriority = 1;
        game->popups[i] = SpriteSystem_NewSprite(spriteSystem, spriteManager, &template);
        ManagedSprite_TickTwoFrames(game->popups[i]);
        ManagedSprite_SetPaletteOverrideOffset(game->popups[i], 1);
    }
}

static void MimeJrTopGame_FreePopups(MimeJrTopGame *game) {
    int i;
    for (i = 0; i < 4; i++) {
        Sprite_DeleteAndFreeResources(game->popups[i]);
    }
}

static void MimeJrTopGame_SetStarsVisible(MimeJrTopGame *game, BOOL visible, int unused) {
    int i;
    for (i = 0; i < 36; i++) {
        ManagedSprite_SetDrawFlag(game->stars[i], visible);
    }
}

static void MimeJrTopGame_UpdateStars(MimeJrTopGame *game) {
    int i;
    for (i = 0; i < 36; i++) {
        ManagedSprite_TickTwoFrames(game->stars[i]);
    }
}

static void MimeJrTopGame_FreeStars(MimeJrTopGame *game) {
    int i;
    for (i = 0; i < 36; i++) {
        Sprite_DeleteAndFreeResources(game->stars[i]);
    }
}

// Sets the face and body animation of another player's top (anim: 0 spinning, 1 slow, 3 fallen).
static void MimeJrTopGame_SetRivalAnim(MimeJrTopGame *game, int player, int anim) {
    MimeJrTopRivalAnims anims = gMimeJrTopRivalSetAnims;
    u16 slot = game->rivalSlots[player];

    ManagedSprite_SetAnim(game->rivalFaces[slot], anims.anims[anim][0]);
    ManagedSprite_SetAnim(game->rivalBodies[slot], anims.anims[anim][1]);
}

static int MimeJrTopGame_GetRivalAnim(MimeJrTopGame *game, int player) {
    MimeJrTopRivalAnims anims = gMimeJrTopRivalAnims;
    u16 slot = game->rivalSlots[player];
    int face = ManagedSprite_GetActiveAnim(game->rivalFaces[slot]);
    int body = ManagedSprite_GetActiveAnim(game->rivalBodies[slot]);
    int i;

    for (i = 0; i < 4; i++) {
        if (face == anims.anims[i][0] && body == anims.anims[i][1]) {
            return i;
        }
    }
    GF_ASSERT(FALSE);
    return 0;
}

static void MimeJrTopGame_UpdateRivalSprites(MimeJrTopGame *game) {
    int i = 0;
    int rival = 0;
    if (game->playerCount > 0) {
        do {
            if (i != game->localIndex) {
                ManagedSprite_TickTwoFrames(game->rivalFaces[rival]);
                ManagedSprite_TickTwoFrames(game->rivalBodies[rival]);
                ManagedSprite_TickTwoFrames(game->rivalBadges[rival]);
                rival++;
            }
            i++;
        } while (i < game->playerCount);
    }
}

static void MimeJrTopGame_FreeRivalSprites(MimeJrTopGame *game) {
    int i = 0;
    int rival = 0;
    if (game->playerCount > 0) {
        do {
            if (i != game->localIndex) {
                Sprite_DeleteAndFreeResources(game->rivalFaces[rival]);
                Sprite_DeleteAndFreeResources(game->rivalBodies[rival]);
                Sprite_DeleteAndFreeResources(game->rivalBadges[rival]);
                rival++;
            }
            i++;
        } while (i < game->playerCount);
    }
}

static void MimeJrTopGame_ShowInstructions(MimeJrTopGame *game) {
    MsgData *msgData;
    String *string;

    InitWindow(&game->instructions);
    AddWindowParameterized(game->resources->bgConfig, &game->instructions, GF_BG_LYR_SUB_3, 2, 0x13, 0x1C, 4, 0xE, 0x130);
    FillWindowPixelBuffer(&game->instructions, 0xFF);
    msgData = NewMsgDataFromNarc(MSGDATA_LOAD_DIRECT, NARC_msgdata_msg, NARC_msg_msg_0235_bin, HEAP_ID_113);
    string = NewString_ReadMsgData(msgData, msg_0235_00000);
    AddTextPrinterParameterized(&game->instructions, 0, string, 0, 0, 0, NULL);
    String_Delete(string);
    DestroyMsgData(msgData);
    DrawFrameAndWindow1(&game->instructions, FALSE, 0xB4, 0xD);
    CopyWindowToVram(&game->instructions);
}

static void MimeJrTopGame_HideInstructions(MimeJrTopGame *game) {
    if (WindowIsInUse(&game->instructions) == TRUE) {
        EraseFrameAndWindow1(&game->instructions, TRUE);
        ClearWindowTilemapAndCopyToVram(&game->instructions);
        RemoveWindow(&game->instructions);
    }
}

// The four player colours of the 3D scene (palette per player count and player index).
static void MimeJrTop_LoadPlayerPalettes(MimeJrTopApp *app, int index) {
    PaletteData *palette = app->resources.palette;
    int i;
    MimeJrTopPlayerPalettes *entry = &gMimeJrTopPlayerPalettes[index];
    for (i = 0; i < 4; i++) {
#if defined(SDK_PORT)
        PaletteData_LoadFromNarc(palette, NARC_a_1_9_3, 73, HEAP_ID_113, PLTTBUF_SUB_BG, 32, (i + 1) << 4, (*(int *)((uintptr_t)entry + app->game->playerCount * 80 + i * 4 - 76)) << 4);
#else
        PaletteData_LoadFromNarc(palette, NARC_a_1_9_3, 73, HEAP_ID_113, PLTTBUF_SUB_BG, 32, (i + 1) << 4, (*(int *)((u32)entry + app->game->playerCount * 80 + i * 4 - 76)) << 4);
#endif
    }
}

static int MimeJrTop_GetCenteredX(Window *window, String *string) {
    int width = FontID_String_GetWidth(0, string, 0);
    return (GetWindowWidth(window) * 8 - width) / 2;
}

// The names of the other players next to their tops.
static void MimeJrTopGame_ShowRivalNames(MimeJrTopApp *app, MimeJrTopGame *game) {
    s16 table[3][3][4];
    int tile = 256;
    int i;
    ARRAY_ASSIGN(table, gMimeJrTopRivalNamePositions);
    switch (game->playerCount) {
    case 2:
        GfGfxLoader_LoadScrnDataFromOpenNarc(game->resources->narc, 79, game->resources->bgConfig, GF_BG_LYR_SUB_3, 0, 0, FALSE, HEAP_ID_113);
        break;
    case 3:
        GfGfxLoader_LoadScrnDataFromOpenNarc(game->resources->narc, 80, game->resources->bgConfig, GF_BG_LYR_SUB_3, 0, 0, FALSE, HEAP_ID_113);
        break;
    case 4:
        GfGfxLoader_LoadScrnDataFromOpenNarc(game->resources->narc, 81, game->resources->bgConfig, GF_BG_LYR_SUB_3, 0, 0, FALSE, HEAP_ID_113);
        break;
    default:
        GF_ASSERT(FALSE);
        break;
    }
    i = 0;
    if (game->playerCount > 0) {
        Window *window = game->nameWindows;
        int rival = 0;
        do {
            if (i != game->localIndex) {
                BOOL flag = PlazaGameRoster_IsSpecialVisitor(&app->roster, i);
                PlayerProfile *profile = PlazaGameRoster_GetProfile(&app->roster, i);
                InitWindow(window);
                s16 *coords = (s16 *)((u8 *)table + game->playerCount * 24 + rival * 8);
                int width = coords[-22], height = coords[-21];
                AddWindowParameterized(game->resources->bgConfig, window, GF_BG_LYR_SUB_3, coords[-24], coords[-23], width, height, 14, tile);
                tile += width * height;
                FillWindowPixelBuffer(window, 255);
                String *name = PlayerProfile_GetPlayerName_NewString(profile, HEAP_ID_113);
                int x = MimeJrTop_GetCenteredX(window, name);
                if (flag) {
                    AddTextPrinterParameterizedWithColorAndSpacing(window, 0, name, x, 0, 0, MAKE_TEXT_COLOR(5, 6, 15), 0, 0, NULL);
                } else {
                    AddTextPrinterParameterizedWithColorAndSpacing(window, 0, name, x, 0, 0, MAKE_TEXT_COLOR(1, 2, 15), 0, 0, NULL);
                }
                String_Delete(name);
                CopyWindowToVram(window);
                window++;
                rival++;
            }
            i++;
        } while (i < game->playerCount);
    }
}

static void MimeJrTopGame_HideRivalNames(MimeJrTopGame *game) {
    int i;
    Window *window;

    i = 0;
    if (game->playerCount > 0) {
        window = game->nameWindows;
        do {
            if (i != game->localIndex) {
                if (WindowIsInUse(window) == TRUE) {
                    EraseFrameAndWindow1(window, TRUE);
                    ClearWindowTilemapAndCopyToVram(window);
                    RemoveWindow(window);
                }
                window++;
            }
            i++;
        } while (i < game->playerCount);
    }
    GfGfxLoader_LoadScrnDataFromOpenNarc(game->resources->narc, 78, game->resources->bgConfig, GF_BG_LYR_SUB_3, 0, 0, FALSE, HEAP_ID_113);
}

#define LOAD_SPRITE_RESOURCES(narc, sys, mgr, pal, rid, cf, cv, cellf, animf, pf, pbuf, pnum, pvram)                                           \
    do {                                                                                                                                       \
        SpriteSystem_LoadCharResObjFromOpenNarc((sys), (mgr), (narc), (cf), FALSE, (cv), (rid));                                               \
        SpriteSystem_LoadCellResObjFromOpenNarc((sys), (mgr), (narc), (cellf), FALSE, (rid));                                                  \
        SpriteSystem_LoadAnimResObjFromOpenNarc((sys), (mgr), (narc), (animf), FALSE, (rid));                                                  \
        SpriteSystem_LoadPaletteBufferFromOpenNarc((pal), (PaletteBufferId)(pbuf), (sys), (mgr), (narc), (pf), FALSE, (pnum), (pvram), (rid)); \
    } while (0)

static void MimeJrTop_LoadSpriteResources(MimeJrTopApp *app) {
    NARC *narc = app->resources.narc;
    SpriteSystem *system = app->resources.spriteSystem;
    SpriteManager *manager = app->resources.spriteManager;
    PaletteData *palette = app->resources.palette;

    LOAD_SPRITE_RESOURCES(narc, system, manager, palette, 9000, 0x54, 2, 0x53, 0x52, 0x55, 3, 0xD, 2);
    LOAD_SPRITE_RESOURCES(narc, system, manager, palette, 9003, 0x1D, 1, 0x1C, 0x1B, 0x1E, 2, 1, 1);
    LOAD_SPRITE_RESOURCES(narc, system, manager, palette, 9004, 0x0D, 1, 0x0C, 0x0B, 0x0E, 2, 1, 1);
    LOAD_SPRITE_RESOURCES(narc, system, manager, palette, 9005, 0x15, 1, 0x14, 0x13, 0x16, 2, 1, 1);
    LOAD_SPRITE_RESOURCES(narc, system, manager, palette, 9006, 0x11, 1, 0x10, 0x0F, 0x12, 2, 1, 1);
    LOAD_SPRITE_RESOURCES(narc, system, manager, palette, 9002, 0x19, 1, 0x18, 0x17, 0x1A, 2, 3, 1);
    narc = NARC_New(NARC_a_1_9_8, HEAP_ID_113);
    LOAD_SPRITE_RESOURCES(narc, system, manager, palette, 9007, 0x15, 1, 0x16, 0x17, 0x14, 2, 2, 1);
    NARC_Delete(narc);
}

#undef LOAD_SPRITE_RESOURCES

// The other players' tops on the sub screen: face, body and tier badge per player.
static void MimeJrTopGame_CreateRivalSprites(MimeJrTopApp *app, MimeJrTopGame *game) {
    ManagedSpriteTemplate template;
    SpriteSystem *spriteSystem = game->resources->spriteSystem;
    SpriteManager *spriteManager = game->resources->spriteManager;
    int rival = 0;
    int i;
    for (i = 0; i < game->playerCount; i++) {
        game->rivalSlots[i] = 0xFF;
        if (i == game->localIndex) {
            continue;
        }
        game->rivalSlots[i] = rival;
        MimeJrTop_InitSpriteTemplate(&template, gMimeJrTopRivalSpriteX[game->playerCount - 1][rival], 0xF0, NNS_G2D_VRAM_TYPE_2DSUB, 0, 9000);
        template.drawPriority = 1;
        game->rivalFaces[rival] = SpriteSystem_NewSprite(spriteSystem, spriteManager, &template);
        MimeJrTop_InitSpriteTemplate(&template, gMimeJrTopRivalSpriteX[game->playerCount - 1][rival], 0xF0, NNS_G2D_VRAM_TYPE_2DSUB, 0, 9000);
        template.drawPriority = 2;
        game->rivalBodies[rival] = SpriteSystem_NewSprite(spriteSystem, spriteManager, &template);
        ((MimeJrTopSetPaletteOffsetFunc)ManagedSprite_SetPaletteOverrideOffset)(game->rivalBodies[rival], MimeJrTop_GetPlayerIndex(app, i));
        MimeJrTop_InitSpriteTemplate(&template, gMimeJrTopRivalSpriteX[game->playerCount - 1][rival], 0xF0, NNS_G2D_VRAM_TYPE_2DSUB, 0, 9000);
        template.drawPriority = 3;
        game->rivalBadges[rival] = SpriteSystem_NewSprite(spriteSystem, spriteManager, &template);
        ((MimeJrTopSetPaletteOffsetFunc)ManagedSprite_SetPaletteOverrideOffset)(game->rivalBadges[rival], MimeJrTop_GetPlayerIndex(app, i));
        ManagedSprite_SetAnim(game->rivalBadges[rival], 5);
        ManagedSprite_TickTwoFrames(game->rivalFaces[rival]);
        ManagedSprite_TickTwoFrames(game->rivalBodies[rival]);
        ManagedSprite_TickTwoFrames(game->rivalBadges[rival]);
        MimeJrTopGame_SetRivalAnim(game, i, 0);
        rival++;
    }
}

static ManagedSprite *MimeJrTopGame_NewSpark(MimeJrTopGame *game, u16 x, u16 y) {
    ManagedSpriteTemplate template;
    ManagedSprite *sprite;
    MimeJrTopResources *resources = game->resources;
    SpriteSystem *system = resources->spriteSystem;
    SpriteManager *manager = resources->spriteManager;

    ((MimeJrTopInitTemplateFunc)MimeJrTop_InitSpriteTemplate)(&template, x, y, NNS_G2D_VRAM_TYPE_2DMAIN, 0, 9003);
    template.bgPriority = 0;
    template.drawPriority = 0;
    sprite = SpriteSystem_NewSprite(system, manager, &template);
    ManagedSprite_TickTwoFrames(sprite);
    return sprite;
}

static void MimeJrTopGame_CreateDancerSprites(MimeJrTopGame *game) {
    ManagedSpriteTemplate template;
    SpriteSystem *spriteSystem = game->resources->spriteSystem;
    SpriteManager *spriteManager = game->resources->spriteManager;
    int i;
    for (i = 0; i < 2; i++) {
        MimeJrTop_InitSpriteTemplate(&template, 0, 0, NNS_G2D_VRAM_TYPE_2DMAIN, 4, 9002);
        template.bgPriority = 2;
        template.drawPriority = 20;
        game->frontDancers[i] = SpriteSystem_NewSprite(spriteSystem, spriteManager, &template);
        ManagedSprite_SetAnim(game->frontDancers[i], 0);
        ManagedSprite_TickFrame(game->frontDancers[i]);
        ManagedSprite_SetPaletteOverrideOffset(game->frontDancers[i], 4);
        ManagedSprite_SetOamMode(game->frontDancers[i], GX_OAM_MODE_XLU);
        ManagedSprite_SetDrawFlag(game->frontDancers[i], FALSE);
        MimeJrTop_InitSpriteTemplate(&template, 0, 0, NNS_G2D_VRAM_TYPE_2DSUB, 0, 9000);
        template.bgPriority = 2;
        template.drawPriority = 20;
        game->backDancers[i] = SpriteSystem_NewSprite(spriteSystem, spriteManager, &template);
        ManagedSprite_SetAnim(game->backDancers[i], 6);
        ManagedSprite_TickFrame(game->backDancers[i]);
        ManagedSprite_SetPaletteOverrideOffset(game->backDancers[i], 0);
        ManagedSprite_SetOamMode(game->backDancers[i], GX_OAM_MODE_XLU);
        ManagedSprite_SetDrawFlag(game->backDancers[i], FALSE);
    }
}

static void MimeJrTopGame_FreeDancerSprites(MimeJrTopGame *game) {
    int i;
    for (i = 0; i < 2; i++) {
        Sprite_DeleteAndFreeResources(game->frontDancers[i]);
        Sprite_DeleteAndFreeResources(game->backDancers[i]);
    }
}

static void MimeJrTopGame_CreateDemoHand(MimeJrTopGame *game) {
    ManagedSpriteTemplate template;
    SpriteSystem *spriteSystem = game->resources->spriteSystem;
    SpriteManager *spriteManager = game->resources->spriteManager;
    MimeJrTop_InitSpriteTemplate(&template, 128, 128, NNS_G2D_VRAM_TYPE_2DMAIN, 7, 9007);
    template.bgPriority = 0;
    template.drawPriority = 0;
    game->demoHand = SpriteSystem_NewSprite(spriteSystem, spriteManager, &template);
    ManagedSprite_SetAnim(game->demoHand, 1);
    ManagedSprite_SetDrawFlag(game->demoHand, TRUE);
    ManagedSprite_TickFrame(game->demoHand);
    ManagedSprite_SetPaletteOverrideOffset(game->demoHand, 7);
}

static void MimeJrTopGame_FreeDemoHand(MimeJrTopGame *game) {
    Sprite_DeleteAndFreeResources(game->demoHand);
}

static void MimeJrTop_LoadBgGraphics(MimeJrTopApp *app) {
    NARC *narc;
    BgConfig *bgConfig;
    PaletteData *palette;
    bgConfig = app->resources.bgConfig;
    narc = app->resources.narc;
    palette = app->resources.palette;
    GfGfxLoader_LoadCharDataFromOpenNarc(narc, 72, bgConfig, GF_BG_LYR_SUB_0, 0, 0, FALSE, HEAP_ID_113);
    GfGfxLoader_LoadCharDataFromOpenNarc(narc, 75, bgConfig, GF_BG_LYR_SUB_1, 0, 0, FALSE, HEAP_ID_113);
    GfGfxLoader_LoadScrnDataFromOpenNarc(narc, 74, bgConfig, GF_BG_LYR_SUB_0, 0, 0, FALSE, HEAP_ID_113);
    GfGfxLoader_LoadScrnDataFromOpenNarc(narc, 76, bgConfig, GF_BG_LYR_SUB_1, 0, 0, FALSE, HEAP_ID_113);
    GfGfxLoader_LoadScrnDataFromOpenNarc(narc, 77, bgConfig, GF_BG_LYR_SUB_2, 0, 0, FALSE, HEAP_ID_113);
    GfGfxLoader_LoadScrnDataFromOpenNarc(narc, 78, bgConfig, GF_BG_LYR_SUB_3, 0, 0, FALSE, HEAP_ID_113);
    PaletteData_LoadNarc(palette, NARC_a_1_9_3, 73, HEAP_ID_113, PLTTBUF_SUB_BG, 160, 0);
    GfGfxLoader_LoadCharDataFromOpenNarc(narc, 5, bgConfig, GF_BG_LYR_MAIN_3, 0, 0, FALSE, HEAP_ID_113);
    GfGfxLoader_LoadCharDataFromOpenNarc(narc, 8, bgConfig, GF_BG_LYR_MAIN_1, 0, 0, FALSE, HEAP_ID_113);
    GfGfxLoader_LoadScrnDataFromOpenNarc(narc, 7, bgConfig, GF_BG_LYR_MAIN_3, 0, 0, FALSE, HEAP_ID_113);
    GfGfxLoader_LoadScrnDataFromOpenNarc(narc, 9, bgConfig, GF_BG_LYR_MAIN_1, 0, 0, FALSE, HEAP_ID_113);
    GfGfxLoader_LoadScrnDataFromOpenNarc(narc, 10, bgConfig, GF_BG_LYR_MAIN_2, 0, 0, FALSE, HEAP_ID_113);
    PaletteData_LoadNarc(palette, NARC_a_1_9_3, 6, HEAP_ID_113, PLTTBUF_MAIN_BG, 32, 0);
    PaletteData_LoadNarc(palette, NARC_graphic_font, 8, HEAP_ID_113, PLTTBUF_SUB_BG, 32, 224);
    PaletteData_LoadNarc(palette, NARC_a_0_3_8, GetUserFrameGfx1PaletteMember(), HEAP_ID_113, PLTTBUF_SUB_BG, 32, 208);
    LoadUserFrameGfx1(bgConfig, GF_BG_LYR_SUB_3, 180, 13, 0, HEAP_ID_113);
}

static MimeJrTopClock *MimeJrTopClock_New(u32 playerCount, MimeJrTopResources *resources) {
    MimeJrTopClock *clock = Heap_Alloc(HEAP_ID_113, sizeof(MimeJrTopClock));
    memset(clock, 0, sizeof(MimeJrTopClock));
    clock->playerCount = playerCount;
    clock->timer = MIME_JR_TOP_GAME_FRAMES;
    clock->resources = resources;
    return clock;
}

static void MimeJrTopClock_Free(MimeJrTopClock *clock) {
    Heap_Free(clock);
}

// FALSE once the game time is over.
static BOOL MimeJrTopClock_Tick(MimeJrTopClock *clock) {
    clock->timer--;
    if (clock->timer > 0) {
        return TRUE;
    }
    return FALSE;
}

static MimeJrTopGame *MimeJrTopGame_New(u32 playerCount, u32 localIndex, MimeJrTopResources *resources) {
    MimeJrTopGame *game = Heap_Alloc(HEAP_ID_113, sizeof(MimeJrTopGame));
    memset(game, 0, sizeof(MimeJrTopGame));
    game->playerCount = playerCount;
    game->localIndex = localIndex;
    game->demoFrame = 0;
    game->demoStep = 0;
    game->resources = resources;
    game->rivalFallShown = FALSE;
    game->stageEffects.previousPhase = game->tier;
    game->stageEffects.phase = &game->tier;
    game->everTouched = FALSE;
    MimeJrTopGame_ResetBalance(game);
    return game;
}

// Puts the top upright and the models back to their start rotation.
static void MimeJrTopGame_ResetBalance(MimeJrTopGame *game) {
    int i;
    game->tilt.x = 0;
    game->tilt.y = FX32_CONST(100);
    game->tilt.z = 0;
    game->mimeJr[0].spinX = 0;
    game->mimeJr[0].spinY = 0;
    game->top.spinX = 0;
    game->top.spinY = 0;
    game->mimeJr[0].spinFrames = 0;
    game->top.spinFrames = 0;
    MimeJrTop_QuatIdentity(&game->top.rotation);
    MimeJrTop_QuatIdentity(&game->top.lastTurn);
    MimeJrTop_QuatIdentity(&game->mimeJr[0].rotation);
    MimeJrTop_QuatIdentity(&game->mimeJr[0].lastTurn);
    MimeJrTop_QuatIdentity(&game->stage.rotation);
    MimeJrTop_QuatIdentity(&game->stage.lastTurn);
    for (i = 0; i < 8; i++) {
        MimeJrTop_QuatIdentity(&game->audience[i].rotation);
        MimeJrTop_QuatIdentity(&game->audience[i].lastTurn);
        MimeJrTopModel_Turn(&game->audience[i], 0, 0, 5, 5, 0.8f, FALSE);
        MimeJrTopModel_Turn(&game->audience[i], 0, 0, -5, -5, 0.8f, FALSE);
    }
    MimeJrTopModel_Turn(&game->top, 0, 0, 5, 5, 1.88f, FALSE);
    MimeJrTopModel_Turn(&game->top, 0, 0, -5, -5, 1.88f, FALSE);
    MimeJrTopModel_Turn(&game->mimeJr[0], 0, 0, 5, 5, 0.8f, FALSE);
    MimeJrTopModel_Turn(&game->mimeJr[0], 0, 0, -5, -5, 0.8f, FALSE);
    MimeJrTopModel_Turn(&game->stage, 0, 0, 5, 5, 1.88f, FALSE);
    MimeJrTopModel_Turn(&game->stage, 0, 0, -5, -5, 0.8f, FALSE);
}

static void MimeJrTopGame_Free(MimeJrTopGame *game) {
    Heap_Free(game);
}

static BOOL MimeJrTopGame_HasFallen(MimeJrTopGame *game, int unused) {
    if ((game->tilt.y >> FX32_SHIFT) <= 45) {
        return TRUE;
    }
    return FALSE;
}

// Mime Jr. wobbles harder the further the top leans.
static void MimeJrTopGame_UpdateMimeJrAnimation(MimeJrTopGame *game) {
    int height = game->tilt.y >> FX32_SHIFT;
    if (height < 75 && height > 61) {
        MimeJrTopGame_SetMimeJrAnimation(game, 1);
    } else if (height < 60 && height > 46) {
        MimeJrTopGame_SetMimeJrAnimation(game, 2);
    } else {
        MimeJrTopGame_SetMimeJrAnimation(game, 0);
    }
}

static SysTask *MimeJrTopGame_StartRivalFall(MimeJrTopGame *game, int player) {
    MimeJrTopRivalFallTask *task = Heap_Alloc(HEAP_ID_113, sizeof(MimeJrTopRivalFallTask));
    memset(task, 0, sizeof(MimeJrTopRivalFallTask));
    task->face = game->rivalFaces[game->rivalSlots[player]];
    task->body = game->rivalBodies[game->rivalSlots[player]];
    task->state = MIME_JR_TOP_RIVAL_FALL_START;
    task->step = MIME_JR_TOP_FALL_STEP_FALL_OFF;
    task->resources = game->resources;
    return SysTask_CreateOnMainQueue(MimeJrTopRivalFall_Task, task, 4096);
}

static void MimeJrTopGame_StartRivalTier(MimeJrTopGame *game, int player) {
    MimeJrTopRivalTierTask *task = &game->playerTasks[player].rival;
    if (task->active == TRUE) {
        return;
    }
    task->selection = game->selection;
    task->active = TRUE;
    task->current = (int *)&game->rivalTiers[player];
    task->previous = *task->current;
    task->sprite = game->rivalBadges[game->rivalSlots[player]];
    task->state = MIME_JR_TOP_RIVAL_TIER_START_GROW;
    task->mode = 0;
    task->resources = game->resources;
    SysTask_CreateOnMainQueue(MimeJrTopRivalTier_Task, task, 4096);
}

static void MimeJrTopGame_StartTierModels(MimeJrTopGame *game, int player) {
    MimeJrTopTierModelsTask *task = &game->playerTasks[player].local;

    if (task->active != TRUE) {
        task->selection = game->selection;
        task->active = TRUE;
        task->current = (int *)&game->tier;
        task->previous = *task->current;
        task->modelA = &game->tierModels[0];
        task->modelB = &game->tierModels[1];
        task->state = MIME_JR_TOP_TIER_MODELS_LOAD_TOP;
        task->unk04 = 0;
        task->resources = game->resources;
        SysTask_CreateOnMainQueue((SysTaskFunc)MimeJrTopTierModels_Task, task, 1 << 12);
    }
}

static void MimeJrTopGame_StartFall(MimeJrTopGame *game) {
    MimeJrTopFallTask *task = Heap_Alloc(HEAP_ID_113, sizeof(MimeJrTopFallTask));
    memset(task, 0, sizeof(MimeJrTopFallTask));
    task->pose = &game->mimeJrPose;
    task->mimeJr = &game->mimeJr[0];
    task->top = &game->top;
    task->tilt = game->tilt;
    task->selection = game->selection;
    task->resources = game->resources;
    MimeJrTopGame_SetMimeJrAnimation(game, 0);
    ModelAnimation_SetFrame(&game->mimeJr[0].animations[0], 0);
    ModelAnimation_SetFrame(&game->mimeJr[1].animations[0], 0);
    ModelAnimation_SetFrame(&game->mimeJr[2].animations[0], 0);
    SysTask_CreateOnMainQueue((SysTaskFunc)MimeJrTopFall_Task, task, 1 << 12);
}

// Mime Jr. falls off the side the top leans to (pose 1), lies there (pose 2), then climbs back.
static void MimeJrTopFall_Task(SysTask *sysTask, MimeJrTopFallTask *task) {
    int direction = (s16)(task->tilt.z >> FX32_SHIFT), a, b;
    if (!IsPaletteFadeFinished() || task->resources->connectionLost == TRUE) {
        SysTask_Destroy(sysTask);
        Heap_Free(task);
        return;
    }
    switch (task->state) {
    case MIME_JR_TOP_FALL_START_HOP:
        if (task->step == MIME_JR_TOP_FALL_STEP_FALL_OFF) {
            MimeJrTopTween_Init(&task->tweenY, task->mimeJr->position.y, task->mimeJr->position.y + FX32_CONST(12), 4);
            if (direction > 0) {
                MimeJrTopTween_Init(&task->tweenZ, task->mimeJr->position.z, task->mimeJr->position.z + FX32_CONST(16), 4);
            } else {
                MimeJrTopTween_Init(&task->tweenZ, task->mimeJr->position.z, task->mimeJr->position.z - FX32_CONST(16), 4);
            }
        } else {
            MimeJrTopTween_Init(&task->tweenY, task->mimeJr->position.y, task->mimeJr->position.y + FX32_CONST(48), 8);
            if (direction > 0) {
                MimeJrTopTween_Init(&task->tweenZ, task->mimeJr->position.z, task->mimeJr->position.z - FX32_CONST(16), 8);
            } else {
                MimeJrTopTween_Init(&task->tweenZ, task->mimeJr->position.z, task->mimeJr->position.z + FX32_CONST(16), 4);
            }
        }
        PlaySE(SEQ_SE_DP_DANSA);
        task->timer = 0;
        task->state++;
        break;
    case MIME_JR_TOP_FALL_HOP:
        a = MimeJrTopTween_Update(&task->tweenY);
        b = MimeJrTopTween_Update(&task->tweenZ);
        task->mimeJr->position.y = task->tweenY.value;
        task->mimeJr->position.z = task->tweenZ.value;
        if (!a || !b) {
            break;
        }
        if (task->step == MIME_JR_TOP_FALL_STEP_FALL_OFF) {
            *task->pose = MIME_JR_TOP_POSE_FALLING;
            MimeJrTop_QuatIdentity(&task->mimeJr->rotation);
            MimeJrTop_QuatIdentity(&task->mimeJr->lastTurn);
            MimeJrTopModel_Turn(task->mimeJr, 0, 0, 5, 5, 0.8f, FALSE);
            MimeJrTopModel_Turn(task->mimeJr, 0, 0, -5, -5, 0.8f, FALSE);
            MimeJrTopTween_Init(&task->tweenY, task->mimeJr->position.y, task->mimeJr->position.y - FX32_CONST(48), 8);
            if (direction > 0) {
                MimeJrTopTween_Init(&task->tweenZ, task->mimeJr->position.z, task->mimeJr->position.z + FX32_CONST(16), 8);
            } else {
                MimeJrTopTween_Init(&task->tweenZ, task->mimeJr->position.z, task->mimeJr->position.z - FX32_CONST(16), 8);
            }
        } else {
            MimeJrTopTween_Init(&task->tweenY, task->mimeJr->position.y, task->mimeJr->position.y - FX32_CONST(12), 4);
            if (direction > 0) {
                MimeJrTopTween_Init(&task->tweenZ, task->mimeJr->position.z, task->mimeJr->position.z - FX32_CONST(16), 4);
            } else {
                MimeJrTopTween_Init(&task->tweenZ, task->mimeJr->position.z, task->mimeJr->position.z + FX32_CONST(16), 4);
            }
        }
        task->state++;
        break;
    case MIME_JR_TOP_FALL_LAND:
        a = MimeJrTopTween_Update(&task->tweenY);
        b = MimeJrTopTween_Update(&task->tweenZ);
        task->mimeJr->position.y = task->tweenY.value;
        task->mimeJr->position.z = task->tweenZ.value;
        if (!a || !b) {
            break;
        }
        if (task->step == MIME_JR_TOP_FALL_STEP_FALL_OFF) {
            PlaySE(SEQ_SE_DP_SUTYA);
        }
        task->state++;
        break;
    case MIME_JR_TOP_FALL_LIE:
        if (task->step == MIME_JR_TOP_FALL_STEP_FALL_OFF) {
            if (task->timer == 8) {
                PlaySE(SEQ_SE_PL_KIN);
            }
            task->timer++;
            if (task->timer < 40) {
                break;
            }
            *task->pose = MIME_JR_TOP_POSE_DOWN;
            task->state++;
        } else {
            task->state++;
        }
        break;
    default:
        if (task->step == MIME_JR_TOP_FALL_STEP_FALL_OFF) {
            task->timer++;
            if (task->timer < 45) {
                break;
            }
            *task->pose = MIME_JR_TOP_POSE_BALANCING;
            PlaySE(SEQ_SE_DP_DANSA);
            task->state = MIME_JR_TOP_FALL_START_HOP;
            task->step++;
        } else {
            task->timer++;
            if (task->timer < 10) {
                break;
            }
            if (task->selection) {
                task->mimeJr->position.y = FX32_CONST(-8);
            } else {
                task->mimeJr->position.y = FX32_CONST(-20);
            }
            task->mimeJr->position.z = 0;
            SysTask_Destroy(sysTask);
            Heap_Free(task);
        }
        break;
    }
}

// Another player's top drops down and comes back.
static void MimeJrTopRivalFall_Task(SysTask *sysTask, void *data) {
    MimeJrTopRivalFallTask *task = data;
    ManagedSprite_GetAnimationFrame(task->face);
    if (!IsPaletteFadeFinished() || task->resources->connectionLost == TRUE) {
        SysTask_Destroy(sysTask);
        Heap_Free(task);
        return;
    }
    switch (task->state) {
    case MIME_JR_TOP_RIVAL_FALL_START: {
        fx32 x;
        fx32 y;
        task->timer = 0;
        ManagedSprite_GetSpritePositionFxXY(task->face, &x, &y);
        if (task->step == MIME_JR_TOP_FALL_STEP_FALL_OFF) {
            MimeJrTopTween_Init(&task->tween, y, y + FX32_CONST(32), 0x10);
        } else {
            MimeJrTopTween_Init(&task->tween, y, y - FX32_CONST(32), 4);
        }
        task->state++;
        return;
    }
    case MIME_JR_TOP_RIVAL_FALL_MOVE: {
        fx32 x;
        fx32 y;
        BOOL done = MimeJrTopTween_Update(&task->tween);
        ManagedSprite_GetSpritePositionFxXY(task->face, &x, &y);
        ManagedSprite_SetPositonFxXY(task->face, x, task->tween.value);
        if (done) {
            task->state++;
        }
        return;
    }
    default:
        if (task->step == MIME_JR_TOP_FALL_STEP_FALL_OFF) {
            if (++task->timer >= 35) {
                task->step++;
                task->state = MIME_JR_TOP_RIVAL_FALL_START;
            }
        } else if (++task->timer >= 10) {
            SysTask_Destroy(sysTask);
            Heap_Free(task);
        }
        return;
    }
}

// Another player's tier badge: grows, then pulses until the tier changes; tier 0 hides it.
static void MimeJrTopRivalTier_Task(SysTask *sysTask, void *data) {
    MimeJrTopRivalTierTask *task = data;
    BOOL done;
    f32 x;
    f32 y;
    f32 scale;
    f32 roundedFloat;
    s32 rounded;

    if (!IsPaletteFadeFinished() || task->resources->connectionLost == TRUE) {
        task->active = FALSE;
        SysTask_Destroy(sysTask);
        return;
    }

    switch (task->state) {
    case MIME_JR_TOP_RIVAL_TIER_START_GROW:
        ManagedSprite_SetAnim(task->sprite, 7);
        ManagedSprite_SetDrawPriority(task->sprite, 0);
        ManagedSprite_SetAffineOverwriteMode(task->sprite, NNS_G2D_RND_AFFINE_OVERWRITE_DOUBLE);
        ManagedSprite_GetAffineScale(task->sprite, &x, &y);
        if (y > 0.0f) {
            roundedFloat = y * 4096.0f + 0.5f;
        } else {
            roundedFloat = y * 4096.0f - 0.5f;
        }
        rounded = (s32)roundedFloat;
        MimeJrTopTween_Init(&task->tween, rounded, FX32_CONST(2), 2);
        ((MimeJrTopSetPaletteOffsetFunc)ManagedSprite_SetPaletteOverrideOffset)(task->sprite, *task->current - 1);
        task->state++;
    case MIME_JR_TOP_RIVAL_TIER_GROW:
        done = MimeJrTopTween_Update(&task->tween);
        scale = (f32)task->tween.value / 4096.0f;
        ManagedSprite_SetAffineScale(task->sprite, 1.0f, scale);
        if (done) {
            task->state++;
        }
        return;
    case MIME_JR_TOP_RIVAL_TIER_START_SHRINK:
        MimeJrTopTween_Init(&task->tween, FX32_CONST(2), FX32_CONST(0.4), 4);
        task->state++;
    case MIME_JR_TOP_RIVAL_TIER_SHRINK:
        done = MimeJrTopTween_Update(&task->tween);
        scale = (f32)task->tween.value / 4096.0f;
        ManagedSprite_SetAffineScale(task->sprite, 1.0f, scale);
        if (done) {
            task->state++;
        }
        return;
    case MIME_JR_TOP_RIVAL_TIER_START_PULSE:
        if (task->previous != *task->current) {
            task->state = MIME_JR_TOP_RIVAL_TIER_START_GROW;
        } else {
            if (task->mode == 0) {
                MimeJrTopTween_Init(&task->tween, FX32_CONST(0.4), FX32_CONST(0.8), 2);
            } else {
                MimeJrTopTween_Init(&task->tween, FX32_CONST(0.8), FX32_CONST(0.4), 4);
            }
            task->state++;
        }
        task->previous = *task->current;
        if (*task->current == 0) {
            task->state = MIME_JR_TOP_RIVAL_TIER_HIDE;
        }
        return;
    case MIME_JR_TOP_RIVAL_TIER_PULSE:
        done = MimeJrTopTween_Update(&task->tween);
        scale = (f32)task->tween.value / 4096.0f;
        ManagedSprite_SetAffineScale(task->sprite, 1.0f, scale);
        if (done) {
            task->state = MIME_JR_TOP_RIVAL_TIER_START_PULSE;
            task->mode ^= 1;
        }
        return;
    default:
        ManagedSprite_SetAffineScale(task->sprite, 1.0f, 1.0f);
        ManagedSprite_SetAnim(task->sprite, 5);
        ManagedSprite_SetDrawPriority(task->sprite, 2);
        ManagedSprite_SetAffineOverwriteMode(task->sprite, NNS_G2D_RND_AFFINE_OVERWRITE_NONE);
        task->active = FALSE;
        SysTask_Destroy(sysTask);
        return;
    }
}

#define FREE_TIER_MODELS()                                                    \
    MimeJrTopModel_Free(modelA);                                              \
    MimeJrTopModel_Free(modelB);                                              \
    ModelAnimation_Free(&modelA->animations[0], &task->resources->allocator); \
    ModelAnimation_Free(&modelA->animations[1], &task->resources->allocator); \
    ModelAnimation_Free(&modelB->animations[0], &task->resources->allocator); \
    ModelAnimation_Free(&modelB->animations[1], &task->resources->allocator)

// Loads the two models of the current tier (one step per frame) and swaps them in.
static void MimeJrTopTierModels_Task(SysTask *sysTask, MimeJrTopTierModelsTask *task) {
    MimeJrTopModel *modelA = task->modelA, *modelB = task->modelB;
    int tier, index;
    if (!IsPaletteFadeFinished() || task->resources->connectionLost == TRUE) {
        FREE_TIER_MODELS();
        task->active = FALSE;
        SysTask_Destroy(sysTask);
        return;
    }
    tier = *task->current;
    index = tier - 1;
    if (index < 0 || index > 4) {
        index = 0;
    }
    switch (task->state) {
    case MIME_JR_TOP_TIER_MODELS_LOAD_TOP:
        MimeJrTopModel_Load(modelA, task->resources->narc, gMimeJrTopTierModels[index][0]);
        MimeJrTopModel_Init(modelA, task->selection);
        task->state++;
        return;
    case MIME_JR_TOP_TIER_MODELS_LOAD_TOP_ANIMS:
        ModelAnimation_LoadFromNarc(&modelA->animations[0], &modelA->resource, task->resources->narc, 39, HEAP_ID_113, &task->resources->allocator);
        ModelAnimation_LoadFromNarc(&modelA->animations[1], &modelA->resource, task->resources->narc, 40, HEAP_ID_113, &task->resources->allocator);
        ModelInstance_AddAnimation(&modelA->instance, &modelA->animations[0]);
        ModelInstance_AddAnimation(&modelA->instance, &modelA->animations[1]);
        ModelAnimation_SetFrame(&modelA->animations[0], 0);
        ModelAnimation_SetFrame(&modelA->animations[1], 0);
        ModelInstance_SetVisible(&modelA->instance, FALSE);
        modelA->animate = FALSE;
        MimeJrTop_QuatIdentity(&modelA->rotation);
        MimeJrTop_QuatIdentity(&modelA->lastTurn);
        MimeJrTopModel_Turn(modelA, 0, 0, 5, 5, 0.8f, FALSE);
        MimeJrTopModel_Turn(modelA, 0, 0, -5, -5, 0.8f, FALSE);
        task->state++;
        return;
    case MIME_JR_TOP_TIER_MODELS_LOAD_CHANGE:
        MimeJrTopModel_Load(modelB, task->resources->narc, gMimeJrTopTierChangeModels[index][0]);
        MimeJrTopModel_Init(modelB, task->selection);
        task->state++;
        return;
    case MIME_JR_TOP_TIER_MODELS_LOAD_CHANGE_ANIMS:
        ModelAnimation_LoadFromNarc(&modelB->animations[0], &modelB->resource, task->resources->narc, 41, HEAP_ID_113, &task->resources->allocator);
        ModelAnimation_LoadFromNarc(&modelB->animations[1], &modelB->resource, task->resources->narc, 42, HEAP_ID_113, &task->resources->allocator);
        ModelInstance_AddAnimation(&modelB->instance, &modelB->animations[0]);
        ModelInstance_AddAnimation(&modelB->instance, &modelB->animations[1]);
        ModelAnimation_SetFrame(&modelB->animations[0], 0);
        ModelAnimation_SetFrame(&modelB->animations[1], 0);
        ModelInstance_SetVisible(&modelB->instance, TRUE);
        modelB->animate = TRUE;
        MimeJrTop_QuatIdentity(&modelB->rotation);
        MimeJrTop_QuatIdentity(&modelB->lastTurn);
        MimeJrTopModel_Turn(modelB, 0, 0, 5, 5, 0.8f, FALSE);
        MimeJrTopModel_Turn(modelB, 0, 0, -5, -5, 0.8f, FALSE);
        if (task->selection) {
            ModelInstance_SetPosition(&modelA->instance, 0, FX32_CONST(-8), 0);
            ModelInstance_SetPosition(&modelB->instance, 0, FX32_CONST(-8), 0);
            modelA->position.y = FX32_CONST(-8);
            modelB->position.y = FX32_CONST(-8);
        }
        task->state++;
        return;
    case MIME_JR_TOP_TIER_MODELS_WAIT_CHANGE:
        if (!modelB->animate) {
            ModelInstance_SetVisible(&modelA->instance, TRUE);
            ModelInstance_SetVisible(&modelB->instance, FALSE);
            modelA->animate = TRUE;
            task->state++;
        }
        return;
    case MIME_JR_TOP_TIER_MODELS_WATCH_TIER:
        if (!tier) {
            task->state = MIME_JR_TOP_TIER_MODELS_FREE;
        } else if (task->previous != tier) {
            FREE_TIER_MODELS();
            task->state = MIME_JR_TOP_TIER_MODELS_LOAD_TOP;
        }
        task->previous = *task->current;
        return;
    default:
        FREE_TIER_MODELS();
        task->active = FALSE;
        SysTask_Destroy(sysTask);
        break;
    }
}

#undef FREE_TIER_MODELS

static void MimeJrTopGame_AddTierScore(MimeJrTopGame *game) {
    MimeJrTopTierPoints points = gMimeJrTopTouchData.tierScores;

    game->score += points.values[game->tier];
}

static void MimeJrTopGame_AddTierBonus(MimeJrTopGame *game) {
    MimeJrTopTierPoints points = gMimeJrTopTouchData.tierBonus;
    u32 index = game->tier;

    index--;
    game->score += points.values[index];
}

// TRUE when the spin time reaches the next tier.
static BOOL MimeJrTopGame_UpdateTier(MimeJrTopGame *game) {
    switch (game->spinFrames) {
    case 119:
        game->phase = MIME_JR_TOP_STAGE_LIGHTS;
        break;
    case 269:
        game->phase = MIME_JR_TOP_STAGE_POPUPS;
        break;
    case 419:
        game->phase = MIME_JR_TOP_STAGE_AUDIENCE;
        break;
    case 569:
        game->phase = MIME_JR_TOP_STAGE_MORE_POPUPS;
        break;
    case 719:
        game->phase = MIME_JR_TOP_STAGE_STARS;
        break;
    case 0:
        game->phase = MIME_JR_TOP_STAGE_OFF;
        break;
    }
    return MimeJrTop_GetTier(game, game->spinFrames, &game->tier);
}

static BOOL MimeJrTop_GetTier(MimeJrTopGame *game, int spinFrames, u32 *tier) {
    switch (spinFrames) {
    case 299:
        *tier = 1;
        return TRUE;
    case 599:
        *tier = 2;
        return TRUE;
    case 899:
        *tier = 3;
        return TRUE;
    case 1199:
        *tier = 4;
        return TRUE;
    case 0:
        *tier = 0;
    }
    return FALSE;
}

// Follows the other players' states (sent with command 24).
static void MimeJrTopGame_UpdateRivals(MimeJrTopGame *game) {
    int count = game->playerCount, i;
    for (i = 0; i < count; i++) {
        if (i == game->localIndex) {
            continue;
        }
        switch (game->playerStates[i]) {
        case MIME_JR_TOP_PLAYER_SPINNING:
            if (MimeJrTopGame_GetRivalAnim(game, i) != 1) {
                MimeJrTopGame_SetRivalAnim(game, i, 1);
            }
            if (MimeJrTop_GetTier(game, game->rivalSpinFrames[i], &game->rivalTiers[i])) {
                MimeJrTopGame_StartRivalTier(game, i);
            }
            (game->rivalSpinFrames[i])++;
            break;
        case MIME_JR_TOP_PLAYER_FALLEN:
            game->levelTimer = 0;
            game->rivalSpinFrames[i] = 0;
            game->rivalTiers[i] = 0;
            game->dance.enabled = FALSE;
            if (MimeJrTopGame_GetRivalAnim(game, i) != 3 && !game->rivalFallShown) {
                MimeJrTopGame_SetRivalAnim(game, i, 3);
                MimeJrTopGame_StartRivalFall(game, i);
                game->rivalFallShown = TRUE;
            }
            break;
        case MIME_JR_TOP_PLAYER_GETTING_UP:
            game->rivalFallShown = FALSE;
            game->levelTimer = 0;
            game->rivalSpinFrames[i] = 0;
            game->rivalTiers[i] = 0;
            game->playerStates[i] = MIME_JR_TOP_PLAYER_SPINNING;
            break;
        }
    }
}

// The local player: spinning (0) until the top falls, fallen (1), getting back up (2, 3).
static void MimeJrTopGame_UpdateLocalPlayer(MimeJrTopGame *game) {
    u32 state = game->playerStates[game->localIndex];
    switch (state) {
    case MIME_JR_TOP_PLAYER_SPINNING:
        if (MimeJrTopGame_HasFallen(game, 0)) {
            game->recoverTimer = 0;
            MimeJrTopGame_StartFall(game);
            MimeJrTopGame_ResetMotion(game);
            game->playerStates[game->localIndex] = MIME_JR_TOP_PLAYER_FALLEN;
            Comm_SendCommand(MIME_JR_TOP_CMD_PLAYER_STATE, &game->playerStates[game->localIndex], sizeof(game->playerStates[0]));
            game->spinFrames = 0;
            game->levelTimer = 0;
            game->dance.enabled = FALSE;
        } else {
            MimeJrTopGame_UpdateMimeJrAnimation(game);
            MimeJrTopGame_HandleTouch(game);
            MimeJrTopGame_UpdateWobble(game);
            MimeJrTopGame_AddTierScore(game);
            game->rivalSpinFrames[game->localIndex]++;
        }
        break;
    case MIME_JR_TOP_PLAYER_FALLEN:
        game->recoverTimer++;
        if (game->recoverTimer >= 90) {
            game->playerStates[game->localIndex] = MIME_JR_TOP_PLAYER_GETTING_UP;
        }
        game->spinFrames = 0;
        game->levelTimer = 0;
        game->rivalSpinFrames[game->localIndex] = 0;
        break;
    case MIME_JR_TOP_PLAYER_GETTING_UP:
        if (game->recoverTimer) {
            game->spinFrames = 0;
            game->levelTimer = 0;
            game->recoverTimer = 0;
            game->rivalSpinFrames[game->localIndex] = 0;
            if (game->previousLevel != game->level) {
                game->level = game->previousLevel;
            }
            MimeJrTopGame_ResetBalance(game);
            Comm_SendCommand(MIME_JR_TOP_CMD_PLAYER_STATE, &game->playerStates[game->localIndex], sizeof(game->playerStates[0]));
        } else {
            game->playerStates[game->localIndex] = MIME_JR_TOP_PLAYER_SPINNING;
        }
        break;
    case MIME_JR_TOP_PLAYER_STEADYING:
        game->recoverTimer++;
        if ((u32)game->recoverTimer >= 29) {
            game->recoverTimer = 0;
            MimeJrTopGame_ResetBalance(game);
            game->playerStates[game->localIndex] = MIME_JR_TOP_PLAYER_SPINNING;
        }
        break;
    }
}

static void MimeJrTopGame_Update(MimeJrTopGame *game) {
    if (game->frame < MIME_JR_TOP_GAME_FRAMES) {
        MimeJrTopGame_UpdateLocalPlayer(game);
        MimeJrTopGame_UpdateRivals(game);
        if (MimeJrTopGame_UpdateTier(game) == TRUE) {
            MimeJrTopGame_StartTierModels(game, game->localIndex);
            MimeJrTopGame_AddTierBonus(game);
            PlaySE(SEQ_SE_PL_LV_UP);
        }
        MimeJrTopGame_UpdateLevel(game);
        game->spinFrames++;
        game->frame++;
        game->levelTimer++;
    }
}

// Rubbing the touch screen turns the top (and Mime Jr. with it), which tilts the balance.
static void MimeJrTopGame_HandleTouch(MimeJrTopGame *game) {
    VecFx32 spinAxis;
    VecFx32 turnAxis;
    u32 x, y;
    int hit;
    f32 lean;
    int tiltY;
    f32 mimeJrSpeed;
    System_GetTouchHeldCoords(&x, &y);
    game->touched = FALSE;
    if (game->selection) {
        hit = TouchscreenHitbox_FindHitboxAtPoint(gMimeJrTopSelectionHitboxes, x, y);
    } else {
        hit = TouchscreenHitbox_FindHitboxAtPoint(gMimeJrTopTouchData.hitboxes, x, y);
    }
    if (hit == -1) {
        game->lastTouchX = 0;
        game->lastTouchY = 0;
        if (game->top.spinFrames) {
            MimeJrTopModel_ContinueSpin(&game->top, 1.0f);
            MimeJrTopModel_ContinueSpin(&game->mimeJr[0], 0.5f);
            spinAxis = gMimeJrTopSpinTiltAxis;
            MTX_MultVec43(&spinAxis, &game->mimeJr[0].rotationFx, &game->tilt);
            game->touched = TRUE;
            return;
        }
        game->top.spinX = 0;
        game->top.spinY = 0;
        MimeJrTopModel_Turn(&game->top, game->lastTouchX, game->lastTouchY, game->lastTouchX, game->lastTouchY, 1.88f, FALSE);
        MimeJrTopModel_Turn(&game->mimeJr[0], game->lastTouchX, game->lastTouchY, game->lastTouchX, game->lastTouchY, 0.22f, FALSE);
        return;
    }
    tiltY = game->tilt.y >> FX32_SHIFT;
    lean = (100 - tiltY) / 40;
    if (tiltY >= 95) {
        lean = 0.1f;
    }
    if (lean >= 0.22) {
        mimeJrSpeed = 0.22f;
    } else {
        mimeJrSpeed = 0.22 - lean;
    }
    if (game->lastTouchX == 0) {
        game->lastTouchX = x;
        game->lastTouchY = y;
    }
    MimeJrTopModel_Turn(&game->top, x, y, game->lastTouchX, game->lastTouchY, 2.0f, TRUE);
    MimeJrTopModel_Turn(&game->mimeJr[0], x, y, game->lastTouchX, game->lastTouchY, mimeJrSpeed, TRUE);
    turnAxis = gMimeJrTopTurnTiltAxis;
    MTX_MultVec43(&turnAxis, &game->mimeJr[0].rotationFx, &game->tilt);
    game->touched = TRUE;
    if (!game->everTouched) {
        game->everTouched = TRUE;
    }
    if (x != 0) {
        if (!IsSEPlaying(SEQ_SE_PL_KIRAKIRA)) {
            PlaySE(SEQ_SE_PL_KIRAKIRA);
        }
        ((MimeJrTopStartBurstFunc)MimeJrTopGame_StartBurst)(game, x, y);
    }
    game->lastTouchX = x;
    game->lastTouchY = y;
}

static void MimeJrTopGame_ResetMotion(MimeJrTopGame *game) {
    game->wobbleTime = 0;
    game->wobbleTick = 0;
    game->touched = FALSE;
    game->settled = FALSE;
    game->wobbleDue = FALSE;
    game->wobbling = FALSE;
    game->tier = 0;
}

// The level rises every 149 frames the players keep their tops spinning (up to 8).
static void MimeJrTopGame_UpdateLevel(MimeJrTopGame *game) {
    if (game->levelTimer >= 0x95 && game->level != MIME_JR_TOP_MAX_LEVEL) {
        game->previousLevel = game->level;
        game->level++;
        game->levelTimer = 0;
        game->dance.enabled = TRUE;
    }
}

// Random kicks that tip the top over; harder the higher the tier.
static void MimeJrTopGame_UpdateWobble(MimeJrTopGame *game) {
    MimeJrTopWobbleIntervals intervals;
    VecFx32 wobbleAxis;
    VecFx32 settleAxis;
    VecFx32 settleTouchAxis;
    s16 x;
    s32 y;
    s32 oldX;
    s32 oldY;
    BOOL kickXDone;
    BOOL kickYDone;
    int i;
    s32 total;
    int rnd;
    u32 seed;
    s32 speed;
    s16 dx, dz;
    s16 sx, sy;
    s16 vx, vy;

    intervals = gMimeJrTopTouchData.wobbleIntervals;
    game->wobbleTick++;
    if (game->wobbleTime != 0) {
        if (game->wobbleStep < 9) {
            if (game->wobbleStep <= 8) {
                oldX = game->kickX.value;
                oldY = game->kickY.value;
                kickXDone = MimeJrTopTween_UpdateAccel(&game->kickX);
                kickYDone = MimeJrTopTween_UpdateAccel(&game->kickY);
                x = (game->kickX.value - oldX) >> 12;
                y = (s16)((game->kickY.value - oldY) >> 12);
                if (kickXDone == 1 || kickYDone == 1) {
                    game->kickX.value -= oldX;
                    game->kickY.value -= oldY;
                }
            } else {
                x = game->kickX.value >> FX32_SHIFT;
                y = (s16)(game->kickY.value >> FX32_SHIFT);
            }
        } else {
            int s1, s2;
            oldX = game->settleX.value;
            oldY = game->settleY.value;
            s1 = MimeJrTopTween_UpdateAccel(&game->settleX);
            s2 = MimeJrTopTween_UpdateAccel(&game->settleY);
            x = (game->settleX.value - oldX) >> 12;
            y = (s16)((game->settleY.value - oldY) >> 12);
            if ((s1 == 1 || s2 == 1) && !game->settled) {
                game->settled = TRUE;
                game->settleX.value -= oldX;
                game->settleY.value -= oldY;
            }
        }
        MimeJrTopModel_Turn(&game->top, 0, 0, x, y, 1.88f, FALSE);
        if (MimeJrTopModel_Turn(&game->mimeJr[0], 0, 0, x, y, 0.8f, FALSE)) {
            wobbleAxis = gMimeJrTopWobbleTiltAxis;
            MTX_MultVec43(&wobbleAxis, &game->mimeJr[0].rotationFx, &game->tilt);
        }
        game->wobbleStep++;
        game->wobbleTime--;
        return;
    }
    if (game->wobbleTick % intervals.values[game->tier] == 0) {
        game->wobbleTick = 0;
        game->wobbleDue = TRUE;
    }
    if (!game->touched && game->settled == TRUE) {
        y = (s16)(game->settleY.value >> FX32_SHIFT);
        x = game->settleX.value >> FX32_SHIFT;
        MimeJrTopModel_Turn(&game->top, 0, 0, x, y, 1.88f, FALSE);
        if (MimeJrTopModel_Turn(&game->mimeJr[0], 0, 0, x, y, 0.8f, FALSE)) {
            settleAxis = gMimeJrTopSettleTiltAxis;
            MTX_MultVec43(&settleAxis, &game->mimeJr[0].rotationFx, &game->tilt);
        }
    } else if (game->touched == TRUE && game->settled == TRUE) {
        y = (s16)(game->settleY.value >> FX32_SHIFT);
        x = game->settleX.value >> FX32_SHIFT;
        MimeJrTopModel_Turn(&game->top, 0, 0, x, y, 1.88f, FALSE);
        if (MimeJrTopModel_Turn(&game->mimeJr[0], 0, 0, x, y, 0.8f, FALSE)) {
            settleTouchAxis = gMimeJrTopSettleTouchTiltAxis;
            MTX_MultVec43(&settleTouchAxis, &game->mimeJr[0].rotationFx, &game->tilt);
        }
    }
    if (game->wobbling && (game->wobbleDue != TRUE || game->settled != TRUE)) {
        return;
    }
    // The random numbers must not disturb the shared sequence.
    seed = GetLCRNGSeed();
    rnd = LCRandom();
    SetLCRNGSeed(seed);
    rnd = (u32)rnd % 100;
    total = gMimeJrTopWobbleWeights[game->tier][0];
    for (i = 0; i < 3; i++) {
        if (rnd < total) {
            break;
        }
        total += gMimeJrTopWobbleChoices[game->tier][i + 1].weight;
    }
    speed = gMimeJrTopWobbleChoices[game->tier][i].speed;
    dx = game->tilt.x >> FX32_SHIFT;
    dz = game->tilt.z >> FX32_SHIFT;
    if (game->tilt.z == 0) {
        if (LCRandom() % 2) {
            sy = 1;
        } else {
            sy = -1;
        }
    } else {
        sy = -(dz >= 0 ? 1 : -1);
    }
    if (game->tilt.x == 0) {
        if (LCRandom() % 2) {
            sx = 1;
        } else {
            sx = -1;
        }
    } else {
        if (dx >= 0) {
            sx = 1;
        } else {
            sx = -1;
        }
        if (sy < 0) {
            sx *= -1;
        }
    }
    if (game->tier == 0) {
        sx = 0;
    }
    vx = speed * sx;
    vy = speed * sy;
    sx = 20 * sx;
    sy = 20 * sy;
    game->nextWobbleX = vx;
    game->nextWobbleY = vy;
    MimeJrTopTween_InitAccel(&game->kickX, 0, FX32_CONST(sx), 0, 8);
    MimeJrTopTween_InitAccel(&game->kickY, 0, FX32_CONST(sy), 0, 8);
    MimeJrTopTween_InitAccel(&game->settleX, 0, FX32_CONST(vx), 0, 12);
    MimeJrTopTween_InitAccel(&game->settleY, 0, FX32_CONST(vy), 0, 12);
    game->wobbleTime = 22;
    game->wobbleStep = 0;
    game->wobbleX = vx;
    game->wobbleY = vy;
    game->touched = FALSE;
    game->wobbleDue = FALSE;
    game->settled = FALSE;
    game->wobbling = TRUE;
}

// Turns the model by the drag from (targetX, targetY) to (x, y); with saveVelocity the drag also
// keeps turning it for 8 more frames (MimeJrTopModel_ContinueSpin).
static BOOL MimeJrTopModel_Turn(MimeJrTopModel *model, s32 x, s32 y, s32 targetX, s32 targetY, f32 scale, BOOL saveVelocity) {
    f32 dx = (f32)((double)(x - targetX) * 0.0051953125 * (double)scale);
    f32 dy = (f32)((double)(y - targetY) * 0.005208333333333333 * (double)scale);
    f32 magnitude;
    f32 scaled;
    fx32 angle;
    MimeJrTopQuat turn;
    f32 length;
    MimeJrTopQuat old;

    if (saveVelocity) {
        model->spinX = dx;
        model->spinY = dy;
        model->spinFrames = 8;
        model->spinStepX = model->spinX / (f32)model->spinFrames;
        model->spinStepY = model->spinY / (f32)model->spinFrames;
    }

    magnitude = dx * dx + dy * dy;
    if (magnitude > 0.0f) {
        scaled = 4096.0f * magnitude + 0.5f;
    } else {
        scaled = 4096.0f * magnitude - 0.5f;
    }
    angle = FX_Sqrt((fx32)scaled);
    magnitude = (f32)angle / 4096.0f;
    if (magnitude != 0.0) {
        s64 sinValue = FX_SinFx64c(angle);
        s64 cosValue = FX_CosFx64c(angle);
        f32 ratio = _f_lltof(sinValue) / 4294967296.0f;
        f32 cosine = _f_lltof(cosValue) / 4294967296.0f;
        ratio /= magnitude;
        turn.v[0] = cosine;
        turn.v[1] = dy * ratio;
        turn.v[2] = dx * ratio;
        turn.v[3] = 0.0f;
        MimeJrTop_QuatMul(model->rotation.v, turn.v, model->lastTurn.v);
        MimeJrTop_QuatToMatrix(&model->rotationMatrix, &model->rotation);
        MimeJrTop_QuatCopy(&model->lastTurn, &model->rotation);
        MimeJrTop_FloatMatrixToFx43(&model->rotationMatrix, &model->rotationFx);
        length = MimeJrTop_QuatLength(&model->rotation);
        old = model->rotation;
        MimeJrTop_QuatDivide(model->rotation.v, old.v, length);
        return TRUE;
    }
    return FALSE;
}

// One frame of the spin a flick left behind (slows down linearly).
static BOOL MimeJrTopModel_ContinueSpin(MimeJrTopModel *model, f32 unused) {
    f32 x = model->spinX;
    MimeJrTopQuat turn;
    f32 y = model->spinY;
    f32 magnitude;
    f32 scaled;
    fx32 angle;

    model->spinX = x - model->spinStepX;
    model->spinY = model->spinY - model->spinStepY;
    model->spinFrames--;

    magnitude = x * x + y * y;
    if (magnitude > 0.0f) {
        scaled = 4096.0f * magnitude + 0.5f;
    } else {
        scaled = 4096.0f * magnitude - 0.5f;
    }
    angle = FX_Sqrt((fx32)scaled);
    magnitude = (f32)angle / 4096.0f;
    if (magnitude != 0.0) {
        s64 sinValue = FX_SinFx64c(angle);
        s64 cosValue = FX_CosFx64c(angle);
        f32 ratio = _f_lltof(sinValue) / 4294967296.0f;
        f32 cosine = _f_lltof(cosValue) / 4294967296.0f;
        ratio /= magnitude;
        turn.v[0] = cosine;
        turn.v[1] = y * ratio;
        turn.v[2] = x * ratio;
        turn.v[3] = 0.0f;
        MimeJrTop_QuatMul(model->rotation.v, turn.v, model->lastTurn.v);
        MimeJrTop_QuatToMatrix(&model->rotationMatrix, &model->rotation);
        MimeJrTop_QuatCopy(&model->lastTurn, &model->rotation);
        MimeJrTop_FloatMatrixToFx43(&model->rotationMatrix, &model->rotationFx);
        return TRUE;
    }
    return FALSE;
}

// Index of netId in the player list, 0xFF if it is not playing.
static int MimeJrTop_GetPlayerIndex(MimeJrTopApp *app, u32 netId) {
    int i;
    for (i = 0; i < PLAZA_GAME_MAX_PLAYERS; i++) {
        if (netId != app->netIds[i]) {
            continue;
        } else {
            return i;
        }
    }
    return 0xFF;
}

static void MimeJrTopTween_InitAccel(MimeJrTopTween *tween, fx32 start, fx32 end, fx32 velocity, int frames) {
    fx32 accel = FX_Div(FX_Mul(end - start - FX_Mul(velocity, frames << FX32_SHIFT), FX32_CONST(2)), (frames * frames) << FX32_SHIFT);
    tween->value = start;
    tween->start = start;
    tween->velocity = velocity;
    tween->delta = accel;
    tween->frame = 0;
    tween->duration = frames;
}

static BOOL MimeJrTopTween_UpdateAccel(MimeJrTopTween *tween) {
    int t = tween->frame;
    fx32 a = FX_Mul(tween->velocity, t << FX32_SHIFT);
    fx32 b = FX_Mul(tween->delta, (t * t) << FX32_SHIFT);
    tween->value = tween->start + (a + FX_Div(b, FX32_CONST(2)));
    t = tween->frame;
    b = tween->duration;
    t++;
    if (t <= b) {
        tween->frame = t;
        return FALSE;
    }
    tween->frame = b;
    return TRUE;
}

static void MimeJrTopTween_Init(MimeJrTopTween *tween, fx32 start, fx32 end, int frames) {
    tween->value = start;
    tween->start = start;
    tween->delta = end - start;
    tween->frame = 0;
    tween->duration = frames;
}

static BOOL MimeJrTopTween_Update(MimeJrTopTween *tween) {
    BOOL finished;
    s32 frame;
    s32 value;

    if (tween->frame >= tween->duration) {
        frame = tween->duration;
        finished = TRUE;
    } else {
        frame = tween->frame;
        tween->frame++;
        finished = FALSE;
    }
    value = FX_Div(FX_Mul(tween->delta, frame << FX32_SHIFT), tween->duration << FX32_SHIFT);
    value += tween->start;
    tween->value = value;
    return finished;
}

static void MimeJrTopBurst_Task(SysTask *sysTask, MimeJrTopBurst *burst) {
    BOOL done[4][2];
    MimeJrTopOffsets xOffsets;
    MimeJrTopOffsets yOffsets;
    fx32 x;
    fx32 y;
    BOOL allDone;
    int i;
    MimeJrTopTween *xTween1;

    allDone = TRUE;
    xOffsets = gMimeJrTopBurstOffsetsX;
    yOffsets = gMimeJrTopBurstOffsetsY;

    if (!IsPaletteFadeFinished() || burst->resources->connectionLost == TRUE) {
        for (i = 0; i < 3; i++) {
            Sprite_DeleteAndFreeResources(burst->sprites[i]);
        }
        burst->active = FALSE;
        SysTask_Destroy(sysTask);
        return;
    }

    switch (burst->state) {
    case 0: {
        MimeJrTopBurst *cur;
        const s32 *xOffset;
        MimeJrTopTween *xTween;
        const s32 *yOffset;
        MimeJrTopTween *yTween;
        i = 0;
        cur = burst;
        xOffset = xOffsets.values;
        xTween = &burst->tweens[0][0];
        yOffset = yOffsets.values;
        yTween = &burst->tweens[0][1];
        for (; i < 3; i++) {
            ManagedSprite_GetSpritePositionFxXY(cur->sprites[0], &x, &y);
            MimeJrTopTween_InitAccel(xTween, x, x + *xOffset, FX32_CONST(0.3), 8);
            MimeJrTopTween_InitAccel(yTween, y, y + *yOffset, FX32_CONST(0.2), 8);
            cur = (MimeJrTopBurst *)((u8 *)cur + 4);
            xOffset++;
            xTween += 2;
            yOffset++;
            yTween += 2;
        }
        burst->state++;
        return;
    }
    case 1: {
        MimeJrTopTween *yTween;
        BOOL *donep;
        MimeJrTopBurst *cur;
        MimeJrTopBurst *values;
        i = 0;
        xTween1 = &burst->tweens[0][0];
        donep = &done[0][0];
        yTween = &burst->tweens[0][1];
        values = burst;
        cur = values;
        for (; i < 3; i++) {
            donep[0] = MimeJrTopTween_UpdateAccel(xTween1);
            donep[1] = MimeJrTopTween_UpdateAccel(yTween);
            ManagedSprite_SetPositonFxXY(cur->sprites[0], values->tweens[0][0].value, values->tweens[0][1].value);
            if (!donep[0] || !donep[1]) {
                allDone = FALSE;
            }
            ManagedSprite_TickFrame(cur->sprites[0]);
            xTween1 += 2;
            donep += 2;
            yTween += 2;
            values = (MimeJrTopBurst *)((u8 *)values + 0x30);
            cur = (MimeJrTopBurst *)((u8 *)cur + 4);
        }
        if (allDone) {
            burst->state++;
        }
        break;
    }
    default:
        for (i = 0; i < 3; i++) {
            Sprite_DeleteAndFreeResources(burst->sprites[i]);
        }
        burst->active = FALSE;
        SysTask_Destroy(sysTask);
        break;
    }
}

// Sparks where the screen is rubbed (up to 8 bursts at a time).
static void MimeJrTopGame_StartBurst(MimeJrTopGame *game, u16 x, u16 y) {
    int slot;
    for (slot = 0; slot < 8; slot++) {
        if (game->bursts[slot].active != TRUE) {
            int i;
            game->bursts[slot].resources = game->resources;
            game->bursts[slot].state = 0;
            game->bursts[slot].active = TRUE;
            for (i = 0; i < 3; i++) {
                game->bursts[slot].sprites[i] = MimeJrTopGame_NewSpark(game, x, y);
                ManagedSprite_SetAnim(game->bursts[slot].sprites[i], (x * (slot + 1) + (i + 1) * y) % 3);
            }
            SysTask_CreateOnMainQueue((SysTaskFunc)MimeJrTopBurst_Task, &game->bursts[slot], 1 << 12);
            return;
        }
    }
}

// The star shower of stage effect 4: 36 stars burst from one of five points, then fall.
static void MimeJrTopStars_Task(SysTask *sysTask, MimeJrTopStarsTask *task) {
    MimeJrTopStarOrigins origins;
    s32 startX;
    s32 startY;
    s32 j;
    s32 x;
    s32 y;
    int i;
    MimeJrTopResources *resources = task->game->resources;

    if (!IsPaletteFadeFinished() || resources->connectionLost == TRUE) {
        *task->stop = 0;
        SysTask_Destroy(sysTask);
        Heap_Free(task);
        return;
    }

    switch (task->state) {
    case 0:
        task->group = 0;
        task->state++;
    case 1:
        origins = gMimeJrTopStarOrigins;
        i = 0;
        startX = origins.values[task->group * 2];
        startY = origins.values[task->group * 2 + 1];
        task->timer = i;
        x = startX << 12;
        y = startY << 12;
        for (; i < 36; i++) {
            int rem;
            task->stars[i].delay = i % 2;
            rem = i % 3;
            task->stars[i].radiusX = 6 - rem;
            task->stars[i].radiusY = 6 - rem;
            task->stars[i].sprite = task->game->stars[i];
            task->stars[i].angle = (i / 3) * 30;
            task->stars[i].angle %= 360;
            task->stars[i].fallSpeed = 0;
            task->stars[i].timer = -(rem * 2);
            {
                f32 rounded;
                if (startX) {
                    rounded = (f32)(u32)x + 0.5f;
                } else {
                    rounded = (f32)(u32)x - 0.5f;
                }
                task->stars[i].originX = (s32)rounded;
            }
            {
                f32 rounded;
                if (startY) {
                    rounded = (f32)(u32)y + 0.5f;
                } else {
                    rounded = (f32)(u32)y - 0.5f;
                }
                task->stars[i].originY = (s32)rounded;
            }
            task->stars[i].circleX = 0;
            task->stars[i].circleY = 0;
            task->stars[i].driftX = 0;
            task->stars[i].driftY = 0;
            task->stars[i].jitterX = 0;
            task->stars[i].jitterY = 0;
            task->stars[i].type = 0;
            task->stars[i].visible = TRUE;
            task->stars[i].jitterPhase = 0;
            if (task->stars[i].angle == 180) {
                task->stars[i].type = 2;
            }
            if (task->stars[i].angle >= 270 && task->stars[i].angle <= 90) {
                task->stars[i].type = 1;
            }
            {
                s32 radius = task->stars[i].radiusX;
                f32 rounded;
                s32 amp;
                if (radius > 0) {
                    rounded = (f32)(radius << 12) + 0.5f;
                } else {
                    rounded = (f32)(radius << 12) - 0.5f;
                }
                amp = (s32)rounded;
                task->stars[i].circleX = FX_Mul(GF_SinDeg((u16)task->stars[i].angle), amp);
            }
            {
                s32 radius = task->stars[i].radiusY;
                f32 rounded;
                s32 amp;
                if (radius > 0) {
                    rounded = (f32)(radius << 12) + 0.5f;
                } else {
                    rounded = (f32)(radius << 12) - 0.5f;
                }
                amp = (s32)rounded;
                task->stars[i].circleY = FX_Mul(GF_CosDeg((u16)task->stars[i].angle), amp);
            }
            {
                s32 fallSpeed = task->stars[i].fallSpeed;
                f32 rounded;
                if (fallSpeed > 0) {
                    rounded = (f32)(fallSpeed << 12) + 0.5f;
                } else {
                    rounded = (f32)(fallSpeed << 12) - 0.5f;
                }
                task->stars[i].driftY += (s32)rounded;
            }
            ManagedSprite_SetPositionFxXYWithSubscreenOffset(task->stars[i].sprite,
                task->stars[i].jitterX + task->stars[i].originX + task->stars[i].circleX + task->stars[i].driftX,
                task->stars[i].jitterY + task->stars[i].originY + task->stars[i].circleY + task->stars[i].driftY,
                14 << 16);
        }
        task->state++;
        break;
    case 2:
        if (task->timer == 0) {
            task->timer++;
            MimeJrTopGame_SetStarsVisible(task->game, TRUE, 0);
            PlaySE(SEQ_SE_PL_140_2_2);
        }
        if (task->timer > 30) {
            task->state = 1;
            task->group++;
            task->group %= 5;
            MimeJrTopGame_SetStarsVisible(task->game, FALSE, 1);
        } else {
            task->timer++;
            j = 0;
            for (; j < 36; j++) {
                task->stars[j].timer++;
                if (task->stars[j].timer < 0) {
                    continue;
                }
                if (task->stars[j].timer >= task->stars[j].delay + 10) {
                    if (task->stars[j].timer == task->stars[j].delay + 10) {
                        ManagedSprite_SetAnim(task->stars[j].sprite, 0);
                    }
                    if (task->stars[j].timer == task->stars[j].delay + 18) {
                        task->stars[j].visible = FALSE;
                    } else if ((task->stars[j].timer % 2) != 0) {
                        task->stars[j].visible ^= 1;
                    }
                    ManagedSprite_SetDrawFlag(task->stars[j].sprite, task->stars[j].visible);
                    switch (task->stars[j].jitterPhase) {
                    case 0:
                        task->stars[j].jitterX = -FX32_ONE;
                        break;
                    case 1:
                        task->stars[j].jitterX = 0;
                        break;
                    case 2:
                        task->stars[j].jitterX = FX32_ONE;
                        break;
                    }
                    task->stars[j].jitterPhase++;
                    task->stars[j].jitterPhase %= 3;
                }
                s16 radialX;
                s16 radialY;
                if (task->stars[j].timer > 5) {
                    switch (task->stars[j].type) {
                    case 2:
                        task->stars[j].fallSpeed = (task->stars[j].timer + 5) / 6;
                        break;
                    case 1:
                        task->stars[j].fallSpeed = (task->stars[j].timer + 5) / 6;
                        break;
                    case 0:
                        task->stars[j].fallSpeed = (task->stars[j].timer + 5) / 7;
                        break;
                    }
                }
                switch (task->stars[j].type) {
                case 2:
                    radialX = ((30 - (task->stars[j].timer + 5)) / 8);
                    radialY = ((30 - (task->stars[j].timer + 7)) / 8);
                    break;
                case 1:
                default:
                    radialX = ((30 - (task->stars[j].timer + 5)) / 8);
                    radialY = ((30 - (task->stars[j].timer + 6)) / 8);
                    break;
                case 0:
                    radialX = ((30 - (task->stars[j].timer + 5)) / 8);
                    radialY = ((30 - (task->stars[j].timer + 4)) / 8);
                    break;
                }
                if (radialX > 0) {
                    task->stars[j].radiusX += radialX;
                } else {
                    task->stars[j].radiusX++;
                }
                if (radialY > 0) {
                    task->stars[j].radiusY += radialY;
                } else {
                    task->stars[j].radiusY++;
                }
                {
                    s32 radius = task->stars[j].radiusX;
                    f32 rounded;
                    s32 amp;
                    if (radius > 0) {
                        rounded = (f32)(radius << 12) + 0.5f;
                    } else {
                        rounded = (f32)(radius << 12) - 0.5f;
                    }
                    amp = (s32)rounded;
                    task->stars[j].circleX = FX_Mul(GF_SinDeg((u16)task->stars[j].angle), amp);
                }
                {
                    s32 radius = task->stars[j].radiusY;
                    f32 rounded;
                    s32 amp;
                    if (radius > 0) {
                        rounded = (f32)(radius << 12) + 0.5f;
                    } else {
                        rounded = (f32)(radius << 12) - 0.5f;
                    }
                    amp = (s32)rounded;
                    task->stars[j].circleY = FX_Mul(GF_CosDeg((u16)task->stars[j].angle), amp);
                }
                {
                    s32 fallSpeed = task->stars[j].fallSpeed;
                    f32 rounded;
                    if (fallSpeed > 0) {
                        rounded = (f32)(fallSpeed << 12) + 0.5f;
                    } else {
                        rounded = (f32)(fallSpeed << 12) - 0.5f;
                    }
                    task->stars[j].driftY += (s32)rounded;
                }
                ManagedSprite_SetPositionFxXYWithSubscreenOffset(task->stars[j].sprite,
                    task->stars[j].jitterX + task->stars[j].originX + task->stars[j].circleX + task->stars[j].driftX,
                    task->stars[j].jitterY + task->stars[j].originY + task->stars[j].circleY + task->stars[j].driftY,
                    14 << 16);
            }
        }
        if (*task->stop != 0) {
            task->state++;
        }
        break;
    default:
        MimeJrTopGame_SetStarsVisible(task->game, FALSE, 0);
        Heap_Free(task);
        SysTask_Destroy(sysTask);
        break;
    }
    MimeJrTopGame_UpdateStars(task->game);
}

static void MimeJrTopGame_StartStars(MimeJrTopGame *game, s32 *stop) {
    MimeJrTopStarsTask *task = Heap_Alloc(HEAP_ID_113, sizeof(MimeJrTopStarsTask));
    task->game = game;
    task->state = 0;
    task->timer = 0;
    task->stop = stop;
    SysTask_CreateOnMainQueue((SysTaskFunc)MimeJrTopStars_Task, task, 0x1000);
}

// Effect 0: the stage flashes (palette blend) and its model shows.
static void MimeJrTopStage_UpdateFlash(MimeJrTopStageEffect *effect) {
    MimeJrTopModel *stage = effect->targets[0];
    s32 *level = effect->targets[1];
    s32 *base = effect->targets[3];
    PaletteData *palette = effect->targets[2];

    if (effect->active) {
        switch (effect->state) {
        case MIME_JR_TOP_FLASH_BRIGHTEN:
            if (*level < 4) {
                (*level)++;
            } else {
                *level = 4;
                effect->state++;
            }
            PaletteData_BlendPalettes(palette, PLTTBUF_MAIN_BG, 1, *level + *base, RGB_BLACK);
            PaletteData_BlendPalettes(palette, PLTTBUF_SUB_BG, 1, *level + *base, RGB_BLACK);
            break;
        case MIME_JR_TOP_FLASH_SHOW_STAGE:
            ModelInstance_SetVisible(&stage->instance, TRUE);
            effect->timer = 0;
            effect->state++;
            break;
        case MIME_JR_TOP_FLASH_HOLD:
            if (effect->stop == TRUE && ++effect->timer >= 15) {
                effect->state++;
            }
            break;
        case MIME_JR_TOP_FLASH_DARKEN:
            if (*level > 0) {
                (*level)--;
            } else {
                *level = 0;
                effect->state++;
            }
            PaletteData_BlendPalettes(palette, PLTTBUF_MAIN_BG, 1, *level + *base, RGB_BLACK);
            PaletteData_BlendPalettes(palette, PLTTBUF_SUB_BG, 1, *level + *base, RGB_BLACK);
            break;
        default:
            effect->stop = FALSE;
            effect->active = FALSE;
            effect->state = MIME_JR_TOP_FLASH_BRIGHTEN;
            ModelInstance_SetVisible(&stage->instance, FALSE);
            break;
        }
    }
}

// Effects 1 and 3: two pop-up sprites rise from the bottom edge and sink back.
static void MimeJrTopStage_UpdatePopups(MimeJrTopStageEffect *effect) {
    s16 limit[4] = { gMimeJrTopPopupLimits[0], gMimeJrTopPopupLimits[1], gMimeJrTopPopupLimits[2], gMimeJrTopPopupLimits[3] };
    MimeJrTopDoneFlags done;
    s16 x, y;
    s16 *limitp = limit;
    int i;
    ManagedSprite *sprite;

    done = gMimeJrTopPopupDoneFlags;

    if (effect->active) {
        switch (effect->state) {
        case MIME_JR_TOP_POPUPS_RISE:
            for (i = 0; i < 4; i++) {
                sprite = effect->targets[i];
                if (sprite != NULL) {
                    ManagedSprite_GetPositionXY(sprite, &x, &y);
                    if (y > *limitp) {
                        y -= 6;
                        ManagedSprite_SetPositionXY(sprite, x, y);
                        done.done[i] = FALSE;
                    }
                }
                limitp++;
            }
            if (done.done[0] && done.done[1] && done.done[2] && done.done[2]) {
                effect->timer = 0;
                effect->stop = FALSE;
                effect->state++;
            }
            break;
        case MIME_JR_TOP_POPUPS_HOLD:
            if (effect->stop == TRUE && ++effect->timer >= 15) {
                effect->state++;
            }
            break;
        case MIME_JR_TOP_POPUPS_SINK:
            for (i = 0; i < 4; i++) {
                sprite = effect->targets[i];
                if (sprite != NULL) {
                    ManagedSprite_GetPositionXY(sprite, &x, &y);
                    if (y < 0xE0) {
                        y += 6;
                        ManagedSprite_SetPositionXY(sprite, x, y);
                        done.done[i] = FALSE;
                    }
                }
            }
            if (done.done[0] && done.done[1] && done.done[2] && done.done[2]) {
                effect->state++;
            }
            break;
        default:
            effect->stop = FALSE;
            effect->active = FALSE;
            effect->state = MIME_JR_TOP_POPUPS_RISE;
            break;
        }
        for (i = 0; i < 4; i++) {
            if (effect->targets[i] != NULL) {
                ManagedSprite_TickTwoFrames(effect->targets[i]);
            }
        }
    }
}

// Effect 2: the audience shows up.
static void MimeJrTopStage_UpdateAudience(MimeJrTopStageEffect *effect) {
    int i;
    int frame;
    MimeJrTopModel *model;

    if (effect->active) {
        switch (effect->state) {
        case MIME_JR_TOP_AUDIENCE_SHOW:
            for (i = 0; i < 8; i++) {
                model = effect->targets[i];
                frame = i * 20;
                ModelAnimation_SetFrame(&model->animations[0], frame % 40);
                ModelInstance_SetVisible(&model->instance, TRUE);
            }
            effect->audienceTimer = 0;
            effect->state++;
            break;
        case MIME_JR_TOP_AUDIENCE_HOLD:
            if (effect->stop == TRUE && ++effect->audienceTimer >= 15) {
                effect->state++;
            }
            break;
        default:
            for (i = 0; i < 8; i++) {
                ModelInstance_SetVisible(&((MimeJrTopModel *)effect->targets[i])->instance, FALSE);
            }
            effect->stop = FALSE;
            effect->active = FALSE;
            effect->state = MIME_JR_TOP_AUDIENCE_SHOW;
            break;
        }
    }
}

// Effect 4: the star shower.
static void MimeJrTopStage_UpdateStars(MimeJrTopStageEffect *effect) {
    if (effect->active) {
        switch (effect->state) {
        case MIME_JR_TOP_STARS_START:
            MimeJrTopGame_StartStars(effect->targets[0], (s32 *)&effect->starsRunning);
            effect->timer = 0;
            effect->state++;
            break;
        case MIME_JR_TOP_STARS_HOLD:
            if (effect->stop == TRUE && ++effect->timer >= 15) {
                effect->starsRunning = TRUE;
                effect->state++;
            }
            break;
        default:
            if (!effect->starsRunning) {
                effect->stop = FALSE;
                effect->active = FALSE;
                effect->state = MIME_JR_TOP_STARS_START;
            }
            break;
        }
    }
}

// Each phase adds one more effect; when the phase drops (the top fell) all of them stop.
static void MimeJrTopStage_Task(SysTask *sysTask, void *data) {
    MimeJrTopStage *stage = data;
    MimeJrTopResources *resources = stage->resources;
    if (!IsPaletteFadeFinished() || resources->connectionLost == TRUE) {
        SysTask_Destroy(sysTask);
        return;
    }
    switch (*stage->phase) {
    case MIME_JR_TOP_STAGE_STARS:
        if (!stage->effects[4].active) {
            stage->effects[4].active = TRUE;
            stage->effects[4].stop = FALSE;
        }
    case MIME_JR_TOP_STAGE_MORE_POPUPS:
        if (!stage->effects[3].active) {
            PlaySE(SEQ_SE_DP_DENDOU);
            stage->effects[3].active = TRUE;
            stage->effects[3].stop = FALSE;
        }
    case MIME_JR_TOP_STAGE_AUDIENCE:
        if (!stage->effects[2].active) {
            stage->effects[2].active = TRUE;
            stage->effects[2].stop = FALSE;
        }
    case MIME_JR_TOP_STAGE_POPUPS:
        if (!stage->effects[1].active) {
            PlaySE(SEQ_SE_PL_HAND);
            stage->effects[1].active = TRUE;
            stage->effects[1].stop = FALSE;
        }
    case MIME_JR_TOP_STAGE_LIGHTS:
        if (!stage->effects[0].active) {
            stage->effects[0].active = TRUE;
            stage->effects[0].stop = FALSE;
        }
        break;
    default:
        if (*stage->phase != stage->previousPhase) {
            stage->effects[0].stop = TRUE;
            stage->effects[1].stop = TRUE;
            stage->effects[2].stop = TRUE;
            stage->effects[3].stop = TRUE;
            stage->effects[4].stop = TRUE;
            StopSE(SEQ_SE_PL_HAND, 0);
        }
        break;
    }
    MimeJrTopStage_UpdateFlash(&stage->effects[0]);
    MimeJrTopStage_UpdatePopups(&stage->effects[1]);
    MimeJrTopStage_UpdateAudience(&stage->effects[2]);
    MimeJrTopStage_UpdatePopups(&stage->effects[3]);
    MimeJrTopStage_UpdateStars(&stage->effects[4]);
    stage->previousPhase = *stage->phase;
}

static void MimeJrTopGame_StartStage(MimeJrTopGame *game) {
    int i, j;

    struct {
        s16 y, x;
    } a, b;

    MimeJrTopStageEffect *effect;
    game->stageEffects.resources = game->resources;
    game->stageEffects.phase = &game->phase;
    game->stageEffects.previousPhase = *game->stageEffects.phase;
    for (i = 0, effect = game->stageEffects.effects; i < 5; i++, effect++) {
        effect->state = 0;
        effect->active = FALSE;
        switch (i) {
        case 0:
            effect->targets[0] = &game->stage;
            effect->targets[1] = &game->flashBlend;
            effect->targets[3] = &game->danceBlend;
            effect->targets[2] = game->resources->palette;
            break;
        case 1:
            effect->targets[0] = game->popups[0];
            effect->targets[1] = game->popups[1];
            effect->targets[2] = NULL;
            effect->targets[3] = NULL;
            ManagedSprite_GetPositionXY(game->popups[0], &a.x, &a.y);
            ManagedSprite_SetPositionXY(game->popups[0], a.x, 224);
            ManagedSprite_GetPositionXY(game->popups[1], &a.x, &a.y);
            ManagedSprite_SetPositionXY(game->popups[1], a.x, 224);
            break;
        case 2:
            for (j = 0; j < 8; j++) {
                effect->targets[j] = &game->audience[j];
            }
            break;
        case 3:
            effect->targets[0] = NULL;
            effect->targets[1] = NULL;
            effect->targets[2] = game->popups[2];
            effect->targets[3] = game->popups[3];
            ManagedSprite_GetPositionXY(game->popups[2], &b.x, &b.y);
            ManagedSprite_SetPositionXY(game->popups[2], b.x, 224);
            ManagedSprite_GetPositionXY(game->popups[3], &b.x, &b.y);
            ManagedSprite_SetPositionXY(game->popups[3], b.x, 224);
            break;
        case 4:
            effect->targets[0] = game;
            break;
        }
    }
    game->stageEffects.task = SysTask_CreateOnMainQueue(MimeJrTopStage_Task, &game->stageEffects, 4096);
}

// The sub screen BG layers slide apart for the dance.
static void MimeJrTopDance_SlideIn(MimeJrTopDance *dance) {
    s32 *base = dance->flashBlend;
    s32 *blend = dance->danceBlend;
    BOOL done[5];
    int i;
    switch (dance->slideState) {
    case MIME_JR_TOP_SLIDE_START:
        MimeJrTopTween_Init(&dance->slides[0], 0, FX32_CONST(40), 10);
        MimeJrTopTween_Init(&dance->slides[1], 0, FX32_CONST(80), 10);
        MimeJrTopTween_Init(&dance->slides[2], 0, FX32_CONST(-80), 10);
        dance->slideState++;
        break;
    case MIME_JR_TOP_SLIDE_MOVE:
        for (i = 0; i < 3; i++) {
            done[i] = MimeJrTopTween_Update(&dance->slides[i]);
        }
        ScheduleSetBgPosText(dance->resources->bgConfig, GF_BG_LYR_SUB_3, BG_POS_OP_SET_Y, dance->slides[0].value >> FX32_SHIFT);
        ScheduleSetBgPosText(dance->resources->bgConfig, GF_BG_LYR_SUB_1, BG_POS_OP_SET_X, dance->slides[1].value >> FX32_SHIFT);
        ScheduleSetBgPosText(dance->resources->bgConfig, GF_BG_LYR_SUB_2, BG_POS_OP_SET_X, dance->slides[2].value >> FX32_SHIFT);
        ScheduleSetBgPosText(dance->resources->bgConfig, GF_BG_LYR_MAIN_1, BG_POS_OP_SET_X, dance->slides[1].value >> FX32_SHIFT);
        ScheduleSetBgPosText(dance->resources->bgConfig, GF_BG_LYR_MAIN_2, BG_POS_OP_SET_X, dance->slides[2].value >> FX32_SHIFT);
        if (done[0] && done[1] && done[2]) {
            dance->slideState++;
        }
        break;
    case MIME_JR_TOP_SLIDE_DONE:
        dance->shown = TRUE;
        dance->slideState = MIME_JR_TOP_SLIDE_START;
        break;
    }
    if (*blend < 6) {
        (*blend)++;
    } else {
        *blend = 6;
    }
    PaletteData_BlendPalettes(dance->resources->palette, PLTTBUF_MAIN_BG, 1, *base + *blend, RGB_BLACK);
    PaletteData_BlendPalettes(dance->resources->palette, PLTTBUF_SUB_BG, 1, *base + *blend, RGB_BLACK);
}

static void MimeJrTopDance_SlideOut(MimeJrTopDance *dance) {
    s32 *base = dance->flashBlend;
    s32 *blend = dance->danceBlend;
    BOOL done[5];
    int i;
    switch (dance->slideState) {
    case MIME_JR_TOP_SLIDE_START:
        MimeJrTopTween_Init(&dance->slides[0], FX32_CONST(40), 0, 10);
        MimeJrTopTween_Init(&dance->slides[1], FX32_CONST(80), 0, 10);
        MimeJrTopTween_Init(&dance->slides[2], FX32_CONST(-80), 0, 10);
        dance->slideState++;
        break;
    case MIME_JR_TOP_SLIDE_MOVE:
        for (i = 0; i < 3; i++) {
            done[i] = MimeJrTopTween_Update(&dance->slides[i]);
        }
        ScheduleSetBgPosText(dance->resources->bgConfig, GF_BG_LYR_SUB_3, BG_POS_OP_SET_Y, dance->slides[0].value >> FX32_SHIFT);
        ScheduleSetBgPosText(dance->resources->bgConfig, GF_BG_LYR_SUB_1, BG_POS_OP_SET_X, dance->slides[1].value >> FX32_SHIFT);
        ScheduleSetBgPosText(dance->resources->bgConfig, GF_BG_LYR_SUB_2, BG_POS_OP_SET_X, dance->slides[2].value >> FX32_SHIFT);
        ScheduleSetBgPosText(dance->resources->bgConfig, GF_BG_LYR_MAIN_1, BG_POS_OP_SET_X, dance->slides[1].value >> FX32_SHIFT);
        ScheduleSetBgPosText(dance->resources->bgConfig, GF_BG_LYR_MAIN_2, BG_POS_OP_SET_X, dance->slides[2].value >> FX32_SHIFT);
        if (done[0] && done[1] && done[2]) {
            dance->slideState++;
        }
        break;
    case MIME_JR_TOP_SLIDE_DONE:
        dance->shown = FALSE;
        dance->slideState = MIME_JR_TOP_SLIDE_START;
        break;
    }
    if (*blend > 4) {
        (*blend)--;
    } else {
        *blend = 0;
    }
    PaletteData_BlendPalettes(dance->resources->palette, PLTTBUF_MAIN_BG, 1, *base + *blend, RGB_BLACK);
    PaletteData_BlendPalettes(dance->resources->palette, PLTTBUF_SUB_BG, 1, *base + *blend, RGB_BLACK);
}

// Frames per dance step for a speed value.
static int MimeJrTop_GetDanceStepFrames(int speed) {
    switch (speed) {
    case 1:
        return 28;
    case 2:
        return 14;
    case 3:
        return 12;
    case 4:
        return 10;
    case 5:
        return 9;
    case 6:
        return 32;
    case 7:
        return 14;
    case 8:
        return 12;
    case 9:
        return 11;
    case 10:
        return 11;
    default:
        return 9;
    }
}

// Starts the routine of the current level whenever the previous one has run out.
static void MimeJrTopDance_Task(SysTask *sysTask, void *data) {
    MimeJrTopDance *dance = data;
    s32 level = *dance->level;
    int i;
    if (!IsPaletteFadeFinished() || dance->resources->connectionLost == TRUE) {
        SysTask_Destroy(sysTask);
        return;
    }
    if (dance->enabled) {
        if (!dance->shown) {
            MimeJrTopDance_SlideIn(dance);
        }
        if (dance->timer == 0 && level != 0 && level < MIME_JR_TOP_MAX_LEVEL) {
            dance->timer = gMimeJrTopRoutines[level].timer;
            dance->routineState = 0;
            dance->count = gMimeJrTopRoutineCounts[level].value - 1;
            dance->backSpeed = gMimeJrTopRoutineSpeeds[level].value;
            dance->frontSpeed = gMimeJrTopRoutineSpeeds[level].value + 5;
            for (i = 0; i < 2; i++) {
                ((MimeJrTopSetPaletteOffsetFunc)ManagedSprite_SetPaletteOverrideOffset)(dance->dancers[i].sprite, gMimeJrTopRoutines[level].frontPalettes[i] + 4);
                ((MimeJrTopSetPaletteOffsetFunc)ManagedSprite_SetPaletteOverrideOffset)(dance->dancers[2 + i].sprite, gMimeJrTopRoutines[level].backPalettes[i]);
            }
            SysTask_CreateOnMainQueue(gMimeJrTopRoutineTasks[level].func, dance, 0x1001);
        }
    } else if (!dance->enabled && dance->timer == 0 && dance->shown == TRUE) {
        MimeJrTopDance_SlideOut(dance);
    }
    if (dance->timer) {
        dance->timer--;
    }
}

static void MimeJrTopGame_StartDance(MimeJrTopGame *game) {
    int i;
    game->dance.level = &game->level;
    game->dance.flashBlend = &game->flashBlend;
    game->dance.danceBlend = &game->danceBlend;
    game->dance.resources = game->resources;
    game->dance.timer = 0;
    game->dance.slideState = MIME_JR_TOP_SLIDE_START;
    game->dance.enabled = FALSE;
    for (i = 0; i < 2; i++) {
        game->dance.dancers[i].sprite = game->frontDancers[i];
        game->dance.dancers[i + 2].sprite = game->backDancers[i];
    }
    game->dance.task = SysTask_CreateOnMainQueue(MimeJrTopDance_Task, &game->dance, 1 << 12);
}

// Routine of level 1: the two dancers circle in from opposite sides.
void MimeJrTopDance_Routine1(SysTask *sysTask, MimeJrTopDance *dance) {
    int x, y;
    int ax, ay, bx, by, a, b;
    if (!IsPaletteFadeFinished() || dance->resources->connectionLost == TRUE) {
        SysTask_Destroy(sysTask);
        return;
    }
    switch (dance->routineState) {
    case 0:
        dance->dancers[0].angle = 0;
        dance->dancers[0].baseX = 128;
        dance->dancers[0].baseY = 160;
        dance->dancers[0].radiusX = 64;
        dance->dancers[0].radiusY = 24;
        ManagedSprite_SetDrawFlag(dance->dancers[0].sprite, TRUE);
        dance->dancers[2].angle = 0;
        dance->dancers[2].baseX = 128;
        dance->dancers[2].baseY = 160;
        dance->dancers[2].radiusX = 64;
        dance->dancers[2].radiusY = 24;
        ManagedSprite_SetDrawFlag(dance->dancers[2].sprite, TRUE);
        ManagedSprite_GetPositionFxXYWithSubscreenOffset(dance->dancers[0].sprite, &x, &y, FX32_CONST(256));
        ax = (dance->dancers[0].baseX << 12) + GF_SinDeg(dance->dancers[0].angle) * dance->dancers[0].radiusX;
        ay = (dance->dancers[0].baseY << 12) + GF_CosDeg(dance->dancers[0].angle) * dance->dancers[0].radiusY;
        ManagedSprite_SetPositionFxXYWithSubscreenOffset(dance->dancers[0].sprite, ax, ay, FX32_CONST(256));
        MimeJrTopTween_Init(&dance->dancers[0].tweenX, ax, ax + (45 << 14), MimeJrTop_GetDanceStepFrames(dance->frontSpeed));
        MimeJrTopTween_Init(&dance->dancers[0].tweenY, ay, ay - (6 << 14), MimeJrTop_GetDanceStepFrames(dance->frontSpeed));
        ManagedSprite_GetPositionFxXYWithSubscreenOffset(dance->dancers[2].sprite, &x, &y, FX32_CONST(256));
        bx = (dance->dancers[2].baseX << 12) + GF_SinDeg(dance->dancers[2].angle) * dance->dancers[2].radiusX;
        by = (dance->dancers[2].baseY << 12) - GF_CosDeg(dance->dancers[2].angle) * dance->dancers[2].radiusY;
        ManagedSprite_SetPositionFxXYWithSubscreenOffset(dance->dancers[2].sprite, bx, by, FX32_CONST(256));
        MimeJrTopTween_Init(&dance->dancers[2].tweenX, bx, bx - (45 << 14), MimeJrTop_GetDanceStepFrames(dance->backSpeed));
        MimeJrTopTween_Init(&dance->dancers[2].tweenY, by, by + (6 << 14), MimeJrTop_GetDanceStepFrames(dance->backSpeed));
        MimeJrTopDancer_Reset(&dance->dancers[2]);
        MimeJrTopDancer_Reset(&dance->dancers[0]);
        dance->routineState++;
    case 1:
        a = MimeJrTopDancer_Circle(&dance->dancers[2], dance->backSpeed, 0, 1);
        b = MimeJrTopDancer_EnterHigh(&dance->dancers[0], dance->frontSpeed, 1, 3);
        if (a && b) {
            MimeJrTopDancer_Reset(&dance->dancers[2]);
            MimeJrTopDancer_Reset(&dance->dancers[0]);
            dance->routineState++;
        }
        return;
    case 2:
        a = MimeJrTopDancer_Slide(&dance->dancers[2], dance->backSpeed, 0, 3);
        b = MimeJrTopDancer_Slide(&dance->dancers[0], dance->frontSpeed, 1, 2);
        if (a && b) {
            MimeJrTopDancer_Reset(&dance->dancers[2]);
            MimeJrTopDancer_Reset(&dance->dancers[0]);
            dance->routineState++;
        }
        return;
    case 3:
        a = MimeJrTopDancer_EnterLow(&dance->dancers[2], dance->backSpeed, 0, 2);
        b = MimeJrTopDancer_EnterHigh(&dance->dancers[0], dance->frontSpeed, 1, 3);
        if (a && b) {
            MimeJrTopDancer_Reset(&dance->dancers[2]);
            MimeJrTopDancer_Reset(&dance->dancers[0]);
            dance->routineState++;
        }
        return;
    case 4:
        a = MimeJrTopDancer_Circle(&dance->dancers[2], dance->backSpeed, 0, 1);
        b = MimeJrTopDancer_Slide(&dance->dancers[0], dance->frontSpeed, 1, 2);
        if (a && b) {
            MimeJrTopDancer_Reset(&dance->dancers[2]);
            MimeJrTopDancer_Reset(&dance->dancers[0]);
            dance->routineState++;
        }
        return;
    default:
        SysTask_Destroy(sysTask);
        break;
    }
}

static void MimeJrTopDancer_Reset(MimeJrTopDancer *dancer) {
    dancer->state = MIME_JR_TOP_DANCER_START;
}

static BOOL MimeJrTopDancer_Slide(MimeJrTopDancer *dancer, int speed, s32 xdir, s32 ydir) {
    fx32 x, y;
    BOOL done;
    switch (dancer->state) {
    case MIME_JR_TOP_DANCER_START:
        ManagedSprite_GetPositionFxXYWithSubscreenOffset(dancer->sprite, &x, &y, FX32_CONST(256));
        if (xdir == 0) {
            MimeJrTopTween_Init(&dancer->tweenX, x, x - FX32_CONST(180), MimeJrTop_GetDanceStepFrames(speed));
        } else {
            MimeJrTopTween_Init(&dancer->tweenX, x, x + FX32_CONST(180), MimeJrTop_GetDanceStepFrames(speed));
        }
        if (ydir == 2) {
            MimeJrTopTween_Init(&dancer->tweenY, y, y - FX32_CONST(24), MimeJrTop_GetDanceStepFrames(speed));
        } else {
            MimeJrTopTween_Init(&dancer->tweenY, y, y + FX32_CONST(24), MimeJrTop_GetDanceStepFrames(speed));
        }
        dancer->state++;
    case MIME_JR_TOP_DANCER_MOVE:
        done = MimeJrTopTween_Update(&dancer->tweenX);
        MimeJrTopTween_Update(&dancer->tweenY);
        ManagedSprite_SetPositionFxXYWithSubscreenOffset(dancer->sprite, dancer->tweenX.value, dancer->tweenY.value, FX32_CONST(256));
        if (done) {
            return TRUE;
        }
        break;
    default:
        return TRUE;
    }
    return FALSE;
}

static BOOL MimeJrTopDancer_EnterLow(MimeJrTopDancer *dancer, int speed, s32 xdir, s32 ydir) {
    BOOL done;
    switch (dancer->state) {
    case MIME_JR_TOP_DANCER_START:
        if (xdir == 0) {
            MimeJrTopTween_Init(&dancer->tweenX, FX32_CONST(308), FX32_CONST(128), MimeJrTop_GetDanceStepFrames(speed));
        } else {
            MimeJrTopTween_Init(&dancer->tweenX, FX32_CONST(-62), FX32_CONST(128), MimeJrTop_GetDanceStepFrames(speed));
        }
        if (ydir == 2) {
            MimeJrTopTween_Init(&dancer->tweenY, FX32_CONST(160), FX32_CONST(136), MimeJrTop_GetDanceStepFrames(speed));
        } else {
            MimeJrTopTween_Init(&dancer->tweenY, FX32_CONST(160), FX32_CONST(184), MimeJrTop_GetDanceStepFrames(speed));
        }
        dancer->state++;
    case MIME_JR_TOP_DANCER_MOVE:
        done = MimeJrTopTween_Update(&dancer->tweenX);
        MimeJrTopTween_Update(&dancer->tweenY);
        ManagedSprite_SetPositionFxXYWithSubscreenOffset(dancer->sprite, dancer->tweenX.value, dancer->tweenY.value, FX32_CONST(256));
        if (done) {
            return TRUE;
        }
        break;
    default:
        return TRUE;
    }
    return FALSE;
}

static BOOL MimeJrTopDancer_EnterHigh(MimeJrTopDancer *dancer, int speed, s32 xdir, s32 ydir) {
    BOOL done;
    switch (dancer->state) {
    case MIME_JR_TOP_DANCER_START:
        if (xdir == 0) {
            MimeJrTopTween_Init(&dancer->tweenX, FX32_CONST(308), FX32_CONST(128), MimeJrTop_GetDanceStepFrames(speed));
        } else {
            MimeJrTopTween_Init(&dancer->tweenX, FX32_CONST(-62), FX32_CONST(128), MimeJrTop_GetDanceStepFrames(speed));
        }
        if (ydir == 2) {
            MimeJrTopTween_Init(&dancer->tweenY, FX32_CONST(96), FX32_CONST(72), MimeJrTop_GetDanceStepFrames(speed));
        } else {
            MimeJrTopTween_Init(&dancer->tweenY, FX32_CONST(96), FX32_CONST(120), MimeJrTop_GetDanceStepFrames(speed));
        }
        dancer->state++;
    case MIME_JR_TOP_DANCER_MOVE:
        done = MimeJrTopTween_Update(&dancer->tweenX);
        MimeJrTopTween_Update(&dancer->tweenY);
        ManagedSprite_SetPositionFxXYWithSubscreenOffset(dancer->sprite, dancer->tweenX.value, dancer->tweenY.value, FX32_CONST(256));
        if (done) {
            return TRUE;
        }
        break;
    default:
        return TRUE;
    }
    return FALSE;
}

static BOOL MimeJrTopDancer_Circle(MimeJrTopDancer *dancer, int speed, s32 xdir, s32 cycles) {
    fx32 oldX, oldY;
    fx32 x, y;
    switch (dancer->state) {
    case MIME_JR_TOP_DANCER_START:
        dancer->cycles = 0;
        dancer->angle = 0;
        dancer->state++;
    case MIME_JR_TOP_DANCER_MOVE:
        dancer->angle += 360 / MimeJrTop_GetDanceStepFrames(speed);
        if (dancer->angle >= 360) {
            dancer->angle = 0;
        }
        ManagedSprite_GetPositionFxXYWithSubscreenOffset(dancer->sprite, &oldX, &oldY, FX32_CONST(256));
        if (xdir == 0) {
            x = (dancer->baseX << 12) - GF_SinDeg(dancer->angle) * dancer->radiusX;
            y = (dancer->baseY << 12) - GF_CosDeg(dancer->angle) * dancer->radiusY;
        } else {
            x = (dancer->baseX << 12) + GF_SinDeg(dancer->angle) * dancer->radiusX;
            y = (dancer->baseY << 12) - GF_CosDeg(dancer->angle) * dancer->radiusY;
        }
        ManagedSprite_SetPositionFxXYWithSubscreenOffset(dancer->sprite, x, y, FX32_CONST(256));
        if (dancer->angle == 0) {
            dancer->cycles++;
            if (dancer->cycles >= cycles) {
                dancer->state++;
                return TRUE;
            }
            dancer->state = MIME_JR_TOP_DANCER_MOVE;
        }
        break;
    default:
        return TRUE;
    }
    return FALSE;
}

void MimeJrTopDance_Routine2(SysTask *sysTask, MimeJrTopDance *dance) {
    MimeJrTopDoneFlags flags = gMimeJrTopDance2DoneFlags;
    fx32 x, y;
    fx32 newX, newY;
    s32 i;

    if (!IsPaletteFadeFinished() || dance->resources->connectionLost == 1) {
        SysTask_Destroy(sysTask);
        return;
    }
    switch (dance->routineState) {
    case 0:
        for (i = 0; i < dance->count; i++) {
            dance->dancers[i].angle = 0;
            dance->dancers[i].baseX = 0x80;
            dance->dancers[i].baseY = 0xa0;
            dance->dancers[i].radiusX = 0x40;
            dance->dancers[i].radiusY = 0x18;
            ManagedSprite_SetDrawFlag(dance->dancers[i].sprite, TRUE);
            dance->dancers[i + 2].angle = 0;
            dance->dancers[i + 2].baseX = 0x80;
            dance->dancers[i + 2].baseY = 0xa0;
            dance->dancers[i + 2].radiusX = 0x40;
            dance->dancers[i + 2].radiusY = 0x18;
            ManagedSprite_SetDrawFlag(dance->dancers[i + 2].sprite, TRUE);
            ManagedSprite_GetPositionFxXYWithSubscreenOffset(dance->dancers[i].sprite, &x, &y, FX32_CONST(256));
            newX = (dance->dancers[i].baseX << 12) + GF_SinDeg(dance->dancers[i].angle) * dance->dancers[i].radiusX;
            newY = (dance->dancers[i].baseY << 12) + GF_CosDeg(dance->dancers[i].angle) * dance->dancers[i].radiusY;
            ManagedSprite_SetPositionFxXYWithSubscreenOffset(dance->dancers[i].sprite, newX, newY, FX32_CONST(256));
            ManagedSprite_GetPositionFxXYWithSubscreenOffset(dance->dancers[i + 2].sprite, &x, &y, FX32_CONST(256));
            newX = (dance->dancers[i + 2].baseX << 12) + GF_SinDeg(dance->dancers[i + 2].angle) * dance->dancers[i + 2].radiusX;
            newY = (dance->dancers[i + 2].baseY << 12) - GF_CosDeg(dance->dancers[i + 2].angle) * dance->dancers[i + 2].radiusY;
            ManagedSprite_SetPositionFxXYWithSubscreenOffset(dance->dancers[i + 2].sprite, newX, newY, FX32_CONST(256));
        }
        MimeJrTopDancer_Reset(&dance->dancers[2]);
        MimeJrTopDancer_Reset(&dance->dancers[3]);
        MimeJrTopDancer_Reset(&dance->dancers[0]);
        MimeJrTopDancer_Reset(&dance->dancers[1]);
        ManagedSprite_SetDrawFlag(dance->dancers[1].sprite, FALSE);
        dance->routineState++;
    case 1:
        flags.done[0] = MimeJrTopDancer_Slide(&dance->dancers[2], dance->backSpeed, 0, 3);
        flags.done[1] = MimeJrTopDancer_EnterLow(&dance->dancers[3], dance->backSpeed, 0, 2);
        flags.done[2] = MimeJrTopDancer_EnterHigh(&dance->dancers[0], dance->frontSpeed, 1, 3);
        flags.done[3] = TRUE;
        if (!flags.done[0] || !flags.done[1] || !flags.done[2] || !flags.done[3]) {
            break;
        }
        MimeJrTopDancer_Reset(&dance->dancers[2]);
        MimeJrTopDancer_Reset(&dance->dancers[3]);
        MimeJrTopDancer_Reset(&dance->dancers[0]);
        MimeJrTopDancer_Reset(&dance->dancers[1]);
        ManagedSprite_SetDrawFlag(dance->dancers[1].sprite, TRUE);
        dance->routineState++;
    case 2:
        flags.done[0] = TRUE;
        flags.done[1] = MimeJrTopDancer_Circle(&dance->dancers[3], dance->backSpeed, 0, 1);
        flags.done[2] = MimeJrTopDancer_Slide(&dance->dancers[0], dance->frontSpeed, 1, 2);
        flags.done[3] = MimeJrTopDancer_EnterHigh(&dance->dancers[1], dance->frontSpeed, 1, 3);
        if (!flags.done[0] || !flags.done[1] || !flags.done[2] || !flags.done[3]) {
            break;
        }
        MimeJrTopDancer_Reset(&dance->dancers[2]);
        MimeJrTopDancer_Reset(&dance->dancers[3]);
        MimeJrTopDancer_Reset(&dance->dancers[0]);
        MimeJrTopDancer_Reset(&dance->dancers[1]);
        dance->routineState++;
    case 3:
        flags.done[0] = MimeJrTopDancer_EnterLow(&dance->dancers[2], dance->backSpeed, 0, 2);
        flags.done[1] = MimeJrTopDancer_Slide(&dance->dancers[3], dance->backSpeed, 0, 3);
        flags.done[2] = TRUE;
        flags.done[3] = MimeJrTopDancer_Slide(&dance->dancers[1], dance->frontSpeed, 1, 2);
        if (!flags.done[0] || !flags.done[1] || !flags.done[2] || !flags.done[3]) {
            break;
        }
        MimeJrTopDancer_Reset(&dance->dancers[2]);
        MimeJrTopDancer_Reset(&dance->dancers[3]);
        MimeJrTopDancer_Reset(&dance->dancers[0]);
        MimeJrTopDancer_Reset(&dance->dancers[1]);
        dance->routineState++;
    case 4:
        flags.done[0] = MimeJrTopDancer_Circle(&dance->dancers[2], dance->backSpeed, 0, 1);
        flags.done[1] = TRUE;
        flags.done[2] = TRUE;
        flags.done[3] = MimeJrTopDancer_Slide(&dance->dancers[1], dance->frontSpeed, 1, 2);
        if (!flags.done[0] || !flags.done[1] || !flags.done[2] || !flags.done[3]) {
            break;
        }
        MimeJrTopDancer_Reset(&dance->dancers[2]);
        MimeJrTopDancer_Reset(&dance->dancers[3]);
        MimeJrTopDancer_Reset(&dance->dancers[0]);
        MimeJrTopDancer_Reset(&dance->dancers[1]);
        if (dance->loopCount != 1) {
            dance->loopCount++;
            dance->routineState = 1;
        } else {
            dance->loopCount = 0;
            dance->routineState++;
        }
        break;
    default:
        SysTask_Destroy(sysTask);
        break;
    }
}

void MimeJrTopDance_Routine3(SysTask *sysTask, MimeJrTopDance *dance) {
    MimeJrTopDoneFlags flags = gMimeJrTopDance3DoneFlags;
    fx32 x, y;
    fx32 newX, newY;
    s32 i;

    if (!IsPaletteFadeFinished() || dance->resources->connectionLost == 1) {
        SysTask_Destroy(sysTask);
        return;
    }
    switch (dance->routineState) {
    case 0:
        for (i = 0; i < dance->count; i++) {
            dance->dancers[i].angle = 0;
            dance->dancers[i].baseX = 0x80;
            dance->dancers[i].baseY = 0xa0;
            dance->dancers[i].radiusX = 0x40;
            dance->dancers[i].radiusY = 0x18;
            ManagedSprite_SetDrawFlag(dance->dancers[i].sprite, TRUE);
            dance->dancers[i + 2].angle = 0;
            dance->dancers[i + 2].baseX = 0x80;
            dance->dancers[i + 2].baseY = 0xa0;
            dance->dancers[i + 2].radiusX = 0x40;
            dance->dancers[i + 2].radiusY = 0x18;
            ManagedSprite_SetDrawFlag(dance->dancers[i + 2].sprite, TRUE);
            ManagedSprite_GetPositionFxXYWithSubscreenOffset(dance->dancers[i].sprite, &x, &y, FX32_CONST(256));
            newX = (dance->dancers[i].baseX << 12) + GF_SinDeg(dance->dancers[i].angle) * dance->dancers[i].radiusX;
            newY = (dance->dancers[i].baseY << 12) + GF_CosDeg(dance->dancers[i].angle) * dance->dancers[i].radiusY;
            ManagedSprite_SetPositionFxXYWithSubscreenOffset(dance->dancers[i].sprite, newX, newY, FX32_CONST(256));
            ManagedSprite_GetPositionFxXYWithSubscreenOffset(dance->dancers[i + 2].sprite, &x, &y, FX32_CONST(256));
            newX = (dance->dancers[i + 2].baseX << 12) + GF_SinDeg(dance->dancers[i + 2].angle) * dance->dancers[i + 2].radiusX;
            newY = (dance->dancers[i + 2].baseY << 12) - GF_CosDeg(dance->dancers[i + 2].angle) * dance->dancers[i + 2].radiusY;
            ManagedSprite_SetPositionFxXYWithSubscreenOffset(dance->dancers[i + 2].sprite, newX, newY, FX32_CONST(256));
        }
        MimeJrTopDancer_Reset(&dance->dancers[2]);
        MimeJrTopDancer_Reset(&dance->dancers[3]);
        MimeJrTopDancer_Reset(&dance->dancers[0]);
        MimeJrTopDancer_Reset(&dance->dancers[1]);
        ManagedSprite_SetDrawFlag(dance->dancers[1].sprite, FALSE);
        dance->routineState++;
    case 1:
        flags.done[0] = MimeJrTopDancer_Slide(&dance->dancers[2], dance->backSpeed, 0, 3);
        flags.done[1] = MimeJrTopDancer_EnterLow(&dance->dancers[3], dance->backSpeed, 0, 2);
        flags.done[2] = MimeJrTopDancer_EnterHigh(&dance->dancers[0], dance->frontSpeed, 1, 3);
        flags.done[3] = TRUE;
        if (!flags.done[0] || !flags.done[1] || !flags.done[2] || !flags.done[3]) {
            break;
        }
        MimeJrTopDancer_Reset(&dance->dancers[2]);
        MimeJrTopDancer_Reset(&dance->dancers[3]);
        MimeJrTopDancer_Reset(&dance->dancers[0]);
        MimeJrTopDancer_Reset(&dance->dancers[1]);
        ManagedSprite_SetDrawFlag(dance->dancers[1].sprite, TRUE);
        dance->routineState++;
    case 2:
        flags.done[0] = TRUE;
        flags.done[1] = MimeJrTopDancer_Circle(&dance->dancers[3], dance->backSpeed, 0, 2);
        flags.done[2] = MimeJrTopDancer_Slide(&dance->dancers[0], dance->frontSpeed, 1, 2);
        flags.done[3] = MimeJrTopDancer_EnterHigh(&dance->dancers[1], dance->frontSpeed, 1, 3);
        if (!flags.done[0] || !flags.done[1] || !flags.done[2] || !flags.done[3]) {
            break;
        }
        MimeJrTopDancer_Reset(&dance->dancers[2]);
        MimeJrTopDancer_Reset(&dance->dancers[3]);
        MimeJrTopDancer_Reset(&dance->dancers[0]);
        MimeJrTopDancer_Reset(&dance->dancers[1]);
        dance->routineState++;
    case 3:
        flags.done[0] = MimeJrTopDancer_EnterLow(&dance->dancers[2], dance->backSpeed, 0, 2);
        flags.done[1] = MimeJrTopDancer_Slide(&dance->dancers[3], dance->backSpeed, 0, 3);
        flags.done[2] = TRUE;
        flags.done[3] = MimeJrTopDancer_Slide(&dance->dancers[1], dance->frontSpeed, 1, 2);
        if (!flags.done[0] || !flags.done[1] || !flags.done[2] || !flags.done[3]) {
            break;
        }
        MimeJrTopDancer_Reset(&dance->dancers[2]);
        MimeJrTopDancer_Reset(&dance->dancers[3]);
        MimeJrTopDancer_Reset(&dance->dancers[0]);
        MimeJrTopDancer_Reset(&dance->dancers[1]);
        dance->routineState++;
    case 4:
        flags.done[0] = MimeJrTopDancer_Circle(&dance->dancers[2], dance->backSpeed, 0, 2);
        flags.done[1] = TRUE;
        flags.done[2] = TRUE;
        flags.done[3] = MimeJrTopDancer_Slide(&dance->dancers[1], dance->frontSpeed, 1, 2);
        if (!flags.done[0] || !flags.done[1] || !flags.done[2] || !flags.done[3]) {
            break;
        }
        MimeJrTopDancer_Reset(&dance->dancers[2]);
        MimeJrTopDancer_Reset(&dance->dancers[3]);
        MimeJrTopDancer_Reset(&dance->dancers[0]);
        MimeJrTopDancer_Reset(&dance->dancers[1]);
        if (dance->loopCount != 1) {
            dance->loopCount++;
            dance->routineState = 1;
        } else {
            dance->loopCount = 0;
            dance->routineState++;
        }
        break;
    default:
        SysTask_Destroy(sysTask);
        break;
    }
}

void MimeJrTopDance_Routine4(SysTask *sysTask, MimeJrTopDance *dance) {
    MimeJrTopDoneFlags flags = gMimeJrTopDance4DoneFlags;
    fx32 x, y;
    fx32 newX, newY;
    s32 i;

    if (!IsPaletteFadeFinished() || dance->resources->connectionLost == 1) {
        SysTask_Destroy(sysTask);
        return;
    }
    switch (dance->routineState) {
    case 0:
        for (i = 0; i < dance->count; i++) {
            dance->dancers[i].angle = 0;
            dance->dancers[i].baseX = 0x80;
            dance->dancers[i].baseY = 0xa0;
            dance->dancers[i].radiusX = 0x40;
            dance->dancers[i].radiusY = 0x18;
            ManagedSprite_SetDrawFlag(dance->dancers[i].sprite, TRUE);
            dance->dancers[i + 2].angle = 0;
            dance->dancers[i + 2].baseX = 0x80;
            dance->dancers[i + 2].baseY = 0xa0;
            dance->dancers[i + 2].radiusX = 0x40;
            dance->dancers[i + 2].radiusY = 0x18;
            ManagedSprite_SetDrawFlag(dance->dancers[i + 2].sprite, TRUE);
            ManagedSprite_GetPositionFxXYWithSubscreenOffset(dance->dancers[i].sprite, &x, &y, FX32_CONST(256));
            newX = (dance->dancers[i].baseX << 12) + GF_SinDeg(dance->dancers[i].angle) * dance->dancers[i].radiusX;
            newY = (dance->dancers[i].baseY << 12) + GF_CosDeg(dance->dancers[i].angle) * dance->dancers[i].radiusY;
            ManagedSprite_SetPositionFxXYWithSubscreenOffset(dance->dancers[i].sprite, newX, newY, FX32_CONST(256));
            ManagedSprite_GetPositionFxXYWithSubscreenOffset(dance->dancers[i + 2].sprite, &x, &y, FX32_CONST(256));
            newX = (dance->dancers[i + 2].baseX << 12) + GF_SinDeg(dance->dancers[i + 2].angle) * dance->dancers[i + 2].radiusX;
            newY = (dance->dancers[i + 2].baseY << 12) - GF_CosDeg(dance->dancers[i + 2].angle) * dance->dancers[i + 2].radiusY;
            ManagedSprite_SetPositionFxXYWithSubscreenOffset(dance->dancers[i + 2].sprite, newX, newY, FX32_CONST(256));
        }
        MimeJrTopDancer_Reset(&dance->dancers[2]);
        MimeJrTopDancer_Reset(&dance->dancers[3]);
        MimeJrTopDancer_Reset(&dance->dancers[0]);
        MimeJrTopDancer_Reset(&dance->dancers[1]);
        ManagedSprite_SetDrawFlag(dance->dancers[1].sprite, FALSE);
        dance->routineState++;
    case 1:
        flags.done[0] = MimeJrTopDancer_Slide(&dance->dancers[2], dance->backSpeed, 0, 3);
        flags.done[1] = MimeJrTopDancer_EnterLow(&dance->dancers[3], dance->backSpeed, 0, 2);
        flags.done[2] = MimeJrTopDancer_EnterHigh(&dance->dancers[0], dance->frontSpeed, 1, 3);
        flags.done[3] = TRUE;
        if (!flags.done[0] || !flags.done[1] || !flags.done[2] || !flags.done[3]) {
            break;
        }
        MimeJrTopDancer_Reset(&dance->dancers[2]);
        MimeJrTopDancer_Reset(&dance->dancers[3]);
        MimeJrTopDancer_Reset(&dance->dancers[0]);
        MimeJrTopDancer_Reset(&dance->dancers[1]);
        ManagedSprite_SetDrawFlag(dance->dancers[1].sprite, TRUE);
        dance->routineState++;
    case 2:
        flags.done[0] = TRUE;
        flags.done[1] = MimeJrTopDancer_Circle(&dance->dancers[3], dance->backSpeed, 0, 1);
        flags.done[2] = MimeJrTopDancer_Slide(&dance->dancers[0], dance->frontSpeed, 1, 2);
        flags.done[3] = MimeJrTopDancer_EnterHigh(&dance->dancers[1], dance->frontSpeed, 1, 3);
        if (!flags.done[0] || !flags.done[1] || !flags.done[2] || !flags.done[3]) {
            break;
        }
        MimeJrTopDancer_Reset(&dance->dancers[2]);
        MimeJrTopDancer_Reset(&dance->dancers[3]);
        MimeJrTopDancer_Reset(&dance->dancers[0]);
        MimeJrTopDancer_Reset(&dance->dancers[1]);
        dance->routineState++;
    case 3:
        flags.done[1] = MimeJrTopDancer_Circle(&dance->dancers[3], dance->backSpeed, 1, 1);
        if (!flags.done[1]) {
            break;
        }
        MimeJrTopDancer_Reset(&dance->dancers[3]);
        dance->routineState++;
    case 4:
        flags.done[0] = MimeJrTopDancer_EnterLow(&dance->dancers[2], dance->backSpeed, 0, 2);
        flags.done[1] = MimeJrTopDancer_Slide(&dance->dancers[3], dance->backSpeed, 0, 3);
        flags.done[2] = TRUE;
        flags.done[3] = MimeJrTopDancer_Slide(&dance->dancers[1], dance->frontSpeed, 1, 2);
        if (!flags.done[0] || !flags.done[1] || !flags.done[2] || !flags.done[3]) {
            break;
        }
        MimeJrTopDancer_Reset(&dance->dancers[2]);
        MimeJrTopDancer_Reset(&dance->dancers[3]);
        MimeJrTopDancer_Reset(&dance->dancers[0]);
        MimeJrTopDancer_Reset(&dance->dancers[1]);
        dance->routineState++;
    case 5:
        flags.done[0] = MimeJrTopDancer_Circle(&dance->dancers[2], dance->backSpeed, 0, 1);
        flags.done[1] = TRUE;
        flags.done[2] = TRUE;
        flags.done[3] = MimeJrTopDancer_Slide(&dance->dancers[1], dance->frontSpeed, 1, 2);
        if (!flags.done[0] || !flags.done[1] || !flags.done[2] || !flags.done[3]) {
            break;
        }
        MimeJrTopDancer_Reset(&dance->dancers[2]);
        MimeJrTopDancer_Reset(&dance->dancers[3]);
        MimeJrTopDancer_Reset(&dance->dancers[0]);
        MimeJrTopDancer_Reset(&dance->dancers[1]);
        dance->routineState++;
    case 6:
        flags.done[0] = MimeJrTopDancer_Circle(&dance->dancers[2], dance->backSpeed, 1, 1);
        if (!flags.done[0]) {
            break;
        }
        MimeJrTopDancer_Reset(&dance->dancers[2]);
        if (dance->loopCount != 1) {
            dance->loopCount++;
            dance->routineState = 1;
        } else {
            dance->loopCount = 0;
            dance->routineState++;
        }
        break;
    default:
        SysTask_Destroy(sysTask);
        break;
    }
}

void MimeJrTopDance_Routine5(SysTask *sysTask, MimeJrTopDance *dance) {
    MimeJrTopDoneFlags flags = gMimeJrTopDance5DoneFlags;
    fx32 x, y;
    fx32 newX, newY;
    s32 i;

    if (!IsPaletteFadeFinished() || dance->resources->connectionLost == 1) {
        SysTask_Destroy(sysTask);
        return;
    }
    switch (dance->routineState) {
    case 0:
        for (i = 0; i < dance->count; i++) {
            dance->dancers[i].angle = 0;
            dance->dancers[i].baseX = 0x80;
            dance->dancers[i].baseY = 0xa0;
            dance->dancers[i].radiusX = 0x40;
            dance->dancers[i].radiusY = 0x18;
            ManagedSprite_SetDrawFlag(dance->dancers[i].sprite, TRUE);
            dance->dancers[i + 2].angle = 0;
            dance->dancers[i + 2].baseX = 0x80;
            dance->dancers[i + 2].baseY = 0xa0;
            dance->dancers[i + 2].radiusX = 0x40;
            dance->dancers[i + 2].radiusY = 0x18;
            ManagedSprite_SetDrawFlag(dance->dancers[i + 2].sprite, TRUE);
            ManagedSprite_GetPositionFxXYWithSubscreenOffset(dance->dancers[i].sprite, &x, &y, FX32_CONST(256));
            newX = (dance->dancers[i].baseX << 12) + GF_SinDeg(dance->dancers[i].angle) * dance->dancers[i].radiusX;
            newY = (dance->dancers[i].baseY << 12) + GF_CosDeg(dance->dancers[i].angle) * dance->dancers[i].radiusY;
            ManagedSprite_SetPositionFxXYWithSubscreenOffset(dance->dancers[i].sprite, newX, newY, FX32_CONST(256));
            ManagedSprite_GetPositionFxXYWithSubscreenOffset(dance->dancers[i + 2].sprite, &x, &y, FX32_CONST(256));
            newX = (dance->dancers[i + 2].baseX << 12) + GF_SinDeg(dance->dancers[i + 2].angle) * dance->dancers[i + 2].radiusX;
            newY = (dance->dancers[i + 2].baseY << 12) - GF_CosDeg(dance->dancers[i + 2].angle) * dance->dancers[i + 2].radiusY;
            ManagedSprite_SetPositionFxXYWithSubscreenOffset(dance->dancers[i + 2].sprite, newX, newY, FX32_CONST(256));
        }
        MimeJrTopDancer_Reset(&dance->dancers[2]);
        MimeJrTopDancer_Reset(&dance->dancers[3]);
        MimeJrTopDancer_Reset(&dance->dancers[0]);
        MimeJrTopDancer_Reset(&dance->dancers[1]);
        ManagedSprite_SetDrawFlag(dance->dancers[1].sprite, FALSE);
        dance->routineState++;
    case 1:
        flags.done[0] = MimeJrTopDancer_Slide(&dance->dancers[2], dance->backSpeed, 0, 3);
        flags.done[1] = MimeJrTopDancer_EnterLow(&dance->dancers[3], dance->backSpeed, 0, 2);
        flags.done[2] = MimeJrTopDancer_EnterHigh(&dance->dancers[0], dance->frontSpeed, 1, 3);
        flags.done[3] = TRUE;
        if (!flags.done[0] || !flags.done[1] || !flags.done[2] || !flags.done[3]) {
            break;
        }
        MimeJrTopDancer_Reset(&dance->dancers[2]);
        MimeJrTopDancer_Reset(&dance->dancers[3]);
        MimeJrTopDancer_Reset(&dance->dancers[0]);
        MimeJrTopDancer_Reset(&dance->dancers[1]);
        ManagedSprite_SetDrawFlag(dance->dancers[1].sprite, TRUE);
        dance->routineState++;
    case 2:
        flags.done[0] = TRUE;
        flags.done[1] = MimeJrTopDancer_Circle(&dance->dancers[3], dance->backSpeed, 0, 1);
        flags.done[2] = MimeJrTopDancer_Slide(&dance->dancers[0], dance->frontSpeed, 1, 2);
        flags.done[3] = MimeJrTopDancer_EnterHigh(&dance->dancers[1], dance->frontSpeed, 1, 3);
        if (!flags.done[0] || !flags.done[1] || !flags.done[2] || !flags.done[3]) {
            break;
        }
        MimeJrTopDancer_Reset(&dance->dancers[2]);
        MimeJrTopDancer_Reset(&dance->dancers[3]);
        MimeJrTopDancer_Reset(&dance->dancers[0]);
        MimeJrTopDancer_Reset(&dance->dancers[1]);
        dance->routineState++;
    case 3:
        flags.done[1] = MimeJrTopDancer_Circle(&dance->dancers[3], dance->backSpeed, 1, 1);
        if (!flags.done[1]) {
            break;
        }
        MimeJrTopDancer_Reset(&dance->dancers[3]);
        dance->routineState++;
    case 4:
        flags.done[0] = MimeJrTopDancer_EnterLow(&dance->dancers[2], dance->backSpeed, 0, 2);
        flags.done[1] = MimeJrTopDancer_Slide(&dance->dancers[3], dance->backSpeed, 0, 3);
        flags.done[2] = TRUE;
        flags.done[3] = MimeJrTopDancer_Slide(&dance->dancers[1], dance->frontSpeed, 1, 2);
        if (!flags.done[0] || !flags.done[1] || !flags.done[2] || !flags.done[3]) {
            break;
        }
        MimeJrTopDancer_Reset(&dance->dancers[2]);
        MimeJrTopDancer_Reset(&dance->dancers[3]);
        MimeJrTopDancer_Reset(&dance->dancers[0]);
        MimeJrTopDancer_Reset(&dance->dancers[1]);
        dance->routineState++;
    case 5:
        flags.done[0] = MimeJrTopDancer_Circle(&dance->dancers[2], dance->backSpeed, 0, 1);
        flags.done[1] = TRUE;
        flags.done[2] = TRUE;
        flags.done[3] = MimeJrTopDancer_Slide(&dance->dancers[1], dance->frontSpeed, 1, 2);
        if (!flags.done[0] || !flags.done[1] || !flags.done[2] || !flags.done[3]) {
            break;
        }
        MimeJrTopDancer_Reset(&dance->dancers[2]);
        MimeJrTopDancer_Reset(&dance->dancers[3]);
        MimeJrTopDancer_Reset(&dance->dancers[0]);
        MimeJrTopDancer_Reset(&dance->dancers[1]);
        dance->routineState++;
    case 6:
        flags.done[0] = MimeJrTopDancer_Circle(&dance->dancers[2], dance->backSpeed, 1, 1);
        if (!flags.done[0]) {
            break;
        }
        MimeJrTopDancer_Reset(&dance->dancers[2]);
        if (dance->loopCount != 8) {
            dance->loopCount++;
            dance->routineState = 1;
        } else {
            dance->loopCount = 0;
            dance->routineState++;
        }
        break;
    default:
        SysTask_Destroy(sysTask);
        break;
    }
}

static void MimeJrTop_QuatIdentity(MimeJrTopQuat *quat) {
    quat->v[0] = 1.0f;
    quat->v[1] = 0.0f;
    quat->v[2] = 0.0f;
    quat->v[3] = 0.0f;
}

static void MimeJrTop_QuatCopy(MimeJrTopQuat *dst, const MimeJrTopQuat *src) {
    *dst = *src;
}

static void MimeJrTop_QuatMul(f32 *dst, const f32 *a, const f32 *b) {
    dst[0] = a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3];
    dst[1] = a[2] * b[3] + (a[0] * b[1] + a[1] * b[0]) - a[3] * b[2];
    dst[2] = a[3] * b[1] + (a[2] * b[0] + (a[0] * b[2] - a[1] * b[3]));
    dst[3] = a[3] * b[0] + (a[0] * b[3] + a[1] * b[2] - a[2] * b[1]);
}

// Computes w * w + 2x + 2y + 2z.
static f32 MimeJrTop_QuatLengthSq(const MimeJrTopQuat *quat) {
    f32 a = quat->v[3] + quat->v[3];
    f32 b = quat->v[2] + quat->v[2];
    f32 c = quat->v[0] * quat->v[0];
    c = c + (quat->v[1] + quat->v[1]);
    c = b + c;
    return a + c;
}

static f32 MimeJrTop_QuatLength(const MimeJrTopQuat *quat) {
    f32 value;
    if (MimeJrTop_QuatLengthSq(quat) > 0.0f) {
        value = 4096.0f * MimeJrTop_QuatLengthSq(quat) + 0.5f;
    } else {
        value = 4096.0f * MimeJrTop_QuatLengthSq(quat) - 0.5f;
    }
    return (f32)FX_Sqrt((int)value) / 4096.0f;
}

static void MimeJrTop_QuatDivide(f32 *dst, const f32 *src, f32 divisor) {
    dst[0] = src[0] / divisor;
    dst[1] = src[1] / divisor;
    dst[2] = src[2] / divisor;
    dst[3] = src[3] / divisor;
}

static void MimeJrTop_QuatToMatrix(MimeJrTopFloatMatrix *dst, const MimeJrTopQuat *quat) {
    f32 xx = (f32)((double)(quat->v[1] * quat->v[1]) * 2.0);
    f32 yy = (f32)((double)(quat->v[2] * quat->v[2]) * 2.0);
    f32 zz = (f32)((double)(quat->v[3] * quat->v[3]) * 2.0);
    f32 xy = (f32)((double)(quat->v[1] * quat->v[2]) * 2.0);
    f32 yz = (f32)((double)(quat->v[2] * quat->v[3]) * 2.0);
    f32 zx = (f32)((double)(quat->v[3] * quat->v[1]) * 2.0);
    f32 xw = (f32)((double)(quat->v[1] * quat->v[0]) * 2.0);
    f32 yw = (f32)((double)(quat->v[2] * quat->v[0]) * 2.0);
    f32 zw = (f32)((double)(quat->v[3] * quat->v[0]) * 2.0);

    dst->m[0][0] = (f32)((1.0 - (double)yy) - (double)zz);
    dst->m[0][1] = xy + zw;
    dst->m[0][2] = zx - yw;
    dst->m[0][3] = 0.0f;
    dst->m[1][0] = xy - zw;
    dst->m[1][1] = (f32)((1.0 - (double)zz) - (double)xx);
    dst->m[1][2] = yz + xw;
    dst->m[1][3] = 0.0f;
    dst->m[2][0] = zx + yw;
    dst->m[2][1] = yz - xw;
    dst->m[2][2] = (f32)((1.0 - (double)xx) - (double)yy);
    dst->m[2][3] = 0.0f;
    dst->m[3][0] = 0.0f;
    dst->m[3][1] = 0.0f;
    dst->m[3][2] = 0.0f;
    dst->m[3][3] = 1.0f;
}

#define FLOAT_TO_FX(value) ((int)((value) > 0.0f ? (value) * 4096.0f + 0.5f : (value) * 4096.0f - 0.5f))

static void MimeJrTop_FloatMatrixToFx44(const MimeJrTopFloatMatrix *src, MtxFx44 *dst) {
    dst->_00 = FLOAT_TO_FX(src->m[0][0]);
    dst->_01 = FLOAT_TO_FX(src->m[0][1]);
    dst->_02 = FLOAT_TO_FX(src->m[0][2]);
    dst->_03 = FLOAT_TO_FX(src->m[0][3]);
    dst->_10 = FLOAT_TO_FX(src->m[1][0]);
    dst->_11 = FLOAT_TO_FX(src->m[1][1]);
    dst->_12 = FLOAT_TO_FX(src->m[1][2]);
    dst->_13 = FLOAT_TO_FX(src->m[1][3]);
    dst->_20 = FLOAT_TO_FX(src->m[2][0]);
    dst->_21 = FLOAT_TO_FX(src->m[2][1]);
    dst->_22 = FLOAT_TO_FX(src->m[2][2]);
    dst->_23 = FLOAT_TO_FX(src->m[2][3]);
    dst->_30 = FLOAT_TO_FX(src->m[3][0]);
    dst->_31 = FLOAT_TO_FX(src->m[3][1]);
    dst->_32 = FLOAT_TO_FX(src->m[3][2]);
    dst->_33 = FLOAT_TO_FX(src->m[3][3]);
}

#undef FLOAT_TO_FX

static void MimeJrTop_FloatMatrixToFx43(const MimeJrTopFloatMatrix *src, MtxFx43 *dst) {
    MtxFx44 temp;
    MimeJrTop_FloatMatrixToFx44(src, &temp);
    MTX_Copy44To43_(&temp, dst);
}
#if defined(SDK_PORT)
static void Native_MimeJrTopDance_Routine1(SysTask *task, void *dance) { MimeJrTopDance_Routine1(task, dance); }
static void Native_MimeJrTopDance_Routine2(SysTask *task, void *dance) { MimeJrTopDance_Routine2(task, dance); }
static void Native_MimeJrTopDance_Routine3(SysTask *task, void *dance) { MimeJrTopDance_Routine3(task, dance); }
static void Native_MimeJrTopDance_Routine4(SysTask *task, void *dance) { MimeJrTopDance_Routine4(task, dance); }
static void Native_MimeJrTopDance_Routine5(SysTask *task, void *dance) { MimeJrTopDance_Routine5(task, dance); }






const MimeJrTopRoutineTask gMimeJrTopRoutineTasks[9] = {
    { NULL, { 0x96, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { Native_MimeJrTopDance_Routine1, { 0x96, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { Native_MimeJrTopDance_Routine2, { 0x96, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { Native_MimeJrTopDance_Routine2, { 0x96, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { Native_MimeJrTopDance_Routine3, { 0x96, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { Native_MimeJrTopDance_Routine3, { 0x96, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { Native_MimeJrTopDance_Routine4, { 0x96, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { Native_MimeJrTopDance_Routine5, { 0x96, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { Native_MimeJrTopDance_Routine5, { 0 } },
};

const OverlayManagerTemplate sWiFiPlazaMimeJrTopOverlayTemplate = { MimeJrTop_Init, MimeJrTop_Main, MimeJrTop_Exit, FS_OVERLAY_ID(OVY_92) };
#endif
