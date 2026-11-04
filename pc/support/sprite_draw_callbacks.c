#include "field/map_object_draw_callbacks.h"

static void BillboardDraw_InitNative(LocalMapObject *object) { BillboardDraw_Init(object); }
static void BillboardDraw_UpdateNpcNative(LocalMapObject *object) { BillboardDraw_UpdateNpc(object); }
static void BillboardDraw_DeleteNative(LocalMapObject *object) { BillboardDraw_Delete(object); }
static void BillboardDraw_PauseNative(LocalMapObject *object) { BillboardDraw_Pause(object); }
static void BillboardDraw_ResumeNative(LocalMapObject *object) { BillboardDraw_Resume(object); }
static void BillboardDraw_UpdatePositionOnlyNative(LocalMapObject *object) { BillboardDraw_UpdatePositionOnly(object); }
static void BillboardDraw_UpdateLoopingNative(LocalMapObject *object) { BillboardDraw_UpdateLooping(object); }
static void PlayerDraw_InitNative(LocalMapObject *object) { PlayerDraw_Init(object); }
static void PlayerDraw_UpdateWalkingNative(LocalMapObject *object) { PlayerDraw_UpdateWalking(object); }
static void PlayerDraw_DeleteNative(LocalMapObject *object) { PlayerDraw_Delete(object); }
static void PlayerDraw_PauseNative(LocalMapObject *object) { PlayerDraw_Pause(object); }
static void PlayerDraw_ResumeNative(LocalMapObject *object) { PlayerDraw_Resume(object); }
static void PlayerDraw_UpdateCyclingNative(LocalMapObject *object) { PlayerDraw_UpdateCycling(object); }
static void PlayerDraw_UpdateUseHmOrSavingNative(LocalMapObject *object) { PlayerDraw_UpdateUseHmOrSaving(object); }
static void PlayerDraw_UpdateWateringNative(LocalMapObject *object) { PlayerDraw_UpdateWatering(object); }
static void PlayerDraw_UpdateSurfingNative(LocalMapObject *object) { PlayerDraw_UpdateSurfing(object); }
static void PlayerDraw_UpdateFishingNative(LocalMapObject *object) { PlayerDraw_UpdateFishing(object); }
static void PlayerDraw_UpdateLadderNative(LocalMapObject *object) { PlayerDraw_UpdateLadder(object); }
static void PlayerDraw_UpdateHealNative(LocalMapObject *object) { PlayerDraw_UpdateHeal(object); }
static void PlayerDraw_UpdatePoketchNative(LocalMapObject *object) { PlayerDraw_UpdatePoketch(object); }
static void BillboardDraw_UpdatePcWomanNative(LocalMapObject *object) { BillboardDraw_UpdatePcWoman(object); }
static void BillboardDraw_UpdateTwoSequencesNative(LocalMapObject *object) { BillboardDraw_UpdateTwoSequences(object); }
static void ModelObjectDraw_InitNative(LocalMapObject *object) { ModelObjectDraw_Init(object); }
static void ModelObjectDraw_UpdateNative(LocalMapObject *object) { ModelObjectDraw_Update(); }
static void ModelObjectDraw_DeleteNative(LocalMapObject *object) { ModelObjectDraw_Delete(object); }
static void ModelObjectDraw_PauseNative(LocalMapObject *object) { ModelObjectDraw_Pause(object); }
static void ModelObjectDraw_ResumeNative(LocalMapObject *object) { ModelObjectDraw_Resume(object); }
static void FollowMonDraw_InitNative(LocalMapObject *object) { FollowMonDraw_Init(object); }
static void FollowMonDraw_DeleteNative(LocalMapObject *object) { FollowMonDraw_Delete(object); }
static void FollowMonDraw_PauseNative(LocalMapObject *object) { FollowMonDraw_Pause(object); }
static void PlayerDraw_UpdateApricornShakeNative(LocalMapObject *object) { PlayerDraw_UpdateApricornShake(object); }

MapObjectDrawCallbacks gNpcDrawCallbacks = {
    BillboardDraw_InitNative, BillboardDraw_UpdateNpcNative, BillboardDraw_DeleteNative, BillboardDraw_PauseNative, BillboardDraw_ResumeNative
};

MapObjectDrawCallbacks gUnanimatedObjectDrawCallbacks = {
    BillboardDraw_InitNative, BillboardDraw_UpdatePositionOnlyNative, BillboardDraw_DeleteNative, BillboardDraw_PauseNative, BillboardDraw_ResumeNative
};

MapObjectDrawCallbacks gLoopingObjectDrawCallbacks = {
    BillboardDraw_InitNative, BillboardDraw_UpdateLoopingNative, BillboardDraw_DeleteNative, BillboardDraw_PauseNative, BillboardDraw_ResumeNative
};

MapObjectDrawCallbacks gPlayerWalkingDrawCallbacks = {
    PlayerDraw_InitNative, PlayerDraw_UpdateWalkingNative, PlayerDraw_DeleteNative, PlayerDraw_PauseNative, PlayerDraw_ResumeNative
};

MapObjectDrawCallbacks gPlayerCyclingDrawCallbacks = {
    PlayerDraw_InitNative, PlayerDraw_UpdateCyclingNative, PlayerDraw_DeleteNative, PlayerDraw_PauseNative, PlayerDraw_ResumeNative
};

MapObjectDrawCallbacks gPlayerUseHmOrSavingDrawCallbacks = {
    PlayerDraw_InitNative, PlayerDraw_UpdateUseHmOrSavingNative, PlayerDraw_DeleteNative, PlayerDraw_PauseNative, PlayerDraw_ResumeNative
};

MapObjectDrawCallbacks gPlayerWateringDrawCallbacks = {
    PlayerDraw_InitNative, PlayerDraw_UpdateWateringNative, PlayerDraw_DeleteNative, PlayerDraw_PauseNative, PlayerDraw_ResumeNative
};

MapObjectDrawCallbacks gPlayerSurfingDrawCallbacks = {
    PlayerDraw_InitNative, PlayerDraw_UpdateSurfingNative, PlayerDraw_DeleteNative, PlayerDraw_PauseNative, PlayerDraw_ResumeNative
};

MapObjectDrawCallbacks gPlayerFishingDrawCallbacks = {
    PlayerDraw_InitNative, PlayerDraw_UpdateFishingNative, PlayerDraw_DeleteNative, PlayerDraw_PauseNative, PlayerDraw_ResumeNative
};

MapObjectDrawCallbacks gPlayerLadderDrawCallbacks = {
    PlayerDraw_InitNative, PlayerDraw_UpdateLadderNative, PlayerDraw_DeleteNative, PlayerDraw_PauseNative, PlayerDraw_ResumeNative
};

MapObjectDrawCallbacks gPlayerHealDrawCallbacks = {
    PlayerDraw_InitNative, PlayerDraw_UpdateHealNative, PlayerDraw_DeleteNative, PlayerDraw_PauseNative, PlayerDraw_ResumeNative
};

MapObjectDrawCallbacks gPlayerPoketchDrawCallbacks = {
    PlayerDraw_InitNative, PlayerDraw_UpdatePoketchNative, PlayerDraw_DeleteNative, PlayerDraw_PauseNative, PlayerDraw_ResumeNative
};

MapObjectDrawCallbacks gPcWomanDrawCallbacks = {
    BillboardDraw_InitNative, BillboardDraw_UpdatePcWomanNative, BillboardDraw_DeleteNative, BillboardDraw_PauseNative, BillboardDraw_ResumeNative
};

MapObjectDrawCallbacks gLegendaryBirdDrawCallbacks = {
    LegendaryBirdObject_Init, LegendaryBirdObject_Update, LegendaryBirdObject_Delete, LegendaryBirdObject_Pause, LegendaryBirdObject_Resume
};

MapObjectDrawCallbacks ov01_02206E2C = {
    BillboardDraw_InitNative, BillboardDraw_UpdateTwoSequencesNative, BillboardDraw_DeleteNative, BillboardDraw_PauseNative, BillboardDraw_ResumeNative
};

MapObjectDrawCallbacks gModelObjectDrawCallbacks = {
    ModelObjectDraw_InitNative, ModelObjectDraw_UpdateNative, ModelObjectDraw_DeleteNative, ModelObjectDraw_PauseNative, ModelObjectDraw_ResumeNative
};

MapObjectDrawCallbacks gLargeFollowMonDrawCallbacks = {
    FollowMonDraw_InitNative, FollowMonDraw_UpdateLarge, FollowMonDraw_DeleteNative, FollowMonDraw_PauseNative, FollowMonDraw_Resume
};

MapObjectDrawCallbacks gFollowMonDrawCallbacks = {
    FollowMonDraw_InitNative, FollowMonDraw_Update, FollowMonDraw_DeleteNative, FollowMonDraw_PauseNative, FollowMonDraw_Resume
};

MapObjectDrawCallbacks gApricornTreeDrawCallbacks = {
    ApricornTreeObject_Init, ApricornTreeObject_Update, ApricornTreeObject_Delete, ApricornTreeObject_Pause, ApricornTreeObject_Resume
};

MapObjectDrawCallbacks gPlayerApricornShakeDrawCallbacks = {
    PlayerDraw_InitNative, PlayerDraw_UpdateApricornShakeNative, PlayerDraw_DeleteNative, PlayerDraw_PauseNative, PlayerDraw_ResumeNative
};

MapObjectDrawCallbacks gCameraFocusObjectDrawCallbacks = {
    CameraFocusObject_Init, CameraFocusObject_Update, CameraFocusObject_Delete, CameraFocusObject_Pause, CameraFocusObject_Resume
};

MapObjectDrawCallbacks *gMapObjectDrawCallbacks[20] = {
    &gNpcDrawCallbacks,
    &gUnanimatedObjectDrawCallbacks,
    &gLoopingObjectDrawCallbacks,
    &gPlayerWalkingDrawCallbacks,
    &gPlayerCyclingDrawCallbacks,
    &gPlayerUseHmOrSavingDrawCallbacks,
    &gPlayerWateringDrawCallbacks,
    &gPlayerSurfingDrawCallbacks,
    &gPlayerFishingDrawCallbacks,
    &gPlayerLadderDrawCallbacks,
    &gPlayerHealDrawCallbacks,
    &gPlayerPoketchDrawCallbacks,
    &gPcWomanDrawCallbacks,
    &gLegendaryBirdDrawCallbacks,
    &ov01_02206E2C,
    &gModelObjectDrawCallbacks,
    &gLargeFollowMonDrawCallbacks,
    &gFollowMonDrawCallbacks,
    &gApricornTreeDrawCallbacks,
    &gPlayerApricornShakeDrawCallbacks,
};
