#include "field/field_move_effects.h"

#include "global.h"

#include "constants/buildmodel.h"
#include "constants/global_fieldmap.h"
#include "constants/scrcmd.h"
#include "constants/sndseq.h"

#include "field/area_data.h"
#include "field/field_3d_model.h"
#include "field/field_3d_object_task.h"
#include "field/map_prop.h"
#include "field/map_prop_animation.h"

#include "camera.h"
#include "field_system.h"
#include "field_terrain.h"
#include "filesystem.h"
#include "follow_mon.h"
#include "gf_gfx_loader.h"
#include "heap.h"
#include "map_matrix.h"
#include "map_object.h"
#include "math_util.h"
#include "overlay_02.h"
#include "player_avatar.h"
#include "sound_playback.h"
#include "sys_task.h"
#include "task.h"

// Hall of Fame Poke Ball animation and the 3D effects of the field moves: Cut, Rock Smash, Headbutt, Waterfall,
// Whirlpool and the rock debris of Rock Climb.

#define HALL_OF_FAME_MACHINE  BUILD_MODEL_YO_H02
#define HALL_OF_FAME_BALL     BUILD_MODEL_TN_GYMBASE
#define HALL_OF_FAME_ANIM_TAG 0x10

// A map tile is 16 units wide; objects stand at the tile center.
#define TILE_TO_POS(tile) ((tile) * MAP_TILE_SIZE + MAP_TILE_SIZE / 2)

#define NUM_ROCK_CLIMB_DEBRIS_PIECES 16
#define NUM_ROCK_DEBRIS_PIECES       18

enum HallOfFameBallAnimState {
    HALL_OF_FAME_BALLS_LOAD,
    HALL_OF_FAME_BALLS_PLACE,
    HALL_OF_FAME_BALLS_WAIT,
    HALL_OF_FAME_BALLS_ANIMATE,
    HALL_OF_FAME_BALLS_WAIT_ANIM,
    HALL_OF_FAME_BALLS_END,
};

enum FieldMoveEffectState {
    FIELD_MOVE_EFFECT_STATE_ANIMATE,
    FIELD_MOVE_EFFECT_STATE_DONE,
};

enum WaterfallEffectState {
    WATERFALL_EFFECT_STATE_LOAD,
    WATERFALL_EFFECT_STATE_START,
    WATERFALL_EFFECT_STATE_SPLASH,
    WATERFALL_EFFECT_STATE_LOOP,
};

enum ScriptFieldMoveEffectState {
    SCRIPT_FIELD_MOVE_EFFECT_WAIT,
    SCRIPT_FIELD_MOVE_EFFECT_END,
};

typedef struct HallOfFameBallAnim {
    VecFx32 basePos;
    u8 numBalls;
    u8 numPlaced;
    u8 timer;
    u8 state;
    u8 ballProps[8];
} HallOfFameBallAnim;

// A model with up to four animations loaded from NARC_a_1_3_4.
typedef struct FieldMoveEffectModel {
    Field3dObject object;
    Field3dModel model;
    Field3DModelAnimation animations[4];
    u32 numAnimations;
} FieldMoveEffectModel;

// Cut and Rock Smash: the animations play once, then the effect hides itself.
typedef struct FieldMoveEffect {
    FieldMoveEffectModel model;
    NNSFndAllocator allocator;
    int state;
} FieldMoveEffect;

typedef struct HeadbuttEffect {
    FieldMoveEffectModel model;
    NNSFndAllocator allocator;
    VecFx32 cameraPos;
    VecFx32 cameraTarget;
    VecFx32 shakeOffset;
    s8 shakeTimer;
    u8 shaken;
    u8 numShakes;
    u8 state;
} HeadbuttEffect;

// Waterfall: the start splash plays once at the player, then the loop splash follows the player.
typedef struct WaterfallEffect {
    FieldMoveEffectModel models[2];
    NNSFndAllocator allocator;
    u16 numLoaded;
    u16 state;
} WaterfallEffect;

typedef struct WhirlpoolEffect {
    FieldMoveEffectModel model;
    NNSFndAllocator allocator;
    u16 state;
} WhirlpoolEffect;

// One rock fragment cloud; the pieces of a debris effect share one model and its animation data.
typedef struct RockDebrisPiece {
    Field3dObject object;
    Field3DModelAnimation animations[4];
    BOOL active;
} RockDebrisPiece;

typedef struct RockClimbDebris {
    Field3dModel model;
    RockDebrisPiece pieces[NUM_ROCK_CLIMB_DEBRIS_PIECES];
    void *animData[4];
    FieldSystem *fieldSystem;
    int spawnTimer;
    fx32 offsetY;
    fx32 offsetZ;
    VecFx32 lastPlayerPos;
    NNSFndAllocator allocator;
    BOOL paused;
} RockClimbDebris;

typedef struct RockDebris {
    Field3dModel model;
    RockDebrisPiece pieces[NUM_ROCK_DEBRIS_PIECES];
    void *animData[4];
    FieldSystem *fieldSystem;
    int spawnTimer;
    fx32 offsetY;
    fx32 offsetZ;
    NNSFndAllocator allocator;
    u16 paused;
    u16 withFollower;
} RockDebris;

typedef struct ScriptFieldMoveEffectTask {
    Field3dObjectTask *effect;
    u16 *done;
    u32 state;
    u32 kind;
} ScriptFieldMoveEffectTask;

typedef struct FileIdPair {
    u8 ids[2];
} FileIdPair;

typedef struct RideEffectFileIds {
    FileIdPair whirlpool;       // model, animation
    FileIdPair waterfallModels; // start, loop
    FileIdPair waterfallAnims;  // first animation of the start and the loop model
    u16 whirlpoolAngles[4];     // by facing direction
} RideEffectFileIds;

extern RideEffectFileIds gRideEffectFileIds;

NNSG3dResFileHeader **AreaDataManager_GetMapPropModel(u32 mapPropId, AreaDataManager *areaDataManager);
void Field3dObject_SetXRotation(Field3dObject *object, u16 angle, int axis);

static BOOL Task_HallOfFameBallAnim(TaskManager *taskManager);
static void Field3dObject_SetPosInFrontOfMapObject(MapObjectManager *manager, u32 id, Field3dObject *object);
static void Field3dObject_SetPosInFrontOfPlayer(PlayerAvatar *playerAvatar, Field3dObject *object);
static void Field3dObject_SetPosAtPlayer(PlayerAvatar *playerAvatar, Field3dObject *object);
static void FieldMoveEffectModel_Load(FieldMoveEffectModel *model, int modelFileId, int firstAnimFileId, int numAnimations, NNSFndAllocator *allocator);
static void FieldMoveEffectModel_Unload(FieldMoveEffectModel *model, NNSFndAllocator *allocator);
static BOOL FieldMoveEffectModel_AnimateOnce(FieldMoveEffectModel *model);
static void FieldMoveEffectModel_AnimateLoop(FieldMoveEffectModel *model);
static void FieldMoveEffectModel_Draw(FieldMoveEffectModel *model);
void CutEffect_InitAtPlayer(Field3dObjectTask *task, FieldSystem *fieldSystem, FieldMoveEffect *effect);
void CutEffect_InitAtFollower(Field3dObjectTask *task, FieldSystem *fieldSystem, FieldMoveEffect *effect);
void CutEffect_Destroy(Field3dObjectTask *task, FieldSystem *fieldSystem, FieldMoveEffect *effect);
void CutEffect_Update(Field3dObjectTask *task, FieldSystem *fieldSystem, FieldMoveEffect *effect);
void CutEffect_Render(Field3dObjectTask *task, FieldSystem *fieldSystem, FieldMoveEffect *effect);
Field3dObjectTask *CutEffect_CreateAtPlayer(FieldSystem *fieldSystem);
Field3dObjectTask *CutEffect_CreateAtFollower(FieldSystem *fieldSystem);
void CutEffect_Delete(Field3dObjectTask *task);
BOOL CutEffect_IsFinished(Field3dObjectTask *task);
void RockSmashEffect_InitAtPlayer(Field3dObjectTask *task, FieldSystem *fieldSystem, FieldMoveEffect *effect);
void RockSmashEffect_InitAtFollower(Field3dObjectTask *task, FieldSystem *fieldSystem, FieldMoveEffect *effect);
void RockSmashEffect_Destroy(Field3dObjectTask *task, FieldSystem *fieldSystem, FieldMoveEffect *effect);
void RockSmashEffect_Update(Field3dObjectTask *task, FieldSystem *fieldSystem, FieldMoveEffect *effect);
void RockSmashEffect_Render(Field3dObjectTask *task, FieldSystem *fieldSystem, FieldMoveEffect *effect);
Field3dObjectTask *RockSmashEffect_CreateAtPlayer(FieldSystem *fieldSystem);
Field3dObjectTask *RockSmashEffect_CreateAtFollower(FieldSystem *fieldSystem);
void RockSmashEffect_Delete(Field3dObjectTask *task);
BOOL RockSmashEffect_IsFinished(Field3dObjectTask *task);
void WaterfallEffect_Init(Field3dObjectTask *task, FieldSystem *fieldSystem, WaterfallEffect *effect);
void WaterfallEffect_Destroy(Field3dObjectTask *task, FieldSystem *fieldSystem, WaterfallEffect *effect);
void WaterfallEffect_Update(Field3dObjectTask *task, FieldSystem *fieldSystem, WaterfallEffect *effect);
void WaterfallEffect_Render(Field3dObjectTask *task, FieldSystem *fieldSystem, WaterfallEffect *effect);
void WhirlpoolEffect_Init(Field3dObjectTask *task, FieldSystem *fieldSystem, WhirlpoolEffect *effect);
void WhirlpoolEffect_Destroy(Field3dObjectTask *task, FieldSystem *fieldSystem, WhirlpoolEffect *effect);
void WhirlpoolEffect_Update(Field3dObjectTask *task, FieldSystem *fieldSystem, WhirlpoolEffect *effect);
void WhirlpoolEffect_Render(Field3dObjectTask *task, FieldSystem *fieldSystem, WhirlpoolEffect *effect);
static void RockClimbDebris_SpawnPiece(RockDebrisPiece *piece, PlayerAvatar *playerAvatar, fx32 offsetY, fx32 offsetZ);
static void RockClimbDebris_UpdatePiece(RockDebrisPiece *piece);
static void RockClimbDebris_InitPiece(RockDebrisPiece *piece, Field3dModel *model, NNSFndAllocator *allocator, void **animData);
static void RockClimbDebris_FreePiece(RockDebrisPiece *piece, NNSFndAllocator *allocator);
static void RockClimbDebris_UpdateOffsets(RockClimbDebris *debris);
static void RockClimbDebris_Spawn(RockClimbDebris *debris);
static void RockClimbDebris_UpdatePieces(RockClimbDebris *debris);
void RockClimbDebris_Init(Field3dObjectTask *task, FieldSystem *fieldSystem, RockClimbDebris *debris);
void RockClimbDebris_Destroy(Field3dObjectTask *task, FieldSystem *fieldSystem, RockClimbDebris *debris);
void RockClimbDebris_Update(Field3dObjectTask *task, FieldSystem *fieldSystem, RockClimbDebris *debris);
void RockClimbDebris_Render(Field3dObjectTask *task, FieldSystem *fieldSystem, RockClimbDebris *debris);
void HeadbuttEffect_InitAtPlayer(Field3dObjectTask *task, FieldSystem *fieldSystem, HeadbuttEffect *effect);
void HeadbuttEffect_InitAtFollower(Field3dObjectTask *task, FieldSystem *fieldSystem, HeadbuttEffect *effect);
void HeadbuttEffect_Destroy(Field3dObjectTask *task, FieldSystem *fieldSystem, HeadbuttEffect *effect);
void HeadbuttEffect_Update(Field3dObjectTask *task, FieldSystem *fieldSystem, HeadbuttEffect *effect);
void HeadbuttEffect_Render(Field3dObjectTask *task, FieldSystem *fieldSystem, HeadbuttEffect *effect);
Field3dObjectTask *HeadbuttEffect_CreateAtPlayer(FieldSystem *fieldSystem);
Field3dObjectTask *HeadbuttEffect_CreateAtFollower(FieldSystem *fieldSystem);
void HeadbuttEffect_Delete(Field3dObjectTask *task);
BOOL HeadbuttEffect_IsFinished(Field3dObjectTask *task);
void RockDebris_Init(Field3dObjectTask *task, FieldSystem *fieldSystem, RockDebris *debris);
void RockDebris_InitWithFollower(Field3dObjectTask *task, FieldSystem *fieldSystem, RockDebris *debris);
void RockDebris_Destroy(Field3dObjectTask *task, FieldSystem *fieldSystem, RockDebris *debris);
void RockDebris_Update(Field3dObjectTask *task, FieldSystem *fieldSystem, RockDebris *debris);
void RockDebris_Render(Field3dObjectTask *task, FieldSystem *fieldSystem, RockDebris *debris);
static void RockDebris_SpawnPiece(RockDebrisPiece *piece, const VecFx32 *position, fx32 offsetY, fx32 offsetZ);
static void RockDebris_UpdatePiece(RockDebrisPiece *piece);
static void RockDebris_InitPiece(RockDebrisPiece *piece, Field3dModel *model, NNSFndAllocator *allocator, void **animData);
static void RockDebris_FreePiece(RockDebrisPiece *piece, NNSFndAllocator *allocator);
static void RockDebris_Spawn(RockDebris *debris);
static void RockDebris_UpdatePieces(RockDebris *debris);
static void Task_ScriptFieldMoveEffect(SysTask *task, void *data);

// Pops up one Poke Ball per party Pokemon on the Hall of Fame machine, then plays their animation.
void HallOfFame_StartBallAnim(FieldSystem *fieldSystem, u8 numBalls) {
    MapProp *machine;
    int machineIndex;
    VecFx32 tileCenter;
    VecFx32 machinePos;
    HallOfFameBallAnim *env;

    if (FieldSystem_FindMapProp(fieldSystem, HALL_OF_FAME_MACHINE, &machine, &machineIndex)) {
        env = Heap_AllocAtEnd(HEAP_ID_FIELD1, sizeof(HallOfFameBallAnim));
        env->numBalls = numBalls;
        env->numPlaced = 0;
        env->timer = 0;
        env->state = HALL_OF_FAME_BALLS_LOAD;
        Terrain_GetChunkOrigin(machineIndex, MapMatrix_GetWidth(fieldSystem->mapMatrix), &tileCenter);
        MapProp_GetTranslation(&machinePos, machine);
        env->basePos = machinePos;
        env->basePos.x += tileCenter.x;
        env->basePos.z += tileCenter.z;
        TaskManager_Call(fieldSystem->taskman, Task_HallOfFameBallAnim, env);
    } else {
        GF_ASSERT(FALSE);
    }
}

static BOOL Task_HallOfFameBallAnim(TaskManager *taskManager) {
    FieldSystem *fieldSystem = TaskManager_GetFieldSystem(taskManager);
    HallOfFameBallAnim *env = TaskManager_GetEnvironment(taskManager);

    switch (env->state) {
    case HALL_OF_FAME_BALLS_LOAD: {
        NNSG3dResMdlSet *mdlSet = NNS_G3dGetMdlSet(*AreaDataManager_GetMapPropModel(HALL_OF_FAME_BALL, fieldSystem->areaDataManager));
        NNSG3dResMdl *mdl = NNS_G3dGetMdlByIdx(mdlSet, 0);
        NNSG3dResTex *tex = AreaDataManager_GetMapPropTexture(fieldSystem->areaDataManager);
        MapPropOneShotAnimationManager_LoadPropAnimations(fieldSystem->mapPropAnimationManager, fieldSystem->mapPropOneShotAnimationManager, HALL_OF_FAME_ANIM_TAG, HALL_OF_FAME_BALL, NULL, mdl, tex, 1, 1, FALSE);
        env->state++;
        break;
    }
    case HALL_OF_FAME_BALLS_PLACE: {
        VecFx32 pos;
        VecFx32 rotation;
        fx32 *coords = (fx32 *)&rotation;
        coords[0] = 0;
        coords[1] = 0;
        coords[2] = 0;
        pos.x = env->basePos.x + gHallOfFameBallOffsets[env->numPlaced].x;
        pos.y = env->basePos.y + gHallOfFameBallOffsets[env->numPlaced].y;
        pos.z = env->basePos.z + gHallOfFameBallOffsets[env->numPlaced].z;
        PlaySE(SEQ_SE_DP_BOWA);
        env->ballProps[env->numPlaced] = MapPropManager_LoadOne(fieldSystem->mapPropManager, HALL_OF_FAME_BALL, &pos, &rotation, fieldSystem->mapPropAnimationManager);
        env->state++;
        break;
    }
    case HALL_OF_FAME_BALLS_WAIT:
        if (env->timer < 15) {
            env->timer++;
        } else {
            env->timer = 0;
            env->numPlaced++;
            if (env->numPlaced < env->numBalls) {
                env->state = HALL_OF_FAME_BALLS_PLACE;
            } else {
                MapPropOneShotAnimationManager_SetAnimationRenderObj(fieldSystem->mapPropOneShotAnimationManager, HALL_OF_FAME_ANIM_TAG, 0, MapProp_GetRenderSurface(MapPropManager_GetMapPropByIndex_Checked_RequireActive(fieldSystem->mapPropManager, env->ballProps[0])));
                env->state++;
            }
        }
        break;
    case HALL_OF_FAME_BALLS_ANIMATE:
        MapPropOneShotAnimationManager_PlayAnimation(fieldSystem->mapPropOneShotAnimationManager, HALL_OF_FAME_ANIM_TAG, 0);
        env->state++;
        break;
    case HALL_OF_FAME_BALLS_WAIT_ANIM:
        if (MapPropOneShotAnimationManager_IsAnimationLoopFinished(fieldSystem->mapPropOneShotAnimationManager, HALL_OF_FAME_ANIM_TAG)) {
            u8 i;
            MapPropOneShotAnimationManager_UnloadAnimation(fieldSystem->mapPropAnimationManager, fieldSystem->mapPropOneShotAnimationManager, HALL_OF_FAME_ANIM_TAG);
            for (i = 0; i < env->numBalls; i++) {
                MapPropManager_RemoveMapPropByIndex(env->ballProps[i], fieldSystem->mapPropManager);
            }
            env->state++;
        }
        break;
    case HALL_OF_FAME_BALLS_END:
        Heap_Free(env);
        return TRUE;
    }
    return FALSE;
}

// Places the object on the tile in front of the map object, at the map object's height.
static void Field3dObject_SetPosInFrontOfMapObject(MapObjectManager *manager, u32 id, Field3dObject *object) {
    VecFx32 pos;
    LocalMapObject *mapObject = MapObjectManager_GetFirstActiveObjectByID(manager, id);
    u32 x;
    u32 z;

    MapObject_CopyPositionVector(mapObject, &pos);
    x = MapObject_GetXCoord(mapObject);
    z = MapObject_GetZCoord(mapObject);
    switch (MapObject_GetFacingDirection(mapObject)) {
    case DIR_NORTH:
        z--;
        break;
    case DIR_SOUTH:
        z++;
        break;
    case DIR_WEST:
        x--;
        break;
    case DIR_EAST:
        x++;
        break;
    default:
        GF_ASSERT(FALSE);
        break;
    }
    Field3dObject_SetPosEx(object, TILE_TO_POS(x), pos.y, TILE_TO_POS(z));
}

static void Field3dObject_SetPosInFrontOfPlayer(PlayerAvatar *playerAvatar, Field3dObject *object) {
    VecFx32 pos;
    u32 x;
    u32 z;

    PlayerAvatar_CopyPositionVector(playerAvatar, &pos);
    x = PlayerAvatar_GetXCoord(playerAvatar);
    z = PlayerAvatar_GetZCoord(playerAvatar);
    switch (PlayerAvatar_GetFacingDirection(playerAvatar)) {
    case DIR_NORTH:
        z--;
        break;
    case DIR_SOUTH:
        z++;
        break;
    case DIR_WEST:
        x--;
        break;
    case DIR_EAST:
        x++;
        break;
    default:
        GF_ASSERT(FALSE);
        break;
    }
    Field3dObject_SetPosEx(object, TILE_TO_POS(x), pos.y, TILE_TO_POS(z));
}

static void Field3dObject_SetPosAtPlayer(PlayerAvatar *playerAvatar, Field3dObject *object) {
    VecFx32 pos;
    PlayerAvatar_CopyPositionVector(playerAvatar, &pos);
    Field3dObject_SetPosEx(object, pos.x, pos.y, pos.z);
}

static void FieldMoveEffectModel_Load(FieldMoveEffectModel *model, int modelFileId, int firstAnimFileId, int numAnimations, NNSFndAllocator *allocator) {
    int i;

    memset(model, 0, sizeof(FieldMoveEffectModel));
    Field3dModel_LoadFromFilesystem(&model->model, NARC_a_1_3_4, modelFileId, HEAP_ID_FIELD1);
    Field3dObject_InitFromModel(&model->object, &model->model);
    model->numAnimations = numAnimations;
    for (i = 0; i < model->numAnimations; i++) {
        Field3dModelAnimation_LoadFromFilesystem(&model->animations[i], &model->model, NARC_a_1_3_4, firstAnimFileId + i, HEAP_ID_FIELD1, allocator);
        Field3dObject_AddAnimation(&model->object, &model->animations[i]);
    }
}

static void FieldMoveEffectModel_Unload(FieldMoveEffectModel *model, NNSFndAllocator *allocator) {
    u32 i;

    Field3dModel_Unload(&model->model);
    for (i = 0; i < model->numAnimations; i++) {
        Field3dModelAnimation_Unload(&model->animations[i], allocator);
    }
}

// Returns TRUE once every animation has reached its last frame.
static BOOL FieldMoveEffectModel_AnimateOnce(FieldMoveEffectModel *model) {
    BOOL finished = TRUE;
    u32 i;

    for (i = 0; i < model->numAnimations; i++) {
        finished &= Field3dModelAnimation_FrameAdvanceAndCheck(&model->animations[i], FX32_ONE);
    }
    return finished;
}

static void FieldMoveEffectModel_AnimateLoop(FieldMoveEffectModel *model) {
    u32 i;

    for (i = 0; i < model->numAnimations; i++) {
        Field3dModelAnimation_FrameAdvanceAndLoop(&model->animations[i], FX32_ONE);
    }
}

static void FieldMoveEffectModel_Draw(FieldMoveEffectModel *model) {
    Field3dObject_Draw(&model->object);
}

void CutEffect_InitAtPlayer(Field3dObjectTask *task, FieldSystem *fieldSystem, FieldMoveEffect *effect) {
    memset(effect, 0, sizeof(FieldMoveEffect));
    HeapExp_FndInitAllocator(&effect->allocator, HEAP_ID_FIELD1, 32);
    FieldMoveEffectModel_Load(&effect->model, 3, 0, 3, &effect->allocator);
    Field3dObject_SetPosInFrontOfPlayer(fieldSystem->playerAvatar, &effect->model.object);
    PlaySE(SEQ_SE_DP_FW015);
    effect->state = FIELD_MOVE_EFFECT_STATE_ANIMATE;
}

void CutEffect_InitAtFollower(Field3dObjectTask *task, FieldSystem *fieldSystem, FieldMoveEffect *effect) {
    memset(effect, 0, sizeof(FieldMoveEffect));
    HeapExp_FndInitAllocator(&effect->allocator, HEAP_ID_FIELD1, 32);
    FieldMoveEffectModel_Load(&effect->model, 3, 0, 3, &effect->allocator);
    Field3dObject_SetPosInFrontOfMapObject(fieldSystem->mapObjectManager, obj_partner_poke, &effect->model.object);
    PlaySE(SEQ_SE_DP_FW015);
    effect->state = FIELD_MOVE_EFFECT_STATE_ANIMATE;
}

void CutEffect_Destroy(Field3dObjectTask *task, FieldSystem *fieldSystem, FieldMoveEffect *effect) {
    FieldMoveEffectModel_Unload(&effect->model, &effect->allocator);
}

void CutEffect_Update(Field3dObjectTask *task, FieldSystem *fieldSystem, FieldMoveEffect *effect) {
    switch (effect->state) {
    case FIELD_MOVE_EFFECT_STATE_ANIMATE:
        if (FieldMoveEffectModel_AnimateOnce(&effect->model) == TRUE) {
            Field3dObject_SetActiveFlag(&effect->model.object, FALSE);
            effect->state++;
        }
        break;
    case FIELD_MOVE_EFFECT_STATE_DONE:
        break;
    }
}

void CutEffect_Render(Field3dObjectTask *task, FieldSystem *fieldSystem, FieldMoveEffect *effect) {
    FieldMoveEffectModel_Draw(&effect->model);
}

Field3dObjectTask *CutEffect_CreateAtPlayer(FieldSystem *fieldSystem) {
    return Field3dObjectTaskManager_CreateTask(fieldSystem->mapSystems->field3dObjectTaskManager, &gCutEffectAtPlayerTemplate);
}

Field3dObjectTask *CutEffect_CreateAtFollower(FieldSystem *fieldSystem) {
    return Field3dObjectTaskManager_CreateTask(fieldSystem->mapSystems->field3dObjectTaskManager, &gCutEffectAtFollowerTemplate);
}

void CutEffect_Delete(Field3dObjectTask *task) {
    Field3dObjectTask_Delete(task);
}

BOOL CutEffect_IsFinished(Field3dObjectTask *task) {
    FieldMoveEffect *effect = Field3dObjectTask_GetData(task);
    return effect->state == FIELD_MOVE_EFFECT_STATE_DONE;
}

void RockSmashEffect_InitAtPlayer(Field3dObjectTask *task, FieldSystem *fieldSystem, FieldMoveEffect *effect) {
    memset(effect, 0, sizeof(FieldMoveEffect));
    HeapExp_FndInitAllocator(&effect->allocator, HEAP_ID_FIELD1, 32);
    FieldMoveEffectModel_Load(&effect->model, 8, 4, 4, &effect->allocator);
    Field3dObject_SetPosInFrontOfPlayer(fieldSystem->playerAvatar, &effect->model.object);
    PlaySE(SEQ_SE_DP_FW088);
    effect->state = FIELD_MOVE_EFFECT_STATE_ANIMATE;
}

void RockSmashEffect_InitAtFollower(Field3dObjectTask *task, FieldSystem *fieldSystem, FieldMoveEffect *effect) {
    memset(effect, 0, sizeof(FieldMoveEffect));
    HeapExp_FndInitAllocator(&effect->allocator, HEAP_ID_FIELD1, 32);
    FieldMoveEffectModel_Load(&effect->model, 8, 4, 4, &effect->allocator);
    Field3dObject_SetPosInFrontOfMapObject(fieldSystem->mapObjectManager, obj_partner_poke, &effect->model.object);
    PlaySE(SEQ_SE_DP_FW088);
    effect->state = FIELD_MOVE_EFFECT_STATE_ANIMATE;
}

void RockSmashEffect_Destroy(Field3dObjectTask *task, FieldSystem *fieldSystem, FieldMoveEffect *effect) {
    FieldMoveEffectModel_Unload(&effect->model, &effect->allocator);
}

void RockSmashEffect_Update(Field3dObjectTask *task, FieldSystem *fieldSystem, FieldMoveEffect *effect) {
    switch (effect->state) {
    case FIELD_MOVE_EFFECT_STATE_ANIMATE:
        if (FieldMoveEffectModel_AnimateOnce(&effect->model) == TRUE) {
            Field3dObject_SetActiveFlag(&effect->model.object, FALSE);
            effect->state++;
        }
        break;
    case FIELD_MOVE_EFFECT_STATE_DONE:
        break;
    }
}

void RockSmashEffect_Render(Field3dObjectTask *task, FieldSystem *fieldSystem, FieldMoveEffect *effect) {
    FieldMoveEffectModel_Draw(&effect->model);
}

Field3dObjectTask *RockSmashEffect_CreateAtPlayer(FieldSystem *fieldSystem) {
    return Field3dObjectTaskManager_CreateTask(fieldSystem->mapSystems->field3dObjectTaskManager, &gRockSmashEffectAtPlayerTemplate);
}

Field3dObjectTask *RockSmashEffect_CreateAtFollower(FieldSystem *fieldSystem) {
    return Field3dObjectTaskManager_CreateTask(fieldSystem->mapSystems->field3dObjectTaskManager, &gRockSmashEffectAtFollowerTemplate);
}

void RockSmashEffect_Delete(Field3dObjectTask *task) {
    Field3dObjectTask_Delete(task);
}

BOOL RockSmashEffect_IsFinished(Field3dObjectTask *task) {
    FieldMoveEffect *effect = Field3dObjectTask_GetData(task);
    return effect->state == FIELD_MOVE_EFFECT_STATE_DONE;
}

void WaterfallEffect_Init(Field3dObjectTask *task, FieldSystem *fieldSystem, WaterfallEffect *effect) {
    memset(effect, 0, sizeof(WaterfallEffect));
    HeapExp_FndInitAllocator(&effect->allocator, HEAP_ID_FIELD1, 32);
    effect->state = WATERFALL_EFFECT_STATE_LOAD;
}

void WaterfallEffect_Destroy(Field3dObjectTask *task, FieldSystem *fieldSystem, WaterfallEffect *effect) {
    int i;
    for (i = 0; i < 2; i++) {
        FieldMoveEffectModel_Unload(&effect->models[i], &effect->allocator);
    }
}

void WaterfallEffect_Update(Field3dObjectTask *task, FieldSystem *fieldSystem, WaterfallEffect *effect) {
    FileIdPair modelFileIds;
    FileIdPair animFileIds;
    u8 slot;
    BOOL finished;

    switch (effect->state) {
    case WATERFALL_EFFECT_STATE_LOAD: // one model per frame
        modelFileIds = gRideEffectFileIds.waterfallModels;
        animFileIds = gRideEffectFileIds.waterfallAnims;
        slot = effect->numLoaded;
        FieldMoveEffectModel_Load(&effect->models[slot], modelFileIds.ids[slot], animFileIds.ids[slot], 2, &effect->allocator);
        Field3dObject_SetPosAtPlayer(fieldSystem->playerAvatar, &effect->models[slot].object);
        effect->numLoaded++;
        if (effect->numLoaded >= 2) {
            effect->state++;
        }
        break;
    case WATERFALL_EFFECT_STATE_START:
        Field3dObject_SetActiveFlag(&effect->models[1].object, FALSE);
        PlaySE(SEQ_SE_DP_FW463);
        effect->state++;
        // fallthrough
    case WATERFALL_EFFECT_STATE_SPLASH:
        finished = FieldMoveEffectModel_AnimateOnce(&effect->models[0]);
        Field3dObject_SetPosAtPlayer(fieldSystem->playerAvatar, &effect->models[0].object);
        if (finished == TRUE) {
            Field3dObject_SetActiveFlag(&effect->models[1].object, TRUE);
            Field3dObject_SetActiveFlag(&effect->models[0].object, FALSE);
            Field3dObject_SetPosAtPlayer(fieldSystem->playerAvatar, &effect->models[1].object);
            effect->state++;
        }
        break;
    case WATERFALL_EFFECT_STATE_LOOP:
        FieldMoveEffectModel_AnimateLoop(&effect->models[1]);
        Field3dObject_SetPosAtPlayer(fieldSystem->playerAvatar, &effect->models[1].object);
        break;
    }
}

void WaterfallEffect_Render(Field3dObjectTask *task, FieldSystem *fieldSystem, WaterfallEffect *effect) {
    int i;
    for (i = 0; i < 2; i++) {
        FieldMoveEffectModel_Draw(&effect->models[i]);
    }
}

Field3dObjectTask *WaterfallEffect_Create(FieldSystem *fieldSystem) {
    return Field3dObjectTaskManager_CreateTask(fieldSystem->mapSystems->field3dObjectTaskManager, &gWaterfallEffectTemplate);
}

void WaterfallEffect_Delete(Field3dObjectTask *task) {
    Field3dObjectTask_Delete(task);
}

void WhirlpoolEffect_Init(Field3dObjectTask *task, FieldSystem *fieldSystem, WhirlpoolEffect *effect) {
    struct {
        u8 animFileId;
        u8 modelFileId;
        u16 angles[4];
    } files;
    u32 direction;

    files.modelFileId = gRideEffectFileIds.whirlpool.ids[0];
    files.animFileId = gRideEffectFileIds.whirlpool.ids[1];
    files.angles[0] = gRideEffectFileIds.whirlpoolAngles[0];
    files.angles[1] = gRideEffectFileIds.whirlpoolAngles[1];
    files.angles[2] = gRideEffectFileIds.whirlpoolAngles[2];
    files.angles[3] = gRideEffectFileIds.whirlpoolAngles[3];
    memset(effect, 0, sizeof(WhirlpoolEffect));
    HeapExp_FndInitAllocator(&effect->allocator, HEAP_ID_FIELD1, 32);
    direction = PlayerAvatar_GetFacingDirection(fieldSystem->playerAvatar);
    FieldMoveEffectModel_Load(&effect->model, files.modelFileId, files.animFileId, 1, &effect->allocator);
    FieldMoveEffectModel_AnimateLoop(&effect->model);
    Field3dObject_SetPosAtPlayer(fieldSystem->playerAvatar, &effect->model.object);
    Field3dObject_SetXRotation(&effect->model.object, GF_DegreeToSinCosIdxNoWrap(files.angles[direction]), 1);
    PlaySE(SEQ_SE_DP_FW463);
    effect->state = 0;
}

void WhirlpoolEffect_Destroy(Field3dObjectTask *task, FieldSystem *fieldSystem, WhirlpoolEffect *effect) {
    FieldMoveEffectModel_Unload(&effect->model, &effect->allocator);
}

void WhirlpoolEffect_Update(Field3dObjectTask *task, FieldSystem *fieldSystem, WhirlpoolEffect *effect) {
    FieldMoveEffectModel_AnimateLoop(&effect->model);
    Field3dObject_SetPosAtPlayer(fieldSystem->playerAvatar, &effect->model.object);
}

void WhirlpoolEffect_Render(Field3dObjectTask *task, FieldSystem *fieldSystem, WhirlpoolEffect *effect) {
    FieldMoveEffectModel_Draw(&effect->model);
}

Field3dObjectTask *WhirlpoolEffect_Create(FieldSystem *fieldSystem) {
    return Field3dObjectTaskManager_CreateTask(fieldSystem->mapSystems->field3dObjectTaskManager, &gWhirlpoolEffectTemplate);
}

void WhirlpoolEffect_Delete(Field3dObjectTask *task) {
    Field3dObjectTask_Delete(task);
}

static void RockClimbDebris_SpawnPiece(RockDebrisPiece *piece, PlayerAvatar *playerAvatar, fx32 offsetY, fx32 offsetZ) {
    VecFx32 pos;
    int i;

    GF_ASSERT(!piece->active);
    PlayerAvatar_CopyPositionVector(playerAvatar, &pos);
    Field3dObject_SetPosEx(&piece->object, pos.x, pos.y + offsetY, pos.z + offsetZ);
    piece->active = TRUE;
    for (i = 0; i < 4; i++) {
        Field3dModelAnimation_FrameSet(&piece->animations[i], 0);
    }
    Field3dObject_SetActiveFlag(&piece->object, TRUE);
    PlaySE(SEQ_SE_DP_UG_023);
}

static void RockClimbDebris_UpdatePiece(RockDebrisPiece *piece) {
    int i;
    BOOL finished;

    if (piece->active) {
        finished = TRUE;
        for (i = 0; i < 4; i++) {
            finished &= Field3dModelAnimation_FrameAdvanceAndCheck(&piece->animations[i], FX32_ONE);
        }
        if (finished == TRUE) {
            piece->active = FALSE;
            Field3dObject_SetActiveFlag(&piece->object, FALSE);
        }
    }
}

static void RockClimbDebris_InitPiece(RockDebrisPiece *piece, Field3dModel *model, NNSFndAllocator *allocator, void **animData) {
    int i;

    memset(piece, 0, sizeof(RockDebrisPiece));
    Field3dObject_InitFromModel(&piece->object, model);
    for (i = 0; i < 4; i++) {
        Field3dModelAnimation_LoadFromData(&piece->animations[i], model, animData[i], allocator);
        Field3dObject_AddAnimation(&piece->object, &piece->animations[i]);
    }
    Field3dObject_SetActiveFlag(&piece->object, FALSE);
}

static void RockClimbDebris_FreePiece(RockDebrisPiece *piece, NNSFndAllocator *allocator) {
    int i;

    for (i = 0; i < 4; i++) {
        Field3dModelAnimation_Unload(&piece->animations[i], allocator);
    }
    memset(piece, 0, sizeof(RockDebrisPiece));
}

// The debris appears below the player while climbing up, behind the player otherwise.
static void RockClimbDebris_UpdateOffsets(RockClimbDebris *debris) {
    VecFx32 pos;

    PlayerAvatar_CopyPositionVector(debris->fieldSystem->playerAvatar, &pos);
    if (pos.x - debris->lastPlayerPos.x == 0 && pos.y < debris->lastPlayerPos.y && pos.z > debris->lastPlayerPos.z) {
        debris->offsetY = 0;
        debris->offsetZ = 2 * MAP_TILE_SIZE;
    } else {
        debris->offsetY = 2 * MAP_TILE_SIZE;
        debris->offsetZ = MAP_TILE_SIZE;
    }
    debris->lastPlayerPos = pos;
}

static void RockClimbDebris_Spawn(RockClimbDebris *debris) {
    int i;

    RockClimbDebris_UpdateOffsets(debris);
    for (i = 0; i < NUM_ROCK_CLIMB_DEBRIS_PIECES; i++) {
        if (!debris->pieces[i].active) {
            RockClimbDebris_SpawnPiece(&debris->pieces[i], debris->fieldSystem->playerAvatar, debris->offsetY, debris->offsetZ);
            return;
        }
    }
    GF_ASSERT(FALSE);
}

static void RockClimbDebris_UpdatePieces(RockClimbDebris *debris) {
    s32 i;
    for (i = 0; i < NUM_ROCK_CLIMB_DEBRIS_PIECES; i++) {
        RockClimbDebris_UpdatePiece(&debris->pieces[i]);
    }
}

void RockClimbDebris_Init(Field3dObjectTask *task, FieldSystem *fieldSystem, RockClimbDebris *debris) {
    int i;

    memset(debris, 0, sizeof(RockClimbDebris));
    HeapExp_FndInitAllocator(&debris->allocator, HEAP_ID_FIELD1, 32);
    Field3dModel_LoadFromFilesystem(&debris->model, NARC_a_1_3_4, 8, HEAP_ID_FIELD1);
    for (i = 0; i < 4; i++) {
        debris->animData[i] = GfGfxLoader_LoadFromNarc(NARC_a_1_3_4, i + 4, FALSE, HEAP_ID_FIELD1, FALSE);
    }
    for (i = 0; i < NUM_ROCK_CLIMB_DEBRIS_PIECES; i++) {
        RockClimbDebris_InitPiece(&debris->pieces[i], &debris->model, &debris->allocator, debris->animData);
    }
    debris->fieldSystem = fieldSystem;
    PlayerAvatar_CopyPositionVector(debris->fieldSystem->playerAvatar, &debris->lastPlayerPos);
    debris->paused = FALSE;
}

void RockClimbDebris_Destroy(Field3dObjectTask *task, FieldSystem *fieldSystem, RockClimbDebris *debris) {
    int i;

    for (i = 0; i < NUM_ROCK_CLIMB_DEBRIS_PIECES; i++) {
        RockClimbDebris_FreePiece(&debris->pieces[i], &debris->allocator);
    }
    Field3dModel_Unload(&debris->model);
    for (i = 0; i < 4; i++) {
        Heap_Free(debris->animData[i]);
    }
}

// Spawns a debris cloud every fifth frame.
void RockClimbDebris_Update(Field3dObjectTask *task, FieldSystem *fieldSystem, RockClimbDebris *debris) {
    if (!debris->paused) {
        debris->spawnTimer = debris->spawnTimer - 1;
        if (debris->spawnTimer < 0) {
            debris->spawnTimer = 4;
            RockClimbDebris_Spawn(debris);
        }
        RockClimbDebris_UpdatePieces(debris);
    }
}

void RockClimbDebris_Render(Field3dObjectTask *task, FieldSystem *fieldSystem, RockClimbDebris *debris) {
    s32 i;
    for (i = 0; i < NUM_ROCK_CLIMB_DEBRIS_PIECES; i++) {
        Field3dObject_Draw(&debris->pieces[i].object);
    }
}

Field3dObjectTask *RockClimbDebris_Create(FieldSystem *fieldSystem) {
    return Field3dObjectTaskManager_CreateTask(fieldSystem->mapSystems->field3dObjectTaskManager, &gRockClimbDebrisTemplate);
}

void RockClimbDebris_Delete(Field3dObjectTask *task) {
    Field3dObjectTask_Delete(task);
}

// Headbutt: the camera shakes twice towards the facing direction while the animation plays.
void HeadbuttEffect_InitAtPlayer(Field3dObjectTask *task, FieldSystem *fieldSystem, HeadbuttEffect *effect) {
    VecFx32 offset;
    fx32 *coords;

    memset(effect, 0, sizeof(HeadbuttEffect));
    HeapExp_FndInitAllocator(&effect->allocator, HEAP_ID_FIELD1, 32);
    FieldMoveEffectModel_Load(&effect->model, 19, 17, 2, &effect->allocator);
    Field3dObject_SetPosInFrontOfPlayer(fieldSystem->playerAvatar, &effect->model.object);
    effect->cameraTarget = Camera_GetLookAtCamTarget(fieldSystem->camera);
    effect->cameraPos = Camera_GetLookAtCamPos(fieldSystem->camera);
    coords = (fx32 *)&offset;
    coords[0] = 0;
    coords[1] = 0;
    coords[2] = 0;
    switch (PlayerAvatar_GetFacingDirection(fieldSystem->playerAvatar)) {
    case DIR_NORTH:
        offset.z -= MAP_TILE_SIZE / 2;
        break;
    case DIR_SOUTH:
        offset.z += MAP_TILE_SIZE / 2;
        break;
    case DIR_WEST:
        offset.x -= MAP_TILE_SIZE / 2;
        break;
    case DIR_EAST:
        offset.x += MAP_TILE_SIZE / 2;
        break;
    }
    effect->shakeOffset = offset;
    PlaySE(SEQ_SE_GS_ZUTUKI);
    effect->state = FIELD_MOVE_EFFECT_STATE_ANIMATE;
}

void HeadbuttEffect_InitAtFollower(Field3dObjectTask *task, FieldSystem *fieldSystem, HeadbuttEffect *effect) {
    VecFx32 offset;
    fx32 *coords;

    memset(effect, 0, sizeof(HeadbuttEffect));
    HeapExp_FndInitAllocator(&effect->allocator, HEAP_ID_FIELD1, 32);
    FieldMoveEffectModel_Load(&effect->model, 19, 17, 2, &effect->allocator);
    Field3dObject_SetPosInFrontOfMapObject(fieldSystem->mapObjectManager, obj_partner_poke, &effect->model.object);
    effect->cameraTarget = Camera_GetLookAtCamTarget(fieldSystem->camera);
    effect->cameraPos = Camera_GetLookAtCamPos(fieldSystem->camera);
    coords = (fx32 *)&offset;
    coords[0] = 0;
    coords[1] = 0;
    coords[2] = 0;
    switch (PlayerAvatar_GetFacingDirection(fieldSystem->playerAvatar)) {
    case DIR_NORTH:
        offset.z -= MAP_TILE_SIZE / 2;
        break;
    case DIR_SOUTH:
        offset.z += MAP_TILE_SIZE / 2;
        break;
    case DIR_WEST:
        offset.x -= MAP_TILE_SIZE / 2;
        break;
    case DIR_EAST:
        offset.x += MAP_TILE_SIZE / 2;
        break;
    }
    effect->shakeOffset = offset;
    PlaySE(SEQ_SE_GS_ZUTUKI);
    effect->state = FIELD_MOVE_EFFECT_STATE_ANIMATE;
}

void HeadbuttEffect_Destroy(Field3dObjectTask *task, FieldSystem *fieldSystem, HeadbuttEffect *effect) {
    FieldMoveEffectModel_Unload(&effect->model, &effect->allocator);
}

void HeadbuttEffect_Update(Field3dObjectTask *task, FieldSystem *fieldSystem, HeadbuttEffect *effect) {
    switch (effect->state) {
    case FIELD_MOVE_EFFECT_STATE_ANIMATE:
        if (effect->numShakes < 2) {
            if (effect->shakeTimer <= 0) {
                effect->shakeTimer = 1;
                effect->shaken = (effect->shaken + 1) % 2;
                if (effect->shaken) {
                    Camera_OffsetLookAtPosAndTarget(&effect->shakeOffset, fieldSystem->camera);
                } else {
                    Camera_SetLookAtCamTarget(&effect->cameraTarget, fieldSystem->camera);
                    Camera_SetLookAtCamPos(&effect->cameraPos, fieldSystem->camera);
                    effect->numShakes++;
                }
            } else {
                effect->shakeTimer--;
            }
        }
        if (FieldMoveEffectModel_AnimateOnce(&effect->model) == TRUE) {
            Field3dObject_SetActiveFlag(&effect->model.object, FALSE);
            effect->state++;
        }
        break;
    case FIELD_MOVE_EFFECT_STATE_DONE:
        Camera_SetLookAtCamTarget(&effect->cameraTarget, fieldSystem->camera);
        Camera_SetLookAtCamPos(&effect->cameraPos, fieldSystem->camera);
        break;
    }
}

void HeadbuttEffect_Render(Field3dObjectTask *task, FieldSystem *fieldSystem, HeadbuttEffect *effect) {
    FieldMoveEffectModel_Draw(&effect->model);
}

Field3dObjectTask *HeadbuttEffect_CreateAtPlayer(FieldSystem *fieldSystem) {
    return Field3dObjectTaskManager_CreateTask(fieldSystem->mapSystems->field3dObjectTaskManager, &gHeadbuttEffectAtPlayerTemplate);
}

Field3dObjectTask *HeadbuttEffect_CreateAtFollower(FieldSystem *fieldSystem) {
    return Field3dObjectTaskManager_CreateTask(fieldSystem->mapSystems->field3dObjectTaskManager, &gHeadbuttEffectAtFollowerTemplate);
}

void HeadbuttEffect_Delete(Field3dObjectTask *task) {
    Field3dObjectTask_Delete(task);
}

BOOL HeadbuttEffect_IsFinished(Field3dObjectTask *task) {
    HeadbuttEffect *effect = Field3dObjectTask_GetData(task);
    return effect->state == FIELD_MOVE_EFFECT_STATE_DONE;
}

void RockDebris_Init(Field3dObjectTask *task, FieldSystem *fieldSystem, RockDebris *debris) {
    int i;
    int j;

    memset(debris, 0, sizeof(RockDebris));
    HeapExp_FndInitAllocator(&debris->allocator, HEAP_ID_FIELD1, 32);
    Field3dModel_LoadFromFilesystem(&debris->model, NARC_a_1_3_4, 8, HEAP_ID_FIELD1);
    for (i = 0; i < 4; i++) {
        debris->animData[i] = GfGfxLoader_LoadFromNarc(NARC_a_1_3_4, i + 4, FALSE, HEAP_ID_FIELD1, FALSE);
    }
    for (j = 0; j < NUM_ROCK_DEBRIS_PIECES; j++) {
        RockDebris_InitPiece(&debris->pieces[j], &debris->model, &debris->allocator, debris->animData);
    }
    debris->fieldSystem = fieldSystem;
    debris->paused = FALSE;
}

// Also spawns debris at the following Pokemon.
void RockDebris_InitWithFollower(Field3dObjectTask *task, FieldSystem *fieldSystem, RockDebris *debris) {
    RockDebris_Init(task, fieldSystem, debris);
    debris->withFollower = TRUE;
}

void RockDebris_Destroy(Field3dObjectTask *task, FieldSystem *fieldSystem, RockDebris *debris) {
    int i;

    for (i = 0; i < NUM_ROCK_DEBRIS_PIECES; i++) {
        RockDebris_FreePiece(&debris->pieces[i], &debris->allocator);
    }
    Field3dModel_Unload(&debris->model);
    for (i = 0; i < 4; i++) {
        Heap_Free(debris->animData[i]);
    }
}

// Spawns a debris cloud every fifth frame.
void RockDebris_Update(Field3dObjectTask *task, FieldSystem *fieldSystem, RockDebris *debris) {
    if (!debris->paused) {
        debris->spawnTimer = debris->spawnTimer - 1;
        if (debris->spawnTimer < 0) {
            debris->spawnTimer = 4;
            RockDebris_Spawn(debris);
        }
        RockDebris_UpdatePieces(debris);
    }
}

void RockDebris_Render(Field3dObjectTask *task, FieldSystem *fieldSystem, RockDebris *debris) {
    int i;
    for (i = 0; i < NUM_ROCK_DEBRIS_PIECES; i++) {
        Field3dObject_Draw(&debris->pieces[i].object);
    }
}

Field3dObjectTask *RockDebris_Create(FieldSystem *fieldSystem) {
    return Field3dObjectTaskManager_CreateTask(fieldSystem->mapSystems->field3dObjectTaskManager, &gRockDebrisTemplate);
}

Field3dObjectTask *RockDebris_CreateWithFollower(FieldSystem *fieldSystem) {
    return Field3dObjectTaskManager_CreateTask(fieldSystem->mapSystems->field3dObjectTaskManager, &gRockDebrisWithFollowerTemplate);
}

void RockDebris_Delete(Field3dObjectTask *task) {
    Field3dObjectTask_Delete(task);
}

static void RockDebris_SpawnPiece(RockDebrisPiece *piece, const VecFx32 *position, fx32 offsetY, fx32 offsetZ) {
    int i;

    GF_ASSERT(!piece->active);
    Field3dObject_SetPosEx(&piece->object, position->x, position->y + offsetY, position->z + offsetZ);
    piece->active = TRUE;
    for (i = 0; i < 4; i++) {
        Field3dModelAnimation_FrameSet(&piece->animations[i], 0);
    }
    Field3dObject_SetActiveFlag(&piece->object, TRUE);
    PlaySE(SEQ_SE_DP_UG_023);
}

static void RockDebris_UpdatePiece(RockDebrisPiece *piece) {
    int i;
    BOOL finished;

    if (piece->active) {
        finished = TRUE;
        for (i = 0; i < 4; i++) {
            finished &= Field3dModelAnimation_FrameAdvanceAndCheck(&piece->animations[i], FX32_ONE);
        }
        if (finished == TRUE) {
            piece->active = FALSE;
            Field3dObject_SetActiveFlag(&piece->object, FALSE);
        }
    }
}

static void RockDebris_InitPiece(RockDebrisPiece *piece, Field3dModel *model, NNSFndAllocator *allocator, void **animData) {
    int i;

    memset(piece, 0, sizeof(RockDebrisPiece));
    Field3dObject_InitFromModel(&piece->object, model);
    for (i = 0; i < 4; i++) {
        Field3dModelAnimation_LoadFromData(&piece->animations[i], model, animData[i], allocator);
        Field3dObject_AddAnimation(&piece->object, &piece->animations[i]);
    }
    Field3dObject_SetActiveFlag(&piece->object, FALSE);
}

static void RockDebris_FreePiece(RockDebrisPiece *piece, NNSFndAllocator *allocator) {
    int i;

    for (i = 0; i < 4; i++) {
        Field3dModelAnimation_Unload(&piece->animations[i], allocator);
    }
    memset(piece, 0, sizeof(RockDebrisPiece));
}

static void RockDebris_Spawn(RockDebris *debris) {
    VecFx32 playerPos;
    VecFx32 followerPos;
    int i;

    debris->offsetY = FX32_ONE;
    debris->offsetZ = 3 * FX32_ONE;
    PlayerAvatar_CopyPositionVector(debris->fieldSystem->playerAvatar, &playerPos);
    if (debris->withFollower) {
        MapObject_CopyPositionVector(FollowMon_GetMapObject(debris->fieldSystem), &followerPos);
        for (i = 0; i < NUM_ROCK_DEBRIS_PIECES; i++) {
            if (!debris->pieces[i].active) {
                RockDebris_SpawnPiece(&debris->pieces[i], &playerPos, debris->offsetY, debris->offsetZ);
                break;
            }
        }
        GF_ASSERT(i != NUM_ROCK_DEBRIS_PIECES);
        for (i = 0; i < NUM_ROCK_DEBRIS_PIECES; i++) {
            if (!debris->pieces[i].active) {
                RockDebris_SpawnPiece(&debris->pieces[i], &followerPos, debris->offsetY, debris->offsetZ);
                return;
            }
        }
    } else {
        for (i = 0; i < NUM_ROCK_DEBRIS_PIECES; i++) {
            if (!debris->pieces[i].active) {
                RockDebris_SpawnPiece(&debris->pieces[i], &playerPos, debris->offsetY, debris->offsetZ);
                return;
            }
        }
    }
    GF_ASSERT(FALSE);
}

static void RockDebris_UpdatePieces(RockDebris *debris) {
    int i;
    for (i = 0; i < NUM_ROCK_DEBRIS_PIECES; i++) {
        RockDebris_UpdatePiece(&debris->pieces[i]);
    }
}

// Script field move effects, by kind: 0 Cut at the player, 1 Rock Smash at the player, 2 Rock Smash at the
// following Pokemon, 3 Cut at the following Pokemon, 4 Headbutt at the player, 5 Headbutt at the following Pokemon.
static void Task_ScriptFieldMoveEffect(SysTask *task, void *data) {
    ScriptFieldMoveEffectTask *env = data;

    switch (env->state) {
    case SCRIPT_FIELD_MOVE_EFFECT_WAIT:
        if (gScriptFieldMoveEffectIsFinishedFuncs[env->kind](env->effect) == TRUE) {
            env->state++;
        }
        break;
    case SCRIPT_FIELD_MOVE_EFFECT_END:
        gScriptFieldMoveEffectDeleteFuncs[env->kind](env->effect);
        *env->done = TRUE;
        Heap_Free(env);
        SysTask_Destroy(task);
        break;
    }
}

void ScriptFieldMoveEffect_Start(FieldSystem *fieldSystem, u16 *done, int kind, enum HeapID heapID) {
    ScriptFieldMoveEffectTask *env = Heap_Alloc(heapID, sizeof(ScriptFieldMoveEffectTask));
    memset(env, 0, sizeof(ScriptFieldMoveEffectTask));
    env->effect = gScriptFieldMoveEffectCreateFuncs[kind](fieldSystem);
    env->done = done;
    env->kind = kind;
    *done = FALSE;
    SysTask_CreateOnMainQueue(Task_ScriptFieldMoveEffect, env, 0);
}
#if defined(SDK_PORT)

/* Native templates and callback tables from asm/field_move_effects_data.s. The
 * DS data sizes (0xF0 and so on) belong to its 32-bit work structs, so each
 * template allocates sizeof its native work type instead; the callbacks get the
 * manager's void * data parameter through adapters. */
#define NATIVE_FIELD3D_TASK(function, Type) \
    static void Native_##function(Field3dObjectTask *task, FieldSystem *fieldSystem, void *data) { function(task, fieldSystem, (Type *)data); }
NATIVE_FIELD3D_TASK(RockSmashEffect_InitAtPlayer, FieldMoveEffect)
NATIVE_FIELD3D_TASK(RockSmashEffect_Destroy, FieldMoveEffect)
NATIVE_FIELD3D_TASK(RockSmashEffect_Update, FieldMoveEffect)
NATIVE_FIELD3D_TASK(RockSmashEffect_Render, FieldMoveEffect)
NATIVE_FIELD3D_TASK(WhirlpoolEffect_Init, WhirlpoolEffect)
NATIVE_FIELD3D_TASK(WhirlpoolEffect_Destroy, WhirlpoolEffect)
NATIVE_FIELD3D_TASK(WhirlpoolEffect_Update, WhirlpoolEffect)
NATIVE_FIELD3D_TASK(WhirlpoolEffect_Render, WhirlpoolEffect)
NATIVE_FIELD3D_TASK(HeadbuttEffect_InitAtFollower, HeadbuttEffect)
NATIVE_FIELD3D_TASK(HeadbuttEffect_Destroy, HeadbuttEffect)
NATIVE_FIELD3D_TASK(HeadbuttEffect_Update, HeadbuttEffect)
NATIVE_FIELD3D_TASK(HeadbuttEffect_Render, HeadbuttEffect)
NATIVE_FIELD3D_TASK(WaterfallEffect_Init, WaterfallEffect)
NATIVE_FIELD3D_TASK(WaterfallEffect_Destroy, WaterfallEffect)
NATIVE_FIELD3D_TASK(WaterfallEffect_Update, WaterfallEffect)
NATIVE_FIELD3D_TASK(WaterfallEffect_Render, WaterfallEffect)
NATIVE_FIELD3D_TASK(RockClimbDebris_Init, RockClimbDebris)
NATIVE_FIELD3D_TASK(RockClimbDebris_Destroy, RockClimbDebris)
NATIVE_FIELD3D_TASK(RockClimbDebris_Update, RockClimbDebris)
NATIVE_FIELD3D_TASK(RockClimbDebris_Render, RockClimbDebris)
NATIVE_FIELD3D_TASK(CutEffect_InitAtPlayer, FieldMoveEffect)
NATIVE_FIELD3D_TASK(CutEffect_Destroy, FieldMoveEffect)
NATIVE_FIELD3D_TASK(CutEffect_Update, FieldMoveEffect)
NATIVE_FIELD3D_TASK(CutEffect_Render, FieldMoveEffect)
NATIVE_FIELD3D_TASK(RockSmashEffect_InitAtFollower, FieldMoveEffect)
NATIVE_FIELD3D_TASK(HeadbuttEffect_InitAtPlayer, HeadbuttEffect)
NATIVE_FIELD3D_TASK(CutEffect_InitAtFollower, FieldMoveEffect)
NATIVE_FIELD3D_TASK(RockDebris_Init, RockDebris)
NATIVE_FIELD3D_TASK(RockDebris_Destroy, RockDebris)
NATIVE_FIELD3D_TASK(RockDebris_Update, RockDebris)
NATIVE_FIELD3D_TASK(RockDebris_Render, RockDebris)
NATIVE_FIELD3D_TASK(RockDebris_InitWithFollower, RockDebris)
#undef NATIVE_FIELD3D_TASK
_Static_assert(sizeof(FieldMoveEffect) >= 0xF0, "gRockSmashEffectAtPlayerTemplate work size");
_Static_assert(sizeof(WhirlpoolEffect) >= 0xF0, "gWhirlpoolEffectTemplate work size");
_Static_assert(sizeof(HeadbuttEffect) >= 0x114, "gHeadbuttEffectAtFollowerTemplate work size");
_Static_assert(sizeof(WaterfallEffect) >= 0x1CC, "gWaterfallEffectTemplate work size");
_Static_assert(sizeof(RockClimbDebris) >= 0xD10, "gRockClimbDebrisTemplate work size");
_Static_assert(sizeof(FieldMoveEffect) >= 0xF0, "gCutEffectAtPlayerTemplate work size");
_Static_assert(sizeof(FieldMoveEffect) >= 0xF0, "gRockSmashEffectAtFollowerTemplate work size");
_Static_assert(sizeof(HeadbuttEffect) >= 0x114, "gHeadbuttEffectAtPlayerTemplate work size");
_Static_assert(sizeof(FieldMoveEffect) >= 0xF0, "gCutEffectAtFollowerTemplate work size");
_Static_assert(sizeof(RockDebris) >= 0xE9C, "gRockDebrisTemplate work size");
_Static_assert(sizeof(RockDebris) >= 0xE9C, "gRockDebrisWithFollowerTemplate work size");
const Field3dObjectTaskTemplate gRockSmashEffectAtPlayerTemplate = { 0x400, sizeof(FieldMoveEffect), Native_RockSmashEffect_InitAtPlayer, Native_RockSmashEffect_Destroy, Native_RockSmashEffect_Update, Native_RockSmashEffect_Render };
const Field3dObjectTaskTemplate gWhirlpoolEffectTemplate = { 0x400, sizeof(WhirlpoolEffect), Native_WhirlpoolEffect_Init, Native_WhirlpoolEffect_Destroy, Native_WhirlpoolEffect_Update, Native_WhirlpoolEffect_Render };
const Field3dObjectTaskTemplate gHeadbuttEffectAtFollowerTemplate = { 0x400, sizeof(HeadbuttEffect), Native_HeadbuttEffect_InitAtFollower, Native_HeadbuttEffect_Destroy, Native_HeadbuttEffect_Update, Native_HeadbuttEffect_Render };
const Field3dObjectTaskTemplate gWaterfallEffectTemplate = { 0x400, sizeof(WaterfallEffect), Native_WaterfallEffect_Init, Native_WaterfallEffect_Destroy, Native_WaterfallEffect_Update, Native_WaterfallEffect_Render };
const Field3dObjectTaskTemplate gRockClimbDebrisTemplate = { 0x400, sizeof(RockClimbDebris), Native_RockClimbDebris_Init, Native_RockClimbDebris_Destroy, Native_RockClimbDebris_Update, Native_RockClimbDebris_Render };
const Field3dObjectTaskTemplate gCutEffectAtPlayerTemplate = { 0x400, sizeof(FieldMoveEffect), Native_CutEffect_InitAtPlayer, Native_CutEffect_Destroy, Native_CutEffect_Update, Native_CutEffect_Render };
const Field3dObjectTaskTemplate gRockSmashEffectAtFollowerTemplate = { 0x400, sizeof(FieldMoveEffect), Native_RockSmashEffect_InitAtFollower, Native_RockSmashEffect_Destroy, Native_RockSmashEffect_Update, Native_RockSmashEffect_Render };
const Field3dObjectTaskTemplate gHeadbuttEffectAtPlayerTemplate = { 0x400, sizeof(HeadbuttEffect), Native_HeadbuttEffect_InitAtPlayer, Native_HeadbuttEffect_Destroy, Native_HeadbuttEffect_Update, Native_HeadbuttEffect_Render };
const Field3dObjectTaskTemplate gCutEffectAtFollowerTemplate = { 0x400, sizeof(FieldMoveEffect), Native_CutEffect_InitAtFollower, Native_CutEffect_Destroy, Native_CutEffect_Update, Native_CutEffect_Render };
const Field3dObjectTaskTemplate gRockDebrisTemplate = { 0x400, sizeof(RockDebris), Native_RockDebris_Init, Native_RockDebris_Destroy, Native_RockDebris_Update, Native_RockDebris_Render };
const Field3dObjectTaskTemplate gRockDebrisWithFollowerTemplate = { 0x400, sizeof(RockDebris), Native_RockDebris_InitWithFollower, Native_RockDebris_Destroy, Native_RockDebris_Update, Native_RockDebris_Render };
Field3dObjectTask *(*const gScriptFieldMoveEffectCreateFuncs[])(FieldSystem *fieldSystem) = {
    CutEffect_CreateAtPlayer,
    RockSmashEffect_CreateAtPlayer,
    RockSmashEffect_CreateAtFollower,
    CutEffect_CreateAtFollower,
    HeadbuttEffect_CreateAtPlayer,
    HeadbuttEffect_CreateAtFollower,
};
void (*const gScriptFieldMoveEffectDeleteFuncs[])(Field3dObjectTask *task) = {
    CutEffect_Delete,
    RockSmashEffect_Delete,
    RockSmashEffect_Delete,
    CutEffect_Delete,
    HeadbuttEffect_Delete,
    HeadbuttEffect_Delete,
};
BOOL (*const gScriptFieldMoveEffectIsFinishedFuncs[])(Field3dObjectTask *task) = {
    CutEffect_IsFinished,
    RockSmashEffect_IsFinished,
    RockSmashEffect_IsFinished,
    CutEffect_IsFinished,
    HeadbuttEffect_IsFinished,
    HeadbuttEffect_IsFinished,
};
#endif
