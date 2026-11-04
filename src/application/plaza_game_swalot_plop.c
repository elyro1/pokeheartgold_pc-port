#include "application/plaza_game_swalot_plop.h"

#include <nnsys/g3d_internal.h>
#include <nnsys/g3d_util_internal.h>
#include <string.h>

#include "global.h"

#include "constants/sndseq.h"

#include "application/plaza_game.h"
#include "msgdata/msg.naix"
#include "msgdata/msg/msg_0230.h"

#include "bg_window.h"
#include "brightness.h"
#include "camera.h"
#include "comm_state.h"
#include "communication_command.h"
#include "communication_core.h"
#include "communication_player_state.h"
#include "filesystem.h"
#include "font.h"
#include "font_sprite.h"
#include "geometry_math.h"
#include "gf_3d_render.h"
#include "gf_gfx_loader.h"
#include "gf_gfx_planes.h"
#include "heap.h"
#include "link_synchronization.h"
#include "math_util.h"
#include "model_animation.h"
#include "oam_manager.h"
#include "obj_char_transfer.h"
#include "obj_pltt_transfer.h"
#include "palette.h"
#include "render_window.h"
#include "screen_fade_internal.h"
#include "screen_fade_legacy.h"
#include "sound_playback.h"
#include "sound_system_internal.h"
#include "sprite.h"
#include "sprite_transfer.h"
#include "string_util.h"
#include "swap_3d_buffers.h"
#include "system.h"
#include "text.h"
#include "touchpad.h"
#include "vram_transfer_manager.h"
#include "wifi_connection.h"
#include "wireless_status_icon.h"

BOOL GF_TouchpadStartRecording(TPData *data, u32 capacity, u32 frequency);
int GF_TouchpadUpdate(SwalotPlopTouch *out, int mode, u32 threshold);
int MTX_Inverse33(const MtxFx33 *src, MtxFx33 *dst);

enum SwalotPlopState {
    SWALOT_PLOP_STATE_CREATE_ENTRY,
    SWALOT_PLOP_STATE_WAIT_ENTRY,
    SWALOT_PLOP_STATE_INIT_GAME,
    SWALOT_PLOP_STATE_FADE_IN,
    SWALOT_PLOP_STATE_SEND_START,
    SWALOT_PLOP_STATE_WAIT_START,
    SWALOT_PLOP_STATE_RUN_INTRO,
    SWALOT_PLOP_STATE_PLAY,
    SWALOT_PLOP_STATE_SEND_SCORES,
    SWALOT_PLOP_STATE_WAIT_SCORES,
    SWALOT_PLOP_STATE_RUN_FINISH,
    SWALOT_PLOP_STATE_FADE_OUT,
    SWALOT_PLOP_STATE_WAIT_FADE_OUT,
    SWALOT_PLOP_STATE_FREE_GAME,
    SWALOT_PLOP_STATE_SHOW_RESULTS,
    SWALOT_PLOP_STATE_WAIT_RESULTS,
};

enum SwalotPlopExitState {
    SWALOT_PLOP_EXIT_STATE_FREE,
    SWALOT_PLOP_EXIT_STATE_WAIT_SYNC,
};

enum SwalotPlopConnectionLostState {
    SWALOT_PLOP_CONNECTION_LOST_BLACK_OUT,
    SWALOT_PLOP_CONNECTION_LOST_WAIT,
    SWALOT_PLOP_CONNECTION_LOST_ABORT,
};

enum SwalotPlopIntroState {
    SWALOT_PLOP_INTRO_START_DEMO,
    SWALOT_PLOP_INTRO_RUN_DEMO,
    SWALOT_PLOP_INTRO_WAIT_SYNC,
    SWALOT_PLOP_INTRO_OPEN_SCENE,
    SWALOT_PLOP_INTRO_WAIT_READY,
    SWALOT_PLOP_INTRO_SHOW_WINDOW,
    SWALOT_PLOP_INTRO_COUNTDOWN,
    SWALOT_PLOP_INTRO_WAIT,
    SWALOT_PLOP_INTRO_DONE,
};

enum SwalotPlopFinishState {
    SWALOT_PLOP_FINISH_START,
    SWALOT_PLOP_FINISH_WAIT,
    SWALOT_PLOP_FINISH_HOLD,
    SWALOT_PLOP_FINISH_DONE,
};

enum SwalotPlopDemoState {
    SWALOT_PLOP_DEMO_SHOW_HAND,
    SWALOT_PLOP_DEMO_THROW,
    SWALOT_PLOP_DEMO_PAUSE,
    SWALOT_PLOP_DEMO_WAIT_SHOTS,
};

enum SwalotPlopMouthState {
    SWALOT_PLOP_MOUTH_SPEED_UP,
    SWALOT_PLOP_MOUTH_HOLD,
    SWALOT_PLOP_MOUTH_LOOP,
};

enum SwalotPlopBgFadeState {
    SWALOT_PLOP_BG_FADE_IDLE,
    SWALOT_PLOP_BG_FADE_START,
    SWALOT_PLOP_BG_FADE_CROSS,
    SWALOT_PLOP_BG_FADE_IN,
};

enum SwalotPlopComboState {
    SWALOT_PLOP_COMBO_HIDDEN,
    SWALOT_PLOP_COMBO_SLIDE_IN,
    SWALOT_PLOP_COMBO_SHOWN,
    SWALOT_PLOP_COMBO_SLIDE_OUT,
};

enum SwalotPlopSpeedUpState {
    SWALOT_PLOP_SPEED_UP_RAMP,
    SWALOT_PLOP_SPEED_UP_START_FLASH,
    SWALOT_PLOP_SPEED_UP_WAIT_FLASH,
    SWALOT_PLOP_SPEED_UP_START_RESTORE,
    SWALOT_PLOP_SPEED_UP_WAIT_RESTORE,
    SWALOT_PLOP_SPEED_UP_END,
};

enum SwalotPlopCommandId {
    SWALOT_PLOP_CMD_START = COMM_BASE_COMMAND_COUNT,
    SWALOT_PLOP_CMD_FINISH,
    SWALOT_PLOP_CMD_SHOT,
    SWALOT_PLOP_CMD_FINAL_SCORE,
    SWALOT_PLOP_CMD_SCORES,
    SWALOT_PLOP_CMD_LEVEL,
    SWALOT_PLOP_CMD_SCORE_REPORT,
    SWALOT_PLOP_CMD_SYNC,
};

static void SwalotPlop_ReceiveStart(SwalotPlopApp *app);
static void SwalotPlop_ReceiveFinish(SwalotPlopApp *app);
static void SwalotPlop_TallyShot(SwalotPlopApp *app, const SwalotPlopShotRecord *record, int netId);
static void SwalotPlop_ReceiveShot(SwalotPlopApp *app, const SwalotPlopShotRecord *record, int netId);
static void SwalotPlop_ReceiveFinalScore(SwalotPlopApp *app, u32 score, int netId);
static void SwalotPlop_ReceiveScores(SwalotPlopApp *app, const u32 *scores, int netId);
static void SwalotPlop_ReceiveLevel(SwalotPlopApp *app, u32 level);
static void SwalotPlop_ReceiveScoreReport(SwalotPlopApp *app, u32 score, int netId);
static void SwalotPlop_ReceiveSync(SwalotPlopApp *app);
static void SwalotPlop_VBlankCallback(void *data);
static void SwalotPlop_SendShots(SwalotPlopApp *app);
static void SwalotPlop_SendScoreReport(SwalotPlopApp *app);
static void SwalotPlop_LoadSharedData(SwalotPlopApp *app, enum HeapID heapId);
static void SwalotPlop_FreeSharedData(SwalotPlopApp *app);
static void SwalotPlop_Abort(SwalotPlopApp *app, PlazaGameArgs *args);
static BOOL SwalotPlop_SendCommand(SwalotPlopApp *app, int command, void *data, int size);
static SwalotPlopTally *SwalotPlopTally_New(enum HeapID heapId, int duration, u32 playerCount, SwalotPlopShared *shared);
static void SwalotPlopTally_Free(SwalotPlopTally *tally);
static BOOL SwalotPlopTally_Update(SwalotPlopTally *tally);
static u8 SwalotPlopTally_LevelChanged(SwalotPlopTally *tally);
static void SwalotPlopTally_ClearLevelChanged(SwalotPlopTally *tally);
static u8 SwalotPlopTally_GetLevel(SwalotPlopTally *tally);
static void SwalotPlopTally_AddShot(SwalotPlopTally *tally, const SwalotPlopShotRecord *record, int index);
static void SwalotPlopTally_SetScore(SwalotPlopTally *tally, u32 score, int index);
static BOOL SwalotPlopTally_HasAllScores(SwalotPlopTally *tally);
static void SwalotPlopTally_GetScores(SwalotPlopTally *tally, u32 *scores);
static u32 SwalotPlopTally_CountScores(SwalotPlopTally *tally);
static SwalotPlopGame *SwalotPlopGame_New(enum HeapID heapId, u32 duration, u32 playerCount, u32 localIndex, SwalotPlopShared *shared);
static void SwalotPlopGame_Free(SwalotPlopGame *game);
static BOOL SwalotPlopGame_RunIntro(SwalotPlopGame *game, BOOL ready);
static BOOL SwalotPlopGame_RunFinish(SwalotPlopGame *game, BOOL ready);
static BOOL SwalotPlopGame_Update(SwalotPlopGame *game);
static void SwalotPlopGame_VBlank(SwalotPlopGame *game);
static u32 SwalotPlopGame_GetFrame(SwalotPlopGame *game);
static void SwalotPlopGame_AddRemoteShot(SwalotPlopGame *game, const SwalotPlopShotRecord *record);
static BOOL SwalotPlopGame_TakeShotReport(SwalotPlopGame *game, SwalotPlopShotRecord *record);
static void SwalotPlopGame_SetFinalScores(SwalotPlopGame *game, const u32 *scores);
static u32 SwalotPlopGame_GetScore(SwalotPlopGame *game);
static void SwalotPlopGame_SetLevel(SwalotPlopGame *game, u32 level);
static void SwalotPlopGame_SetScoreReport(SwalotPlopGame *game, u32 score, int index);
static void SwalotPlopGame_SetOptions(SwalotPlopGame *game, const u8 *options);
static void SwalotPlopCurve_Init(SwalotPlopCurve *curve, s32 start, s32 end, s32 slope, s32 duration);
static BOOL SwalotPlopCurve_Update(SwalotPlopCurve *curve);
static void SwalotPlop_GetPopupPos(int seat, int playerCount, int index, u32 *x, u32 *y);
static void SwalotPlopGame_ThrowDemoShot(SwalotPlopGame *game, s32 x, s32 y, s32 endX, u16 endY, int state);
static u32 SwalotPlopGame_GetHitPoints(SwalotPlopGame *game);
static void SwalotPlopPopups_Init(SwalotPlopPopups *popups, SwalotPlopGfx *gfx, int playerCount, int localIndex, enum HeapID heapId);
static void SwalotPlopPopups_Free(SwalotPlopPopups *popups, SwalotPlopGfx *gfx);
static void SwalotPlopPopups_Show(SwalotPlopPopups *popups, int seat, int combo, int phase);
static void SwalotPlopPopups_Update(SwalotPlopPopups *popups);
static void SwalotPlopPopups_InitSeat(SwalotPlopPopups *popups, SpriteTemplate *template, SwalotPlopGfx *gfx, int playerCount, int seat, int localIndex, enum HeapID heapId);
static void SwalotPlopPopups_FreeSeat(SwalotPlopPopups *popups);
static void SwalotPlopPopups_ShowSeat(SwalotPlopPopups *popups, int seat, u32 combo, int phase);
static void SwalotPlopPopups_UpdateSeat(SwalotPlopPopups *popups);
static void SwalotPlopPopups_SetPos(SwalotPlopPopups *popups, int i);
static void SwalotPlopPopups_HideSprite(SwalotPlopPopups *popups, int i);
static void SwalotPlopIntro_Init(SwalotPlopIntro *intro, SwalotPlopGfx *gfx, SwalotPlopShared *shared, u32 playerCount, u32 localIndex, NARC *narc, enum HeapID heapId);
static void SwalotPlopIntro_Free(SwalotPlopIntro *intro, SwalotPlopGfx *gfx);
static void SwalotPlopIntro_ShowWindow(SwalotPlopIntro *intro, SwalotPlopGfx *gfx);
static BOOL SwalotPlopIntro_IsCountingDown(SwalotPlopIntro *intro, SwalotPlopGfx *gfx);
static void SwalotPlopIntro_HideWindow(SwalotPlopIntro *intro, SwalotPlopGfx *gfx);
static void SwalotPlopIntro_StartScene(SwalotPlopIntro *intro, SwalotPlopGfx *gfx);
static BOOL SwalotPlopIntro_Wait(SwalotPlopIntro *intro, SwalotPlopGfx *gfx);
static void SwalotPlopGfx_SetScenePos(SwalotPlopGfx *gfx, SwalotPlopBody *body);
static void SwalotPlopFinish_Init(SwalotPlopGfx *gfx, u32 playerCount, u32 localIndex, enum HeapID heapId);
static void SwalotPlopGfx_FreeDummy1(SwalotPlopGfx *gfx, u32 playerCount);
static void SwalotPlopFinish_Start(SwalotPlopGfx *gfx);
static BOOL SwalotPlopFinish_Update(SwalotPlopGfx *gfx);
static BOOL SwalotPlopFinish_IsSignDone(SwalotPlopGfx *gfx);
static void SwalotPlopBody_Init(SwalotPlopGame *game, SwalotPlopBody *body);
static void SwalotPlopBody_Update(SwalotPlopGame *game, SwalotPlopBody *body);
static void SwalotPlopBody_SetActive(SwalotPlopBody *body, u8 active);
static void SwalotPlopBody_SetTilt(SwalotPlopBody *body, int tilt);
static void SwalotPlopBody_SetSpin(SwalotPlopBody *body, u16 spin);
static int SwalotPlopBody_CheckMouth(SwalotPlopBody *body, const VecFx32 *position, VecFx32 *move, VecFx32 *push);
static BOOL SwalotPlopBody_IsInside(SwalotPlopBody *body, const VecFx32 *pos);
static fx32 SwalotPlopBody_CalcMouthDepth(SwalotPlopBody *body, s32 tilt);
static fx32 SwalotPlopBody_CalcHeight(SwalotPlopBody *body, s32 tilt);
static void SwalotPlopBody_SetScale(SwalotPlopBody *body, fx32 scale);
static void SwalotPlopBody_SetSpinSpeed(SwalotPlopBody *body, u16 spinSpeed);
static void SwalotPlopBody_SetMouthOffset(SwalotPlopBody *body, u32 offset);
static void SwalotPlopShot_ToRecord(const SwalotPlopShotParams *params, SwalotPlopShotRecord *record);
static void SwalotPlopShot_FromRecord(const SwalotPlopShotRecord *record, SwalotPlopShotParams *params);
static void SwalotPlopGame_AddShot(SwalotPlopGame *game, const SwalotPlopShotParams *params, u32 state);
static void SwalotPlopShot_End(SwalotPlopGame *game, SwalotPlopShot *shot, u16 caught, u32 combo);
static void SwalotPlopShot_Clear(SwalotPlopShot *shot);
static void SwalotPlopGame_UpdateShots(SwalotPlopGame *game);
static void SwalotPlopGame_UpdateFlyingShot(SwalotPlopGame *game, SwalotPlopShot *shot);
static void SwalotPlopGame_RemoveEndedShots(SwalotPlopGame *game);
static void SwalotPlopGame_DropEndedShots(SwalotPlopGame *game);
static void SwalotPlopShot_CheckRemove(SwalotPlopShot *shot, SwalotPlopGame *game);
static void SwalotPlopGame_DropFlyingShots(SwalotPlopGame *game);
static BOOL SwalotPlopGame_HasActiveShots(SwalotPlopGame *game);
static fx32 SwalotPlopShot_GetSpeed(SwalotPlopShot *shot);
static void SwalotPlopShot_GetMove(SwalotPlopShot *shot, VecFx32 *move);
static void SwalotPlopShot_Bounce(SwalotPlopShot *shot, SwalotPlopGame *game, BOOL bounce);
static void SwalotPlopShot_BounceEx(SwalotPlopShot *shot, SwalotPlopGame *game, BOOL reverse, fx32 damping, BOOL jitter);
static void SwalotPlopGame_UpdateBouncedShot(SwalotPlopShot *shot, SwalotPlopGame *game);
static BOOL SwalotPlopShot_IsOnRim(SwalotPlopShot *shot);
static BOOL SwalotPlopShot_IsInPool(SwalotPlopShot *shot);
static BOOL SwalotPlopShot_CheckOutside(SwalotPlopShot *shot);
static void SwalotPlopShot_Catch(SwalotPlopShot *shot, SwalotPlopGame *game);
static void SwalotPlopGame_UpdateCaughtShot(SwalotPlopShot *shot, SwalotPlopGame *game);
static void SwalotPlopShot_Move(SwalotPlopShot *shot, const VecFx32 *move);
static void SwalotPlopGame_HandleTouch(SwalotPlopGame *game);
static void SwalotPlopGame_Throw(SwalotPlopGame *game);
static void SwalotPlopGame_TrackFlick(SwalotPlopGame *game);
static BOOL SwalotPlopGame_CanStartFlick(SwalotPlopGame *game);
static void SwalotPlopFlickQueue_Clear(SwalotPlopFlickQueue *queue);
static void SwalotPlopFlickQueue_Push(SwalotPlopFlickQueue *queue, SwalotPlopFlick point);
static BOOL SwalotPlopFlickQueue_Pop(SwalotPlopFlickQueue *queue, SwalotPlopFlick *point);
static BOOL SwalotPlopFlickQueue_PeekFirst(SwalotPlopFlickQueue *queue, SwalotPlopPoint *point);
static BOOL SwalotPlopFlickQueue_PeekLast(SwalotPlopFlickQueue *queue, SwalotPlopPoint *point);
static u32 SwalotPlopFlickQueue_Count(SwalotPlopFlickQueue *queue);
static void SwalotPlopGame_EndFlick(SwalotPlopGame *game);
static void SwalotPlopLevelBlend_Init(SwalotPlopLevelBlend *blend, SwalotPlopShared *shared, const u8 *options);
static void SwalotPlopLevelBlend_Start(SwalotPlopLevelBlend *blend, SwalotPlopShared *shared, const u8 *options, int from, u16 to);
static BOOL SwalotPlopLevelBlend_Update(SwalotPlopLevelBlend *blend);
static void SwalotPlopGame_ApplyLevel(SwalotPlopGame *game, SwalotPlopLevelBlend *blend);
static void SwalotPlopGame_ApplyLevelParams(SwalotPlopGame *game, SwalotPlopLevelBlend *blend);
static void SwalotPlop_GetLevel(SwalotPlopShared *shared, const u8 *options, int index, SwalotPlopLevel *level);
static void SwalotPlopDemo_Init(SwalotPlopDemo *demo, SwalotPlopGfx *gfx, enum HeapID heapId);
static void SwalotPlopDemo_Free(SwalotPlopDemo *demo, SwalotPlopGfx *gfx);
static void SwalotPlopDemo_Start(SwalotPlopDemo *demo);
static BOOL SwalotPlopDemo_Update(SwalotPlopDemo *demo, SwalotPlopGame *game);
static void SwalotPlopScoreReports_Init(SwalotPlopScoreReports *reports);
static void SwalotPlopScoreReports_Set(SwalotPlopScoreReports *reports, u32 score, u32 index);
static void SwalotPlopScoreReports_Reset(SwalotPlopScoreReports *reports);
static u32 SwalotPlopScoreReports_GetLeader(SwalotPlopScoreReports *reports);
static BOOL SwalotPlopScoreReports_HasAll(SwalotPlopScoreReports *reports, u32 playerCount);
static void SwalotPlopGfx_SetBanks(void);
static void SwalotPlopGfx_Setup3D(void);
static void SwalotPlopGfx_Init3D(SwalotPlopGfx *gfx, int playerCount, int localIndex, enum HeapID heapId);
static void SwalotPlopGfx_Free3D(SwalotPlopGfx *gfx);
static void SwalotPlopGame_InitGfx(SwalotPlopGame *game, enum HeapID heapId);
static void SwalotPlopGame_FreeGfx(SwalotPlopGame *game);
static void SwalotPlopGame_UpdateGfx(SwalotPlopGame *game, SwalotPlopGfx *gfx);
static void SwalotPlopGame_Draw(SwalotPlopGame *game, SwalotPlopGfx *gfx);
static void SwalotPlopGfx_InitBgs(BgConfig **bgConfig, enum HeapID heapId);
static void SwalotPlopGfx_FreeBgs(SwalotPlopGfx *gfx);
static void SwalotPlopGfx_InitSprites(SwalotPlopGfx *gfx, enum HeapID heapId);
static void SwalotPlopGfx_FreeSprites(SwalotPlopGfx *gfx);
static void SwalotPlopGfx_LoadSpriteRes(SwalotPlopGfx *gfx, NARC *narc, enum HeapID heapId);
static void SwalotPlopGfx_FreeSpriteRes(SwalotPlopGfx *gfx);
static void SwalotPlopGfx_InitText(SwalotPlopGfx *gfx, enum HeapID heapId);
static void SwalotPlopGfx_FreeText(SwalotPlopGfx *gfx);
static void SwalotPlopGfx_InitCamera(SwalotPlopGfx *gfx, int playerCount, int localIndex, enum HeapID heapId);
static void SwalotPlopGfx_FreeCamera(SwalotPlopGfx *gfx);
static void SwalotPlopGfx_ApplyCamera(SwalotPlopGfx *gfx);
static void SwalotPlopGfx_LoadSubBgs(SwalotPlopGfx *gfx, NARC *narc, int localIndex, enum HeapID heapId);
static void SwalotPlopGfx_FreeDummy2(SwalotPlopGfx *gfx);
static inline const NNSG3dResName *GetResNameByIdx(const NNSG3dResDict *dict, u32 idx);
static void SwalotPlopScene_Init(SwalotPlopScene *scene, NARC *narc, enum HeapID heapId, NNSFndAllocator *allocator);
static void SwalotPlopScene_Free(SwalotPlopScene *scene, NNSFndAllocator *allocator);
static void SwalotPlopScene_Update(SwalotPlopScene *scene, SwalotPlopBody *body, u32 phase);
static void SwalotPlopScene_Animate(SwalotPlopScene *scene, SwalotPlopBody *body, u32 phase);
static void SwalotPlopScene_SetPos(SwalotPlopScene *scene, SwalotPlopBody *body);
static void SwalotPlopScene_SetSpin(SwalotPlopScene *scene, SwalotPlopBody *body);
static void SwalotPlopScene_SetScale(SwalotPlopScene *scene, SwalotPlopBody *body);
static void SwalotPlopScene_Draw(SwalotPlopScene *scene, SwalotPlopBody *body);
static void SwalotPlopScene_SetTiltFrame(SwalotPlopScene *scene, u16 tilt);
static void SwalotPlopScene_SetAnimation(SwalotPlopScene *scene, u32 animation);
static BOOL SwalotPlopScene_AdvanceAnimation(SwalotPlopScene *scene, fx32 step);
static void SwalotPlopScene_SetBaseAnimation(SwalotPlopScene *scene, int state);
static void SwalotPlopScene_SetBonus(SwalotPlopScene *scene, u16 bonus);
static void SwalotPlopScene_SetLeader(SwalotPlopScene *scene, u32 leader);
static void SwalotPlopScene_UpdateSpecial(SwalotPlopScene *scene);
static void SwalotPlop_TouchToWorld(int x, int y, VecFx32 *pos);
static void SwalotPlop_WorldToTouch(const VecFx32 *pos, SwalotPlopPoint *point);
static void SwalotPlop_RotateToSeat(const VecFx32 *src, VecFx32 *dst, int seat, int playerCount);
static void SwalotPlop_RotateFromSeat(const VecFx32 *src, VecFx32 *dst, int seat, int playerCount);
static void SwalotPlopGfx_InitShotViews(SwalotPlopGfx *gfx, NARC *narc, enum HeapID heapId);
static void SwalotPlopGfx_FreeShotViews(SwalotPlopGfx *gfx);
static void SwalotPlopGfx_AddShotView(SwalotPlopGfx *gfx, SwalotPlopShot *shot, u32 playerCount, u32 localIndex);
static void SwalotPlopGfx_RemoveShotView(SwalotPlopGfx *gfx, SwalotPlopShot *shot);
static void SwalotPlopGfx_DrawShotViews(SwalotPlopGfx *gfx, u32 playerCount);
static void SwalotPlopShotView_Init(SwalotPlopGfx *gfx, SwalotPlopShotView *view, enum HeapID heapId);
static void SwalotPlopShotView_Free(SwalotPlopGfx *gfx, SwalotPlopShotView *view);
static void SwalotPlopShotView_StartSprite(SwalotPlopGfx *gfx, SwalotPlopShotView *view, SwalotPlopShot *shot, u32 playerCount);
static void SwalotPlopShotView_StartModel(SwalotPlopGfx *gfx, SwalotPlopShotView *view, SwalotPlopShot *shot);
static void SwalotPlopShotView_Draw(SwalotPlopShotView *view, SwalotPlopGfx *gfx, u32 playerCount);
static void SwalotPlopShotView_PlaceShadow(SwalotPlopShotView *view);
static void SwalotPlopShotView_ResetPolygonAttr(SwalotPlopShotView *view);
static void SwalotPlopShotView_Clear(SwalotPlopShotView *view);
static SwalotPlopShotView *SwalotPlopGfx_GetFreeShotView(SwalotPlopGfx *gfx);
static BOOL SwalotPlopShotView_UpdateSprite(SwalotPlopShotView *view, SwalotPlopShot *shot, u32 playerCount);
static void SwalotPlopShotView_SwitchToModel(SwalotPlopShotView *view, SwalotPlopGfx *gfx);
static ModelResource *SwalotPlop_GetShotModel(SwalotPlopShot *shot, ModelResource *models);
static inline s64 SinCosComponent(s32 value, s32 scale);
static void SwalotPlopShotView_Spin(SwalotPlopShotView *view);
static void SwalotPlopCursor_Init(SwalotPlopGfx *gfx, SwalotPlopCursor *cursor, int palette, enum HeapID heapId);
static void SwalotPlopCursor_Free(SwalotPlopCursor *cursor);
static void SwalotPlopCursor_Show(SwalotPlopCursor *cursor);
static void SwalotPlopCursor_Update(SwalotPlopCursor *cursor);
static void SwalotPlopCursor_SetPos(SwalotPlopCursor *cursor, int x, int y);
static void SwalotPlopCursor_Hide(SwalotPlopCursor *cursor);
static void SwalotPlopParticles_Init(SwalotPlopGfx *gfx, SwalotPlopParticle *particle, int palette, enum HeapID heapId);
static void SwalotPlopParticles_Free(SwalotPlopGfx *gfx, SwalotPlopParticle *particle);
static void SwalotPlopParticles_Update(SwalotPlopParticle *particles);
static void SwalotPlopParticles_Spawn(SwalotPlopParticle *particles, s32 x, s32 y);
static inline int FxMul(int a, int b);
static void SwalotPlop_CalcArcStep(int step, int steps, int radius, int startAngle, int endAngle, int *x, int *y);
static void SwalotPlopStage_Init(SwalotPlopStage *stage, SwalotPlopGfx *gfx, NARC *narc, int playerCount, int localIndex, enum HeapID heapId, NNSFndAllocator *allocator);
static void SwalotPlopStage_Free(SwalotPlopStage *stage, SwalotPlopGfx *gfx, NNSFndAllocator *allocator);
static void SwalotPlopStage_Draw(SwalotPlopStage *stage);
static void SwalotPlopStage_StartMouth(SwalotPlopStage *stage);
static void SwalotPlopStage_SetAnimSpeed(SwalotPlopStage *stage, fx32 speed);
static void SwalotPlop_IntersectPlane(const VecFx32 *pos, const VecFx32 *dir, const VecFx32 *normal, fx32 planeDist, VecFx32 *hit, fx32 *t);
static void SwalotPlopGfx_SetCameraHeight(SwalotPlopGfx *gfx, fx32 height);
static void SwalotPlopGfx_SetCameraPitch(SwalotPlopGfx *gfx, u16 pitch);
static void SwalotPlopBgFade_Init(SwalotPlopBgFade *fade, u8 palette);
static void SwalotPlopBgFade_Update(SwalotPlopBgFade *fade, SwalotPlopGfx *gfx);
static void SwalotPlopBgFade_Step(SwalotPlopBgFade *fade);
static void SwalotPlopBgFade_SetLayers(SwalotPlopGfx *gfx, int top, int palette);
static void SwalotPlopCombo_Init(SwalotPlopCombo *combo, SwalotPlopGfx *gfx, NARC *narc, enum HeapID heapId);
static void SwalotPlopCombo_Free(SwalotPlopCombo *combo, SwalotPlopGfx *gfx);
static void SwalotPlopCombo_Show(SwalotPlopCombo *combo, SwalotPlopGfx *gfx, int count);
static void SwalotPlopCombo_Hide(SwalotPlopCombo *combo);
static void SwalotPlopCombo_SetCount(SwalotPlopCombo *combo, SwalotPlopGfx *gfx, int count);
static void SwalotPlopCombo_Update(SwalotPlopCombo *combo);
static void SwalotPlopSpeedUp_Start(SwalotPlopSpeedUp *speedUp);
static void SwalotPlopSpeedUp_Update(SwalotPlopSpeedUp *speedUp, SwalotPlopStage *stage, SwalotPlopScene *scene);
static void SwalotPlopSpeedUp_ResetTempo(SwalotPlopSpeedUp *speedUp);
static const SwalotPlopCommand *SwalotPlop_GetCommandTable(void);
static int SwalotPlop_GetCommandCount(void);
static void SwalotPlop_RegisterCommands(SwalotPlopApp *app);

const u8 sSwalotPlopModelIds[] = { 0, 0, 0, 0 };

static const u8 sSwalotPlopSubBgPalettes[] = { 0, 9, 10, 11 };

static const u8 sSwalotPlopShotModelFiles[] = { 41, 42, 43, 44, 45, 0, 0, 0 };

static TPData sSwalotPlopTouchpadBuffer[8];

BOOL SwalotPlop_Init(OverlayManager *man, int *state) {
    PlazaGameArgs *args = OverlayManager_GetArgs(man);
    SwalotPlopApp *app;
    Heap_Create(HEAP_ID_3, HEAP_ID_106, 3 << 17);
    app = OverlayManager_CreateAndGetData(man, sizeof(SwalotPlopApp), HEAP_ID_106);
    memset(app, 0, sizeof(SwalotPlopApp));
    PlazaGameRoster_Init(&app->roster, args->fromLobby, args->saveData, args->voiceChat, args);
    SwalotPlop_LoadSharedData(app, HEAP_ID_106);
    return TRUE;
}

BOOL SwalotPlop_Main(OverlayManager *man, int *state) {
    SwalotPlopApp *app = OverlayManager_GetData(man);
    PlazaGameArgs *args = OverlayManager_GetArgs(man);

    struct {
        u32 level;
        u32 score;
        u8 options[4];
        u32 scores[PLAZA_GAME_MAX_PLAYERS];
    } msg;

    if (PlazaGameRoster_CheckConnectionLost(&app->roster) == TRUE) {
        switch (app->connectionLostState) {
        case SWALOT_PLOP_CONNECTION_LOST_BLACK_OUT:
            if (IsPaletteFadeFinished() == TRUE) {
                StopPaletteFade();
            }
            SetBothScreensBrightnessByColor(0);
            app->connectionLostState++;
            break;
        case SWALOT_PLOP_CONNECTION_LOST_WAIT:
            if (PlazaGameRoster_ProcessConnectionLost(&app->roster) == TRUE) {
                app->connectionLostState++;
            }
            break;
        case SWALOT_PLOP_CONNECTION_LOST_ABORT:
            SwalotPlop_Abort(app, args);
            return TRUE;
        default:
            GF_ASSERT(FALSE);
            return TRUE;
        }
        return FALSE;
    }

    switch (*state) {
    case SWALOT_PLOP_STATE_CREATE_ENTRY:
        app->entry = PlazaGameEntry_NewSwalotPlop(&app->roster, HEAP_ID_106);
        (*state)++;
        break;
    case SWALOT_PLOP_STATE_WAIT_ENTRY:
        if (PlazaGameEntry_IsDone(app->entry) == TRUE) {
            app->selection = PlazaGameEntry_GetSelection(app->entry);
            PlazaGameEntry_Free(app->entry);
            app->entry = NULL;
            (*state)++;
        }
        break;
    case SWALOT_PLOP_STATE_INIT_GAME:
        if (gSystem.heldKeys & PAD_BUTTON_A) {
            break;
        }
        Main_SetVBlankIntrCB(SwalotPlop_VBlankCallback, app);
        HBlankInterruptDisable();
        app->startReceived = FALSE;
        app->finishReceived = FALSE;
        app->scoresReceived = FALSE;
        {
            u8 *first = &app->syncCounts[0];
            u8 *counts = &app->syncCounts[0];
            *first = 0;
            counts[1] = 0;
            counts[2] = 0;
            counts[3] = 0;
        }
        GF_CreateVramTransferManager(0x20, HEAP_ID_106);
        SwalotPlop_RegisterCommands(app);
        app->canSend = TRUE;
        app->playerCount = CommPlayerInfo_CountAccepted();
        GF_ASSERT(app->playerCount > 1);
        app->netId = Comm_GetLocalNetId();
        app->localIndex = PlazaGameRoster_GetIndexOfNetId(&app->roster, app->netId);
        if (app->netId == 0) {
            app->tally = SwalotPlopTally_New(HEAP_ID_106, 0x4B0, app->playerCount, &app->shared);
            app->syncRound = 1;
        }
        app->game = SwalotPlopGame_New(HEAP_ID_106, 0x4B0, app->playerCount, app->localIndex, &app->shared);
        WirelessStatusIcon_Create();
        if (args->voiceChat != 0) {
            WifiConnection_StartVoiceChat(HEAP_ID_106);
        }
        switch (app->selection) {
        case 0:
            msg.options[0] = 0;
            msg.options[1] = 0;
            break;
        case 1:
            msg.options[0] = 0;
            msg.options[1] = 1;
            break;
        case 2:
            msg.options[0] = 1;
            msg.options[1] = 0;
            break;
        default:
            GF_ASSERT(FALSE);
            break;
        }
        SwalotPlopGame_SetOptions(app->game, msg.options);
        LinkSync_StartSync(1);
        (*state)++;
        break;
    case SWALOT_PLOP_STATE_FADE_IN:
        if (LinkSync_IsSynced(1)) {
            BeginNormalPaletteFade(0, 0x1B, 0x1B, 0xFFFF, 6, 1, HEAP_ID_106);
            (*state)++;
        }
        break;
    case SWALOT_PLOP_STATE_SEND_START:
        SwalotPlopGame_RunIntro(app->game, FALSE);
        if (IsPaletteFadeFinished()) {
            if (app->netId == 0) {
                if (Comm_SendCommand(SWALOT_PLOP_CMD_START, NULL, 0)) {
                    (*state)++;
                }
            } else {
                (*state)++;
            }
        }
        break;
    case SWALOT_PLOP_STATE_WAIT_START:
        SwalotPlopGame_RunIntro(app->game, FALSE);
        if (app->startReceived) {
            (*state)++;
        }
        break;
    case SWALOT_PLOP_STATE_RUN_INTRO:
        if (!SwalotPlopGame_RunIntro(app->game, TRUE)) {
            (*state)++;
        }
        break;
    case SWALOT_PLOP_STATE_PLAY:
        if (app->finishReceived) {
            msg.score = SwalotPlopGame_GetScore(app->game);
            if (Comm_SendCommand(SWALOT_PLOP_CMD_FINAL_SCORE, &msg.score, sizeof(msg.score))) {
                (*state)++;
            }
        } else {
            if (app->netId == 0) {
                u32 running = SwalotPlopTally_Update(app->tally);
                if (SwalotPlopTally_LevelChanged(app->tally) != 0) {
                    msg.level = SwalotPlopTally_GetLevel(app->tally);
                    if (SwalotPlop_SendCommand(app, SWALOT_PLOP_CMD_LEVEL, &msg.level, sizeof(msg.level)) == TRUE) {
                        SwalotPlopTally_ClearLevelChanged(app->tally);
                    }
                }
                if (running == 0) {
                    SwalotPlop_SendCommand(app, SWALOT_PLOP_CMD_FINISH, NULL, 0);
                }
            }
            SwalotPlopGame_Update(app->game);
            SwalotPlop_SendShots(app);
            SwalotPlop_SendScoreReport(app);
        }
        break;
    case SWALOT_PLOP_STATE_SEND_SCORES:
        SwalotPlopGame_RunFinish(app->game, FALSE);
        if (app->netId == 0) {
            if (SwalotPlopTally_HasAllScores(app->tally) == TRUE) {
                SwalotPlopTally_GetScores(app->tally, msg.scores);
                if (Comm_SendCommand(SWALOT_PLOP_CMD_SCORES, msg.scores, sizeof(msg.scores))) {
                    (*state)++;
                }
            }
        } else {
            (*state)++;
        }
        break;
    case SWALOT_PLOP_STATE_WAIT_SCORES:
        SwalotPlopGame_RunFinish(app->game, FALSE);
        if (app->scoresReceived == TRUE) {
            (*state)++;
        }
        break;
    case SWALOT_PLOP_STATE_RUN_FINISH:
        if (!SwalotPlopGame_RunFinish(app->game, TRUE)) {
            (*state)++;
        }
        break;
    case SWALOT_PLOP_STATE_FADE_OUT:
        BeginNormalPaletteFade(0, 0x1A, 0x1A, 0xFFFF, 6, 1, HEAP_ID_106);
        SwalotPlopGame_RunFinish(app->game, TRUE);
        (*state)++;
        break;
    case SWALOT_PLOP_STATE_WAIT_FADE_OUT:
        SwalotPlopGame_RunFinish(app->game, TRUE);
        if (IsPaletteFadeFinished()) {
            LinkSync_StartSync(3);
            (*state)++;
        }
        break;
    case SWALOT_PLOP_STATE_FREE_GAME:
        if (!LinkSync_IsSynced(3)) {
            return FALSE;
        }
        if (args->voiceChat != 0) {
            WifiConnection_StopVoiceChat();
        }
        if (app->netId == 0) {
            SwalotPlopTally_Free(app->tally);
            app->tally = NULL;
        }
        SwalotPlopGame_Free(app->game);
        app->game = NULL;
        Main_SetVBlankIntrCB(NULL, NULL);
        HBlankInterruptDisable();
        GF_DestroyVramTransferManager();
        (*state)++;
        break;
    case SWALOT_PLOP_STATE_SHOW_RESULTS:
        PlazaGame_CalcRanks(&app->resultData, app->roster.count);
        app->results = PlazaGameResults_NewSwalotPlop(&app->roster, &app->resultData, HEAP_ID_106);
        (*state)++;
        break;
    case SWALOT_PLOP_STATE_WAIT_RESULTS:
        if (PlazaGameResults_IsDone(app->results) == TRUE) {
            u32 keepPlaying = PlazaGameResults_GetContinue(app->results);
            PlazaGameResults_Free(app->results);
            app->results = NULL;
            if (keepPlaying == 0) {
                return TRUE;
            }
            *state = SWALOT_PLOP_STATE_CREATE_ENTRY;
        }
        break;
    }
    return FALSE;
}

BOOL SwalotPlop_Exit(OverlayManager *man, int *state) {
    SwalotPlopApp *app = OverlayManager_GetData(man);
    BOOL connectionLost;
    OverlayManager_GetArgs(man);
    switch (*state) {
    case SWALOT_PLOP_EXIT_STATE_FREE:
        connectionLost = PlazaGameRoster_CheckConnectionLost(&app->roster);
        SwalotPlop_FreeSharedData(app);
        OverlayManager_FreeData(man);
        Heap_Destroy(HEAP_ID_106);
        CommState_SetErrorWatch(FALSE, TRUE);
        if (connectionLost == TRUE) {
            return TRUE;
        }
        LinkSync_StartSync(4);
        (*state)++;
        break;
    case SWALOT_PLOP_EXIT_STATE_WAIT_SYNC:
        if (LinkSync_IsSynced(4) || Comm_CountConnected() < CommPlayerInfo_CountAccepted()) {
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static void SwalotPlop_ReceiveStart(SwalotPlopApp *app) {
    app->startReceived = TRUE;
}

static void SwalotPlop_ReceiveFinish(SwalotPlopApp *app) {
    app->finishReceived = TRUE;
}

static void SwalotPlop_TallyShot(SwalotPlopApp *app, const SwalotPlopShotRecord *record, int netId) {
    int index = PlazaGameRoster_GetIndexOfNetId(&app->roster, netId);
    SwalotPlopTally_AddShot(app->tally, record, index);
}

static void SwalotPlop_ReceiveShot(SwalotPlopApp *app, const SwalotPlopShotRecord *record, int netId) {
    SwalotPlopGame_AddRemoteShot(app->game, record);
}

static void SwalotPlop_ReceiveFinalScore(SwalotPlopApp *app, u32 score, int netId) {
    int index = PlazaGameRoster_GetIndexOfNetId(&app->roster, netId);
    SwalotPlopTally *tally = app->tally;
    if (tally != NULL) {
        SwalotPlopTally_SetScore(tally, score, index);
    }
}

static void SwalotPlop_ReceiveScores(SwalotPlopApp *app, const u32 *scores, int netId) {
    SwalotPlopGame_SetFinalScores(app->game, scores);
    app->scoresReceived = TRUE;
    for (int i = 0; i < PLAZA_GAME_MAX_PLAYERS; i++) {
        app->resultData.scores[i] = *scores;
        scores++;
    }
}

static void SwalotPlop_ReceiveLevel(SwalotPlopApp *app, u32 level) {
    SwalotPlopGame_SetLevel(app->game, level);
}

static void SwalotPlop_ReceiveScoreReport(SwalotPlopApp *app, u32 score, int netId) {
    int index = PlazaGameRoster_GetIndexOfNetId(&app->roster, netId);
    u32 i;
    BOOL allReported;
    SwalotPlopGame_SetScoreReport(app->game, score, index);
    app->syncCounts[index]++;
    if (app->netId == 0) {
        allReported = TRUE;
        for (i = 0; i < app->playerCount; i++) {
            if (app->syncRound > app->syncCounts[i]) {
                allReported = FALSE;
            }
        }
        if (allReported == TRUE) {
            app->syncRound++;
            Comm_SendCommand(SWALOT_PLOP_CMD_SYNC, NULL, 0);
        }
    }
}

static void SwalotPlop_ReceiveSync(SwalotPlopApp *app) {
    app->canSend = TRUE;
}

static void SwalotPlop_VBlankCallback(void *data) {
    SwalotPlopApp *app = data;
    SwalotPlopGame_VBlank(app->game);
}

static void SwalotPlop_SendShots(SwalotPlopApp *app) {
    SwalotPlopShotRecord record;
    if (SwalotPlopGame_TakeShotReport(app->game, &record) == TRUE) {
        do {
            SwalotPlop_SendCommand(app, SWALOT_PLOP_CMD_SHOT, &record, sizeof(SwalotPlopShotRecord));
        } while (SwalotPlopGame_TakeShotReport(app->game, &record) == TRUE);
    }
}

static void SwalotPlop_SendScoreReport(SwalotPlopApp *app) {
    int frame = SwalotPlopGame_GetFrame(app->game);
    if (frame != 0 && frame % 30 == 0 && app->canSend == TRUE) {
        u32 score = SwalotPlopGame_GetScore(app->game);
        if (!SwalotPlop_SendCommand(app, SWALOT_PLOP_CMD_SCORE_REPORT, &score, sizeof(score))) {
            GF_ASSERT(FALSE);
        }
        app->canSend = FALSE;
    }
}

static void SwalotPlop_LoadSharedData(SwalotPlopApp *app, enum HeapID heapId) {
    u32 size;
    int i;
    app->shared.levelTable.levels = GfGfxLoader_LoadFromNarc_GetSizeOut(NARC_a_2_1_0, 0, FALSE, (enum HeapID)heapId, FALSE, &size);
    app->shared.levelTable.count = size / sizeof(SwalotPlopLevel);
    i = 0;
    if (i < app->roster.count) {
        do {
            app->shared.profiles[i] = PlazaGameRoster_GetProfile(&app->roster, i);
            app->shared.nameFlags[i] = PlazaGameRoster_IsSpecialVisitor(&app->roster, i);
            i++;
        } while (i < app->roster.count);
    }
}

static void SwalotPlop_FreeSharedData(SwalotPlopApp *app) {
    Heap_Free(app->shared.levelTable.levels);
    app->shared.levelTable.levels = NULL;
    app->shared.levelTable.count = 0;
}

static void SwalotPlop_Abort(SwalotPlopApp *app, PlazaGameArgs *args) {
    Main_SetVBlankIntrCB(NULL, NULL);
    HBlankInterruptDisable();
    if (app->entry != NULL) {
        PlazaGameEntry_Free(app->entry);
        app->entry = NULL;
    }
    if (app->game != NULL) {
        if (app->netId == 0) {
            SwalotPlopTally_Free(app->tally);
        }
        SwalotPlopGame_Free(app->game);
        GF_DestroyVramTransferManager();
    }
    if (args->voiceChat != 0) {
        WifiConnection_StopVoiceChat();
    }
    if (app->results != NULL) {
        PlazaGameResults_Free(app->results);
        app->results = NULL;
    }
}

static BOOL SwalotPlop_SendCommand(SwalotPlopApp *app, int command, void *data, int size) {
    if (app->canSend == FALSE) {
        return FALSE;
    }
    return Comm_SendCommand(command, data, size);
}

static SwalotPlopTally *SwalotPlopTally_New(enum HeapID heapId, int duration, u32 playerCount, SwalotPlopShared *shared) {
    SwalotPlopTally *tally = Heap_Alloc(heapId, sizeof(SwalotPlopTally));
    memset(tally, 0, sizeof(SwalotPlopTally));
    tally->timer = duration;
    tally->playerCount = playerCount;
    tally->shared = shared;
    tally->running = TRUE;
    return tally;
}

static void SwalotPlopTally_Free(SwalotPlopTally *tally) {
    Heap_Free(tally);
}

static BOOL SwalotPlopTally_Update(SwalotPlopTally *tally) {
    int i;
    u32 count, walk;
    u8 previous;
    SwalotPlopLevelTable *table;
    if (tally->timer > 0 && tally->running == TRUE) {
        tally->timer--;
    }
    if (tally->timer <= 0) {
        return FALSE;
    }
    previous = tally->level;
    table = &tally->shared->levelTable;
    count = table->count;
    i = previous;
    walk = previous;
    if (previous < count) {
        SwalotPlopLevel *level = &table->levels[previous];
        u16 hitCount = tally->hitCount;
        do {
            if (level->hitCount >= hitCount) {
                break;
            }
            i++;
            level++;
            walk = i;
        } while (i < count);
    }
    if (walk == count) {
        i--;
    }
    if (i != previous) {
        tally->level = i;
        tally->levelChanged = TRUE;
    }
    return TRUE;
}

static u8 SwalotPlopTally_LevelChanged(SwalotPlopTally *tally) {
    return tally->levelChanged;
}

static void SwalotPlopTally_ClearLevelChanged(SwalotPlopTally *tally) {
    tally->levelChanged = FALSE;
}

static u8 SwalotPlopTally_GetLevel(SwalotPlopTally *tally) {
    return tally->level;
}

static void SwalotPlopTally_AddShot(SwalotPlopTally *tally, const SwalotPlopShotRecord *record, int index) {
    if (record->caught != 0) {
        tally->hitCount++;
    }
}

static void SwalotPlopTally_SetScore(SwalotPlopTally *tally, u32 score, int index) {
    u32 mask;
    if (score > 99999) {
        score = 99999;
    }
    mask = 1 << index;
    if (!(tally->receivedMask & mask)) {
        tally->scores[index] = score;
        tally->receivedMask |= mask;
    }
}

static BOOL SwalotPlopTally_HasAllScores(SwalotPlopTally *tally) {
    if (SwalotPlopTally_CountScores(tally) >= tally->playerCount) {
        return TRUE;
    }
    return FALSE;
}

static void SwalotPlopTally_GetScores(SwalotPlopTally *tally, u32 *scores) {
    *(SwalotPlopScores *)scores = *(SwalotPlopScores *)tally->scores;
}

static u32 SwalotPlopTally_CountScores(SwalotPlopTally *tally) {
    s32 i;
    s32 count = 0;
    u32 mask = tally->receivedMask;
    for (i = count; i < PLAZA_GAME_MAX_PLAYERS; i++) {
        if ((1 << i) & mask) {
            count++;
        }
    }
    return count;
}

static SwalotPlopGame *SwalotPlopGame_New(enum HeapID heapId, u32 duration, u32 playerCount, u32 localIndex, SwalotPlopShared *shared) {
    SwalotPlopGame *game = Heap_Alloc(heapId, sizeof(SwalotPlopGame));
    memset(game, 0, sizeof(SwalotPlopGame));
    game->running = TRUE;
    game->frame = 0;
    game->duration = duration;
    game->playerCount = playerCount;
    game->localIndex = localIndex;
    game->shared = shared;
    SwalotPlopBody_Init(game, &game->body);
    SwalotPlopGame_InitGfx(game, heapId);
    SwalotPlopDemo_Init(&game->demo, &game->gfx, heapId);
    SwalotPlopScoreReports_Init(&game->scoreReports);
    if (GF_TouchpadStartRecording(sSwalotPlopTouchpadBuffer, 8, 4) != TRUE) {
        GF_ASSERT(FALSE);
    }
    return game;
}

static void SwalotPlopGame_Free(SwalotPlopGame *game) {
    if (GF_TouchpadStopSampling() != TRUE) {
        GF_ASSERT(FALSE);
    }
    SwalotPlopDemo_Free(&game->demo, &game->gfx);
    SwalotPlopGame_FreeGfx(game);
    Heap_Free(game);
}

static BOOL SwalotPlopGame_RunIntro(SwalotPlopGame *game, BOOL ready) {
    BOOL result = TRUE;
    if (game->gfx.intro.state >= SWALOT_PLOP_INTRO_WAIT_READY) {
        int speed = game->options[1] ? -120 : 120;
        SwalotPlopBody_SetSpin(&game->body, game->body.spin + speed);
    }
    SwalotPlopGfx_SetScenePos(&game->gfx, &game->body);
    SwalotPlopScene_SetSpin(&game->gfx.scene, &game->body);
    SwalotPlopScene_SetScale(&game->gfx.scene, &game->body);
    switch (game->gfx.intro.state) {
    case SWALOT_PLOP_INTRO_START_DEMO:
        SwalotPlopDemo_Start(&game->demo);
        game->gfx.intro.state++;
        break;
    case SWALOT_PLOP_INTRO_RUN_DEMO:
        if (SwalotPlopDemo_Update(&game->demo, game) == TRUE) {
            game->gfx.intro.state++;
            LinkSync_StartSync(2);
        }
        break;
    case SWALOT_PLOP_INTRO_WAIT_SYNC:
        if (LinkSync_IsSynced(2)) {
            game->gfx.intro.state++;
            game->gfx.intro.counter = 65;
        }
        break;
    case SWALOT_PLOP_INTRO_OPEN_SCENE:
        game->gfx.intro.counter--;
        if (game->gfx.intro.counter == 50) {
            PlaySE(SEQ_SE_DP_KI_GASYAN);
        }
        if (game->gfx.intro.counter == 20) {
            PlaySE(SEQ_SE_DP_HAMARU);
        }
        SwalotPlopScene_AdvanceAnimation(&game->gfx.scene, FX32_ONE);
        if (!game->gfx.intro.counter) {
            SwalotPlopIntro_StartScene(&game->gfx.intro, &game->gfx);
            game->gfx.intro.state = SWALOT_PLOP_INTRO_WAIT_READY;
        }
        break;
    case SWALOT_PLOP_INTRO_WAIT_READY:
        if (ready == TRUE) {
            game->gfx.intro.state = SWALOT_PLOP_INTRO_SHOW_WINDOW;
        }
        break;
    case SWALOT_PLOP_INTRO_SHOW_WINDOW:
        SwalotPlopIntro_ShowWindow(&game->gfx.intro, &game->gfx);
        PlaySE(SEQ_SE_DP_DECIDE);
        game->gfx.intro.state = SWALOT_PLOP_INTRO_COUNTDOWN;
        break;
    case SWALOT_PLOP_INTRO_COUNTDOWN:
        if (!SwalotPlopIntro_IsCountingDown(&game->gfx.intro, &game->gfx)) {
            game->gfx.intro.state = SWALOT_PLOP_INTRO_WAIT;
        }
        break;
    case SWALOT_PLOP_INTRO_WAIT:
        if (!SwalotPlopIntro_Wait(&game->gfx.intro, &game->gfx)) {
            game->gfx.intro.state = SWALOT_PLOP_INTRO_DONE;
        }
        break;
    case SWALOT_PLOP_INTRO_DONE:
        SwalotPlopIntro_HideWindow(&game->gfx.intro, &game->gfx);
        SwalotPlopBody_SetActive(&game->body, 1);
        result = FALSE;
        break;
    }
    SwalotPlopScene_Animate(&game->gfx.scene, &game->body, game->phase);
    SwalotPlopGame_Draw(game, &game->gfx);
    return result;
}

static BOOL SwalotPlopGame_RunFinish(SwalotPlopGame *game, BOOL ready) {
    BOOL result = TRUE;
    switch (game->gfx.finish.state) {
    case SWALOT_PLOP_FINISH_START:
        SwalotPlopGame_DropFlyingShots(game);
        SwalotPlopGame_EndFlick(game);
        StartBrightnessTransition(24, -14, 0, (GXBlendPlaneMask)0x3F, 2);
        SwalotPlopFinish_Start(&game->gfx);
        PlazaGameSign_StartFinish(game->gfx.sign);
        SwalotPlopSpeedUp_ResetTempo(&game->speedUp);
        SwalotPlopCombo_Hide(&game->gfx.combo);
        game->gfx.finish.state++;
        break;
    case SWALOT_PLOP_FINISH_WAIT: {
        BOOL done = TRUE;
        if (!SwalotPlopFinish_Update(&game->gfx)) {
            done = FALSE;
        }
        SwalotPlopGame_UpdateShots(game);
        SwalotPlopGame_DropEndedShots(game);
        if (SwalotPlopGame_HasActiveShots(game) == TRUE) {
            done = FALSE;
        }
        if (!SwalotPlopFinish_IsSignDone(&game->gfx)) {
            done = FALSE;
        }
        if (!IsBrightnessTransitionActive(2)) {
            done = FALSE;
        }
        if (done == TRUE && ready == TRUE) {
            game->gfx.finish.counter = 0;
            game->gfx.finish.state++;
        }
        break;
    }
    case SWALOT_PLOP_FINISH_HOLD:
        game->gfx.finish.counter++;
        if (game->gfx.finish.counter >= 64) {
            game->gfx.finish.state++;
        }
        break;
    case SWALOT_PLOP_FINISH_DONE:
        result = FALSE;
        break;
    }
    int speed = game->options[1] ? -120 : 120;
    SwalotPlopBody_SetSpin(&game->body, game->body.spin + speed);
    SwalotPlopScene_SetPos(&game->gfx.scene, &game->body);
    SwalotPlopScene_SetSpin(&game->gfx.scene, &game->body);
    SwalotPlopScene_Animate(&game->gfx.scene, &game->body, game->phase);
    SwalotPlopGame_Draw(game, &game->gfx);
    SwalotPlopCombo_Update(&game->gfx.combo);
    return result;
}

static BOOL SwalotPlopGame_Update(SwalotPlopGame *game) {
    u32 phase = game->phase;
    u32 leader;
    if (phase < 2 && gSwalotPlopPhaseFrames[phase] <= game->frame) {
        game->phase = phase + 1;
        if (game->phase == SWALOT_PLOP_PHASE_FAST) {
            SwalotPlopSpeedUp_Start(&game->speedUp);
        }
    }
    GF_TouchpadUpdate(&game->touch, 1, 0);
    SwalotPlopGame_HandleTouch(game);
    if (!SwalotPlopLevelBlend_Update(&game->levelBlend)) {
        SwalotPlopGame_ApplyLevel(game, &game->levelBlend);
    }
    if (SwalotPlopScoreReports_HasAll(&game->scoreReports, game->playerCount) == TRUE) {
        leader = SwalotPlopScoreReports_GetLeader(&game->scoreReports);
        if (leader != 0xFFFF) {
            SwalotPlopScene_SetLeader(&game->gfx.scene, leader);
        }
        SwalotPlopScoreReports_Reset(&game->scoreReports);
    }
    SwalotPlopGame_UpdateShots(game);
    SwalotPlopBody_Update(game, &game->body);
    SwalotPlopGame_UpdateGfx(game, &game->gfx);
    SwalotPlopGame_RemoveEndedShots(game);
    SwalotPlopSpeedUp_Update(&game->speedUp, &game->gfx.stage, &game->gfx.scene);
    if (game->running == TRUE) {
        game->frame++;
    }
    if ((s32)game->frame >= (s32)game->duration) {
        game->frame = game->duration;
        return TRUE;
    }
    return FALSE;
}

static void SwalotPlopGame_VBlank(SwalotPlopGame *game) {
    DoScheduledBgGpuUpdates(game->gfx.bgConfig);
    GF_RunVramTransferTasks();
    OamManager_ApplyAndResetBuffers();
}

static u32 SwalotPlopGame_GetFrame(SwalotPlopGame *game) {
    return game->frame;
}

static void SwalotPlopGame_AddRemoteShot(SwalotPlopGame *game, const SwalotPlopShotRecord *record) {
    SwalotPlopShotParams params;
    if (game->localIndex != record->owner) {
        SwalotPlopShot_FromRecord(record, &params);
        SwalotPlopGame_AddShot(game, &params, SHOT_STATE_FALLING);
        if (record->caught != 0) {
            SwalotPlopPopups_Show(game->gfx.popups, record->owner, record->combo, game->phase);
        }
    }
}

static BOOL SwalotPlopGame_TakeShotReport(SwalotPlopGame *game, SwalotPlopShotRecord *record) {
    int i;
    for (i = 0; i < 0x60; i++) {
        if (game->shots[i].state == SHOT_STATE_REPORT) {
#if defined(SDK_PORT)
            SwalotPlopShot_ToRecord(&game->shots[i].params, record);
            SwalotPlopShot_Clear(&game->shots[i]);
#else
            SwalotPlopShot_ToRecord((SwalotPlopShotParams *)((u32)&game->shots[0].params + i * sizeof(SwalotPlopShot)), record);
            SwalotPlopShot_Clear((SwalotPlopShot *)((u32)game->shots + i * sizeof(SwalotPlopShot)));
#endif
            return TRUE;
        }
    }
    return FALSE;
}

static void SwalotPlopGame_SetFinalScores(SwalotPlopGame *game, const u32 *scores) {
    *(SwalotPlopScores *)game->finalScores = *(const SwalotPlopScores *)scores;
}

static u32 SwalotPlopGame_GetScore(SwalotPlopGame *game) {
    return game->score;
}

static void SwalotPlopGame_SetLevel(SwalotPlopGame *game, u32 level) {
    u16 prev = game->level;
    game->prevLevel = prev;
    game->level = level;
    SwalotPlopLevelBlend_Start(&game->levelBlend, game->shared, game->options, game->prevLevel, game->level);
}

static void SwalotPlopGame_SetScoreReport(SwalotPlopGame *game, u32 score, int index) {
    SwalotPlopScoreReports_Set(&game->scoreReports, score, index);
}

static void SwalotPlopGame_SetOptions(SwalotPlopGame *game, const u8 *options) {
    game->options[0] = options[0];
    game->options[1] = options[1];
    game->options[2] = options[2];
    game->options[3] = options[3];
    game->optionsSet = TRUE;
    SwalotPlopLevelBlend_Init(&game->levelBlend, game->shared, game->options);
    SwalotPlopGame_ApplyLevel(game, &game->levelBlend);
}

static void SwalotPlopCurve_Init(SwalotPlopCurve *curve, s32 start, s32 end, s32 slope, s32 duration) {
    s32 delta = end - start - FX_Mul(slope, duration << FX32_SHIFT);
    s32 curvature = FX_Div(FX_Mul(delta, FX32_CONST(2)), (duration * duration) << FX32_SHIFT);
    curve->value = start;
    curve->start = start;
    curve->slope = slope;
    curve->curvature = curvature;
    curve->step = 0;
    curve->duration = duration;
}

static BOOL SwalotPlopCurve_Update(SwalotPlopCurve *curve) {
    s32 step = curve->step;
    s32 slopePart = FX_Mul(curve->slope, step << FX32_SHIFT);
    s32 curvePart = FX_Mul(curve->curvature, (step * step) << FX32_SHIFT);
    curve->value = curve->start + (slopePart + FX_Div(curvePart, FX32_CONST(2)));
    s32 current = curve->step;
    s32 duration = curve->duration;
    s32 next = current + 1;
    if (next <= duration) {
        curve->step = next;
        return FALSE;
    }
    curve->step = duration;
    return TRUE;
}

static void SwalotPlop_GetPopupPos(int seat, int playerCount, int index, u32 *x, u32 *y) {
    *x = *(const u32 *)(gSwalotPlopPopupPosX + playerCount * 0x80 + seat * 0x20 + index * 8);
    *y = *(const u32 *)(gSwalotPlopPopupPosY + playerCount * 0x80 + seat * 0x20 + index * 8);
}

static void SwalotPlopGame_ThrowDemoShot(SwalotPlopGame *game, s32 x, s32 y, s32 endX, u16 endY, int state) {
    SwalotPlopShotParams params;
    params.owner = game->localIndex;
    params.caught = 0;
    params.direction.x = (u32)(s32)(s16)((s32)((u32)x - (u32)endX) / 2) << 12;
    params.direction.z = (u32)(s32)(s16)((s32)((u32)y - (u32)endY) / 2) << 12;
    params.direction.y = 0;
    if (VEC_Mag(&params.direction) > 0x1E200) {
        VEC_Normalize(&params.direction, &params.direction);
        params.direction.x = FX_Mul(params.direction.x, FX32_CONST(30.125));
        params.direction.z = FX_Mul(params.direction.z, FX32_CONST(30.125));
    }
    params.direction.y = 38 << 12;
    params.speed = VEC_Mag(&params.direction);
    params.speed = FX_Mul(params.speed, FX32_CONST(0.6));
    VEC_Normalize(&params.direction, &params.direction);
    SwalotPlop_TouchToWorld((s16)endX, (s16)endY, &params.start);
    params.start.y = 0;
    SwalotPlop_RotateToSeat(&params.start, &params.start, params.owner, game->playerCount);
    SwalotPlop_RotateToSeat(&params.direction, &params.direction, params.owner, game->playerCount);
    params.big = FALSE;
    SwalotPlopGame_AddShot(game, &params, state);
}

static u32 SwalotPlopGame_GetHitPoints(SwalotPlopGame *game) {
    u32 index;
    u32 points;
    s32 combo = game->combo;
    if (combo >= 3) {
        index = 2;
    } else {
        index = (u8)combo;
    }
    points = gSwalotPlopHitPoints[index];
    if (game->phase == SWALOT_PLOP_PHASE_FASTEST) {
        points = points + points * 2;
    }
    return points;
}

static void SwalotPlopPopups_Init(SwalotPlopPopups *popups, SwalotPlopGfx *gfx, int playerCount, int localIndex, enum HeapID heapId) {
    SpriteTemplate template;
    int i;
    template.spriteList = gfx->spriteList;
    template.header = &gfx->header;
    template.scale.x = FX32_ONE;
    template.scale.y = FX32_ONE;
    template.scale.z = FX32_ONE;
    template.rotation = 0;
    template.drawPriority = 0;
    template.whichScreen = NNS_G2D_VRAM_TYPE_2DMAIN;
    template.heapID = heapId;
    for (i = 0; i < 4; i++, popups++) {
        SwalotPlopPopups_InitSeat(popups, &template, gfx, playerCount, i, localIndex, heapId);
    }
}

static void SwalotPlopPopups_Free(SwalotPlopPopups *popups, SwalotPlopGfx *gfx) {
    int i;
    for (i = 0; i < 4; i++) {
        SwalotPlopPopups_FreeSeat(popups);
        popups++;
    }
}

static void SwalotPlopPopups_Show(SwalotPlopPopups *popups, int seat, int combo, int phase) {
    SwalotPlopPopups_ShowSeat(&popups[seat], seat, combo, phase);
}

static void SwalotPlopPopups_Update(SwalotPlopPopups *popups) {
    int i;
    for (i = 0; i < 4; i++) {
        SwalotPlopPopups_UpdateSeat(popups);
        popups++;
    }
}

static void SwalotPlopPopups_InitSeat(SwalotPlopPopups *popups, SpriteTemplate *template, SwalotPlopGfx *gfx, int playerCount, int seat, int localIndex, enum HeapID heapId) {
    int i;

    struct {
        u32 y;
        u32 x;
    } coord;

    for (i = 0; i < 3; i++) {
        SwalotPlop_GetPopupPos(playerCount, localIndex, seat, &coord.x, &coord.y);
        template->position.x = coord.x << 12;
        template->position.y = coord.y << 12;
        popups->pos[i] = template->position;
        popups->sprites[i] = Sprite_CreateAffine(template);
        Sprite_SetAnimCtrlSeq(popups->sprites[i], 2);
        Sprite_SetDrawFlag(popups->sprites[i], FALSE);
        Sprite_SetAnimActiveFlag(popups->sprites[i], TRUE);
        Sprite_SetAnimSpeed(popups->sprites[i], 6 << 10);
        popups->timers[i] = 0;
    }
    if (localIndex == seat) {
        popups->mode = 1;
    } else {
        popups->mode = 0;
    }
}

static void SwalotPlopPopups_FreeSeat(SwalotPlopPopups *popups) {
    int i;
    Sprite **sprite = popups->sprites;
    for (i = 0; i < 3; i++) {
        Sprite_Delete(*sprite);
        sprite++;
    }
}

static void SwalotPlopPopups_ShowSeat(SwalotPlopPopups *popups, int seat, u32 combo, int phase) {
    int index;
    BOOL found;
    u32 maxPriority;
    int i;
    u32 priority;
    int palette;
    found = FALSE;
    maxPriority = 0;
    index = -1;
    for (i = 0; i < 3; i++) {
        if (!Sprite_GetDrawFlag(popups->sprites[i])) {
            index = i;
            found = TRUE;
        } else {
            priority = Sprite_GetDrawPriority(popups->sprites[i]);
            Sprite_SetDrawPriority(popups->sprites[i], priority + 1);
            if (!found && maxPriority <= priority + 1) {
                maxPriority = priority + 1;
                index = i;
            }
        }
    }
    GF_ASSERT(index >= 0);
    if (combo >= 3) {
        combo = 2;
    }
    if (phase != SWALOT_PLOP_PHASE_FASTEST) {
        if (popups->mode == 1) {
            combo += 2;
        } else {
            combo += 8;
        }
        palette = seat + 3;
    } else {
        if (popups->mode == 1) {
            combo += 5;
        } else {
            combo += 11;
        }
        palette = 7;
    }
    Sprite_SetAnimCtrlSeq(popups->sprites[index], combo);
    popups->timers[index] = 0;
    Sprite_SetDrawFlag(popups->sprites[index], TRUE);
    SwalotPlopPopups_SetPos(popups, index);
    Sprite_SetDrawPriority(popups->sprites[index], 0);
    Sprite_SetPaletteOverride(popups->sprites[index], palette);
}

static void SwalotPlopPopups_UpdateSeat(SwalotPlopPopups *popups) {
    int i;
    SwalotPlopPopups *slot;
    i = 0;
    slot = popups;
    while (i < 3) {
        if (Sprite_GetDrawFlag(slot->sprites[0]) == TRUE) {
            u32 timer = ++slot->timers[0];
            if (timer > 16) {
                SwalotPlopPopups_HideSprite(popups, i);
            } else {
                SwalotPlopPopups_SetPos(popups, i);
            }
        }
        i++;
        slot = (SwalotPlopPopups *)((u8 *)slot + 4);
    }
}

static void SwalotPlopPopups_SetPos(SwalotPlopPopups *popups, int i) {
    VecFx32 pos;
    fx32 y = FX_Div(FX_Mul(popups->timers[i] << FX32_SHIFT, FX32_CONST(-16)), FX32_CONST(16));
    pos = popups->pos[i];
    pos.y += y;
    Sprite_SetMatrix(popups->sprites[i], &pos);
}

static void SwalotPlopPopups_HideSprite(SwalotPlopPopups *popups, int i) {
    Sprite_SetDrawFlag(popups->sprites[i], FALSE);
}

static void SwalotPlopIntro_Init(SwalotPlopIntro *intro, SwalotPlopGfx *gfx, SwalotPlopShared *shared, u32 playerCount, u32 localIndex, NARC *narc, enum HeapID heapId) {
    u8 *bytes = (u8 *)intro;
    u32 size = sizeof(*intro);
    do {
        *bytes++ = 0;
    } while (--size);
    AddWindowParameterized(gfx->bgConfig, &intro->window, GF_BG_LYR_MAIN_1, 2, 19, 28, 4, 5, 10);
    FillWindowPixelRect(&intro->window, 15, 0, 0, 224, 32);
    String *string = String_New(128, heapId);
    ReadMsgDataIntoString(gfx->msgData, msg_0230_00000, string);
    AddTextPrinterParameterizedWithColor(&intro->window, 0, string, 0, 0, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(1, 2, 15), NULL);
    String_Delete(string);
    GfGfxLoader_LoadCharDataFromOpenNarc(narc, 18, gfx->bgConfig, GF_BG_LYR_MAIN_2, 0, 0, FALSE, heapId);
    GfGfxLoader_LoadScrnDataFromOpenNarc(narc, playerCount + 18, gfx->bgConfig, GF_BG_LYR_MAIN_2, 0, 0, FALSE, heapId);
    GfGfxLoader_GXLoadPalFromOpenNarc(narc, 19, GF_PAL_LOCATION_MAIN_BG, GF_PAL_SLOT_0_OFFSET, 128, heapId);
    Window window;
    InitWindow(&window);
    AddWindowParameterized(gfx->bgConfig, &window, GF_BG_LYR_MAIN_2, 0, 0, 8, 2, 5, 256);
    u32 x;
    u32 baseTile = 256;
    string = String_New(128, heapId);
    u32 i;
    for (i = 0; i < playerCount; i++) {
        const u32 *table = gSwalotPlopIntroNamePositions + localIndex * 32 + playerCount * 8 + i * 2;
        if (i != localIndex) {
            x = table[-8];
            u32 y = table[-7];
            BgTilemapRectChangePalette(gfx->bgConfig, GF_BG_LYR_MAIN_2, x - 1, y - 1, 10, 4, i);
            FillWindowPixelBuffer(&window, 15);
            u32 color = shared->nameFlags[i] == 1 ? MAKE_TEXT_COLOR(5, 6, 15) : MAKE_TEXT_COLOR(1, 2, 15);
            PlayerName_FlatToString(shared->profiles[i], string);
            SetWindowX(&window, x);
            SetWindowY(&window, y);
            window.baseTile = baseTile;
            u32 width = FontID_String_GetWidth(0, string, 0);
            AddTextPrinterParameterizedWithColor(&window, 0, string, (64 - width) >> 1, 0, TEXT_SPEED_INSTANT, color, NULL);
            baseTile += 16;
        }
    }
    String_Delete(string);
    RemoveWindow(&window);
    intro->delay = 0;
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_BG1, GF_PLANE_TOGGLE_OFF);
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_BG2, GF_PLANE_TOGGLE_OFF);
}

static void SwalotPlopIntro_Free(SwalotPlopIntro *intro, SwalotPlopGfx *gfx) {
    RemoveWindow(&intro->window);
}

static void SwalotPlopIntro_ShowWindow(SwalotPlopIntro *intro, SwalotPlopGfx *gfx) {
    DrawFrameAndWindow1(&intro->window, FALSE, 1, 4);
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_BG1, GF_PLANE_TOGGLE_ON);
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_BG2, GF_PLANE_TOGGLE_ON);
    PlazaGameSign_StartCountdown(gfx->sign);
}

static BOOL SwalotPlopIntro_IsCountingDown(SwalotPlopIntro *intro, SwalotPlopGfx *gfx) {
    if (PlazaGameSign_IsDone(gfx->sign) == TRUE) {
        return FALSE;
    }
    return TRUE;
}

static void SwalotPlopIntro_HideWindow(SwalotPlopIntro *intro, SwalotPlopGfx *gfx) {
    EraseFrameAndWindow1(&intro->window, TRUE);
    ClearWindowTilemapAndScheduleTransfer(&intro->window);
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_BG2, GF_PLANE_TOGGLE_OFF);
}

static void SwalotPlopIntro_StartScene(SwalotPlopIntro *intro, SwalotPlopGfx *gfx) {
    SwalotPlopScene_SetAnimation(&gfx->scene, 1);
    ModelAnimation_SetFrame(&gfx->scene.animations[1], FX32_CONST(180));
    SwalotPlopScene_SetBaseAnimation(&gfx->scene, 1);
}

static BOOL SwalotPlopIntro_Wait(SwalotPlopIntro *intro, SwalotPlopGfx *gfx) {
    intro->delay++;
    if (intro->delay < 8) {
        return TRUE;
    }
    return FALSE;
}

static void SwalotPlopGfx_SetScenePos(SwalotPlopGfx *gfx, SwalotPlopBody *body) {
    SwalotPlopScene_SetPos(&gfx->scene, body);
}

static void SwalotPlopFinish_Init(SwalotPlopGfx *gfx, u32 playerCount, u32 localIndex, enum HeapID heapId) {
    gfx->finish.state = SWALOT_PLOP_FINISH_START;
    gfx->finish.counter = 0;
}

static void SwalotPlopGfx_FreeDummy1(SwalotPlopGfx *gfx, u32 playerCount) {
}

static void SwalotPlopFinish_Start(SwalotPlopGfx *gfx) {
    SwalotPlopScene_SetAnimation(&gfx->scene, 5);
}

static BOOL SwalotPlopFinish_Update(SwalotPlopGfx *gfx) {
    gfx->finish.counter = gfx->finish.counter + 1;
    if (gfx->finish.counter >= 60) {
        if (ModelAnimation_Advance(&gfx->scene.animations[5], FX32_CONST(2)) == TRUE) {
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL SwalotPlopFinish_IsSignDone(SwalotPlopGfx *gfx) {
    if (PlazaGameSign_IsDone(gfx->sign) == TRUE) {
        return TRUE;
    }
    return FALSE;
}

static void SwalotPlopBody_Init(SwalotPlopGame *game, SwalotPlopBody *body) {
    VecFx32 offset;
    body->spinSpeed = 0;
    SwalotPlopBody_SetSpin(body, 0x7FFF);
    SwalotPlopBody_SetTilt(body, 0xE38);
    body->active = FALSE;
    body->mouthOpen = TRUE;
    body->pushRadius = FX32_CONST(200);
    body->pushStrength = 123;
    body->mouthDepth = FX32_CONST(80);
    offset = gSwalotPlopBodyOffset;
    body->offset = offset;
    SwalotPlopBody_SetScale(body, 0x9C4);
    body->position = offset;
    body->height = body->heightMin;
}

static void SwalotPlopBody_Update(SwalotPlopGame *game, SwalotPlopBody *body) {
    if (game->phase == SWALOT_PLOP_PHASE_FAST) {
        body->mouthOpen = FALSE;
    } else {
        body->mouthOpen = TRUE;
    }
    int (*getTilt)(SwalotPlopBody *, u32, u32) = gSwalotPlopTiltFuncs[game->phase];
    int tilt = getTilt(body, game->frame, game->duration);
    SwalotPlopBody_SetTilt(body, tilt);
    body->mouthDepth = SwalotPlopBody_CalcMouthDepth(body, body->tilt);
    body->height = SwalotPlopBody_CalcHeight(body, body->tilt);
    SwalotPlopBody_SetSpin(body, body->spin + body->spinSpeed);
}

static void SwalotPlopBody_SetActive(SwalotPlopBody *body, u8 active) {
    body->active = active;
}

static void SwalotPlopBody_SetTilt(SwalotPlopBody *body, int tilt) {
    body->tilt = tilt;
    MTX_Identity33_(&body->tiltMtx);
    MTX_RotX33_(&body->tiltMtx, FX_SinIdx(tilt), FX_CosIdx(tilt));
}

static void SwalotPlopBody_SetSpin(SwalotPlopBody *body, u16 spin) {
    VecFx32 mouthOffset;
    fx32 dot;
    body->spin = spin;
    MTX_Identity33_(&body->rotation);
    MTX_Identity33_(&body->spinMtx);
    MTX_RotY33_(&body->spinMtx, FX_SinIdx(spin), FX_CosIdx(spin));
    MTX_Concat33(&body->tiltMtx, &body->spinMtx, &body->rotation);
    MTX_MultVec33(&gSwalotPlopBodyBaseNormal, &body->rotation, &body->normal);
    mouthOffset.x = 0;
    mouthOffset.y = 0;
    mouthOffset.z = -body->mouthDepth;
    MTX_MultVec33(&mouthOffset, &body->rotation, &body->mouthPos);
    mouthOffset.x = body->mouthOffsetX + body->offset.x;
    mouthOffset.y = body->mouthOffsetY;
    mouthOffset.z = body->offset.z + FX32_CONST(80);
    MTX_MultVec33(&mouthOffset, &body->spinMtx, &mouthOffset);
    VEC_Add(&mouthOffset, &body->mouthPos, &body->mouthPos);
    dot = FX_Mul(body->normal.x, body->mouthPos.x) + FX_Mul(body->normal.y, body->mouthPos.y) + FX_Mul(body->normal.z, body->mouthPos.z);
    body->mouthPlane = dot;
    MTX_MultVec33(&body->offset, &body->spinMtx, &body->position);
}

static int SwalotPlopBody_CheckMouth(SwalotPlopBody *body, const VecFx32 *position, VecFx32 *move, VecFx32 *push) {
    VecFx32 hit;
    VecFx32 delta;
    VecFx32 normalized;
    fx32 hitFraction;
    fx32 distance;
    fx32 strength;
    int result = 0;
    if (!body->active) {
        return result;
    }
    SwalotPlop_IntersectPlane(position, move, &body->normal, body->mouthPlane, &hit, &hitFraction);
    VEC_Subtract(&body->mouthPos, position, &delta);
    distance = VEC_Mag(&delta);
    if (hitFraction >= 0 && hitFraction <= FX32_ONE) {
        if (distance <= body->mouthDepth) {
            if (body->mouthOpen == TRUE) {
                result = 1;
            } else {
                result = 2;
            }
        } else {
            return 2;
        }
    }
    if (hitFraction > 0 && distance < body->pushRadius) {
        strength = FX_Mul(body->pushRadius - distance, body->pushStrength);
        VEC_Normalize(&delta, &normalized);
        push->x = FX_Mul(normalized.x, strength);
        push->y = FX_Mul(normalized.y, strength);
        push->z = FX_Mul(normalized.z, strength);
        if (distance - strength <= body->mouthDepth) {
            if (body->mouthOpen == TRUE) {
                result = 1;
            } else {
                result = 2;
            }
        } else {
            result = 3;
        }
    }
    return result;
}

static BOOL SwalotPlopBody_IsInside(SwalotPlopBody *body, const VecFx32 *pos) {
    VecFx32 delta;
    VecFx32 center;
    VecFx32 horizontal;
    fx32 distance;
    s32 narrowing;
    s32 y;
    if (!body->active) {
        return FALSE;
    }
    if (body->position.y < pos->y && body->position.y + body->height > pos->y) {
        center.x = body->position.x;
        center.z = body->position.z;
        center.y = 0;
        horizontal.x = pos->x;
        horizontal.z = pos->z;
        horizontal.y = 0;
        VEC_Subtract(&center, &horizontal, &delta);
        distance = VEC_Mag(&delta);
        y = (pos->y - body->position.y) >> FX32_SHIFT;
        narrowing = (y * body->slope) / (body->height >> FX32_SHIFT);
        if (distance < body->radius - narrowing) {
            return TRUE;
        }
    }
    return FALSE;
}

static fx32 SwalotPlopBody_CalcMouthDepth(SwalotPlopBody *body, s32 tilt) {
    s32 degrees = 90;
    s32 depth;
    degrees <<= 2;
    depth = (degrees * tilt) / 0xFFFF;
    depth = ((90 - depth) * body->mouthDepthRange) / 90;
    depth += body->mouthDepthMin;
    return depth;
}

static fx32 SwalotPlopBody_CalcHeight(SwalotPlopBody *body, s32 tilt) {
    s32 degrees = 90;
    s32 height;
    degrees <<= 2;
    height = (degrees * tilt) / 0xFFFF;
    height = (height * body->heightRange) / 90;
    height += body->heightMin;
    return height;
}

static void SwalotPlopBody_SetScale(SwalotPlopBody *body, fx32 scale) {
    s32 topRadius;
    body->scale = scale;
    body->mouthDepthRange = FX_Mul(scale, 30 << FX32_SHIFT);
    body->mouthDepthMin = FX_Mul(scale, 50 << FX32_SHIFT);
    topRadius = FX_Mul(scale, 80 << FX32_SHIFT);
    body->unkB4 = topRadius;
    body->mouthOffsetX = 0;
    body->mouthOffsetY = FX_Mul(scale, 55 << FX32_SHIFT);
    body->heightMin = FX_Mul(scale, 150 << FX32_SHIFT);
    body->heightRange = FX_Mul(scale, 60 << FX32_SHIFT);
    body->topRadius = topRadius;
    body->radius = FX_Mul(scale, 120 << FX32_SHIFT);
    body->slope = body->radius - body->topRadius;
    body->modelScale = FX_Mul(scale, 0x1800);
}

static void SwalotPlopBody_SetSpinSpeed(SwalotPlopBody *body, u16 spinSpeed) {
    body->spinSpeed = spinSpeed;
}

static void SwalotPlopBody_SetMouthOffset(SwalotPlopBody *body, u32 offset) {
    body->offset.z = offset;
}

int SwalotPlopBody_GetTiltNormal(SwalotPlopBody *body, u32 frame, u32 duration) {
    return 0xE38;
}

int SwalotPlopBody_GetTiltFast(SwalotPlopBody *body, u32 frame, u32 duration) {
    return 0x3FFF;
}

u16 SwalotPlopBody_GetTiltFastest(SwalotPlopBody *body, u32 frame, u32 duration) {
    s32 tilt = (s32)(((frame - 750) % 50) * 0x638E) / 50;
    if (tilt > 0x31C7) {
        tilt = 0x31C7 - tilt % 0x31C7;
    }
    return tilt + 0xE38;
}

static void SwalotPlopShot_ToRecord(const SwalotPlopShotParams *params, SwalotPlopShotRecord *record) {
    record->owner = params->owner;
    record->caught = params->caught;
    record->dir[0] = params->direction.x;
    record->dir[1] = params->direction.y;
    record->dir[2] = params->direction.z;
    record->speed = FX_Mul(params->speed, FX32_CONST(0.16));
    record->start[0] = params->start.x >> FX32_SHIFT;
    record->start[1] = params->start.y >> FX32_SHIFT;
    record->start[2] = params->start.z >> FX32_SHIFT;
    record->combo = params->combo;
    record->big = params->big;
}

static void SwalotPlopShot_FromRecord(const SwalotPlopShotRecord *record, SwalotPlopShotParams *params) {
    params->owner = record->owner;
    params->caught = record->caught;
    params->direction.x = record->dir[0];
    params->direction.y = record->dir[1];
    params->direction.z = record->dir[2];
    params->speed = FX_Div(record->speed, FX32_CONST(0.16));
    params->start.x = record->start[0] << 12;
    params->start.y = record->start[1] << 12;
    params->start.z = record->start[2] << 12;
    params->big = record->big;
}

static void SwalotPlopGame_AddShot(SwalotPlopGame *game, const SwalotPlopShotParams *params, u32 state) {
    int i;
    for (i = 0; i < 0x60; i++) {
        if (game->shots[i].state == SHOT_STATE_FREE) {
            game->shots[i].params = *params;
            ((SwalotPlopShot *)((u8 *)game + offsetof(SwalotPlopGame, shots)))[i].state = state;
            game->shots[i].position = params->start;
            game->shots[i].frames = 0;
            game->shots[i].crossedRim = FALSE;
            SwalotPlopGfx_AddShotView(&game->gfx, &game->shots[i], game->playerCount, game->localIndex);
            return;
        }
    }
    if (state == SHOT_STATE_FLYING) {
        for (i = 0; i < 0x60; i++) {
            if (game->shots[i].state == SHOT_STATE_FALLING || game->shots[i].state == SHOT_STATE_BOUNCED) {
                SwalotPlopGfx_RemoveShotView(&game->gfx, &((SwalotPlopShot *)((u8 *)game + offsetof(SwalotPlopGame, shots)))[i]);
                SwalotPlopShot_Clear(&game->shots[i]);
                game->shots[i].params = *params;
                game->shots[i].state = state;
                game->shots[i].position = params->start;
                game->shots[i].frames = 0;
                game->shots[i].crossedRim = FALSE;
                SwalotPlopGfx_AddShotView(&game->gfx, &((SwalotPlopShot *)((u8 *)game + offsetof(SwalotPlopGame, shots)))[i], game->playerCount, game->localIndex);
                return;
            }
        }
        GF_ASSERT(FALSE);
    }
}

static void SwalotPlopShot_End(SwalotPlopGame *game, SwalotPlopShot *shot, u16 caught, u32 combo) {
    shot->remove = TRUE;
    shot->params.caught = caught;
    shot->params.combo = combo;
}

static void SwalotPlopShot_Clear(SwalotPlopShot *shot) {
    memset(shot, 0, sizeof(SwalotPlopShot));
}

static void SwalotPlopGame_UpdateShots(SwalotPlopGame *game) {
    SwalotPlopShot *shot = (SwalotPlopShot *)game;
    int i = 0;
    SwalotPlopGame *walker = game;
    shot = (SwalotPlopShot *)((u8 *)shot + offsetof(SwalotPlopGame, shots));
    for (; i < 0x60; i++, walker = (SwalotPlopGame *)((u8 *)walker + sizeof(SwalotPlopShot)), shot++) {
        u8 state = walker->shots[0].state;
        if ((u8)(state + 255) <= 1) { // flying or falling
            SwalotPlopGame_UpdateFlyingShot(game, shot);
        } else if (state == SHOT_STATE_BOUNCED) {
            SwalotPlopGame_UpdateBouncedShot(shot, game);
        } else if (state == SHOT_STATE_CAUGHT) {
            SwalotPlopGame_UpdateCaughtShot(shot, game);
        }
    }
}

static void SwalotPlopGame_UpdateFlyingShot(SwalotPlopGame *game, SwalotPlopShot *shot) {
    VecFx32 move;
    VecFx32 push;
    fx32 oldY;
    int result;
    int state = shot->state;

    if (state != SHOT_STATE_FLYING && state != SHOT_STATE_FALLING) {
        return;
    }
    oldY = shot->position.y;
    SwalotPlopShot_GetMove(shot, &move);
    if (shot->state == SHOT_STATE_FLYING) {
        result = SwalotPlopBody_CheckMouth(&game->body, &shot->position, &move, &push);
    } else {
        result = 0;
    }
    SwalotPlopShot_Move(shot, &move);
    if (result == 3 || result == 1) {
        SwalotPlopShot_Move(shot, &push);
    }
    shot->frames++;
    if (result == 1) {
        game->score += SwalotPlopGame_GetHitPoints(game);
        if (game->score > 99999) {
            game->score = 99999;
        }
        switch (game->combo) {
        case 0:
            PlaySE(SEQ_SE_PL_NOMI02);
            PlaySE(SEQ_SE_PL_POINT1);
            break;
        case 1:
            PlaySE(SEQ_SE_PL_NOMI02);
            PlaySE(SEQ_SE_PL_POINT2);
            break;
        default:
            PlaySE(SEQ_SE_PL_NOMI02);
            PlaySE(SEQ_SE_PL_POINT3);
            break;
        }
        SwalotPlopShot_Catch(shot, game);
        SwalotPlopPopups_Show(game->gfx.popups, game->localIndex, game->combo, game->phase);
        if (game->combo < 99) {
            game->combo++;
        }
        if (game->combo == 3) {
            SwalotPlopCombo_Show(&game->gfx.combo, &game->gfx, game->combo);
        } else {
            SwalotPlopCombo_SetCount(&game->gfx.combo, &game->gfx, game->combo);
        }
        return;
    }
    result = 0;
    if (SwalotPlopBody_IsInside(&game->body, &shot->position) == TRUE) {
        result = 1;
        if (state == SHOT_STATE_FLYING) {
            PlaySE(SEQ_SE_DP_023);
        }
        SwalotPlopShot_Bounce(shot, game, TRUE);
    } else if (shot->position.y <= FX32_CONST(-50)) {
        if (!SwalotPlopShot_IsOnRim(shot)) {
            result = 1;
            if (oldY <= FX32_CONST(-50)) {
                SwalotPlopShot_End(game, shot, 0, game->combo);
            } else {
                SwalotPlopShot_Bounce(shot, game, FALSE);
            }
        } else if (shot->position.y <= FX32_CONST(0.25)) {
            result = 1;
            SwalotPlopShot_End(game, shot, 0, game->combo);
        }
    } else if (SwalotPlopShot_CheckOutside(shot) == TRUE) {
        result = 1;
        VEC_Subtract(&shot->position, &move, &shot->position);
        SwalotPlopShot_BounceEx(shot, game, TRUE, FX32_CONST(0.25), FALSE);
    }
    if (result == 1 && state == SHOT_STATE_FLYING) {
        game->combo = 0;
        SwalotPlopCombo_Hide(&game->gfx.combo);
    }
}

static void SwalotPlopGame_RemoveEndedShots(SwalotPlopGame *game) {
    int i;
    for (i = 0; i < 0x60; i++) {
        SwalotPlopShot_CheckRemove(&game->shots[i], game);
    }
}

static void SwalotPlopGame_DropEndedShots(SwalotPlopGame *game) {
    SwalotPlopGame *walker;
    int i;
    SwalotPlopShot *shot;
    shot = (SwalotPlopShot *)game;
    i = 0;
    walker = game;
    shot = (SwalotPlopShot *)((u8 *)shot + offsetof(SwalotPlopGame, shots));
    for (; i < 0x60; i++, walker = (SwalotPlopGame *)((u8 *)walker + sizeof(SwalotPlopShot)), shot++) {
        if (walker->shots[0].remove == TRUE) {
            walker->shots[0].state = SHOT_STATE_FALLING;
        }
        SwalotPlopShot_CheckRemove(shot, game);
    }
}

static void SwalotPlopShot_CheckRemove(SwalotPlopShot *shot, SwalotPlopGame *game) {
    if (shot->remove) {
        if (shot->state == SHOT_STATE_CAUGHT && shot->params.caught == TRUE) {
            shot->state = SHOT_STATE_REPORT;
        } else {
            SwalotPlopShot_Clear(shot);
        }
        SwalotPlopGfx_RemoveShotView(&game->gfx, shot);
    }
}

static void SwalotPlopGame_DropFlyingShots(SwalotPlopGame *game) {
    int i;
    for (i = 0; i < 0x60; i++) {
        if (game->shots[i].state == SHOT_STATE_FLYING) {
            game->shots[i].state = SHOT_STATE_FALLING;
        }
    }
}

static BOOL SwalotPlopGame_HasActiveShots(SwalotPlopGame *game) {
    int i;
    for (i = 0; i < 0x60; i++) {
        u8 state = game->shots[i].state;
        if (state == SHOT_STATE_FLYING || state == SHOT_STATE_FALLING || state == SHOT_STATE_BOUNCED || state == SHOT_STATE_CAUGHT) {
            return TRUE;
        }
    }
    return FALSE;
}

static fx32 SwalotPlopShot_GetSpeed(SwalotPlopShot *shot) {
    fx32 speed = shot->params.speed - FX_Mul(shot->frames << FX32_SHIFT, 0x670);
    if (speed < 0) {
        speed = 0;
    }
    return speed;
}

static void SwalotPlopShot_GetMove(SwalotPlopShot *shot, VecFx32 *move) {
    fx32 speed = SwalotPlopShot_GetSpeed(shot);
    move->x = FX_Mul(shot->params.direction.x, speed);
    move->y = FX_Mul(shot->params.direction.y, speed);
    move->y += FX_Mul(shot->frames << FX32_SHIFT, -0x1334);
    move->z = FX_Mul(shot->params.direction.z, speed);
}

static void SwalotPlopShot_Bounce(SwalotPlopShot *shot, SwalotPlopGame *game, BOOL bounce) {
    fx32 damping;
    if (bounce == TRUE) {
        damping = FX32_CONST(0.85);
    } else {
        damping = FX32_CONST(0.8);
    }
    SwalotPlopShot_BounceEx(shot, game, bounce, damping, TRUE);
}

static void SwalotPlopShot_BounceEx(SwalotPlopShot *shot, SwalotPlopGame *game, BOOL reverse, fx32 damping, BOOL jitter) {
    VecFx32 move;
    if (reverse) {
        shot->params.direction.x = -shot->params.direction.x;
        shot->params.direction.z = -shot->params.direction.z;
        if (jitter) {
            u32 random = MTRandom();
            int z = shot->params.direction.z;
            int absZ = z < 0 ? -z : z;
            int x = shot->params.direction.x;
            int absX = x < 0 ? -x : x;
            if (absX <= absZ) {
                int value = FX_Mul(z, FX32_ONE);
                if ((random & 1) == 1) {
                    shot->params.direction.x = -value;
                } else {
                    shot->params.direction.x = value;
                }
            } else {
                int value = FX_Mul(x, FX32_ONE);
                if ((random & 1) == 1) {
                    shot->params.direction.z = -value;
                } else {
                    shot->params.direction.z = value;
                }
            }
        }
    }
    shot->params.speed = FX_Mul(shot->params.speed, damping);
    shot->frames = 0;
    shot->state = SHOT_STATE_BOUNCED;
    SwalotPlopShot_GetMove(shot, &move);
    SwalotPlopShot_Move(shot, &move);
    shot->frames++;
}

static void SwalotPlopGame_UpdateBouncedShot(SwalotPlopShot *shot, SwalotPlopGame *game) {
    BOOL done = FALSE;
    fx32 oldY;
    VecFx32 move;
    if (shot->state == SHOT_STATE_BOUNCED) {
        oldY = shot->position.y;
        SwalotPlopShot_GetMove(shot, &move);
        SwalotPlopShot_Move(shot, &move);
        shot->frames++;
        if (SwalotPlopBody_IsInside(&game->body, &shot->position) == TRUE) {
            SwalotPlopShot_Bounce(shot, game, TRUE);
        } else if (shot->position.y <= FX32_CONST(-50)) {
            if (!SwalotPlopShot_IsOnRim(shot)) {
                if (oldY <= FX32_CONST(-50)) {
                    done = TRUE;
                } else {
                    SwalotPlopShot_Bounce(shot, game, FALSE);
                }
            } else if (shot->position.y <= FX32_CONST(-100)) {
                done = TRUE;
            }
            if (shot->params.speed <= FX32_CONST(2)) {
                done = TRUE;
            }
        } else if (SwalotPlopShot_CheckOutside(shot) == TRUE) {
            VEC_Subtract(&shot->position, &move, &shot->position);
            SwalotPlopShot_BounceEx(shot, game, TRUE, FX32_CONST(0.25), FALSE);
        }
        if (shot->params.speed <= FX32_ONE) {
            done = TRUE;
        }
        if (done == TRUE) {
            SwalotPlopShot_End(game, shot, 0, game->combo);
        }
    }
}

static BOOL SwalotPlopShot_IsOnRim(SwalotPlopShot *shot) {
    fx32 distance = shot->distance;
    return distance >= FX32_CONST(188) && distance <= FX32_CONST(240);
}

static BOOL SwalotPlopShot_IsInPool(SwalotPlopShot *shot) {
    return shot->distance <= FX32_CONST(120);
}

static BOOL SwalotPlopShot_CheckOutside(SwalotPlopShot *shot) {
    fx32 distance = shot->distance;
    if (distance >= FX32_CONST(240)) {
        if (shot->crossedRim == TRUE) {
            return TRUE;
        }
    } else if (!shot->crossedRim) {
        shot->crossedRim = TRUE;
    }
    return FALSE;
}

static void SwalotPlopShot_Catch(SwalotPlopShot *shot, SwalotPlopGame *game) {
    shot->state = SHOT_STATE_CAUGHT;
    shot->timer = 8;
    shot->combo = game->combo;
}

static void SwalotPlopGame_UpdateCaughtShot(SwalotPlopShot *shot, SwalotPlopGame *game) {
    VecFx32 move;
    VecFx32 push;
    int result;

    SwalotPlopShot_GetMove(shot, &move);
    result = SwalotPlopBody_CheckMouth(&game->body, &shot->position, &move, &push);
    SwalotPlopShot_Move(shot, &move);
    if (result == 3 || result == 1) {
        SwalotPlopShot_Move(shot, &push);
    }
    shot->frames++;
    shot->timer--;
    if (shot->timer <= 0) {
        SwalotPlopShot_End(game, shot, 1, shot->combo);
    }
}

static void SwalotPlopShot_Move(SwalotPlopShot *shot, const VecFx32 *move) {
    VecFx32 horizontal;

    VEC_Add(&shot->position, move, &shot->position);
    horizontal.y = 0;
    horizontal.x = shot->position.x;
    horizontal.z = shot->position.z;
    shot->distance = VEC_Mag(&horizontal);
}

static void SwalotPlopGame_HandleTouch(SwalotPlopGame *game) {
    if (gSystem.touchHeld == 0) {
        SwalotPlopGame_Throw(game);
    } else {
        SwalotPlopGame_TrackFlick(game);
    }
}

static void SwalotPlopGame_Throw(SwalotPlopGame *game) {
    SwalotPlopPoint last;
    SwalotPlopPoint first;
    SwalotPlopShotParams params;
    BOOL tooSlow = FALSE;
    int count;
    fx32 speed;
    if (!game->flick.active) {
        game->releasedFrames++;
        return;
    }
    if (!SwalotPlopFlickQueue_PeekFirst(&game->flick, &last)) {
        GF_ASSERT(FALSE);
    }
    if (!SwalotPlopFlickQueue_PeekLast(&game->flick, &first)) {
        GF_ASSERT(FALSE);
    }
    count = SwalotPlopFlickQueue_Count(&game->flick);
    if (count >= 2) {
        params.owner = game->localIndex;
        params.caught = 0;
        params.direction.x = (s16)((last.x - first.x) / count) << 12;
        params.direction.z = (s16)((last.y - first.y) / count) << 12;
        params.direction.y = 0;
        speed = VEC_Mag(&params.direction);
        if (speed < FX32_CONST(8)) {
            if (speed) {
                VEC_Normalize(&params.direction, &params.direction);
                params.direction.x = (s64)(params.direction.x) * 0x8000 + 0x800 >> 12;
                params.direction.z = (s64)(params.direction.z) * 0x8000 + 0x800 >> 12;
            } else {
                tooSlow = TRUE;
            }
        } else if (speed > 0x1E200) {
            VEC_Normalize(&params.direction, &params.direction);
            params.direction.x = FX_Mul(params.direction.x, FX32_CONST(30.125));
            params.direction.z = FX_Mul(params.direction.z, FX32_CONST(30.125));
        }
        if (!tooSlow) {
            params.direction.y = FX32_CONST(38);
            params.speed = VEC_Mag(&params.direction);
            params.speed = FX_Mul(params.speed, FX32_CONST(0.6));
            VEC_Normalize(&params.direction, &params.direction);
            SwalotPlop_TouchToWorld(first.x, first.y, &params.start);
            params.start.y = 0;
            SwalotPlop_RotateToSeat(&params.start, &params.start, params.owner, game->playerCount);
            SwalotPlop_RotateToSeat(&params.direction, &params.direction, params.owner, game->playerCount);
            if (game->combo >= 5) {
                params.big = TRUE;
            } else {
                params.big = FALSE;
            }
            SwalotPlopGame_AddShot(game, &params, SHOT_STATE_FLYING);
            PlaySE(SEQ_SE_DP_FW104);
        }
    } else {
        tooSlow = TRUE;
    }
    if (tooSlow == TRUE) {
        SwalotPlopParticles_Spawn(game->gfx.particles, first.x, first.y);
    }
    SwalotPlopGame_EndFlick(game);
    game->releasedFrames = 0;
}

static void SwalotPlopGame_TrackFlick(SwalotPlopGame *game) {
    SwalotPlopFlick point;
    if (!game->flick.active && SwalotPlopGame_CanStartFlick(game) == TRUE) {
        game->flick.active = TRUE;
        SwalotPlopCursor_Show(&game->gfx.cursor);
        SwalotPlopBgFade_Step(&game->gfx.bgFade);
    }
    if (game->flick.active == TRUE) {
        u16 count = game->touch.count;
        if (count == 1) {
            point.x = game->touch.samples[0].x;
            point.y = game->touch.samples[0].y;
            SwalotPlopFlickQueue_Push(&game->flick, point);
        } else if (count >= 2) {
            point.x = game->touch.samples[0].x;
            point.y = game->touch.samples[0].y;
            SwalotPlopFlickQueue_Push(&game->flick, point);
            count = game->touch.count;
            point.x = game->touch.samples[count - 1].x;
            point.y = game->touch.samples[count - 1].y;
            SwalotPlopFlickQueue_Push(&game->flick, point);
        } else {
            point.x = gSystem.touchX;
            point.y = gSystem.touchY;
            SwalotPlopFlickQueue_Push(&game->flick, point);
        }
    }
}

static BOOL SwalotPlopGame_CanStartFlick(SwalotPlopGame *game) {
    s32 x;
    s32 y;

    if (game->releasedFrames < 2) {
        return FALSE;
    }
    x = 128 - gSystem.touchX;
    y = -96 - gSystem.touchY;
    if ((FX_Sqrt((x * x + y * y) << FX32_SHIFT) >> FX32_SHIFT) <= 286) {
        return TRUE;
    }
    return FALSE;
}

static void SwalotPlopFlickQueue_Clear(SwalotPlopFlickQueue *queue) {
    memset(queue->points, 0, sizeof(queue->points));
    queue->read = 0;
    queue->write = 0;
}

static void SwalotPlopFlickQueue_Push(SwalotPlopFlickQueue *queue, SwalotPlopFlick point) {
    SwalotPlopFlick dropped;
    if ((queue->write + 1) % 8 == queue->read) {
        SwalotPlopFlickQueue_Pop(queue, &dropped);
    }
    queue->points[queue->write] = point;
    queue->write = (queue->write + 1) % 8;
}

static BOOL SwalotPlopFlickQueue_Pop(SwalotPlopFlickQueue *queue, SwalotPlopFlick *point) {
    if (queue->write == queue->read) {
        return FALSE;
    }
    *point = queue->points[queue->read];
    queue->read = (queue->read + 1) % 8;
    return TRUE;
}

static BOOL SwalotPlopFlickQueue_PeekFirst(SwalotPlopFlickQueue *queue, SwalotPlopPoint *point) {
    u16 read = queue->read;
    if (queue->write == read) {
        return FALSE;
    }
    point->x = queue->points[read].x;
    point->y = queue->points[read].y;
    return TRUE;
}

static BOOL SwalotPlopFlickQueue_PeekLast(SwalotPlopFlickQueue *queue, SwalotPlopPoint *point) {
    int last;
    if (queue->write == queue->read) {
        return FALSE;
    }
    last = queue->write - 1;
    if (last < 0) {
        last += 8;
    }
    point->x = queue->points[last].x;
    point->y = queue->points[last].y;
    return TRUE;
}

static u32 SwalotPlopFlickQueue_Count(SwalotPlopFlickQueue *queue) {
    if (queue->read > queue->write) {
        return 8 - queue->read + queue->write;
    }
    return queue->write - queue->read;
}

static void SwalotPlopGame_EndFlick(SwalotPlopGame *game) {
    game->flick.active = FALSE;
    SwalotPlopFlickQueue_Clear(&game->flick);
    SwalotPlopCursor_Hide(&game->gfx.cursor);
}

static void SwalotPlopLevelBlend_Init(SwalotPlopLevelBlend *blend, SwalotPlopShared *shared, const u8 *options) {
    blend->step = 16;
    SwalotPlop_GetLevel(shared, options, 0, (SwalotPlopLevel *)blend);
}

static void SwalotPlopLevelBlend_Start(SwalotPlopLevelBlend *blend, SwalotPlopShared *shared, const u8 *options, int from, u16 to) {
    SwalotPlopLevel start;
    SwalotPlopLevel end;
    s32 forward;
    s32 wrapped;
    SwalotPlop_GetLevel(shared, options, from, &start);
    SwalotPlop_GetLevel(shared, options, to, &end);
    *(SwalotPlopLevel *)blend->start = start;
    *(SwalotPlopLevel *)blend->current = start;
    blend->delta[0] = end.hitCount - start.hitCount;
    blend->delta[2] = end.cameraHeight - start.cameraHeight;
    blend->delta[4] = end.mouthOffset - start.mouthOffset;
    blend->delta[5] = end.scale - start.scale;
    wrapped = end.spinSpeed + 0xFFFF - start.spinSpeed;
    forward = end.spinSpeed - start.spinSpeed;
    if (forward < wrapped) {
        blend->delta[1] = forward;
    } else {
        blend->delta[1] = wrapped;
    }
    wrapped = end.cameraPitch + 0xFFFF - start.cameraPitch;
    forward = end.cameraPitch - start.cameraPitch;
    if (forward < wrapped) {
        blend->delta[3] = forward;
    } else {
        blend->delta[3] = wrapped;
    }
    blend->step = 0;
}

static BOOL SwalotPlopLevelBlend_Update(SwalotPlopLevelBlend *blend) {
    s32 step = blend->step;
    if (step >= 16) {
        return TRUE;
    }
    step++;
    blend->step = step;
    blend->current[0] = blend->start[0] + blend->delta[0] * step / 16;
    blend->current[1] = blend->start[1] + blend->delta[1] * blend->step / 16;
    blend->current[2] = blend->start[2] + blend->delta[2] * blend->step / 16;
    blend->current[3] = blend->start[3] + blend->delta[3] * blend->step / 16;
    blend->current[4] = blend->start[4] + blend->delta[4] * blend->step / 16;
    blend->current[5] = blend->start[5] + blend->delta[5] * blend->step / 16;
    return FALSE;
}

static void SwalotPlopGame_ApplyLevel(SwalotPlopGame *game, SwalotPlopLevelBlend *blend) {
    SwalotPlopGame_ApplyLevelParams(game, blend);
}

static void SwalotPlopGame_ApplyLevelParams(SwalotPlopGame *game, SwalotPlopLevelBlend *blend) {
    SwalotPlopLevel *level = (SwalotPlopLevel *)blend->current;
    SwalotPlopBody_SetSpinSpeed(&game->body, level->spinSpeed);
    SwalotPlopGfx_SetCameraHeight(&game->gfx, level->cameraHeight);
    SwalotPlopGfx_SetCameraPitch(&game->gfx, level->cameraPitch);
    SwalotPlopBody_SetMouthOffset(&game->body, level->mouthOffset);
    SwalotPlopBody_SetScale(&game->body, level->scale);
}

static void SwalotPlop_GetLevel(SwalotPlopShared *shared, const u8 *options, int index, SwalotPlopLevel *level) {
    int mirrored;
    GF_ASSERT((u32)index < shared->levelTable.count);
    *level = shared->levelTable.levels[index];
    level->spinSpeed = (u16)level->spinSpeed;
    level->cameraPitch = (u16)level->cameraPitch;
    if (options[0] != 0) {
        mirrored = 6 - (index + 1);
        if (mirrored < 0) {
            mirrored = 0;
        }
        level->scale = shared->levelTable.levels[mirrored].scale;
    }
    if (options[1] != 0) {
        level->spinSpeed = -level->spinSpeed;
    }
}

static void SwalotPlopDemo_Init(SwalotPlopDemo *demo, SwalotPlopGfx *gfx, enum HeapID heapId) {
    SimpleSpriteTemplate template;
    NARC *narc;
    memset(demo, 0, sizeof(SwalotPlopDemo));
    narc = NARC_New(NARC_a_1_9_8, heapId);
    demo->res[GF_GFX_RES_TYPE_CHAR] = AddCharResObjFromOpenNarc(gfx->resMan[GF_GFX_RES_TYPE_CHAR], narc, 21, FALSE, 120, NNS_G2D_VRAM_TYPE_2DSUB, heapId);
    demo->res[GF_GFX_RES_TYPE_PLTT] = AddPlttResObjFromOpenNarc(gfx->resMan[GF_GFX_RES_TYPE_PLTT], narc, 20, FALSE, 120, NNS_G2D_VRAM_TYPE_2DSUB, 2, heapId);
    demo->res[GF_GFX_RES_TYPE_CELL] = AddCellOrAnimResObjFromOpenNarc(gfx->resMan[GF_GFX_RES_TYPE_CELL], narc, 22, FALSE, 120, GF_GFX_RES_TYPE_CELL, heapId);
    demo->res[GF_GFX_RES_TYPE_ANIM] = AddCellOrAnimResObjFromOpenNarc(gfx->resMan[GF_GFX_RES_TYPE_ANIM], narc, 23, FALSE, 120, GF_GFX_RES_TYPE_ANIM, heapId);
    if (!SpriteTransfer_CreateCharTransferTask_UpdateMappingTypeFromHW_AllocAtEnd(demo->res[GF_GFX_RES_TYPE_CHAR])) {
        GF_ASSERT(FALSE);
    }
    if (!SpriteTransfer_CreatePlttTransferTask(demo->res[GF_GFX_RES_TYPE_PLTT])) {
        GF_ASSERT(FALSE);
    }
    GF2DGfxResObj_FreeData(demo->res[GF_GFX_RES_TYPE_CHAR]);
    GF2DGfxResObj_FreeData(demo->res[GF_GFX_RES_TYPE_PLTT]);
    CreateSpriteResourcesHeader(&demo->header, 120, 120, 120, 120, -1, -1, FALSE, 0, gfx->resMan[GF_GFX_RES_TYPE_CHAR], gfx->resMan[GF_GFX_RES_TYPE_PLTT], gfx->resMan[GF_GFX_RES_TYPE_CELL], gfx->resMan[GF_GFX_RES_TYPE_ANIM], NULL, NULL);
    template.spriteList = gfx->spriteList;
    template.header = &demo->header;
    template.position.x = 128 * FX32_ONE;
    template.position.y = FX32_CONST(632);
    template.priority = 0;
    template.whichScreen = NNS_G2D_VRAM_TYPE_2DSUB;
    template.heapID = heapId;
    demo->sprite = Sprite_Create(&template);
    Sprite_SetDrawFlag(demo->sprite, FALSE);
    NARC_Delete(narc);
}

static void SwalotPlopDemo_Free(SwalotPlopDemo *demo, SwalotPlopGfx *gfx) {
    Sprite_Delete(demo->sprite);
    SpriteTransfer_DeleteCharTransferTask(demo->res[GF_GFX_RES_TYPE_CHAR]);
    SpriteTransfer_DeletePlttTransferTask(demo->res[GF_GFX_RES_TYPE_PLTT]);
    DestroySingle2DGfxResObj(gfx->resMan[GF_GFX_RES_TYPE_CHAR], demo->res[GF_GFX_RES_TYPE_CHAR]);
    DestroySingle2DGfxResObj(gfx->resMan[GF_GFX_RES_TYPE_PLTT], demo->res[GF_GFX_RES_TYPE_PLTT]);
    DestroySingle2DGfxResObj(gfx->resMan[GF_GFX_RES_TYPE_CELL], demo->res[GF_GFX_RES_TYPE_CELL]);
    DestroySingle2DGfxResObj(gfx->resMan[GF_GFX_RES_TYPE_ANIM], demo->res[GF_GFX_RES_TYPE_ANIM]);
    memset(demo, 0, sizeof(SwalotPlopDemo));
}

static void SwalotPlopDemo_Start(SwalotPlopDemo *demo) {
    demo->active = TRUE;
    demo->state = SWALOT_PLOP_DEMO_SHOW_HAND;
    demo->count = 0;
    Sprite_SetDrawFlag(demo->sprite, TRUE);
}

static BOOL SwalotPlopDemo_Update(SwalotPlopDemo *demo, SwalotPlopGame *game) {
    switch (demo->state) {
    case SWALOT_PLOP_DEMO_SHOW_HAND:
        Sprite_SetAnimationFrame(demo->sprite, 0);
        SwalotPlopCursor_Show(&game->gfx.cursor);
        SwalotPlopCursor_SetPos(&game->gfx.cursor, 128, 177);
        demo->thrown = FALSE;
        demo->state++;
        break;
    case SWALOT_PLOP_DEMO_THROW: {
        int frame;
        Sprite_UpdateAnim(demo->sprite, 2 * FX32_ONE);
        frame = Sprite_GetAnimationFrame(demo->sprite);
        switch (frame) {
        case 10:
            if (!demo->thrown) {
                SwalotPlopGame_ThrowDemoShot(game, 128, 96, 128, 64, SHOT_STATE_FALLING);
                PlaySE(SEQ_SE_DP_FW104);
                demo->thrown = TRUE;
            }
            break;
        case 11:
            demo->count++;
            if (demo->count >= 2) {
                demo->state = SWALOT_PLOP_DEMO_WAIT_SHOTS;
            } else {
                demo->state = SWALOT_PLOP_DEMO_PAUSE;
                demo->timer = 16;
            }
            SwalotPlopCursor_Hide(&game->gfx.cursor);
            break;
        }
        SwalotPlopCursor_SetPos(&game->gfx.cursor, 128, gSwalotPlopDemoCursorYs[frame]);
        break;
    }
    case SWALOT_PLOP_DEMO_PAUSE:
        demo->timer--;
        if (demo->timer <= 0) {
            demo->state = SWALOT_PLOP_DEMO_SHOW_HAND;
        }
        break;
    case SWALOT_PLOP_DEMO_WAIT_SHOTS:
        if (!SwalotPlopGame_HasActiveShots(game)) {
            Sprite_SetDrawFlag(demo->sprite, FALSE);
            return TRUE;
        }
        break;
    }
    SwalotPlopGame_UpdateShots(game);
    SwalotPlopGame_RemoveEndedShots(game);
    return FALSE;
}

static void SwalotPlopScoreReports_Init(SwalotPlopScoreReports *reports) {
    u8 *bytes = (u8 *)reports;
    u32 size = sizeof(SwalotPlopScoreReports);
    do {
        *bytes++ = 0;
    } while (--size != 0);
    reports->leader = 0xFFFF;
}

static void SwalotPlopScoreReports_Set(SwalotPlopScoreReports *reports, u32 score, u32 index) {
    reports->scores[index] = score;
    reports->mask |= 1 << index;
    if (score != 0) {
        if (reports->leader != 0xFFFF) {
            if (reports->scores[reports->leader] >= score) {
                return;
            }
            reports->leader = index;
            return;
        }
        reports->leader = index;
    }
}

static void SwalotPlopScoreReports_Reset(SwalotPlopScoreReports *reports) {
    reports->mask = 0;
    reports->leader = 0xFFFF;
}

static u32 SwalotPlopScoreReports_GetLeader(SwalotPlopScoreReports *reports) {
    return reports->leader;
}

static BOOL SwalotPlopScoreReports_HasAll(SwalotPlopScoreReports *reports, u32 playerCount) {
    int i;
    int count = 0;
    for (i = 0; i < PLAZA_GAME_MAX_PLAYERS; i++) {
        if (reports->mask & (1 << i)) {
            count++;
        }
    }
    return playerCount <= count;
}

static void SwalotPlopGfx_SetBanks(void) {
    GraphicsBanks banks;
    banks = gSwalotPlopBanks;
    GfGfx_SetBanks(&banks);
}

static void SwalotPlopGfx_Setup3D(void) {
    volatile u16 *reg = &reg_G2_BG0CNT;
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_BG0, GF_PLANE_TOGGLE_ON);
    *reg = (*reg & ~3) | 2;
    reg = &reg_G3X_DISP3DCNT;
    *reg &= ~0x3002;
    *reg = (*reg & ~0x3000) | 16;
    *reg &= 0xCFFB;
    *reg = (*reg & ~0x3000) | 8;
    *reg = (*reg & ~0x3000) | 32;
    G3X_SetFog(FALSE, GX_FOGBLEND_COLOR_ALPHA, GX_FOGSLOPE_0x8000, 0);
    G3X_SetClearColor(GX_RGB(26, 26, 26), 0, GF_GX_CLEARCOLORDEPTH_MAX, 63, FALSE);
    reg_G3_VIEWPORT = 0xBFFF0000;
}

static void SwalotPlopGfx_Init3D(SwalotPlopGfx *gfx, int playerCount, int localIndex, enum HeapID heapId) {
    gfx->vramMan = GF_3DVramMan_Create(heapId, 0, 1, 0, 4, SwalotPlopGfx_Setup3D);
    SwalotPlopGfx_InitCamera(gfx, playerCount, localIndex, heapId);
}

static void SwalotPlopGfx_Free3D(SwalotPlopGfx *gfx) {
    SwalotPlopGfx_FreeCamera(gfx);
    GF_3DVramMan_Delete(gfx->vramMan);
}

static void SwalotPlopGame_InitGfx(SwalotPlopGame *game, enum HeapID heapId) {
    HeapExp_FndInitAllocator(&game->allocator, heapId, 0x20);
    SwalotPlopGfx_SetBanks();
    SwalotPlopGfx_Init3D(&game->gfx, game->playerCount, game->localIndex, heapId);
    SwalotPlopGfx_InitBgs(&game->gfx.bgConfig, heapId);
    SwalotPlopGfx_InitSprites(&game->gfx, heapId);
    SwalotPlopGfx_InitText(&game->gfx, heapId);
    NARC *narc = NARC_New(NARC_a_1_8_9, heapId);
    SwalotPlopGfx_LoadSpriteRes(&game->gfx, narc, heapId);
    SwalotPlopStage_Init(&game->gfx.stage, &game->gfx, narc, game->playerCount, game->localIndex, heapId, &game->allocator);
    SwalotPlopScene_Init(&game->gfx.scene, narc, heapId, &game->allocator);
    SwalotPlopGfx_InitShotViews(&game->gfx, narc, heapId);
    SwalotPlopCursor_Init(&game->gfx, &game->gfx.cursor, game->localIndex, heapId);
    SwalotPlopParticles_Init(&game->gfx, game->gfx.particles, game->localIndex, heapId);
    SwalotPlopGfx_LoadSubBgs(&game->gfx, narc, game->localIndex, heapId);
    SwalotPlopIntro_Init(&game->gfx.intro, &game->gfx, game->shared, game->playerCount, game->localIndex, narc, heapId);
    SwalotPlopFinish_Init(&game->gfx, game->playerCount, game->localIndex, heapId);
    SwalotPlopPopups_Init(game->gfx.popups, &game->gfx, game->playerCount, game->localIndex, heapId);
    SwalotPlopBgFade_Init(&game->gfx.bgFade, game->localIndex);
    SwalotPlopCombo_Init(&game->gfx.combo, &game->gfx, narc, heapId);
    NARC_Delete(narc);
    game->gfx.sign = PlazaGameSign_New(game->gfx.spriteList, heapId);
}

static void SwalotPlopGame_FreeGfx(SwalotPlopGame *game) {
    SwalotPlopCombo_Free(&game->gfx.combo, &game->gfx);
    SwalotPlopPopups_Free(game->gfx.popups, &game->gfx);
    SwalotPlopGfx_FreeDummy1(&game->gfx, game->playerCount);
    SwalotPlopIntro_Free(&game->gfx.intro, &game->gfx);
    SwalotPlopGfx_FreeDummy2(&game->gfx);
    SwalotPlopParticles_Free(&game->gfx, game->gfx.particles);
    SwalotPlopCursor_Free(&game->gfx.cursor);
    SwalotPlopGfx_FreeShotViews(&game->gfx);
    SwalotPlopScene_Free(&game->gfx.scene, &game->allocator);
    SwalotPlopStage_Free(&game->gfx.stage, &game->gfx, &game->allocator);
    SwalotPlopGfx_FreeSpriteRes(&game->gfx);
    PlazaGameSign_Free(game->gfx.sign);
    SwalotPlopGfx_FreeBgs(&game->gfx);
    SwalotPlopGfx_FreeSprites(&game->gfx);
    SwalotPlopGfx_FreeText(&game->gfx);
    SwalotPlopGfx_Free3D(&game->gfx);
    G2_BlendNone();
    G2S_BlendNone();
}

static void SwalotPlopGame_UpdateGfx(SwalotPlopGame *game, SwalotPlopGfx *gfx) {
    SwalotPlopScene_Update(&gfx->scene, &game->body, game->phase);
    SwalotPlopScene_Animate(&gfx->scene, &game->body, game->phase);
    SwalotPlopCursor_Update(&gfx->cursor);
    SwalotPlopGame_Draw(game, gfx);
    SwalotPlopBgFade_Update(&gfx->bgFade, gfx);
    SwalotPlopCombo_Update(&gfx->combo);
}

static void SwalotPlopGame_Draw(SwalotPlopGame *game, SwalotPlopGfx *gfx) {
    u32 pop0;
    u32 pop1;
    u32 pop2;
    SwalotPlopParticles_Update(gfx->particles);
    SwalotPlopPopups_Update(gfx->popups);
    Thunk_G3X_Reset();
    SwalotPlopGfx_ApplyCamera(gfx);
    NNS_G3dGlbLightVector(GX_LIGHTID_0, 0, -FX32_ONE, 0);
    NNS_G3dGlbLightColor(GX_LIGHTID_0, GX_RGB(31, 31, 31));
    NNS_G3dGlbMaterialColorDiffAmb(GX_RGB(31, 31, 31), GX_RGB(31, 31, 31), FALSE);
    NNS_G3dGlbMaterialColorSpecEmi(GX_RGB(31, 31, 31), GX_RGB(31, 31, 31), FALSE);
    NNS_G3dGeBufferOP_N(G3OP_MTX_PUSH, NULL, 0);
    SwalotPlopStage_Draw(&gfx->stage);
    pop0 = 1;
    NNS_G3dGeBufferOP_N(G3OP_MTX_POP, &pop0, 1);
    NNS_G3dGeBufferOP_N(G3OP_MTX_PUSH, NULL, 0);
    SwalotPlopScene_Draw(&gfx->scene, &game->body);
    pop1 = 1;
    NNS_G3dGeBufferOP_N(G3OP_MTX_POP, &pop1, 1);
    NNS_G3dGeBufferOP_N(G3OP_MTX_PUSH, NULL, 0);
    SwalotPlopGfx_DrawShotViews(gfx, game->playerCount);
    pop2 = 1;
    NNS_G3dGeBufferOP_N(G3OP_MTX_POP, &pop2, 1);
    RequestSwap3DBuffers(GX_SORTMODE_AUTO, GX_BUFFERMODE_Z);
    SpriteList_RenderAndAnimateSprites(gfx->spriteList);
}

static void SwalotPlopGfx_InitBgs(BgConfig **bgConfig, enum HeapID heapId) {
    *bgConfig = BgConfig_Alloc(heapId);
    GraphicsModes modes = gSwalotPlopGraphicsModes;
    SetBothScreensModesAndDisable(&modes);
    BgTemplate main1 = gSwalotPlopMain1BgTemplate;
    InitBgFromTemplate(*bgConfig, GF_BG_LYR_MAIN_1, &main1, GF_BG_TYPE_TEXT);
    BG_ClearCharDataRange(GF_BG_LYR_MAIN_1, 0x20, 0, heapId);
    BgClearTilemapBufferAndCommit(*bgConfig, GF_BG_LYR_MAIN_1);
    BgTemplate main2 = gSwalotPlopMain2BgTemplate;
    InitBgFromTemplate(*bgConfig, GF_BG_LYR_MAIN_2, &main2, GF_BG_TYPE_TEXT);
    BG_ClearCharDataRange(GF_BG_LYR_MAIN_2, 0x20, 0, heapId);
    BgClearTilemapBufferAndCommit(*bgConfig, GF_BG_LYR_MAIN_2);
    BgTemplate sub0 = gSwalotPlopSub0BgTemplate;
    InitBgFromTemplate(*bgConfig, GF_BG_LYR_SUB_0, &sub0, GF_BG_TYPE_TEXT);
    BG_ClearCharDataRange(GF_BG_LYR_SUB_0, 0x20, 0, heapId);
    BgClearTilemapBufferAndCommit(*bgConfig, GF_BG_LYR_SUB_0);
    BgTemplate sub1 = gSwalotPlopSub1BgTemplate;
    InitBgFromTemplate(*bgConfig, GF_BG_LYR_SUB_1, &sub1, GF_BG_TYPE_TEXT);
    BG_ClearCharDataRange(GF_BG_LYR_SUB_1, 0x20, 0, heapId);
    BgClearTilemapBufferAndCommit(*bgConfig, GF_BG_LYR_SUB_1);
    BgTemplate sub2 = gSwalotPlopSub2BgTemplate;
    InitBgFromTemplate(*bgConfig, GF_BG_LYR_SUB_2, &sub2, GF_BG_TYPE_TEXT);
    BG_ClearCharDataRange(GF_BG_LYR_SUB_2, 0x20, 0, heapId);
    BgClearTilemapBufferAndCommit(*bgConfig, GF_BG_LYR_SUB_2);
    BgTemplate sub3 = gSwalotPlopSub3BgTemplate;
    InitBgFromTemplate(*bgConfig, GF_BG_LYR_SUB_3, &sub3, GF_BG_TYPE_TEXT);
    BG_ClearCharDataRange(GF_BG_LYR_SUB_3, 0x20, 0, heapId);
    BgClearTilemapBufferAndCommit(*bgConfig, GF_BG_LYR_SUB_3);
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_BG0, GF_PLANE_TOGGLE_ON);
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_BG1, GF_PLANE_TOGGLE_OFF);
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_BG2, GF_PLANE_TOGGLE_ON);
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_BG3, GF_PLANE_TOGGLE_OFF);
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_OBJ, GF_PLANE_TOGGLE_ON);
    GfGfx_EngineBTogglePlanes(GX_PLANEMASK_BG0, GF_PLANE_TOGGLE_ON);
    GfGfx_EngineBTogglePlanes(GX_PLANEMASK_BG1, GF_PLANE_TOGGLE_ON);
    GfGfx_EngineBTogglePlanes(GX_PLANEMASK_BG2, GF_PLANE_TOGGLE_ON);
    GfGfx_EngineBTogglePlanes(GX_PLANEMASK_BG3, GF_PLANE_TOGGLE_ON);
    GfGfx_EngineBTogglePlanes(GX_PLANEMASK_OBJ, GF_PLANE_TOGGLE_ON);
    LoadFontPal1(GF_PAL_LOCATION_MAIN_BG, GF_PAL_SLOT_5_OFFSET, heapId);
    LoadUserFrameGfx1(*bgConfig, GF_BG_LYR_MAIN_1, 1, 4, 0, heapId);
}

static void SwalotPlopGfx_FreeBgs(SwalotPlopGfx *gfx) {
    FreeBgTilemapBuffer(gfx->bgConfig, GF_BG_LYR_MAIN_1);
    FreeBgTilemapBuffer(gfx->bgConfig, GF_BG_LYR_MAIN_2);
    FreeBgTilemapBuffer(gfx->bgConfig, GF_BG_LYR_SUB_0);
    FreeBgTilemapBuffer(gfx->bgConfig, GF_BG_LYR_SUB_1);
    FreeBgTilemapBuffer(gfx->bgConfig, GF_BG_LYR_SUB_2);
    FreeBgTilemapBuffer(gfx->bgConfig, GF_BG_LYR_SUB_3);
    Heap_Free(gfx->bgConfig);
}

static void SwalotPlopGfx_InitSprites(SwalotPlopGfx *gfx, enum HeapID heapId) {
    int i;
    ObjCharTransferTemplate template;

    NNS_G2dInitOamManagerModule();
    OamManager_Create(0, 0x7E, 0, 0x1F, 0, 0x7E, 0, 0x1F, heapId);
    template = gSwalotPlopObjCharTransfer;
    template.heapID = heapId;
    ObjCharTransfer_InitEx(&template, GX_OBJVRAMMODE_CHAR_1D_128K, GX_OBJVRAMMODE_CHAR_1D_128K);
    ObjPlttTransfer_Init(0x10, heapId);
    ObjCharTransfer_ClearBuffers();
    ObjPlttTransfer_Reset();
    G2dRenderer_SetObjCharTransferReservedRegion(NNS_G2D_VRAM_TYPE_2DMAIN, GX_OBJVRAMMODE_CHAR_1D_128K);
    G2dRenderer_SetPlttTransferReservedRegion(NNS_G2D_VRAM_TYPE_2DMAIN);
    gfx->spriteList = G2dRenderer_Init(0x80, &gfx->renderer, heapId);
    G2dRenderer_SetSubSurfaceCoords(&gfx->renderer, 0, 2 << 20);
    for (i = 0; i < 4; i++) {
        gfx->resMan[i] = Create2DGfxResObjMan(0x10, (GfGfxResType)i, heapId);
    }
    gfx->fontSystem = FontSystem_NewInit(8, heapId);
}

static void SwalotPlopGfx_FreeSprites(SwalotPlopGfx *gfx) {
    int i;
    FontSystem_Delete(gfx->fontSystem);
    SpriteList_Delete(gfx->spriteList);
    for (i = 0; i < 4; i++) {
        Destroy2DGfxResObjMan(gfx->resMan[i]);
    }
    ObjCharTransfer_Destroy();
    ObjPlttTransfer_Destroy();
    OamManager_Free();
}

static void SwalotPlopGfx_LoadSpriteRes(SwalotPlopGfx *gfx, NARC *narc, enum HeapID heapId) {
    gfx->res[GF_GFX_RES_TYPE_CHAR] = AddCharResObjFromOpenNarc(gfx->resMan[GF_GFX_RES_TYPE_CHAR], narc, 2, FALSE, 200, NNS_G2D_VRAM_TYPE_2DMAIN, heapId);
    gfx->res[GF_GFX_RES_TYPE_PLTT] = AddPlttResObjFromOpenNarc(gfx->resMan[GF_GFX_RES_TYPE_PLTT], narc, 3, FALSE, 200, NNS_G2D_VRAM_TYPE_2DMAIN, 8, heapId);
    gfx->res[GF_GFX_RES_TYPE_CELL] = AddCellOrAnimResObjFromOpenNarc(gfx->resMan[GF_GFX_RES_TYPE_CELL], narc, 1, FALSE, 200, GF_GFX_RES_TYPE_CELL, heapId);
    gfx->res[GF_GFX_RES_TYPE_ANIM] = AddCellOrAnimResObjFromOpenNarc(gfx->resMan[GF_GFX_RES_TYPE_ANIM], narc, 0, FALSE, 200, GF_GFX_RES_TYPE_ANIM, heapId);
    if (!SpriteTransfer_CreateCharTransferTask_UpdateMappingTypeFromHW_AllocAtEnd(gfx->res[GF_GFX_RES_TYPE_CHAR])) {
        GF_ASSERT(FALSE);
    }
    if (!SpriteTransfer_CreatePlttTransferTask(gfx->res[GF_GFX_RES_TYPE_PLTT])) {
        GF_ASSERT(FALSE);
    }
    GF2DGfxResObj_FreeData(gfx->res[GF_GFX_RES_TYPE_CHAR]);
    GF2DGfxResObj_FreeData(gfx->res[GF_GFX_RES_TYPE_PLTT]);
    CreateSpriteResourcesHeader(&gfx->header, 200, 200, 200, 200, -1, -1, FALSE, 0, gfx->resMan[GF_GFX_RES_TYPE_CHAR], gfx->resMan[GF_GFX_RES_TYPE_PLTT], gfx->resMan[GF_GFX_RES_TYPE_CELL], gfx->resMan[GF_GFX_RES_TYPE_ANIM], NULL, NULL);
}

static void SwalotPlopGfx_FreeSpriteRes(SwalotPlopGfx *gfx) {
    SpriteTransfer_DeleteCharTransferTask(gfx->res[GF_GFX_RES_TYPE_CHAR]);
    SpriteTransfer_DeletePlttTransferTask(gfx->res[GF_GFX_RES_TYPE_PLTT]);
    DestroySingle2DGfxResObj(gfx->resMan[GF_GFX_RES_TYPE_CHAR], gfx->res[GF_GFX_RES_TYPE_CHAR]);
    DestroySingle2DGfxResObj(gfx->resMan[GF_GFX_RES_TYPE_PLTT], gfx->res[GF_GFX_RES_TYPE_PLTT]);
    DestroySingle2DGfxResObj(gfx->resMan[GF_GFX_RES_TYPE_CELL], gfx->res[GF_GFX_RES_TYPE_CELL]);
    DestroySingle2DGfxResObj(gfx->resMan[GF_GFX_RES_TYPE_ANIM], gfx->res[GF_GFX_RES_TYPE_ANIM]);
}

static void SwalotPlopGfx_InitText(SwalotPlopGfx *gfx, enum HeapID heapId) {
    gfx->msgFormat = MessageFormat_New(heapId);
    gfx->msgData = NewMsgDataFromNarc(MSGDATA_LOAD_DIRECT, NARC_msgdata_msg, NARC_msg_msg_0230_bin, heapId);
    gfx->string1 = String_New(0x80, heapId);
    gfx->string2 = String_New(0x80, heapId);
}

static void SwalotPlopGfx_FreeText(SwalotPlopGfx *gfx) {
    String_Delete(gfx->string2);
    String_Delete(gfx->string1);
    DestroyMsgData(gfx->msgData);
    MessageFormat_Delete(gfx->msgFormat);
}

static void SwalotPlopGfx_InitCamera(SwalotPlopGfx *gfx, int playerCount, int localIndex, enum HeapID heapId) {
    CameraAngle angle;
    VecFx32 up;
    gfx->camera = Camera_New(heapId);
    gfx->cameraTarget.x = 0;
    gfx->cameraTarget.y = 0;
    gfx->cameraTarget.z = 0;
    angle.x = 0xF112;
    angle.y = gSwalotPlopCameraAngles[playerCount][localIndex];
    angle.z = 0;
    Camera_Init_FromTargetDistanceAndAngle(&gfx->cameraTarget, 0x1D9000, &angle, 0xFA4, 0, TRUE, gfx->camera);
    up.x = 0;
    up.y = FX32_ONE;
    up.z = 0;
    Camera_SetLookAtCamUp(&up, gfx->camera);
    Camera_SetStaticPtr(gfx->camera);
    Camera_SetPerspectiveClippingPlane(FX32_CONST(200), FX32_CONST(1000), gfx->camera);
}

static void SwalotPlopGfx_FreeCamera(SwalotPlopGfx *gfx) {
    Camera_Delete(gfx->camera);
}

static void SwalotPlopGfx_ApplyCamera(SwalotPlopGfx *gfx) {
    Camera_PushLookAtToNNSGlb();
}

static void SwalotPlopGfx_LoadSubBgs(SwalotPlopGfx *gfx, NARC *narc, int localIndex, enum HeapID heapId) {
    int palette;
    GfGfxLoader_LoadCharDataFromOpenNarc(narc, 12, gfx->bgConfig, GF_BG_LYR_SUB_0, 0, 0, FALSE, heapId);
    GfGfxLoader_LoadScrnDataFromOpenNarc(narc, 14, gfx->bgConfig, GF_BG_LYR_SUB_3, 0, 0, FALSE, heapId);
    GfGfxLoader_LoadScrnDataFromOpenNarc(narc, 15, gfx->bgConfig, GF_BG_LYR_SUB_2, 0, 0, FALSE, heapId);
    GfGfxLoader_LoadScrnDataFromOpenNarc(narc, 16, gfx->bgConfig, GF_BG_LYR_SUB_1, 0, 0, FALSE, heapId);
    GfGfxLoader_LoadScrnDataFromOpenNarc(narc, 17, gfx->bgConfig, GF_BG_LYR_SUB_0, 0, 0, FALSE, heapId);
    GfGfxLoader_GXLoadPalFromOpenNarc(narc, 13, GF_PAL_LOCATION_SUB_BG, GF_PAL_SLOT_0_OFFSET, 384, heapId);
    BgTilemapRectChangePalette(gfx->bgConfig, GF_BG_LYR_SUB_3, 0, 0, 32, 32, sSwalotPlopSubBgPalettes[localIndex]);
    palette = localIndex * 2;
    BgTilemapRectChangePalette(gfx->bgConfig, GF_BG_LYR_SUB_2, 0, 0, 32, 32, palette + 2);
    palette = localIndex * 2 + 1;
    BgTilemapRectChangePalette(gfx->bgConfig, GF_BG_LYR_SUB_1, 0, 0, 32, 32, palette);
    BgTilemapRectChangePalette(gfx->bgConfig, GF_BG_LYR_SUB_0, 0, 0, 32, 32, palette);
    BgCommitTilemapBufferToVram(gfx->bgConfig, GF_BG_LYR_SUB_0);
    BgCommitTilemapBufferToVram(gfx->bgConfig, GF_BG_LYR_SUB_1);
    BgCommitTilemapBufferToVram(gfx->bgConfig, GF_BG_LYR_SUB_2);
    BgCommitTilemapBufferToVram(gfx->bgConfig, GF_BG_LYR_SUB_3);
}

static void SwalotPlopGfx_FreeDummy2(SwalotPlopGfx *gfx) {
}

static inline const NNSG3dResName *GetResNameByIdx(const NNSG3dResDict *dict, u32 idx) {
    if (dict != NULL && idx < dict->numEntry) {
        const NNSG3dResDictEntryHeader *header = (const NNSG3dResDictEntryHeader *)((const u8 *)dict + dict->ofsEntry);
        return (const NNSG3dResName *)((const u8 *)header + header->ofsName + idx * sizeof(NNSG3dResName));
    }
    return NULL;
}

static void SwalotPlopScene_Init(SwalotPlopScene *scene, NARC *narc, enum HeapID heapId, NNSFndAllocator *allocator) {
    int i;
    int j;
    int node;
    BOOL missing;
    NNSG3dResMdl *model;
    const NNSG3dResDict *dict;
    int id;
    u8 modelId;
    ModelAnimation *special;

    memset(scene, 0, sizeof(SwalotPlopScene));
    for (i = 0; i < 3; i++) {
        ModelResource_LoadFromNarc(&scene->resources[i], narc, gSwalotPlopSceneModelFiles[i], heapId);
        ModelInstance_Init(&scene->models[i], &scene->resources[i]);
        ModelInstance_SetVisible(&scene->models[i], FALSE);
        ModelInstance_SetPosition(&scene->models[i], 0, FX32_CONST(-290), 0);
        ModelInstance_SetScale(&scene->models[i], FX32_CONST(1.5), FX32_CONST(1.5), FX32_CONST(1.5));
    }
    for (i = 0; i < 6; i++) {
        ModelAnimation_LoadFromNarc(&scene->animations[i], &scene->resources[gSwalotPlopSceneAnimModels[i]], narc, gSwalotPlopSceneAnimFiles[i], heapId, allocator);
        ModelAnimation_SetFrame(&scene->animations[i], 0);
        node = 0;
        model = scene->resources[gSwalotPlopSceneAnimModels[i]].model;
        dict = model != NULL ? &model->nodeInfo.dict : NULL;
        missing = dict == NULL;
        while ((!missing ? GetResNameByIdx(dict, node) : NULL) != NULL) {
            NNS_G3dAnmObjDisableID(scene->animations[i].object, node);
            node++;
        }
        for (j = 0; j < 15; j++) {
            id = dict != NULL ? NNS_G3dGetResDictIdxByName(dict, &gSwalotPlopSceneNodeNames[i][j]) : -1;
            if (id != -1) {
                NNS_G3dAnmObjEnableID(scene->animations[i].object, id);
            }
        }
    }
    modelId = sSwalotPlopModelIds[0];
    special = &scene->special;
    ModelAnimation_LoadFromNarc(special, &scene->resources[modelId], narc, 32, heapId, allocator);
    ModelInstance_AddAnimation(&scene->models[modelId], special);
    scene->leader = 4;
    scene->bonus = 0;
    SwalotPlopScene_SetBaseAnimation(scene, 0);
    ModelInstance_AddAnimation(&scene->models[1], &scene->animations[4]);
    ModelAnimation_SetFrame(&scene->animations[4], 0);
    ModelInstance_SetVisible(&scene->models[1], TRUE);
    scene->current = 4;
}

static void SwalotPlopScene_Free(SwalotPlopScene *scene, NNSFndAllocator *allocator) {
    int i = 0;
    ModelResource *resource = scene->resources;
    for (; i < 3; i++) {
        ModelResource_Free(resource);
        resource++;
    }
    {
        ModelAnimation *animation;
        int j;
        j = 0;
        animation = scene->animations;
        for (; j < 6; j++) {
            ModelAnimation_Free(animation, allocator);
            animation++;
        }
    }
    ModelAnimation_Free(&scene->special, allocator);
    memset(scene, 0, sizeof(SwalotPlopScene));
}

static void SwalotPlopScene_Update(SwalotPlopScene *scene, SwalotPlopBody *body, u32 phase) {
    SwalotPlopScene_SetPos(scene, body);
    SwalotPlopScene_SetSpin(scene, body);
    SwalotPlopScene_SetScale(scene, body);
    switch (phase) {
    case SWALOT_PLOP_PHASE_NORMAL:
    case SWALOT_PLOP_PHASE_FASTEST:
        SwalotPlopScene_SetAnimation(scene, 1);
        SwalotPlopScene_SetTiltFrame(scene, body->tilt);
        SwalotPlopScene_SetBaseAnimation(scene, 1);
        break;
    case SWALOT_PLOP_PHASE_FAST:
        SwalotPlopScene_SetAnimation(scene, 3);
        SwalotPlopScene_SetBaseAnimation(scene, 0);
        SwalotPlopScene_AdvanceAnimation(scene, FX32_ONE);
        break;
    }
}

static void SwalotPlopScene_Animate(SwalotPlopScene *scene, SwalotPlopBody *body, u32 phase) {
    ModelAnimation_AdvanceLooped(&scene->animations[0], FX32_ONE);
    SwalotPlopScene_UpdateSpecial(scene);
}

static void SwalotPlopScene_SetPos(SwalotPlopScene *scene, SwalotPlopBody *body) {
    int i;
    ModelInstance *model = scene->models;
    for (i = 0; i < 3; i++, model++) {
        ModelInstance_SetPosition(model, body->position.x, body->position.y, body->position.z);
    }
}

static void SwalotPlopScene_SetSpin(SwalotPlopScene *scene, SwalotPlopBody *body) {
    ModelInstance *model;
    u32 spin;
    int i;
    model = scene->models;
    spin = body->spin;
    for (i = 0; i < 3; i++, model++) {
        ModelInstance_SetRotation(model, spin, 1);
    }
}

static void SwalotPlopScene_SetScale(SwalotPlopScene *scene, SwalotPlopBody *body) {
    int i;
    ModelInstance *model = scene->models;
    for (i = 0; i < 3; i++, model++) {
        fx32 scale = body->modelScale;
        ModelInstance_SetScale(model, scale, scale, scale);
    }
}

static void SwalotPlopScene_Draw(SwalotPlopScene *scene, SwalotPlopBody *body) {
    int i;
    ModelInstance *model = scene->models;
    for (i = 0; i < 3; i++, model++) {
        ModelInstance_Draw(model);
    }
}

static void SwalotPlopScene_SetTiltFrame(SwalotPlopScene *scene, u16 tilt) {
    u32 maxTilt = GF_DegreeToSinCosIdx(0xF222);
    u32 degrees = (90 * (tilt - 0xE38)) / maxTilt;
    s32 frame = ((s32)((45 << 14) * degrees)) / 90;
    frame = (45 << 14) - frame;
    ModelAnimation_SetFrame(&scene->animations[1], frame);
}

static void SwalotPlopScene_SetAnimation(SwalotPlopScene *scene, u32 animation) {
    u16 old;
    GF_ASSERT(animation < 6);
    old = scene->current;
    if (old != animation) {
        ModelInstance_RemoveAnimation(&scene->models[gSwalotPlopSceneAnimModels[old]], &scene->animations[old]);
        ModelInstance_AddAnimation(&scene->models[gSwalotPlopSceneAnimModels[animation]], &scene->animations[animation]);
        ModelAnimation_SetFrame(&scene->animations[animation], 0);
        ModelInstance_SetVisible(&scene->models[gSwalotPlopSceneAnimModels[scene->current]], FALSE);
        ModelInstance_SetVisible(&scene->models[gSwalotPlopSceneAnimModels[animation]], TRUE);
        scene->current = animation;
    }
}

static BOOL SwalotPlopScene_AdvanceAnimation(SwalotPlopScene *scene, fx32 step) {
    return ModelAnimation_Advance(&scene->animations[scene->current], step);
}

static void SwalotPlopScene_SetBaseAnimation(SwalotPlopScene *scene, int state) {
    u16 old = scene->state;
    if (old != state) {
        if (state == 1) {
            ModelInstance_AddAnimation(&scene->models[0], &scene->animations[0]);
        } else {
            ModelInstance_RemoveAnimation(&scene->models[0], &scene->animations[0]);
        }
        scene->state = state;
    }
}

static void SwalotPlopScene_SetBonus(SwalotPlopScene *scene, u16 bonus) {
    scene->bonus = bonus;
}

static void SwalotPlopScene_SetLeader(SwalotPlopScene *scene, u32 leader) {
    scene->leader = leader;
}

static void SwalotPlopScene_UpdateSpecial(SwalotPlopScene *scene) {
    if (scene->bonus == 1) {
        int frame = scene->frame + 0x2000;
        if (frame < 0x28000) {
            scene->frame = scene->frame + 0x2000;
        } else {
            scene->frame = FX32_ONE;
        }
    } else {
        int i = scene->leader == 4 ? 0 : scene->leader + 1;
        int start = gSwalotPlopSpecialAnimRanges[i].start << 12;
        int frame = scene->frame;
        if (frame < start) {
            scene->frame = start;
        } else {
            int next = frame + 0x2000;
            if (next < (gSwalotPlopSpecialAnimRanges[i].end << 12)) {
                scene->frame = next;
            } else {
                scene->frame = start;
            }
        }
    }
    ModelAnimation_SetFrame(&scene->special, scene->frame);
}

static void SwalotPlop_TouchToWorld(int x, int y, VecFx32 *pos) {
    pos->x = (255 - x) * 0x960 - (0x960 << 7);
    pos->z = -0x12C000 - (y * 0x960 - (0x960 << 7));
}

static void SwalotPlop_WorldToTouch(const VecFx32 *pos, SwalotPlopPoint *point) {
    point->x = 0xFF - (pos->x + FX32_CONST(75)) / 0x960;
    point->y = (FX32_CONST(-225) - pos->z) / 0x960;
}

static void SwalotPlop_RotateToSeat(const VecFx32 *src, VecFx32 *dst, int seat, int playerCount) {
    MtxFx33 matrix;
    u16 angle;

    MTX_Identity33_(&matrix);
    angle = gSwalotPlopSeatAngles[playerCount][seat];
    MTX_RotY33_(&matrix, FX_SinIdx(angle), FX_CosIdx(angle));
    MTX_MultVec33(src, &matrix, dst);
}

static void SwalotPlop_RotateFromSeat(const VecFx32 *src, VecFx32 *dst, int seat, int playerCount) {
    MtxFx33 matrix;
    u16 angle;

    MTX_Identity33_(&matrix);
    angle = gSwalotPlopSeatAngles[playerCount][seat];
    MTX_RotY33_(&matrix, FX_SinIdx(angle), FX_CosIdx(angle));
    MTX_Inverse33(&matrix, &matrix);
    MTX_MultVec33(src, &matrix, dst);
}

static void SwalotPlopGfx_InitShotViews(SwalotPlopGfx *gfx, NARC *narc, enum HeapID heapId) {
    int i;

    for (i = 0; i < 5; i++) {
        ModelResource_LoadFromNarc(&gfx->shotModels[i], narc, sSwalotPlopShotModelFiles[i], heapId);
    }
    ModelResource_LoadFromNarc(&gfx->shadowModel, narc, 0x2E, heapId);
    NNSi_G3dModifyPolygonAttrMask(gfx->shadowModel.model, FALSE, 0x1F << 16);

    gfx->shotRes[GF_GFX_RES_TYPE_CHAR] = AddCharResObjFromOpenNarc(gfx->resMan[GF_GFX_RES_TYPE_CHAR], narc, 7, FALSE, 100, NNS_G2D_VRAM_TYPE_2DSUB, heapId);
    gfx->shotRes[GF_GFX_RES_TYPE_PLTT] = AddPlttResObjFromOpenNarc(gfx->resMan[GF_GFX_RES_TYPE_PLTT], narc, 4, FALSE, 100, NNS_G2D_VRAM_TYPE_2DSUB, 4, heapId);
    gfx->shotRes[GF_GFX_RES_TYPE_CELL] = AddCellOrAnimResObjFromOpenNarc(gfx->resMan[GF_GFX_RES_TYPE_CELL], narc, 6, FALSE, 100, GF_GFX_RES_TYPE_CELL, heapId);
    gfx->shotRes[GF_GFX_RES_TYPE_ANIM] = AddCellOrAnimResObjFromOpenNarc(gfx->resMan[GF_GFX_RES_TYPE_ANIM], narc, 5, FALSE, 100, GF_GFX_RES_TYPE_ANIM, heapId);

    if (!SpriteTransfer_CreateCharTransferTask_UpdateMappingTypeFromHW_AllocAtEnd(gfx->shotRes[GF_GFX_RES_TYPE_CHAR])) {
        GF_ASSERT(FALSE);
    }
    if (!SpriteTransfer_CreatePlttTransferTask(gfx->shotRes[GF_GFX_RES_TYPE_PLTT])) {
        GF_ASSERT(FALSE);
    }
    GF2DGfxResObj_FreeData(gfx->shotRes[GF_GFX_RES_TYPE_CHAR]);
    GF2DGfxResObj_FreeData(gfx->shotRes[GF_GFX_RES_TYPE_PLTT]);

    CreateSpriteResourcesHeader(&gfx->shotHeader, 100, 100, 100, 100, -1, -1, FALSE, 0, gfx->resMan[GF_GFX_RES_TYPE_CHAR], gfx->resMan[GF_GFX_RES_TYPE_PLTT], gfx->resMan[GF_GFX_RES_TYPE_CELL], gfx->resMan[GF_GFX_RES_TYPE_ANIM], NULL, NULL);

    for (i = 0; i < 0x60; i++) {
        SwalotPlopShotView_Init(gfx, &gfx->shotViews[i], heapId);
    }
}

static void SwalotPlopGfx_FreeShotViews(SwalotPlopGfx *gfx) {
    ModelResource *model;
    int i;

    for (i = 0; i < 0x60; i++) {
        SwalotPlopShotView_Free(gfx, &gfx->shotViews[i]);
    }

    SpriteTransfer_DeleteCharTransferTask(gfx->shotRes[GF_GFX_RES_TYPE_CHAR]);
    SpriteTransfer_DeletePlttTransferTask(gfx->shotRes[GF_GFX_RES_TYPE_PLTT]);
    DestroySingle2DGfxResObj(gfx->resMan[GF_GFX_RES_TYPE_CHAR], gfx->shotRes[GF_GFX_RES_TYPE_CHAR]);
    DestroySingle2DGfxResObj(gfx->resMan[GF_GFX_RES_TYPE_PLTT], gfx->shotRes[GF_GFX_RES_TYPE_PLTT]);
    DestroySingle2DGfxResObj(gfx->resMan[GF_GFX_RES_TYPE_CELL], gfx->shotRes[GF_GFX_RES_TYPE_CELL]);
    DestroySingle2DGfxResObj(gfx->resMan[GF_GFX_RES_TYPE_ANIM], gfx->shotRes[GF_GFX_RES_TYPE_ANIM]);

    model = gfx->shotModels;
    for (i = 0; i < 5; i++, model++) {
        ModelResource_Free(model);
    }
    ModelResource_Free(&gfx->shadowModel);
    memset(gfx->shotModels, 0, sizeof(gfx->shotModels) + sizeof(gfx->shadowModel) + sizeof(gfx->shotRes) + sizeof(gfx->shotHeader));
    memset(gfx->shotViews, 0, sizeof(gfx->shotViews));
}

static void SwalotPlopGfx_AddShotView(SwalotPlopGfx *gfx, SwalotPlopShot *shot, u32 playerCount, u32 localIndex) {
    SwalotPlopShotView *view = SwalotPlopGfx_GetFreeShotView(gfx);
    if (shot->params.owner == localIndex) {
        SwalotPlopShotView_StartSprite(gfx, view, shot, playerCount);
    } else {
        SwalotPlopShotView_StartModel(gfx, view, shot);
    }
}

static void SwalotPlopGfx_RemoveShotView(SwalotPlopGfx *gfx, SwalotPlopShot *shot) {
    int i;

    for (i = 0; i < 0x60; i++) {
        if (gfx->shotViews[i].shot == shot) {
            SwalotPlopShotView_Clear(&gfx->shotViews[i]);
            return;
        }
    }
    GF_ASSERT(FALSE);
}

static void SwalotPlopGfx_DrawShotViews(SwalotPlopGfx *gfx, u32 playerCount) {
    int i;

    for (i = 0; i < 0x60; i++) {
        if (gfx->shotViews[i].shot != NULL) {
            SwalotPlopShotView_Draw(&gfx->shotViews[i], gfx, playerCount);
        }
    }
}

static void SwalotPlopShotView_Init(SwalotPlopGfx *gfx, SwalotPlopShotView *view, enum HeapID heapId) {
    SpriteTemplate template = { 0 };
    Sprite *sprite;

    template.spriteList = gfx->spriteList;
    template.header = &gfx->shotHeader;
    template.scale.x = FX32_ONE;
    template.scale.y = FX32_ONE;
    template.scale.z = FX32_ONE;
    template.drawPriority = 0x20;
    template.whichScreen = NNS_G2D_VRAM_TYPE_2DSUB;
    template.heapID = heapId;
    sprite = Sprite_CreateAffine(&template);
    view->sprite = sprite;
    Sprite_SetDrawFlag(sprite, FALSE);
    ModelInstance_Init(&view->shadow, &gfx->shadowModel);
    ModelInstance_SetVisible(&view->shadow, FALSE);
}

static void SwalotPlopShotView_Free(SwalotPlopGfx *gfx, SwalotPlopShotView *view) {
    Sprite_Delete(view->sprite);
    memset(view, 0, sizeof(SwalotPlopShotView));
}

static void SwalotPlopShotView_StartSprite(SwalotPlopGfx *gfx, SwalotPlopShotView *view, SwalotPlopShot *shot, u32 playerCount) {
    view->shot = shot;
    view->state = 1;
    view->timer = 0;
    Sprite_SetPaletteOverride(view->sprite, shot->params.owner);
    SwalotPlopShotView_UpdateSprite(view, shot, playerCount);
    Sprite_SetDrawFlag(view->sprite, TRUE);
}

static void SwalotPlopShotView_StartModel(SwalotPlopGfx *gfx, SwalotPlopShotView *view, SwalotPlopShot *shot) {
    view->shot = shot;
    view->timer = 0;
    Sprite_SetDrawFlag(view->sprite, FALSE);
    SwalotPlopShotView_SwitchToModel(view, gfx);
}

static void SwalotPlopShotView_Draw(SwalotPlopShotView *view, SwalotPlopGfx *gfx, u32 playerCount) {
    if (view->state == 1 && !SwalotPlopShotView_UpdateSprite(view, view->shot, playerCount)) {
        SwalotPlopShotView_SwitchToModel(view, gfx);
    }
    if (view->state == 0) {
        ModelResource *model = SwalotPlop_GetShotModel(view->shot, gfx->shotModels);
        if (view->shot->state == SHOT_STATE_BOUNCED) {
            NNS_G3dMdlSetMdlAmbAll(model->model, GX_RGB(31, 31, 31));
        } else {
            NNS_G3dMdlSetMdlAmbAll(model->model, GX_RGB(18, 18, 18));
        }
        SwalotPlopShotView_Spin(view);
        ModelInstance_SetPosition(&view->model, view->shot->position.x, view->shot->position.y, view->shot->position.z);
        ModelInstance_Draw(&view->model);
        if (ModelInstance_IsVisible(&view->shadow) == TRUE) {
            SwalotPlopShotView_PlaceShadow(view);
            ModelInstance_Draw(&view->shadow);
            SwalotPlopShotView_ResetPolygonAttr(view);
        }
    }
}

static void SwalotPlopShotView_PlaceShadow(SwalotPlopShotView *view) {
    fx32 groundY, height, scale, value;
    fx32 factor = FX32_ONE;
    if (SwalotPlopShot_IsInPool(view->shot) == TRUE) {
        groundY = FX32_CONST(-300);
    } else if (!SwalotPlopShot_IsOnRim(view->shot)) {
        groundY = FX32_CONST(-51);
    } else {
        groundY = FX32_CONST(-300);
    }
    ModelInstance_SetPosition(&view->shadow, view->shot->position.x, groundY, view->shot->position.z);
    height = view->shot->position.y - groundY;
    value = (fx32)(((s64)height * factor + factor / 2) >> 12);
    scale = FX_Div(value, FX32_CONST(300)) + FX32_ONE;
    ModelInstance_SetScale(&view->shadow, scale, scale, scale);
    value = (fx32)(((s64)height * 65536 + 2048) >> 12);
    value = FX_Div(value, FX32_CONST(300));
    NNS_G3dGlbPolygonAttr(0, GX_POLYGONMODE_MODULATE, GX_CULL_ALL, 0, 24 - (value >> FX32_SHIFT), 0);
}

static void SwalotPlopShotView_ResetPolygonAttr(SwalotPlopShotView *view) {
    NNS_G3dGlbPolygonAttr(0, GX_POLYGONMODE_MODULATE, GX_CULL_ALL, 0, 31, 0);
}

static void SwalotPlopShotView_Clear(SwalotPlopShotView *view) {
    view->shot = NULL;
    view->state = 0;
    Sprite_SetDrawFlag(view->sprite, FALSE);
    ModelInstance_SetVisible(&view->shadow, FALSE);
}

static SwalotPlopShotView *SwalotPlopGfx_GetFreeShotView(SwalotPlopGfx *gfx) {
    int i;
    for (i = 0; i < 0x60; i++) {
        if (gfx->shotViews[i].shot == NULL) {
            return &gfx->shotViews[i];
        }
    }
    GF_ASSERT(FALSE);
    return NULL;
}

static BOOL SwalotPlopShotView_UpdateSprite(SwalotPlopShotView *view, SwalotPlopShot *shot, u32 playerCount) {
    SwalotPlopPoint point;
    VecFx32 pos = view->shot->position;
    SwalotPlop_RotateFromSeat(&pos, &pos, view->shot->params.owner, playerCount);
    SwalotPlop_WorldToTouch(&pos, &point);
    pos.x = point.x << 12;
    pos.y = (point.y << 12) + (512 << FX32_SHIFT);
    pos.z = 0;
    Sprite_SetMatrix(view->sprite, &pos);
    if (point.x < 0 || point.y < 0) {
        return FALSE;
    }
    return TRUE;
}

static void SwalotPlopShotView_SwitchToModel(SwalotPlopShotView *view, SwalotPlopGfx *gfx) {
    Sprite_SetDrawFlag(view->sprite, FALSE);
    view->state = 0;
    ModelInstance_Init(&view->model, SwalotPlop_GetShotModel(view->shot, gfx->shotModels));
    ModelInstance_SetVisible(&view->model, TRUE);
    if (view->shot->state == SHOT_STATE_FLYING) {
        ModelInstance_SetVisible(&view->shadow, TRUE);
    }
}

static ModelResource *SwalotPlop_GetShotModel(SwalotPlopShot *shot, ModelResource *models) {
    u32 index;
    if (shot->params.big == TRUE) {
        index = 4;
    } else {
        index = shot->params.owner;
    }
    return &models[index];
}

static inline s64 SinCosComponent(s32 value, s32 scale) {
    return (s64)value * scale;
}

static void SwalotPlopShotView_Spin(SwalotPlopShotView *view) {
    view->timer = view->timer - 1;
    if (view->timer < 0) {
        int speed = SwalotPlopShot_GetSpeed(view->shot);
        int angle = (FX_Atan2Idx(view->shot->params.direction.z, view->shot->params.direction.x) >> 4) * 2;
        s32 value = (u16)((s32)((u32)speed << 9) / 0x1E200) << 12;
        view->rotSpeedX = (((fx32)((SinCosComponent(FX_SinCosTable_[angle], value) + 2048) >> 12)) >> 12) + 512;
        view->rotSpeedZ = (((fx32)((SinCosComponent(FX_SinCosTable_[angle + 1], value) + 2048) >> 12)) >> 12) + 512;
        view->timer = 8;
    }
    view->rotX += view->rotSpeedX;
    view->rotZ += view->rotSpeedZ;
    ModelInstance_SetRotation(&view->model, view->rotX, 0);
    ModelInstance_SetRotation(&view->model, view->rotZ, 2);
}

static void SwalotPlopCursor_Init(SwalotPlopGfx *gfx, SwalotPlopCursor *cursor, int palette, enum HeapID heapId) {
    SpriteTemplate template = { 0 };
    template.spriteList = gfx->spriteList;
    template.header = &gfx->shotHeader;
    template.heapID = heapId;
    template.scale.x = FX32_ONE;
    template.scale.y = FX32_ONE;
    template.scale.z = FX32_ONE;
    template.drawPriority = 32;
    template.whichScreen = NNS_G2D_VRAM_TYPE_2DSUB;
    cursor->sprite = Sprite_CreateAffine(&template);
    Sprite_SetDrawFlag(cursor->sprite, FALSE);
    Sprite_SetPaletteOverride(cursor->sprite, palette);
    cursor->active = FALSE;
}

static void SwalotPlopCursor_Free(SwalotPlopCursor *cursor) {
    Sprite_Delete(cursor->sprite);
}

static void SwalotPlopCursor_Show(SwalotPlopCursor *cursor) {
    Sprite_SetDrawFlag(cursor->sprite, TRUE);
    cursor->active = TRUE;
}

static void SwalotPlopCursor_Update(SwalotPlopCursor *cursor) {
    if (cursor->active) {
        SwalotPlopCursor_SetPos(cursor, gSystem.touchX, gSystem.touchY);
    }
}

static void SwalotPlopCursor_SetPos(SwalotPlopCursor *cursor, int x, int y) {
    VecFx32 pos;
    pos.x = x << 12;
    pos.y = (y << 12) + (512 << FX32_SHIFT);
    Sprite_SetMatrix(cursor->sprite, &pos);
}

static void SwalotPlopCursor_Hide(SwalotPlopCursor *cursor) {
    Sprite_SetDrawFlag(cursor->sprite, FALSE);
    cursor->active = FALSE;
}

static void SwalotPlopParticles_Init(SwalotPlopGfx *gfx, SwalotPlopParticle *particle, int palette, enum HeapID heapId) {
    SpriteTemplate template = { 0 };
    int i;
    template.spriteList = gfx->spriteList;
    template.header = &gfx->shotHeader;
    template.scale.x = 9;
    template.scale.y = 9;
    template.scale.z = 9;
    template.drawPriority = 32;
    template.whichScreen = NNS_G2D_VRAM_TYPE_2DSUB;
    template.heapID = heapId;
    for (i = 0; i < 8; i++) {
        particle->sprite = Sprite_CreateAffine(&template);
        Sprite_SetDrawFlag(particle->sprite, FALSE);
        Sprite_SetAffineOverwriteMode(particle->sprite, NNS_G2D_RND_AFFINE_OVERWRITE_NORMAL);
        Sprite_SetPaletteOverride(particle->sprite, palette);
        particle->active = FALSE;
        particle++;
    }
}

static void SwalotPlopParticles_Free(SwalotPlopGfx *gfx, SwalotPlopParticle *particle) {
    int i;
    for (i = 0; i < 8; i++, particle++) {
        Sprite_Delete(particle->sprite);
    }
}

static void SwalotPlopParticles_Update(SwalotPlopParticle *particles) {
    int i;
    s32 dx;
    s32 dy;
    VecFx32 scale;

    for (i = 0; i < 8; i++) {
        SwalotPlopParticle *particle = &particles[i];

        if (particle->active == TRUE) {
            if (particle->age < 16) {
                SwalotPlop_CalcArcStep(particle->age, 16, 8, 190, 125, &dx, &dy);
            } else if (particle->age < 26) {
                SwalotPlop_CalcArcStep(particle->age - 16, 10, 4, 182, 140, &dx, &dy);
            } else {
                SwalotPlop_CalcArcStep(particle->age - 26, 6, 3, 180, 160, &dx, &dy);
            }

            particle->age++;
            scale.x = FX_Div((9 - particle->age * 3 / 32) * FX32_ONE, 10 * FX32_ONE);
            scale.y = scale.x;
            Sprite_SetAffineScale(particle->sprite, &scale);

            if (particle->direction) {
                particle->position.x += dx << FX32_SHIFT;
            } else {
                particle->position.x -= dx << FX32_SHIFT;
            }
            particle->position.y += dy << FX32_SHIFT;
            Sprite_SetMatrix(particle->sprite, &particle->position);

            if (particle->position.x < FX32_CONST(-32) || particle->position.x > FX32_CONST(288) || particle->position.y < FX32_CONST(480) || particle->position.y > FX32_CONST(736)) {
                particle->active = FALSE;
                Sprite_SetDrawFlag(particle->sprite, FALSE);
            }
        }
    }
}

static void SwalotPlopParticles_Spawn(SwalotPlopParticle *particles, s32 x, s32 y) {
    int i;
    SwalotPlopParticle *particle = NULL;

    for (i = 0; i < 8; i++) {
        if (!particles[i].active) {
            particle = &particles[i];
            break;
        }
    }

    if (particle != NULL) {
        particle->direction = x < 0x80;
        particle->position.x = x << FX32_SHIFT;
        particle->position.y = (y << FX32_SHIFT) + (512 << FX32_SHIFT);
        Sprite_SetMatrix(particle->sprite, &particle->position);
        particle->active = TRUE;
        Sprite_SetDrawFlag(particle->sprite, TRUE);
        particle->age = 0;
    }
}

static inline int FxMul(int a, int b) {
    return FX_Mul(a, b);
}

static void SwalotPlop_CalcArcStep(int step, int steps, int radius, int startAngle, int endAngle, int *x, int *y) {
    u16 angle = (s16)(startAngle + ((endAngle - startAngle) * step) / steps);
    int cosine = GF_CosDegNoWrap(angle);
    int radiusFx = radius << 12;
    *x = FxMul(cosine, radiusFx) >> 12;
    *y = FxMul(GF_SinDegNoWrap(angle), radiusFx) >> 12;
}

static void SwalotPlopStage_Init(SwalotPlopStage *stage, SwalotPlopGfx *gfx, NARC *narc, int playerCount, int localIndex, enum HeapID heapId, NNSFndAllocator *allocator) {
    int i, fileId;
    ModelInstance *model;
    ModelResource *resource;
    resource = stage->resources;
    model = stage->models;
    for (i = 0; i < 3; i++, resource++, model++) {
        if (i == 0) {
            fileId = 36;
        } else if (i == 1) {
            fileId = 37;
        } else {
            fileId = gSwalotPlopStageFiles[playerCount - 1];
        }
        ModelResource_LoadFromNarc(resource, narc, fileId, heapId);
        ModelInstance_Init(model, resource);
        ModelInstance_SetPosition(model, 0, FX32_CONST(-98), 0);
        ModelInstance_SetScale(model, FX32_CONST(1.5), FX32_CONST(1.5), FX32_CONST(1.5));
        if (i == 1) {
            ModelInstance_SetVisible(model, FALSE);
        }
        if ((u32)i <= 1) {
            ModelInstance_SetRotation(model, gSwalotPlopStageRotations[playerCount - 1][localIndex], 1);
        } else {
            ModelInstance_SetRotation(model, GF_DegreeToSinCosIdxNoWrap(180), 1);
        }
    }
    for (i = 0; i < 3; i++) {
        ModelAnimation_LoadFromNarc(&stage->animations[i], &stage->resources[gSwalotPlopStageAnimModels[i]], narc, gSwalotPlopStageAnimFiles[i], heapId, allocator);
        ModelInstance_AddAnimation(&stage->models[gSwalotPlopStageAnimModels[i]], &stage->animations[i]);
    }
    stage->animSpeed = FX32_ONE;
}

static void SwalotPlopStage_Free(SwalotPlopStage *stage, SwalotPlopGfx *gfx, NNSFndAllocator *allocator) {
    int i = 0;
    ModelAnimation *animation = stage->animations;
    for (; i < 3; i++, animation++) {
        ModelAnimation_Free(animation, allocator);
    }
    i = 0;
    ModelResource *resource = stage->resources;
    for (; i < 3; i++, resource++) {
        ModelResource_Free(resource);
    }
}

static void SwalotPlopStage_Draw(SwalotPlopStage *stage) {
    int i;
    ModelAnimation_AdvanceLooped(&stage->animations[0], stage->animSpeed);
    if (stage->mouthActive) {
        switch (stage->mouthState) {
        case SWALOT_PLOP_MOUTH_SPEED_UP: {
            s32 step = stage->mouthStep;
            if (stage->mouthFrame + step < 0x33000) {
                stage->mouthFrame += step;
            } else {
                stage->mouthFrame = 0;
            }
            if (stage->mouthStep + FX32_CONST(0.1) <= FX32_CONST(2.3)) {
                stage->mouthStep += FX32_CONST(0.1);
            } else {
                stage->mouthStep = FX32_CONST(2.3);
                stage->mouthTimer = 24;
                stage->mouthState++;
            }
            break;
        }
        case SWALOT_PLOP_MOUTH_HOLD: {
            s32 step = stage->mouthStep;
            if (stage->mouthFrame + step < 0x33000) {
                stage->mouthFrame += step;
            } else {
                stage->mouthFrame = 0;
            }
            stage->mouthTimer--;
            if (stage->mouthTimer <= 0) {
                stage->mouthState++;
                stage->mouthFrame = FX32_CONST(51);
            }
            break;
        }
        case SWALOT_PLOP_MOUTH_LOOP:
            if (stage->mouthFrame + FX32_ONE < FX32_CONST(100)) {
                stage->mouthFrame += FX32_ONE;
            } else {
                stage->mouthFrame = FX32_CONST(51);
            }
            break;
        default:
            GF_ASSERT(FALSE);
            break;
        }
        ModelAnimation_SetFrame(&stage->animations[1], stage->mouthFrame);
        ModelAnimation_SetFrame(&stage->animations[2], stage->mouthFrame);
    }
    ModelInstance *model = stage->models;
    for (i = 0; i < 3; i++, model++) {
        ModelInstance_Draw(model);
    }
}

static void SwalotPlopStage_StartMouth(SwalotPlopStage *stage) {
    ModelInstance_SetVisible(&stage->models[0], FALSE);
    ModelInstance_SetVisible(&stage->models[1], TRUE);
    stage->mouthFrame = 0;
    stage->mouthStep = FX32_ONE;
    stage->mouthActive = TRUE;
    stage->mouthState = SWALOT_PLOP_MOUTH_SPEED_UP;
}

static void SwalotPlopStage_SetAnimSpeed(SwalotPlopStage *stage, fx32 speed) {
    stage->animSpeed = speed;
}

static void SwalotPlop_IntersectPlane(const VecFx32 *pos, const VecFx32 *dir, const VecFx32 *normal, fx32 planeDist, VecFx32 *hit, fx32 *t) {
    fx32 scale;
    scale = FX_Div(planeDist + (-FX_Mul(normal->x, pos->x) - FX_Mul(normal->y, pos->y) - FX_Mul(normal->z, pos->z)),
        FX_Mul(normal->z, dir->z) + (FX_Mul(normal->x, dir->x) + FX_Mul(normal->y, dir->y)));
    hit->x = pos->x + FX_Mul(dir->x, scale);
    hit->y = pos->y + FX_Mul(dir->y, scale);
    // the z result is written to x as well
    hit->x = pos->z + FX_Mul(dir->z, scale);
    *t = scale;
}

static void SwalotPlopGfx_SetCameraHeight(SwalotPlopGfx *gfx, fx32 height) {
    gfx->cameraTarget.y = height;
}

static void SwalotPlopGfx_SetCameraPitch(SwalotPlopGfx *gfx, u16 pitch) {
    CameraAngle angle;
    angle = Camera_GetAngle(gfx->camera);
    angle.x = pitch;
    Camera_SetAnglePos(&angle, gfx->camera);
}

static void SwalotPlopBgFade_Init(SwalotPlopBgFade *fade, u8 palette) {
    u8 *bytes = (u8 *)fade;
    bytes[0] = 0;
    bytes[1] = 0;
    bytes[2] = 0;
    bytes[3] = 0;
    bytes[4] = 0;
    bytes[5] = 0;
    bytes[6] = 0;
    bytes[7] = 0;
    fade->layer = 2;
    fade->palette = palette;
}

static void SwalotPlopBgFade_Update(SwalotPlopBgFade *fade, SwalotPlopGfx *gfx) {
    int layer, older, prev, alpha;
    switch (fade->state) {
    case SWALOT_PLOP_BG_FADE_START:
        SwalotPlopBgFade_SetLayers(gfx, (s16)(fade->layer - 1), fade->palette);
        fade->state++;
        fade->timer = 0;
    case SWALOT_PLOP_BG_FADE_CROSS:
        if (fade->timer >= 16) {
            fade->state = SWALOT_PLOP_BG_FADE_IN;
            fade->timer = 0;
            SwalotPlopBgFade_SetLayers(gfx, fade->layer, fade->palette);
#if defined(SDK_PORT)
            G2S_SetBlendAlpha(1 << fade->layer, GX_BLEND_PLANEMASK_BG3, 0, 16);
#else
            G2x_SetBlendAlpha_((u32)&reg_G2S_DB_BLDCNT, 1 << fade->layer, GX_BLEND_PLANEMASK_BG3, 0, 16);
#endif
            return;
        }
        fade->timer++;
        layer = fade->layer;
        older = layer - 2;
        prev = layer - 1;
        if (layer < 0) {
            layer += 3;
        }
        if (older < 0) {
            older += 3;
        }
        if (prev < 0) {
            prev += 3;
        }
        alpha = 16 - (fade->timer * 16) / 16;
#if defined(SDK_PORT)
        G2S_SetBlendAlpha(1 << layer, GX_BLEND_PLANEMASK_BG3 | ((1 << older) | (1 << prev)), alpha, 16 - alpha);
#else
        G2x_SetBlendAlpha_((u32)&reg_G2S_DB_BLDCNT, 1 << layer, GX_BLEND_PLANEMASK_BG3 | ((1 << older) | (1 << prev)), alpha, 16 - alpha);
#endif
        return;
    case SWALOT_PLOP_BG_FADE_IN:
        if (fade->timer >= 8) {
            fade->state = SWALOT_PLOP_BG_FADE_IDLE;
            G2S_BlendNone();
            return;
        }
        fade->timer++;
        alpha = (fade->timer * 16) / 8;
#if defined(SDK_PORT)
        G2S_SetBlendAlpha(1 << fade->layer, GX_BLEND_PLANEMASK_BG3, alpha, 16 - alpha);
#else
        G2x_SetBlendAlpha_((u32)&reg_G2S_DB_BLDCNT, 1 << fade->layer, GX_BLEND_PLANEMASK_BG3, alpha, 16 - alpha);
#endif
        break;
    case SWALOT_PLOP_BG_FADE_IDLE:
    default:
        break;
    }
}

static void SwalotPlopBgFade_Step(SwalotPlopBgFade *fade) {
    fade->counter++;
    if (fade->counter >= 10) {
        fade->state = SWALOT_PLOP_BG_FADE_START;
        fade->layer = (fade->layer + 1) % 3;
        fade->counter = 0;
    }
}

static void SwalotPlopBgFade_SetLayers(SwalotPlopGfx *gfx, int top, int palette) {
    int i;
    int layer;
    for (i = 0; i < 3; i++) {
        layer = top - i;
        if (layer < 0) {
            layer += 3;
        }
        SetBgPriority(layer + GF_BG_LYR_SUB_0, (u8)(2 - i));
        if (i == 0) {
            BgTilemapRectChangePalette(gfx->bgConfig, layer + GF_BG_LYR_SUB_0, 0, 0, 32, 32, palette * 2 + 2);
        } else {
            BgTilemapRectChangePalette(gfx->bgConfig, layer + GF_BG_LYR_SUB_0, 0, 0, 32, 32, palette * 2 + 1);
        }
        BgCommitTilemapBufferToVram(gfx->bgConfig, layer + GF_BG_LYR_SUB_0);
    }
}

static void SwalotPlopCombo_Init(SwalotPlopCombo *combo, SwalotPlopGfx *gfx, NARC *narc, enum HeapID heapId) {
    SimpleSpriteTemplate spriteTemplate;
    TextOBJTemplate textTemplate;
    combo->heapId = heapId;
    combo->res[GF_GFX_RES_TYPE_CHAR] = AddCharResObjFromOpenNarc(gfx->resMan[GF_GFX_RES_TYPE_CHAR], narc, 10, FALSE, 140, NNS_G2D_VRAM_TYPE_2DMAIN, heapId);
    combo->res[GF_GFX_RES_TYPE_PLTT] = AddPlttResObjFromOpenNarc(gfx->resMan[GF_GFX_RES_TYPE_PLTT], narc, 11, FALSE, 140, NNS_G2D_VRAM_TYPE_2DMAIN, 1, heapId);
    combo->res[GF_GFX_RES_TYPE_CELL] = AddCellOrAnimResObjFromOpenNarc(gfx->resMan[GF_GFX_RES_TYPE_CELL], narc, 9, FALSE, 140, GF_GFX_RES_TYPE_CELL, heapId);
    combo->res[GF_GFX_RES_TYPE_ANIM] = AddCellOrAnimResObjFromOpenNarc(gfx->resMan[GF_GFX_RES_TYPE_ANIM], narc, 8, FALSE, 140, GF_GFX_RES_TYPE_ANIM, heapId);
    GF_ASSERT(SpriteTransfer_CreateCharTransferTask_UpdateMappingTypeFromHW_AllocAtEnd(combo->res[GF_GFX_RES_TYPE_CHAR]));
    GF_ASSERT(SpriteTransfer_CreatePlttTransferTask(combo->res[GF_GFX_RES_TYPE_PLTT]));
    GF2DGfxResObj_FreeData(combo->res[GF_GFX_RES_TYPE_CHAR]);
    GF2DGfxResObj_FreeData(combo->res[GF_GFX_RES_TYPE_PLTT]);
    CreateSpriteResourcesHeader(&combo->header, 140, 140, 140, 140, -1, -1, FALSE, 0, gfx->resMan[GF_GFX_RES_TYPE_CHAR], gfx->resMan[GF_GFX_RES_TYPE_PLTT], gfx->resMan[GF_GFX_RES_TYPE_CELL], gfx->resMan[GF_GFX_RES_TYPE_ANIM], NULL, NULL);
    spriteTemplate.spriteList = gfx->spriteList;
    spriteTemplate.header = &combo->header;
    spriteTemplate.position.x = FX32_CONST(-64);
    spriteTemplate.position.y = FX32_CONST(48);
    spriteTemplate.priority = 1;
    spriteTemplate.whichScreen = NNS_G2D_VRAM_TYPE_2DMAIN;
    spriteTemplate.heapID = heapId;
    combo->sprite = Sprite_Create(&spriteTemplate);
    Sprite_SetDrawFlag(combo->sprite, FALSE);
    combo->string = String_New(0x10, heapId);
    AddTextWindowTopLeftCorner(gfx->bgConfig, &combo->window, 3, 2, 0, 0);
    combo->textLayout = TextOBJLayout_New(&combo->window, heapId);
    GF_ASSERT(ObjCharTransfer_ReserveCharSpace(TextOBJLayout_GetCharSize(combo->textLayout, NNS_G2D_VRAM_TYPE_2DMAIN), TRUE, NNS_G2D_VRAM_TYPE_2DMAIN, &combo->charTransfer) == TRUE);
    combo->textPltt = AddPlttResObjFromNarc(gfx->resMan[GF_GFX_RES_TYPE_PLTT], NARC_graphic_font, 7, FALSE, 150, NNS_G2D_VRAM_TYPE_2DMAIN, 1, heapId);
    GF_ASSERT(SpriteTransfer_CreatePlttTransferTask(combo->textPltt));
    GF2DGfxResObj_FreeData(combo->textPltt);
    textTemplate.fontSystem = gfx->fontSystem;
    textTemplate.window = &combo->window;
    textTemplate.spriteList = gfx->spriteList;
    textTemplate.plttResourceProxy = SpriteTransfer_GetPaletteProxy(combo->textPltt, NULL);
    textTemplate.sprite = combo->sprite;
    textTemplate.offset = combo->charTransfer.offset;
    textTemplate.x = -7;
    textTemplate.y = -5;
    textTemplate.bgPriority = 0;
    textTemplate.drawPriority = 0;
    textTemplate.vram = NNS_G2D_VRAM_TYPE_2DMAIN;
    textTemplate.heapID = heapId;
    combo->textObj = TextOBJ_Create(&textTemplate, combo->textLayout);
    TextOBJ_SetSpritesDrawFlag(combo->textObj, FALSE);
    RemoveWindow(&combo->window);
    SwalotPlopCurve_Init(&combo->slideX, -0x40000, 0x20000, 0x24000, 8);
    SwalotPlopCurve_Init(&combo->slideY, 0x30000, 0x30000, 0xC000, 4);
    combo->slideFrame = 0;
    combo->bounceFrame = 4;
}

static void SwalotPlopCombo_Free(SwalotPlopCombo *combo, SwalotPlopGfx *gfx) {
    TextOBJ_Destroy(combo->textObj);
    SpriteTransfer_DeletePlttTransferTask(combo->textPltt);
    DestroySingle2DGfxResObj(gfx->resMan[GF_GFX_RES_TYPE_PLTT], combo->textPltt);
    ObjCharTransfer_FreeCharSpace(&combo->charTransfer);
    TextOBJLayout_Delete(combo->textLayout);
    String_Delete(combo->string);
    Sprite_Delete(combo->sprite);
    SpriteTransfer_DeleteCharTransferTask(combo->res[GF_GFX_RES_TYPE_CHAR]);
    SpriteTransfer_DeletePlttTransferTask(combo->res[GF_GFX_RES_TYPE_PLTT]);
    DestroySingle2DGfxResObj(gfx->resMan[GF_GFX_RES_TYPE_CHAR], combo->res[GF_GFX_RES_TYPE_CHAR]);
    DestroySingle2DGfxResObj(gfx->resMan[GF_GFX_RES_TYPE_PLTT], combo->res[GF_GFX_RES_TYPE_PLTT]);
    DestroySingle2DGfxResObj(gfx->resMan[GF_GFX_RES_TYPE_CELL], combo->res[GF_GFX_RES_TYPE_CELL]);
    DestroySingle2DGfxResObj(gfx->resMan[GF_GFX_RES_TYPE_ANIM], combo->res[GF_GFX_RES_TYPE_ANIM]);
}

static void SwalotPlopCombo_Show(SwalotPlopCombo *combo, SwalotPlopGfx *gfx, int count) {
    String16_FormatInteger(combo->string, count, 2, PRINTING_MODE_LEADING_ZEROS, TRUE);
    AddTextWindowTopLeftCorner(gfx->bgConfig, &combo->window, 3, 2, 0, 0);
    AddTextPrinterParameterizedWithColor(&combo->window, 0, combo->string, 0, 0, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(15, 14, 0), NULL);
    TextOBJ_CopyFromBGWindow(combo->textObj, combo->textLayout, &combo->window, combo->heapId);
    RemoveWindow(&combo->window);
    TextOBJ_SetSpritesDrawFlag(combo->textObj, TRUE);
    Sprite_SetDrawFlag(combo->sprite, TRUE);
    combo->state = SWALOT_PLOP_COMBO_SLIDE_IN;
    combo->hideDelay = 0;
}

static void SwalotPlopCombo_Hide(SwalotPlopCombo *combo) {
    u16 state = combo->state;
    if (state != SWALOT_PLOP_COMBO_SLIDE_OUT) {
        if (state == SWALOT_PLOP_COMBO_SHOWN) {
            combo->hideDelay = 12;
        }
        combo->state = SWALOT_PLOP_COMBO_SLIDE_OUT;
    }
}

static void SwalotPlopCombo_SetCount(SwalotPlopCombo *combo, SwalotPlopGfx *gfx, int count) {
    if (combo->state != SWALOT_PLOP_COMBO_SLIDE_OUT) {
        String16_FormatInteger(combo->string, count, 2, PRINTING_MODE_LEADING_ZEROS, TRUE);
        AddTextWindowTopLeftCorner(gfx->bgConfig, &combo->window, 3, 2, 0, 0);
        AddTextPrinterParameterizedWithColor(&combo->window, 0, combo->string, 0, 0, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(15, 14, 0), NULL);
        TextOBJ_CopyFromBGWindow(combo->textObj, combo->textLayout, &combo->window, combo->heapId);
        RemoveWindow(&combo->window);
        combo->bounceFrame = 0;
    }
}

static void SwalotPlopCombo_Update(SwalotPlopCombo *combo) {
    switch (combo->state) {
    case SWALOT_PLOP_COMBO_HIDDEN:
        break;
    case SWALOT_PLOP_COMBO_SLIDE_IN:
        if (combo->slideFrame < 8) {
            combo->slideFrame++;
        } else {
            combo->state = SWALOT_PLOP_COMBO_SHOWN;
        }
        break;
    case SWALOT_PLOP_COMBO_SHOWN:
        break;
    case SWALOT_PLOP_COMBO_SLIDE_OUT:
        if (combo->hideDelay > 0) {
            combo->hideDelay--;
        } else if (combo->slideFrame > 0) {
            combo->slideFrame--;
        } else {
            combo->state = SWALOT_PLOP_COMBO_HIDDEN;
            TextOBJ_SetSpritesDrawFlag(combo->textObj, FALSE);
            Sprite_SetDrawFlag(combo->sprite, FALSE);
        }
        break;
    }

    if (combo->bounceFrame < 4) {
        combo->bounceFrame++;
        combo->slideY.step = combo->bounceFrame;
        SwalotPlopCurve_Update(&combo->slideY);
    }
    combo->slideX.step = combo->slideFrame;
    SwalotPlopCurve_Update(&combo->slideX);
    {
        VecFx32 pos;
        pos.x = combo->slideX.value;
        pos.y = combo->slideY.value;
        Sprite_SetMatrix(combo->sprite, &pos);
    }
    TextOBJ_UpdatePosition(combo->textObj);
}

static void SwalotPlopSpeedUp_Start(SwalotPlopSpeedUp *speedUp) {
    speedUp->state = SWALOT_PLOP_SPEED_UP_RAMP;
    speedUp->active = TRUE;
    speedUp->frame = 0;
    PlaySE(SEQ_SE_PL_PINPON);
}

static void SwalotPlopSpeedUp_Update(SwalotPlopSpeedUp *speedUp, SwalotPlopStage *stage, SwalotPlopScene *scene) {
    if (!speedUp->active) {
        return;
    }
    switch (speedUp->state) {
    case SWALOT_PLOP_SPEED_UP_RAMP:
        speedUp->frame++;
        SwalotPlopStage_SetAnimSpeed(stage, speedUp->frame * 102400 / 58 + FX32_ONE);
        GF_SndHandleSetTempoRatio(7, speedUp->frame * 150 / 58 + 256);
        if (speedUp->frame == 12) {
            PlaySE(SEQ_SE_PL_BALLOON05_2);
        }
        if (speedUp->frame >= 58) {
            speedUp->state++;
        }
        break;
    case SWALOT_PLOP_SPEED_UP_START_FLASH:
        StartBrightnessTransition(4, 16, 0, (GXBlendPlaneMask)0x3F, 1);
        speedUp->state++;
        break;
    case SWALOT_PLOP_SPEED_UP_WAIT_FLASH:
        if (IsBrightnessTransitionActive(1) == TRUE) {
            SwalotPlopStage_StartMouth(stage);
            SwalotPlopStage_SetAnimSpeed(stage, 2 * FX32_ONE);
            SwalotPlopScene_SetBonus(scene, 1);
            speedUp->state++;
        }
        break;
    case SWALOT_PLOP_SPEED_UP_START_RESTORE:
        StartBrightnessTransition(14, 0, 16, (GXBlendPlaneMask)0x3F, 1);
        speedUp->state++;
        break;
    case SWALOT_PLOP_SPEED_UP_WAIT_RESTORE:
        if (IsBrightnessTransitionActive(1) == TRUE) {
            speedUp->state++;
        }
        break;
    case SWALOT_PLOP_SPEED_UP_END:
        speedUp->active = FALSE;
        speedUp->state = SWALOT_PLOP_SPEED_UP_RAMP;
        break;
    }
}

static void SwalotPlopSpeedUp_ResetTempo(SwalotPlopSpeedUp *speedUp) {
    GF_SndHandleSetTempoRatio(7, 0x100);
}

static const SwalotPlopCommand *SwalotPlop_GetCommandTable(void) {
    return gSwalotPlopCommands;
}

static int SwalotPlop_GetCommandCount(void) {
    return NELEMS(gSwalotPlopCommands);
}

static void SwalotPlop_RegisterCommands(SwalotPlopApp *app) {
    Comm_RegisterCommandTable(SwalotPlop_GetCommandTable(), SwalotPlop_GetCommandCount(), app);
}

u32 SwalotPlop_GetNoDataSize(void) {
    return 0;
}

void SwalotPlop_HandleStart(int netId, int size, void *data, void *context) {
    SwalotPlop_ReceiveStart(context);
}

void SwalotPlop_HandleFinish(int netId, int size, void *data, void *context) {
    SwalotPlop_ReceiveFinish(context);
}

void SwalotPlop_HandleShot(int netId, int size, void *data, void *context) {
    SwalotPlop_ReceiveShot(context, data, netId);
    if (!Comm_GetLocalNetId()) {
        SwalotPlop_TallyShot(context, data, netId);
    }
}

void SwalotPlop_HandleFinalScore(int netId, int size, void *data, void *context) {
    SwalotPlop_ReceiveFinalScore(context, *(u32 *)data, netId);
}

void SwalotPlop_HandleScores(int netId, int size, void *data, void *context) {
    SwalotPlop_ReceiveScores(context, data, netId);
}

void SwalotPlop_HandleScoreReport(int netId, int size, void *data, void *context) {
    SwalotPlop_ReceiveScoreReport(context, *(u32 *)data, netId);
}

void SwalotPlop_HandleSync(int netId, int size, void *data, void *context) {
    SwalotPlop_ReceiveSync(context);
}

void SwalotPlop_HandleLevel(int netId, int size, void *data, void *context) {
    u32 level = *(u32 *)data;
    if (netId == 0) {
        SwalotPlop_ReceiveLevel(context, level);
    }
}

u32 SwalotPlop_GetShotSize(void) {
    return sizeof(SwalotPlopShotRecord);
}

u32 SwalotPlop_GetScoresSize(void) {
    return sizeof(SwalotPlopScores);
}

u32 SwalotPlop_GetScoreSize(void) {
    return sizeof(u32);
}

u32 SwalotPlop_GetLevelSize(void) {
    return sizeof(u32);
}
#if defined(SDK_PORT)

/* Native tables and touchpad buffer from asm/plaza_game_swalot_plop_data.s. The
 * recording buffer is written, so it is real C storage (8 samples of TPData). */
static int Native_SwalotPlopBody_GetTiltFastest(SwalotPlopBody *body, u32 frame, u32 duration) { return SwalotPlopBody_GetTiltFastest(body, frame, duration); }
int (*const gSwalotPlopTiltFuncs[3])(SwalotPlopBody *body, u32 frame, u32 duration) = {
    SwalotPlopBody_GetTiltNormal,
    SwalotPlopBody_GetTiltFast,
    Native_SwalotPlopBody_GetTiltFastest,
};
const SwalotPlopCommand gSwalotPlopCommands[8] = {
    SwalotPlop_HandleStart,
    SwalotPlop_GetNoDataSize,
    0,
    SwalotPlop_HandleFinish,
    SwalotPlop_GetNoDataSize,
    0,
    SwalotPlop_HandleShot,
    SwalotPlop_GetShotSize,
    0,
    SwalotPlop_HandleFinalScore,
    SwalotPlop_GetScoreSize,
    0,
    SwalotPlop_HandleScores,
    SwalotPlop_GetScoresSize,
    0,
    SwalotPlop_HandleLevel,
    SwalotPlop_GetLevelSize,
    0,
    SwalotPlop_HandleScoreReport,
    SwalotPlop_GetScoreSize,
    0,
    SwalotPlop_HandleSync,
    SwalotPlop_GetNoDataSize,
    0,
};
_Static_assert(sizeof(TPData) * 8 == 0x40, "Swalot Plop touchpad buffer");

/* Native tables from asm/wifi_plaza_lobby_data.s; adapters give differing originals the field signatures. */
const OverlayManagerTemplate sWiFiPlazaSwalotPlopOverlayTemplate = { SwalotPlop_Init, SwalotPlop_Main, SwalotPlop_Exit, FS_OVERLAY_ID(OVY_91) };
#endif
