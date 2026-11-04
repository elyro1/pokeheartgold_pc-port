#ifndef POKEHEARTGOLD_FIELD_MAP_OBJECT_DRAW_CALLBACKS_H
#define POKEHEARTGOLD_FIELD_MAP_OBJECT_DRAW_CALLBACKS_H

#include "map_object.h"

// Shared declarations keep native callback adapters consistent with their producers.
void BillboardDraw_Init(void *object);
void BillboardDraw_UpdateNpc(void *object);
void BillboardDraw_Delete(void *object);
void BillboardDraw_Pause(void *object);
void BillboardDraw_Resume(void *object);
void BillboardDraw_UpdatePositionOnly(void *object);
void BillboardDraw_UpdateLooping(void *object);
void PlayerDraw_Init(void *object);
void PlayerDraw_UpdateWalking(void *object);
void PlayerDraw_Delete(void *object);
void PlayerDraw_Pause(void *object);
void PlayerDraw_Resume(void *object);
void PlayerDraw_UpdateCycling(void *object);
void PlayerDraw_UpdateUseHmOrSaving(void *object);
void PlayerDraw_UpdateWatering(void *object);
void PlayerDraw_UpdateSurfing(void *object);
void PlayerDraw_UpdateFishing(void *object);
void PlayerDraw_UpdateLadder(void *object);
void PlayerDraw_UpdateHeal(void *object);
void PlayerDraw_UpdatePoketch(void *object);
void BillboardDraw_UpdatePcWoman(void *object);
void LegendaryBirdObject_Init(LocalMapObject *object);
void LegendaryBirdObject_Update(LocalMapObject *object);
void LegendaryBirdObject_Delete(LocalMapObject *object);
void LegendaryBirdObject_Pause(LocalMapObject *object);
void LegendaryBirdObject_Resume(LocalMapObject *object);
void BillboardDraw_UpdateTwoSequences(void *object);
void ModelObjectDraw_Init(void *object);
void ModelObjectDraw_Update(void);
void ModelObjectDraw_Delete(void *object);
void ModelObjectDraw_Pause(void *object);
void ModelObjectDraw_Resume(void *object);
void FollowMonDraw_Init(void *object);
void FollowMonDraw_UpdateLarge(LocalMapObject *object);
void FollowMonDraw_Delete(void *object);
void FollowMonDraw_Pause(void *object);
void FollowMonDraw_Resume(LocalMapObject *object);
void FollowMonDraw_Update(LocalMapObject *object);
void ApricornTreeObject_Init(LocalMapObject *object);
void ApricornTreeObject_Update(LocalMapObject *object);
void ApricornTreeObject_Delete(LocalMapObject *object);
void ApricornTreeObject_Pause(LocalMapObject *object);
void ApricornTreeObject_Resume(LocalMapObject *object);
void PlayerDraw_UpdateApricornShake(void *object);
void CameraFocusObject_Init(LocalMapObject *object);
void CameraFocusObject_Update(LocalMapObject *object);
void CameraFocusObject_Delete(LocalMapObject *object);
void CameraFocusObject_Pause(LocalMapObject *object);
void CameraFocusObject_Resume(LocalMapObject *object);
extern MapObjectDrawCallbacks gNpcDrawCallbacks;
extern MapObjectDrawCallbacks gUnanimatedObjectDrawCallbacks;
extern MapObjectDrawCallbacks gLoopingObjectDrawCallbacks;
extern MapObjectDrawCallbacks gPlayerWalkingDrawCallbacks;
extern MapObjectDrawCallbacks gPlayerCyclingDrawCallbacks;
extern MapObjectDrawCallbacks gPlayerUseHmOrSavingDrawCallbacks;
extern MapObjectDrawCallbacks gPlayerWateringDrawCallbacks;
extern MapObjectDrawCallbacks gPlayerSurfingDrawCallbacks;
extern MapObjectDrawCallbacks gPlayerFishingDrawCallbacks;
extern MapObjectDrawCallbacks gPlayerLadderDrawCallbacks;
extern MapObjectDrawCallbacks gPlayerHealDrawCallbacks;
extern MapObjectDrawCallbacks gPlayerPoketchDrawCallbacks;
extern MapObjectDrawCallbacks gPcWomanDrawCallbacks;
extern MapObjectDrawCallbacks gLegendaryBirdDrawCallbacks;
extern MapObjectDrawCallbacks ov01_02206E2C;
extern MapObjectDrawCallbacks gModelObjectDrawCallbacks;
extern MapObjectDrawCallbacks gLargeFollowMonDrawCallbacks;
extern MapObjectDrawCallbacks gFollowMonDrawCallbacks;
extern MapObjectDrawCallbacks gApricornTreeDrawCallbacks;
extern MapObjectDrawCallbacks gPlayerApricornShakeDrawCallbacks;
extern MapObjectDrawCallbacks gCameraFocusObjectDrawCallbacks;
extern MapObjectDrawCallbacks *gMapObjectDrawCallbacks[20];

#endif // POKEHEARTGOLD_FIELD_MAP_OBJECT_DRAW_CALLBACKS_H
