#ifndef POKEHEARTGOLD_FIELD_TERRAIN_H
#define POKEHEARTGOLD_FIELD_TERRAIN_H

#include "field/field_control.h"

struct FieldTerrainQueries;
struct MapProp;
#if defined(SDK_PORT)
typedef s8 *FieldSlopeOutput;
#endif

// Width of one map tile in world units
#define MAP_TILE_SIZE FX32_CONST(16)

// Where the height returned by the terrain height queries came from
enum CalculatedHeightSource {
    CALCULATED_HEIGHT_SOURCE_NONE = 0,
    CALCULATED_HEIGHT_SOURCE_BDHC,
    CALCULATED_HEIGHT_SOURCE_DYNAMIC
};

SoundplateStruct *FieldSystem_GetSoundplateAt(FieldSystem *fieldSystem, int x, int z);
void FieldSystem_SetTerrainQueries(const struct FieldTerrainQueries **queries, int mode);
BOOL FieldSystem_CheckTileCollision(FieldSystem *fieldSystem, int x, int z);
u8 FieldSystem_GetTileAttrBits8To14(FieldSystem *fieldSystem, int x, int z);
u8 GetMetatileBehavior(FieldSystem *fieldSystem, int x, int z);
int FieldSystem_GetTerrainHeight(FieldSystem *fieldSystem, int objectHeight, fx32 x, fx32 z, u8 *outKind);
BOOL FieldSystem_FindMapProp(FieldSystem *fieldSystem, int buildModel, struct MapProp **outProp, int *outIdx);
BOOL FieldSystem_FindMapPropOfModels(FieldSystem *fieldSystem, int *models, u32 count, struct MapProp **outProp, int *outModel);
int *FieldSystem_CollectMapPropModelsInRect(FieldSystem *fieldSystem, enum HeapID heapId, int capacity, fx32 *rect, u32 fill);
void Terrain_GetChunkOrigin(int idx, int width, VecFx32 *pos);
BOOL MapModelNo_HasNoAreaMapAnimation(u16 modelNo);
BOOL FieldSystem_CheckCollisionAndHeight(FieldSystem *fieldSystem, VecFx32 *playerPos, int xInFront, int yInFront, s8 *outSlope);
u32 FieldSystem_CheckCollisionHeightAndGimmick(FieldSystem *fieldSystem, VecFx32 *playerPos, u32 x, u32 y, u32 *outSlope);
void Terrain_MakeTileRect(int x, int z, int offsetX, int offsetZ, int width, int depth, fx32 *out);
BOOL FieldSystem_FindMapPropInRect(FieldSystem *fieldSystem, int buildModel, fx32 *rect, struct MapProp **outProp);
BOOL FieldSystem_FindMapPropOfModelsInRect(FieldSystem *fieldSystem, int *models, u32 count, fx32 *rect, struct MapProp **outProp, int *outModel);
BOOL MapObject_IsTileOccupiedInLink(LocalMapObject *playerObj, int xInFront, int playerElev, int yInFront);
int Terrain_GetHeight_Bdhc(FieldSystem *fieldSystem, int objectHeight, fx32 x, fx32 z, u8 *outKind);
int Terrain_CalcHeightWithMode(FieldSystem *fieldSystem, int objectHeightMode, int objectHeight, fx32 x, fx32 z, u8 *outKind);

#endif // POKEHEARTGOLD_FIELD_TERRAIN_H
