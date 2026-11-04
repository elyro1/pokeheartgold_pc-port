#include "map_object_movement_commands.h"

#include "global.h"
#if defined(SDK_PORT)
#include "port/sim_config_prj.h"
#endif

#include "field_effects.h"
#include "field_emote_mark.h"
#include "field_trainer_reveal.h"
#include "heap.h"
#include "map_object.h"
#include "map_object_follow_internal.h"
#include "map_object_movement_commands_internal.h"
#include "map_object_movement_utilities.h"
#include "map_object_movement_utilities_internal.h"
#include "math_util.h"
#include "sound_playback.h"
#include "sys_task_api.h"

struct EventObjectMovementWork {
    u32 state;
    BOOL finished;
    s32 index;
    LocalMapObject *object;
    const MovementScriptCommand *command;
};

typedef BOOL (*MovementScriptStateFunc)(EventObjectMovementWork *work);

typedef struct WalkMovementWork {
    u16 animation;
    s16 timer;
    u32 direction;
    u32 distance;
} WalkMovementWork;

typedef struct WalkInPlaceMovementWork {
    s16 animation;
    s16 timer;
    u8 unused[8];
} WalkInPlaceMovementWork;

typedef struct JumpMovementWork {
    s32 distance;
    s32 progress;
    u16 heightStep;
    u16 heightPos;
    s8 direction;
    s8 timer;
#if defined(SDK_PORT)
    u8 interpolateHeight; // Reuse the otherwise unused animation byte.
#else
    u8 animation;
#endif
    s8 heightTable;
} JumpMovementWork;

typedef struct RiseMovementWork {
    s32 height;
    s32 delta;
} RiseMovementWork;

typedef struct EmoteMovementWork {
    u32 type;
    FieldEffect *effect;
} EmoteMovementWork;

typedef struct JumpOffsetData {
#if defined(SDK_PORT)
    fx32 angleStep;
#else
    u8 angleStep;
#endif
    s8 dx;
    s8 dy;
    s8 dz;
    fx32 stepX;
    fx32 stepY;
    fx32 stepZ;
    VecFx32 origin;
} JumpOffsetData;

typedef struct JumpByOffsetWork {
    s8 direction;
    s8 timer;
    u8 step;
    u8 animation;
    fx32 height;
    JumpOffsetData *data;
} JumpByOffsetWork;

typedef struct VariableSpeedWalkWork {
    s16 direction;
    s16 animation;
    s16 count;
    s16 index;
#if defined(SDK_PORT)
    s16 multiplier;
#endif
} VariableSpeedWalkWork;
#if defined(SDK_PORT)
_Static_assert(sizeof(JumpMovementWork) <= sizeof(((LocalMapObject *)0)->movementCmdData), "jump movement storage");
_Static_assert(sizeof(VariableSpeedWalkWork) <= sizeof(((LocalMapObject *)0)->movementCmdData), "variable walk storage");
#endif

enum MovementScriptMachineState {
    MOVEMENT_SCRIPT_INIT,
    MOVEMENT_SCRIPT_WAIT_READY,
    MOVEMENT_SCRIPT_SET_COMMAND,
    MOVEMENT_SCRIPT_WAIT_COMMAND,
    MOVEMENT_SCRIPT_LOOP_CHECK,
    MOVEMENT_SCRIPT_DONE,
};

extern const MovementScriptStateFunc gMovementScriptMachineStateFuncs[];

static void MapObject_StartWalkMovement(LocalMapObject *object, u32 direction, u32 distance, s32 duration, u16 animation);
static void MapObject_StartWalkInPlaceMovement(LocalMapObject *object, u32 direction, int duration, int animation);
static void MapObject_StartJumpMovement(LocalMapObject *object, s8 direction, int distance, s8 duration, u16 animation, s16 heightTable, u16 heightStep);
static void MapObject_StartDelayMovement(LocalMapObject *object, s32 duration);
static void MapObject_StartEmoteMovement(LocalMapObject *object, u32 type);
static void MapObject_StartVariableSpeedWalk(LocalMapObject *object, u32 direction, u32 count, u32 animation);
static BOOL MapObject_UpdateVariableSpeedWalk(LocalMapObject *object, const u32 *distances);
static void MapObject_StartJumpByOffsetMovement(LocalMapObject *object, int dx, int dy, s16 dz, s8 direction, s16 duration, u16 animation);

BOOL MapObject_AreBitsSetForMovementScriptInit(LocalMapObject *object) {
    if (!MapObject_TestFlagsBits(object, MAPOBJECTFLAG_ACTIVE)) {
        return FALSE;
    }
    if (MapObject_TestFlagsBits(object, MAPOBJECTFLAG_SINGLE_MOVEMENT) == TRUE) {
        return FALSE;
    }
    if (MapObject_TestFlagsBits(object, MAPOBJECTFLAG_UNK4) == TRUE
        && !MapObject_TestFlagsBits(object, MAPOBJECTFLAG_UNK5)) {
        return FALSE;
    }
    return TRUE;
}

void MapObject_SetHeldMovement(LocalMapObject *object, u32 movement) {
    GF_ASSERT((int)movement < 0x71);
    MapObject_SetMovementCommand(object, movement);
    MapObject_SetMovementStep(object, 0);
    MapObject_SetFlagsBits(object, MAPOBJECTFLAG_UNK4);
    MapObject_ClearFlagsBits(object, MAPOBJECTFLAG_UNK5);
}

void MapObject_ForceSetHeldMovement(LocalMapObject *object, u32 movement) {
    MapObject_SetMovementCommand(object, movement);
    MapObject_SetMovementStep(object, 0);
    MapObject_ClearFlagsBits(object, MAPOBJECTFLAG_UNK5);
}

BOOL MapObject_IsMovementPaused(LocalMapObject *object) {
    if (!MapObject_TestFlagsBits(object, MAPOBJECTFLAG_UNK4)) {
        return TRUE;
    }
    if (MapObject_TestFlagsBits(object, MAPOBJECTFLAG_UNK5)) {
        return TRUE;
    }
    return FALSE;
}

BOOL MapObject_ClearHeldMovementIfActive(LocalMapObject *object) {
    if (!MapObject_TestFlagsBits(object, MAPOBJECTFLAG_UNK4)) {
        return TRUE;
    }
    if (!MapObject_TestFlagsBits(object, MAPOBJECTFLAG_UNK5)) {
        return FALSE;
    }
    MapObject_ClearFlagsBits(object, (MapObjectFlagBits)(MAPOBJECTFLAG_UNK4 | MAPOBJECTFLAG_UNK5));
    return TRUE;
}

void MapObject_ClearHeldMovement(LocalMapObject *object) {
    MapObject_ClearFlagsBits(object, MAPOBJECTFLAG_UNK4);
    MapObject_SetFlagsBits(object, MAPOBJECTFLAG_UNK5);
    MapObject_SetMovementCommand(object, MOVEMENT_NONE);
    MapObject_SetMovementStep(object, 0);
}

EventObjectMovementMan *EventObjectMovementMan_Create(LocalMapObject *object, const MovementScriptCommand *command) {
    EventObjectMovementWork *work;
    SysTask *task;

    work = Heap_AllocAtEnd(HEAP_ID_FIELD1, sizeof(EventObjectMovementWork));
    GF_ASSERT(work != NULL);
    {
        u8 *dest = (u8 *)work;
        u32 size = sizeof(EventObjectMovementWork);
        while (size != 0) {
            *dest++ = 0;
            size--;
        }
    }
    {
        u32 priority = MapObjectManager_GetPriority(MapObject_GetManager(object)) - 1;
        task = SysTask_CreateOnMainQueue(MovementScriptMachine, work, priority);
    }
    GF_ASSERT(task != NULL);
    work->object = object;
    work->command = command;
    return (EventObjectMovementMan *)task;
}

BOOL EventObjectMovementMan_IsFinish(EventObjectMovementMan *movementMan) {
    EventObjectMovementWork *work = SysTask_GetData((SysTask *)movementMan);
    return work->finished;
}

void EventObjectMovementMan_Delete(EventObjectMovementMan *movementMan) {
    SysTask *task = (SysTask *)movementMan;
    EventObjectMovementWork *work = SysTask_GetData(task);

    if (MapObject_IsMovementPaused(work->object) != TRUE) {
        GF_ASSERT(FALSE);
    }
    MapObject_ClearHeldMovementIfActive(work->object);
    Heap_FreeExplicit(HEAP_ID_FIELD1, work);
    SysTask_Destroy(task);
}

void MovementScriptMachine(SysTask *task, void *data) {
    EventObjectMovementWork *work = data;
    while (gMovementScriptMachineStateFuncs[work->state](work) == TRUE) {}
}

BOOL MovementScriptMachineSub_Init(EventObjectMovementWork *work) {
    work->index = 0;
    work->state = MOVEMENT_SCRIPT_WAIT_READY;
    return TRUE;
}

BOOL MovementScriptMachineSub_WaitReady(EventObjectMovementWork *work) {
    if (!MapObject_AreBitsSetForMovementScriptInit(work->object)) {
        return FALSE;
    }
    work->state = MOVEMENT_SCRIPT_SET_COMMAND;
    return TRUE;
}

BOOL MovementScriptMachineSub_SetMovementCommand(EventObjectMovementWork *work) {
    MapObject_SetHeldMovement(work->object, work->command->command);
    work->state = MOVEMENT_SCRIPT_WAIT_COMMAND;
    return FALSE;
}

BOOL MovementScriptMachineSub_WaitMovementCommand(EventObjectMovementWork *work) {
    if (!MapObject_IsMovementPaused(work->object)) {
        return FALSE;
    }
    work->state = MOVEMENT_SCRIPT_LOOP_CHECK;
    return TRUE;
}

BOOL MovementScriptMachineSub_LoopCheck(EventObjectMovementWork *work) {
    s32 index = work->index;
    const MovementScriptCommand *command = work->command;
    index++;
    work->index = index;
    if (index < command->length) {
        work->state = MOVEMENT_SCRIPT_WAIT_READY;
        return TRUE;
    }
    work->command = command + 1;
    if (command[1].command != MOVEMENT_STEP_END) {
        work->state = MOVEMENT_SCRIPT_INIT;
        return TRUE;
    }
    work->finished = TRUE;
    work->state = MOVEMENT_SCRIPT_DONE;
    return FALSE;
}

BOOL MovementScriptMachineSub_Done(EventObjectMovementWork *work) {
    return FALSE;
}

u32 GetMovementForDirection(s32 direction, u32 movement) {
    s32 i;
    const u32 *const *table;
    const u32 *scan;
    const u32 *entry;

    GF_ASSERT(direction < DIR_MAX);
    table = gMovementsByDirection;
    entry = *table;
    while (entry != NULL) {
        for (i = 0, scan = entry; i < DIR_MAX; i++, scan++) {
            if (movement == *scan) {
                return entry[direction];
            }
        }
        table++;
        entry = *table;
    }
    GF_ASSERT(FALSE);
    return movement;
}

s32 GetMovementDirection(u32 movement) {
    s32 i;
    const u32 *const *table = gMovementsByDirection;
    const u32 *entry = *table;

    while (entry != NULL) {
        for (i = 0; i < DIR_MAX; i++) {
            if (entry[i] == movement) {
                return i;
            }
        }
        table++;
        entry = *table;
    }
    return DIR_NONE;
}

BOOL IsMovement56To59(u32 movement) {
    movement -= 0x38;
    if (movement <= 3) {
        return TRUE;
    }
    return FALSE;
}

u32 GetMovement8ForDirection(s32 direction) {
    MovementList table = _020FDA28;

    GF_ASSERT(direction < DIR_MAX);
    return table.movements[direction];
}

void MapObject_ExecuteMovementCommand(LocalMapObject *object) {
    u32 command;
    do {
        command = MapObject_GetMovementCommand(object);
        if (command == MOVEMENT_NONE) {
            break;
        }
    } while (MapObject_RunMovementCommand(object, command, MapObject_GetMovementStep(object)));
}

BOOL MapObject_RunForcedMovement(LocalMapObject *object) {
    MapObject_ExecuteMovementCommand(object);
    if (!MapObject_TestFlagsBits(object, MAPOBJECTFLAG_UNK5)) {
        return FALSE;
    }
    MapObject_ClearFlagsBits(object, MAPOBJECTFLAG_UNK5);
    MapObject_SetMovementCommand(object, MOVEMENT_NONE);
    MapObject_SetMovementStep(object, 0);
    return TRUE;
}

BOOL MapObject_RunMovementCommand(LocalMapObject *object, u32 command, u32 step) {
    return gMovementCmdTable[command][step](object);
}

BOOL MapObjectMovementCmd098_Step2(LocalMapObject *object) {
    MapObject_SetFlagsBits(object, MAPOBJECTFLAG_UNK5);
    return FALSE;
}

void MapObject_StartFaceMovement(LocalMapObject *object, u32 direction) {
    MapObject_SetFacingDirection(object, direction);
    MapObject_SetAnimation(object, MAP_OBJECT_ANIM_STAND);
    MapObject_SetPreviousCoordsToCurrent(object);
    MapObject_IncrementMovementStep(object);
}

BOOL MapObjectMovementCmd000_Step0(LocalMapObject *object) {
    MapObject_StartFaceMovement(object, DIR_NORTH);
    return TRUE;
}

BOOL MapObjectMovementCmd001_Step0(LocalMapObject *object) {
    MapObject_StartFaceMovement(object, DIR_SOUTH);
    return TRUE;
}

BOOL MapObjectMovementCmd002_Step0(LocalMapObject *object) {
    MapObject_StartFaceMovement(object, DIR_WEST);
    return TRUE;
}

BOOL MapObjectMovementCmd003_Step0(LocalMapObject *object) {
    MapObject_StartFaceMovement(object, DIR_EAST);
    return TRUE;
}

static void MapObject_StartWalkMovement(LocalMapObject *object, u32 direction, u32 distance, s32 duration, u16 animation) {
#if defined(SDK_PORT)
    // Keep each tile step in sync with the half-speed billboard animation.
    if (SIM_Config_prj_GetConfig()->enable60fps) {
        duration *= 2;
        distance /= 2;
    }
#endif
    WalkMovementWork *work = MapObject_InitMovementCmdData(object, sizeof(WalkMovementWork));

    work->animation = animation;
    work->timer = duration;
    work->direction = direction;
    work->distance = distance;
    MapObject_MoveCoordsInDirection(object, direction);
    MapObject_SetOrQueueFacing(object, direction);
    MapObject_SetAnimation(object, animation);
    MapObject_SetFlagsBits(object, MAPOBJECTFLAG_START_MOVEMENT);
    MapObject_IncrementMovementStep(object);
}

BOOL MapObjectMovementCmd090_Step1(LocalMapObject *object) {
    WalkMovementWork *work = MapObject_GetMovementCmdData(object);

    MapObject_OffsetPositionInDirection(object, work->direction, work->distance);
    MapObject_UpdateHeight(object);
    work->timer--;
    if (work->timer > 0) {
        return FALSE;
    }
    MapObject_SetFlagsBits(object, (MapObjectFlagBits)(MAPOBJECTFLAG_END_MOVEMENT | MAPOBJECTFLAG_UNK5));
    MapObject_SetPreviousCoordsToCurrent(object);
    MapObject_CallDrawUpdate(object);
    MapObject_SetAnimation(object, MAP_OBJECT_ANIM_STAND);
    MapObject_IncrementMovementStep(object);
    return TRUE;
}

BOOL MapObjectMovementCmd004_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_NORTH, FX32_CONST(0.5), 32, MAP_OBJECT_ANIM_WALK_SLOWER);
    return TRUE;
}

BOOL MapObjectMovementCmd005_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_SOUTH, FX32_CONST(0.5), 32, MAP_OBJECT_ANIM_WALK_SLOWER);
    return TRUE;
}

BOOL MapObjectMovementCmd006_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_WEST, FX32_CONST(0.5), 32, MAP_OBJECT_ANIM_WALK_SLOWER);
    return TRUE;
}

BOOL MapObjectMovementCmd007_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_EAST, FX32_CONST(0.5), 32, MAP_OBJECT_ANIM_WALK_SLOWER);
    return TRUE;
}

BOOL MapObjectMovementCmd008_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_NORTH, FX32_ONE, 16, MAP_OBJECT_ANIM_WALK_SLOW);
    return TRUE;
}

BOOL MapObjectMovementCmd009_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_SOUTH, FX32_ONE, 16, MAP_OBJECT_ANIM_WALK_SLOW);
    return TRUE;
}

BOOL MapObjectMovementCmd010_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_WEST, FX32_ONE, 16, MAP_OBJECT_ANIM_WALK_SLOW);
    return TRUE;
}

BOOL MapObjectMovementCmd011_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_EAST, FX32_ONE, 16, MAP_OBJECT_ANIM_WALK_SLOW);
    return TRUE;
}

BOOL MapObjectMovementCmd012_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_NORTH, FX32_CONST(2), 8, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd013_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_SOUTH, FX32_CONST(2), 8, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd014_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_WEST, FX32_CONST(2), 8, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd015_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_EAST, FX32_CONST(2), 8, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd016_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_NORTH, FX32_CONST(4), 4, MAP_OBJECT_ANIM_WALK_FAST);
    return TRUE;
}

BOOL MapObjectMovementCmd017_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_SOUTH, FX32_CONST(4), 4, MAP_OBJECT_ANIM_WALK_FAST);
    return TRUE;
}

BOOL MapObjectMovementCmd018_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_WEST, FX32_CONST(4), 4, MAP_OBJECT_ANIM_WALK_FAST);
    return TRUE;
}

BOOL MapObjectMovementCmd019_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_EAST, FX32_CONST(4), 4, MAP_OBJECT_ANIM_WALK_FAST);
    return TRUE;
}

BOOL MapObjectMovementCmd020_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_NORTH, FX32_CONST(8), 2, MAP_OBJECT_ANIM_WALK_FASTER);
    return TRUE;
}

BOOL MapObjectMovementCmd021_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_SOUTH, FX32_CONST(8), 2, MAP_OBJECT_ANIM_WALK_FASTER);
    return TRUE;
}

BOOL MapObjectMovementCmd022_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_WEST, FX32_CONST(8), 2, MAP_OBJECT_ANIM_WALK_FASTER);
    return TRUE;
}

BOOL MapObjectMovementCmd023_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_EAST, FX32_CONST(8), 2, MAP_OBJECT_ANIM_WALK_FASTER);
    return TRUE;
}

BOOL MapObjectMovementCmd084_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_NORTH, FX32_CONST(16), 1, MAP_OBJECT_ANIM_STAND);
    return TRUE;
}

BOOL MapObjectMovementCmd085_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_SOUTH, FX32_CONST(16), 1, MAP_OBJECT_ANIM_STAND);
    return TRUE;
}

BOOL MapObjectMovementCmd086_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_WEST, FX32_CONST(16), 1, MAP_OBJECT_ANIM_STAND);
    return TRUE;
}

BOOL MapObjectMovementCmd087_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_EAST, FX32_CONST(16), 1, MAP_OBJECT_ANIM_STAND);
    return TRUE;
}

BOOL MapObjectMovementCmd088_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_NORTH, FX32_CONST(4), 4, MAP_OBJECT_ANIM_RUN);
    return TRUE;
}

BOOL MapObjectMovementCmd089_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_SOUTH, FX32_CONST(4), 4, MAP_OBJECT_ANIM_RUN);
    return TRUE;
}

BOOL MapObjectMovementCmd090_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_WEST, FX32_CONST(4), 4, MAP_OBJECT_ANIM_RUN);
    return TRUE;
}

BOOL MapObjectMovementCmd091_Step0(LocalMapObject *object) {
    MapObject_StartWalkMovement(object, DIR_EAST, FX32_CONST(4), 4, MAP_OBJECT_ANIM_RUN);
    return TRUE;
}

static void MapObject_StartWalkInPlaceMovement(LocalMapObject *object, u32 direction, int duration, int animation) {
#if defined(SDK_PORT)
    if (SIM_Config_prj_GetConfig()->enable60fps) {
        duration = (duration + 1) * 2 - 1;
    }
#endif
    WalkInPlaceMovementWork *work = MapObject_InitMovementCmdData(object, sizeof(WalkInPlaceMovementWork));

    work->animation = animation;
    work->timer = duration + 1;
    MapObject_SetFacingDirection(object, direction);
    MapObject_SetAnimation(object, animation);
    MapObject_SetPreviousCoordsToCurrent(object);
    MapObject_IncrementMovementStep(object);
}

BOOL MapObjectMovementCmd040_Step1(LocalMapObject *object) {
    WalkInPlaceMovementWork *work = MapObject_GetMovementCmdData(object);

    work->timer--;
    if (work->timer > 0) {
        return FALSE;
    }
    MapObject_SetFlagsBits(object, MAPOBJECTFLAG_UNK5);
    MapObject_SetAnimation(object, MAP_OBJECT_ANIM_STAND);
    MapObject_IncrementMovementStep(object);
    return TRUE;
}

BOOL MapObjectMovementCmd024_Step0(LocalMapObject *object) {
    MapObject_StartWalkInPlaceMovement(object, DIR_NORTH, 32, MAP_OBJECT_ANIM_WALK_SLOWER);
    return TRUE;
}

BOOL MapObjectMovementCmd025_Step0(LocalMapObject *object) {
    MapObject_StartWalkInPlaceMovement(object, DIR_SOUTH, 32, MAP_OBJECT_ANIM_WALK_SLOWER);
    return TRUE;
}

BOOL MapObjectMovementCmd026_Step0(LocalMapObject *object) {
    MapObject_StartWalkInPlaceMovement(object, DIR_WEST, 32, MAP_OBJECT_ANIM_WALK_SLOWER);
    return TRUE;
}

BOOL MapObjectMovementCmd027_Step0(LocalMapObject *object) {
    MapObject_StartWalkInPlaceMovement(object, DIR_EAST, 32, MAP_OBJECT_ANIM_WALK_SLOWER);
    return TRUE;
}

BOOL MapObjectMovementCmd028_Step0(LocalMapObject *object) {
    MapObject_StartWalkInPlaceMovement(object, DIR_NORTH, 16, MAP_OBJECT_ANIM_WALK_SLOW);
    return TRUE;
}

BOOL MapObjectMovementCmd029_Step0(LocalMapObject *object) {
    MapObject_StartWalkInPlaceMovement(object, DIR_SOUTH, 16, MAP_OBJECT_ANIM_WALK_SLOW);
    return TRUE;
}

BOOL MapObjectMovementCmd030_Step0(LocalMapObject *object) {
    MapObject_StartWalkInPlaceMovement(object, DIR_WEST, 16, MAP_OBJECT_ANIM_WALK_SLOW);
    return TRUE;
}

BOOL MapObjectMovementCmd031_Step0(LocalMapObject *object) {
    MapObject_StartWalkInPlaceMovement(object, DIR_EAST, 16, MAP_OBJECT_ANIM_WALK_SLOW);
    return TRUE;
}

BOOL MapObjectMovementCmd032_Step0(LocalMapObject *object) {
    MapObject_StartWalkInPlaceMovement(object, DIR_NORTH, 8, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd033_Step0(LocalMapObject *object) {
    MapObject_StartWalkInPlaceMovement(object, DIR_SOUTH, 8, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd034_Step0(LocalMapObject *object) {
    MapObject_StartWalkInPlaceMovement(object, DIR_WEST, 8, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd035_Step0(LocalMapObject *object) {
    MapObject_StartWalkInPlaceMovement(object, DIR_EAST, 8, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd036_Step0(LocalMapObject *object) {
    MapObject_StartWalkInPlaceMovement(object, DIR_NORTH, 4, MAP_OBJECT_ANIM_WALK_FAST);
    return TRUE;
}

BOOL MapObjectMovementCmd037_Step0(LocalMapObject *object) {
    MapObject_StartWalkInPlaceMovement(object, DIR_SOUTH, 4, MAP_OBJECT_ANIM_WALK_FAST);
    return TRUE;
}

BOOL MapObjectMovementCmd038_Step0(LocalMapObject *object) {
    MapObject_StartWalkInPlaceMovement(object, DIR_WEST, 4, MAP_OBJECT_ANIM_WALK_FAST);
    return TRUE;
}

BOOL MapObjectMovementCmd039_Step0(LocalMapObject *object) {
    MapObject_StartWalkInPlaceMovement(object, DIR_EAST, 4, MAP_OBJECT_ANIM_WALK_FAST);
    return TRUE;
}

BOOL MapObjectMovementCmd040_Step0(LocalMapObject *object) {
    MapObject_StartWalkInPlaceMovement(object, DIR_NORTH, 2, MAP_OBJECT_ANIM_WALK_FASTER);
    return TRUE;
}

BOOL MapObjectMovementCmd041_Step0(LocalMapObject *object) {
    MapObject_StartWalkInPlaceMovement(object, DIR_SOUTH, 2, MAP_OBJECT_ANIM_WALK_FASTER);
    return TRUE;
}

BOOL MapObjectMovementCmd042_Step0(LocalMapObject *object) {
    MapObject_StartWalkInPlaceMovement(object, DIR_WEST, 2, MAP_OBJECT_ANIM_WALK_FASTER);
    return TRUE;
}

BOOL MapObjectMovementCmd043_Step0(LocalMapObject *object) {
    MapObject_StartWalkInPlaceMovement(object, DIR_EAST, 2, MAP_OBJECT_ANIM_WALK_FASTER);
    return TRUE;
}

static void MapObject_StartJumpMovement(LocalMapObject *object, s8 direction, int distance, s8 duration, u16 animation, s16 heightTable, u16 heightStep) {
#if defined(SDK_PORT)
    if (SIM_Config_prj_GetConfig()->enable60fps) {
        duration *= 2;
        distance /= 2;
        heightStep /= 2;
    }
#endif
    JumpMovementWork *work = MapObject_InitMovementCmdData(object, sizeof(JumpMovementWork));

    work->direction = direction;
    work->distance = distance;
    work->timer = duration;
#if !defined(SDK_PORT)
    work->animation = animation;
#endif
    work->heightTable = heightTable;
    work->heightStep = heightStep;
#if defined(SDK_PORT)
    work->interpolateHeight = SIM_Config_prj_GetConfig()->enable60fps;
#endif
    if (distance == 0) {
        MapObject_SetPreviousCoordsToCurrent(object);
    } else {
        MapObject_MoveCoordsInDirection(object, direction);
    }
    MapObject_SetFlagsBits(object, (MapObjectFlagBits)(MAPOBJECTFLAG_UNK16 | MAPOBJECTFLAG_START_MOVEMENT));
    MapObject_SetOrQueueFacing(object, direction);
    MapObject_SetAnimation(object, animation);
    MapObject_IncrementMovementStep(object);
    if (!MapObject_CheckVisible(object)) {
        PlaySE(SEQ_SE_DP_DANSA);
    }
}

BOOL MapObjectMovementCmd092_Step1(LocalMapObject *object) {
    JumpMovementWork *work = MapObject_GetMovementCmdData(object);
    VecFx32 offset;

    if (work->distance) {
        MapObject_OffsetPositionInDirection(object, work->direction, work->distance);
        MapObject_UpdateHeight(object);
        if (work->progress >= FX32_ONE * 16) {
            work->progress = 0;
            MapObject_MoveCoordsInDirection(object, work->direction);
            MapObject_SetFlagsBits(object, MAPOBJECTFLAG_START_MOVEMENT);
        }
        work->progress += (work->distance < 0) ? -work->distance : work->distance;
    }
    work->heightPos += work->heightStep;
    if (work->heightPos > 0xF00) {
        work->heightPos = 0xF00;
    }
    {
        int heightTable;
        int heightPos;
        u16 index;
        const fx32 *heights;

        heightTable = work->heightTable;
        heightPos = work->heightPos;
        index = (u16)(heightPos / 256);
        heights = gJumpHeightTables[heightTable];
        offset.x = 0;
        offset.y = heights[index];
#if defined(SDK_PORT)
        if (work->interpolateHeight && index < 15)
            offset.y += (heights[index + 1] - offset.y) * (work->heightPos % 256) / 256;
#endif
        offset.z = 0;
    }
    MapObject_SetFacingVector(object, &offset);
    work->timer--;
    if (work->timer > 0) {
        return FALSE;
    }
    {
        VecFx32 zero = { 0, 0, 0 };
        MapObject_SetFacingVector(object, &zero);
    }
    MapObject_SetFlagsBits(object, (MapObjectFlagBits)(MAPOBJECTFLAG_UNK17 | MAPOBJECTFLAG_END_MOVEMENT | MAPOBJECTFLAG_UNK5));
    MapObject_SetPreviousCoordsToCurrent(object);
    MapObject_CallDrawUpdate(object);
    MapObject_SetAnimation(object, MAP_OBJECT_ANIM_STAND);
    MapObject_IncrementMovementStep(object);
    if (!MapObject_CheckVisible(object)) {
        PlaySE(SEQ_SE_DP_SUTYA2);
    }
    return TRUE;
}

BOOL MapObjectMovementCmd044_Step0(LocalMapObject *object) {
    MapObject_StartJumpMovement(object, DIR_NORTH, 0, 16, MAP_OBJECT_ANIM_WALK_SLOW, 0, 0x100);
    return TRUE;
}

BOOL MapObjectMovementCmd045_Step0(LocalMapObject *object) {
    MapObject_StartJumpMovement(object, DIR_SOUTH, 0, 16, MAP_OBJECT_ANIM_WALK_SLOW, 0, 0x100);
    return TRUE;
}

BOOL MapObjectMovementCmd046_Step0(LocalMapObject *object) {
    MapObject_StartJumpMovement(object, DIR_WEST, 0, 16, MAP_OBJECT_ANIM_WALK_SLOW, 0, 0x100);
    return TRUE;
}

BOOL MapObjectMovementCmd047_Step0(LocalMapObject *object) {
    MapObject_StartJumpMovement(object, DIR_EAST, 0, 16, MAP_OBJECT_ANIM_WALK_SLOW, 0, 0x100);
    return TRUE;
}

BOOL MapObjectMovementCmd048_Step0(LocalMapObject *object) {
    MapObject_StartJumpMovement(object, DIR_NORTH, 0, 8, MAP_OBJECT_ANIM_WALK_NORMAL, 0, 0x200);
    return TRUE;
}

BOOL MapObjectMovementCmd049_Step0(LocalMapObject *object) {
    MapObject_StartJumpMovement(object, DIR_SOUTH, 0, 8, MAP_OBJECT_ANIM_WALK_NORMAL, 0, 0x200);
    return TRUE;
}

BOOL MapObjectMovementCmd050_Step0(LocalMapObject *object) {
    MapObject_StartJumpMovement(object, DIR_WEST, 0, 8, MAP_OBJECT_ANIM_WALK_NORMAL, 0, 0x200);
    return TRUE;
}

BOOL MapObjectMovementCmd051_Step0(LocalMapObject *object) {
    MapObject_StartJumpMovement(object, DIR_EAST, 0, 8, MAP_OBJECT_ANIM_WALK_NORMAL, 0, 0x200);
    return TRUE;
}

BOOL MapObjectMovementCmd052_Step0(LocalMapObject *object) {
    MapObject_StartJumpMovement(object, DIR_NORTH, FX32_CONST(2), 8, MAP_OBJECT_ANIM_WALK_NORMAL, 0, 0x200);
    return TRUE;
}

BOOL MapObjectMovementCmd053_Step0(LocalMapObject *object) {
    MapObject_StartJumpMovement(object, DIR_SOUTH, FX32_CONST(2), 8, MAP_OBJECT_ANIM_WALK_NORMAL, 0, 0x200);
    return TRUE;
}

BOOL MapObjectMovementCmd054_Step0(LocalMapObject *object) {
    MapObject_StartJumpMovement(object, DIR_WEST, FX32_CONST(2), 8, MAP_OBJECT_ANIM_WALK_NORMAL, 0, 0x200);
    return TRUE;
}

BOOL MapObjectMovementCmd055_Step0(LocalMapObject *object) {
    MapObject_StartJumpMovement(object, DIR_EAST, FX32_CONST(2), 8, MAP_OBJECT_ANIM_WALK_NORMAL, 0, 0x200);
    return TRUE;
}

BOOL MapObjectMovementCmd056_Step0(LocalMapObject *object) {
    MapObject_StartJumpMovement(object, DIR_NORTH, FX32_CONST(2), 16, MAP_OBJECT_ANIM_WALK_NORMAL, 0, 0x100);
    return TRUE;
}

BOOL MapObjectMovementCmd057_Step0(LocalMapObject *object) {
    MapObject_StartJumpMovement(object, DIR_SOUTH, FX32_CONST(2), 16, MAP_OBJECT_ANIM_WALK_NORMAL, 0, 0x100);
    return TRUE;
}

BOOL MapObjectMovementCmd058_Step0(LocalMapObject *object) {
    MapObject_StartJumpMovement(object, DIR_WEST, FX32_CONST(2), 16, MAP_OBJECT_ANIM_WALK_NORMAL, 0, 0x100);
    return TRUE;
}

BOOL MapObjectMovementCmd059_Step0(LocalMapObject *object) {
    MapObject_StartJumpMovement(object, DIR_EAST, FX32_CONST(2), 16, MAP_OBJECT_ANIM_WALK_NORMAL, 0, 0x100);
    return TRUE;
}

BOOL MapObjectMovementCmd092_Step0(LocalMapObject *object) {
    MapObject_StartJumpMovement(object, DIR_WEST, FX32_ONE, 16, MAP_OBJECT_ANIM_JUMP_NEAR_SLOW, 0, 0xF0);
    return TRUE;
}

BOOL MapObjectMovementCmd093_Step0(LocalMapObject *object) {
    MapObject_StartJumpMovement(object, DIR_EAST, FX32_ONE, 16, MAP_OBJECT_ANIM_JUMP_NEAR_SLOW, 0, 0xF0);
    return TRUE;
}

BOOL MapObjectMovementCmd094_Step0(LocalMapObject *object) {
    MapObject_StartJumpMovement(object, DIR_WEST, FX32_CONST(4), 12, MAP_OBJECT_ANIM_JUMP_FARTHER, 0, 0x140);
    return TRUE;
}

BOOL MapObjectMovementCmd095_Step0(LocalMapObject *object) {
    MapObject_StartJumpMovement(object, DIR_EAST, FX32_CONST(4), 12, MAP_OBJECT_ANIM_JUMP_FARTHER, 0, 0x140);
    return TRUE;
}

static void MapObject_StartDelayMovement(LocalMapObject *object, s32 duration) {
    s32 *timer = MapObject_InitMovementCmdData(object, sizeof(s32));

    *timer = duration;
    MapObject_IncrementMovementStep(object);
}

BOOL MapObjectMovementCmd064_Step1(LocalMapObject *object) {
    s32 *timer = MapObject_GetMovementCmdData(object);

    if (*timer != 0) {
        (*timer)--;
        return FALSE;
    }
    MapObject_IncrementMovementStep(object);
    return TRUE;
}

BOOL MapObjectMovementCmd060_Step0(LocalMapObject *object) {
    MapObject_StartDelayMovement(object, 1);
    return TRUE;
}

BOOL MapObjectMovementCmd061_Step0(LocalMapObject *object) {
    MapObject_StartDelayMovement(object, 2);
    return TRUE;
}

BOOL MapObjectMovementCmd062_Step0(LocalMapObject *object) {
    MapObject_StartDelayMovement(object, 4);
    return TRUE;
}

BOOL MapObjectMovementCmd063_Step0(LocalMapObject *object) {
    MapObject_StartDelayMovement(object, 8);
    return TRUE;
}

BOOL MapObjectMovementCmd064_Step0(LocalMapObject *object) {
    MapObject_StartDelayMovement(object, 15);
    return TRUE;
}

BOOL MapObjectMovementCmd065_Step0(LocalMapObject *object) {
    MapObject_StartDelayMovement(object, 16);
    return TRUE;
}

BOOL MapObjectMovementCmd066_Step0(LocalMapObject *object) {
    MapObject_StartDelayMovement(object, 32);
    return TRUE;
}

BOOL MapObjectMovementCmd067_Step0(LocalMapObject *object) {
    RiseMovementWork *work = MapObject_InitMovementCmdData(object, sizeof(RiseMovementWork));
    work->delta = FX32_ONE * 16;
    MapObject_SetAnimation(object, MAP_OBJECT_ANIM_STAND);
    MapObject_IncrementMovementStep(object);
    return TRUE;
}

BOOL MapObjectMovementCmd067_Step1(LocalMapObject *object) {
    RiseMovementWork *work = MapObject_GetMovementCmdData(object);
    work->height += work->delta;
    VecFx32 offset = { 0, 0, 0 };
    offset.y = work->height;
    MapObject_SetFacingVector(object, &offset);
    if ((work->height / (FX32_ONE * 8)) < 40) {
        return FALSE;
    }
    MapObject_IncrementMovementStep(object);
    return TRUE;
}

BOOL MapObjectMovementCmd068_Step0(LocalMapObject *object) {
    RiseMovementWork *work = MapObject_InitMovementCmdData(object, sizeof(RiseMovementWork));
    work->height = FX32_ONE * 320;
    work->delta = -FX32_ONE * 16;
    MapObject_SetAnimation(object, MAP_OBJECT_ANIM_STAND);
    MapObject_IncrementMovementStep(object);
    return TRUE;
}

BOOL MapObjectMovementCmd068_Step1(LocalMapObject *object) {
    RiseMovementWork *work = MapObject_GetMovementCmdData(object);
    work->height += work->delta;
    if (work->height < 0) {
        work->height = 0;
    }
    VecFx32 offset = { 0, 0, 0 };
    offset.y = work->height;
    MapObject_SetFacingVector(object, &offset);
    if (work->height > 0) {
        return FALSE;
    }
    MapObject_IncrementMovementStep(object);
    return TRUE;
}

BOOL MapObjectMovementCmd069_Step0(LocalMapObject *object) {
    MapObject_SetFlagsBits(object, MAPOBJECTFLAG_VISIBLE);
    MapObject_IncrementMovementStep(object);
    return TRUE;
}

BOOL MapObjectMovementCmd070_Step0(LocalMapObject *object) {
    MapObject_ClearFlagsBits(object, MAPOBJECTFLAG_VISIBLE);
    MapObject_IncrementMovementStep(object);
    return TRUE;
}

BOOL MapObjectMovementCmd071_Step0(LocalMapObject *object) {
    MapObject_SetFlagsBits(object, MAPOBJECTFLAG_UNK7);
    MapObject_IncrementMovementStep(object);
    return TRUE;
}

BOOL MapObjectMovementCmd072_Step0(LocalMapObject *object) {
    MapObject_ClearFlagsBits(object, MAPOBJECTFLAG_UNK7);
    MapObject_IncrementMovementStep(object);
    return TRUE;
}

BOOL MapObjectMovementCmd073_Step0(LocalMapObject *object) {
    MapObject_SetFlagsBits(object, MAPOBJECTFLAG_UNK8);
    MapObject_IncrementMovementStep(object);
    return TRUE;
}

BOOL MapObjectMovementCmd074_Step0(LocalMapObject *object) {
    MapObject_ClearFlagsBits(object, MAPOBJECTFLAG_UNK8);
    MapObject_IncrementMovementStep(object);
    return TRUE;
}

static void MapObject_StartEmoteMovement(LocalMapObject *object, u32 type) {
    EmoteMovementWork *work = MapObject_InitMovementCmdData(object, sizeof(EmoteMovementWork));
    work->type = type;
    work->effect = EmoteMark_Spawn(object, type, TRUE);
    MapObject_IncrementMovementStep(object);
}

BOOL MapObjectMovementCmd075_Step1(LocalMapObject *object) {
    EmoteMovementWork *work = MapObject_GetMovementCmdData(object);
    if (EmoteMark_IsDone(work->effect) == TRUE) {
#if defined(SDK_PORT)
        FieldEffect_Destroy((FieldEffectHandle)work->effect);
#else
        FieldEffect_Destroy(work->effect);
#endif
        MapObject_IncrementMovementStep(object);
        return TRUE;
    }
    return FALSE;
}

BOOL MapObjectMovementCmd075_Step0(LocalMapObject *object) {
    MapObject_StartEmoteMovement(object, 0);
    return FALSE;
}

BOOL MapObjectMovementCmd103_Step0(LocalMapObject *object) {
    MapObject_StartEmoteMovement(object, 1);
    return FALSE;
}

static void MapObject_StartVariableSpeedWalk(LocalMapObject *object, u32 direction, u32 count, u32 animation) {
    VariableSpeedWalkWork *work = MapObject_InitMovementCmdData(object, sizeof(VariableSpeedWalkWork));
    work->direction = direction;
    work->animation = animation;
    work->count = count;
#if defined(SDK_PORT)
    work->multiplier = SIM_Config_prj_GetConfig()->enable60fps ? 2 : 1;
    work->count *= work->multiplier;
#endif
    MapObject_MoveCoordsInDirection(object, direction);
    MapObject_SetOrQueueFacing(object, direction);
    MapObject_SetAnimation(object, animation);
    MapObject_SetFlagsBits(object, MAPOBJECTFLAG_START_MOVEMENT);
    MapObject_IncrementMovementStep(object);
}

static BOOL MapObject_UpdateVariableSpeedWalk(LocalMapObject *object, const u32 *distances) {
    VariableSpeedWalkWork *work = MapObject_GetMovementCmdData(object);
#if defined(SDK_PORT)
    MapObject_OffsetPositionInDirection(object, work->direction, distances[work->index / work->multiplier] / work->multiplier);
#else
    MapObject_OffsetPositionInDirection(object, work->direction, distances[work->index]);
#endif
    MapObject_UpdateHeight(object);
    work->index++;
    if (work->index < work->count) {
        return FALSE;
    }
    MapObject_SetFlagsBits(object, (MapObjectFlagBits)(MAPOBJECTFLAG_END_MOVEMENT | MAPOBJECTFLAG_UNK5));
    MapObject_SetPreviousCoordsToCurrent(object);
    MapObject_CallDrawUpdate(object);
    MapObject_SetAnimation(object, MAP_OBJECT_ANIM_STAND);
    MapObject_IncrementMovementStep(object);
    return TRUE;
}

BOOL MapObjectMovementCmd076_Step0(LocalMapObject *object) {
    MapObject_StartVariableSpeedWalk(object, DIR_NORTH, 6, MAP_OBJECT_ANIM_WALK_SLIGHTLY_FAST);
    return TRUE;
}

BOOL MapObjectMovementCmd077_Step0(LocalMapObject *object) {
    MapObject_StartVariableSpeedWalk(object, DIR_SOUTH, 6, MAP_OBJECT_ANIM_WALK_SLIGHTLY_FAST);
    return TRUE;
}

BOOL MapObjectMovementCmd078_Step0(LocalMapObject *object) {
    MapObject_StartVariableSpeedWalk(object, DIR_WEST, 6, MAP_OBJECT_ANIM_WALK_SLIGHTLY_FAST);
    return TRUE;
}

BOOL MapObjectMovementCmd079_Step0(LocalMapObject *object) {
    MapObject_StartVariableSpeedWalk(object, DIR_EAST, 6, MAP_OBJECT_ANIM_WALK_SLIGHTLY_FAST);
    return TRUE;
}

BOOL MapObjectMovementCmd076_Step1(LocalMapObject *object) {
    if (MapObject_UpdateVariableSpeedWalk(object, gVariableSpeedWalk_Cmd076) == TRUE) {
        return TRUE;
    }
    return FALSE;
}

BOOL MapObjectMovementCmd080_Step0(LocalMapObject *object) {
    MapObject_StartVariableSpeedWalk(object, DIR_NORTH, 3, MAP_OBJECT_ANIM_WALK_SLIGHTLY_FASTER);
    return TRUE;
}

BOOL MapObjectMovementCmd081_Step0(LocalMapObject *object) {
    MapObject_StartVariableSpeedWalk(object, DIR_SOUTH, 3, MAP_OBJECT_ANIM_WALK_SLIGHTLY_FASTER);
    return TRUE;
}

BOOL MapObjectMovementCmd082_Step0(LocalMapObject *object) {
    MapObject_StartVariableSpeedWalk(object, DIR_WEST, 3, MAP_OBJECT_ANIM_WALK_SLIGHTLY_FASTER);
    return TRUE;
}

BOOL MapObjectMovementCmd083_Step0(LocalMapObject *object) {
    MapObject_StartVariableSpeedWalk(object, DIR_EAST, 3, MAP_OBJECT_ANIM_WALK_SLIGHTLY_FASTER);
    return TRUE;
}

BOOL MapObjectMovementCmd082_Step1(LocalMapObject *object) {
    if (MapObject_UpdateVariableSpeedWalk(object, gVariableSpeedWalk_Cmd082) == TRUE) {
        return TRUE;
    }
    return FALSE;
}

BOOL MapObjectMovementCmd096_Step0(LocalMapObject *object) {
    MapObject_StartVariableSpeedWalk(object, DIR_NORTH, 7, MAP_OBJECT_ANIM_WALK_EVER_SO_SLIGHTLY_FAST);
    return TRUE;
}

BOOL MapObjectMovementCmd097_Step0(LocalMapObject *object) {
    MapObject_StartVariableSpeedWalk(object, DIR_SOUTH, 7, MAP_OBJECT_ANIM_WALK_EVER_SO_SLIGHTLY_FAST);
    return TRUE;
}

BOOL MapObjectMovementCmd098_Step0(LocalMapObject *object) {
    MapObject_StartVariableSpeedWalk(object, DIR_WEST, 7, MAP_OBJECT_ANIM_WALK_EVER_SO_SLIGHTLY_FAST);
    return TRUE;
}

BOOL MapObjectMovementCmd099_Step0(LocalMapObject *object) {
    MapObject_StartVariableSpeedWalk(object, DIR_EAST, 7, MAP_OBJECT_ANIM_WALK_EVER_SO_SLIGHTLY_FAST);
    return TRUE;
}

BOOL MapObjectMovementCmd098_Step1(LocalMapObject *object) {
    if (MapObject_UpdateVariableSpeedWalk(object, gVariableSpeedWalk_Cmd098) == TRUE) {
        return TRUE;
    }
    return FALSE;
}

BOOL MapObjectMovementCmd100_Step0(LocalMapObject *object) {
    MapObject_InitMovementCmdData(object, sizeof(s32));
    MapObject_SetAnimation(object, MAP_OBJECT_ANIM_NURSE_BOW);
    MapObject_IncrementMovementStep(object);
    return FALSE;
}

BOOL MapObjectMovementCmd100_Step1(LocalMapObject *object) {
    s32 *timer = MapObject_GetMovementCmdData(object);
    (*timer)++;
    if (*timer >= 8) {
        MapObject_SetFacingDirection(object, DIR_SOUTH);
        MapObject_SetAnimation(object, MAP_OBJECT_ANIM_STAND);
        MapObject_IncrementMovementStep(object);
    }
    return FALSE;
}

BOOL MapObjectMovementCmd101_Step0(LocalMapObject *object) {
    FieldEffect *effect;

    MapObject_InitMovementCmdData(object, sizeof(s32));
    effect = HiddenObjectMovement_GetCoverEffect(object);
    if (effect) {
        FieldEffect_Destroy(effect);
    }
    VecFx32 offset = { 0, 0, 0 };
    MapObject_SetFacingVector(object, &offset);
    TrainerReveal_Spawn(object);
    MapObject_SetFlagsBits(object, (MapObjectFlagBits)(MAPOBJECTFLAG_UNK16 | MAPOBJECTFLAG_START_MOVEMENT));
    MapObject_ClearFlagsBits(object, MAPOBJECTFLAG_UNK20);
    MapObject_IncrementMovementStep(object);
    return FALSE;
}

BOOL MapObjectMovementCmd101_Step1(LocalMapObject *object) {
    s32 *index = MapObject_GetMovementCmdData(object);
    const fx32 *heights = gJumpHeightTables[0];
    VecFx32 offset = { 0, 0, 0 };

    offset.y = heights[*index];
    MapObject_SetFacingVector(object, &offset);
    *index += 2;
    if ((u32)*index < 16) {
        return FALSE;
    }
    offset.y = 0;
    MapObject_SetFacingVector(object, &offset);
    MapObject_SetFlagsBits(object, (MapObjectFlagBits)(MAPOBJECTFLAG_UNK17 | MAPOBJECTFLAG_END_MOVEMENT | MAPOBJECTFLAG_UNK5));
    HiddenObjectMovement_SetRevealed(object);
    MapObject_IncrementMovementStep(object);
    return TRUE;
}

BOOL MapObjectMovementCmd102_Step0(LocalMapObject *object) {
    MapObject_InitMovementCmdData(object, sizeof(u32));
    MapObject_SetAnimation(object, MAP_OBJECT_ANIM_STAND);
    MapObject_IncrementMovementStep(object);
    return FALSE;
}

BOOL MapObjectMovementCmd104_Step0(LocalMapObject *object) {
    MapObject_InitMovementCmdData(object, sizeof(u32));
    MapObject_SetAnimation(object, MAP_OBJECT_ANIM_WALK_SLOWER);
    MapObject_IncrementMovementStep(object);
    return FALSE;
}

BOOL MapObjectMovementCmd102_Step1(LocalMapObject *object) {
    u32 *timer = MapObject_GetMovementCmdData(object);
    (*timer)++;
    if (*timer < 21) {
        return FALSE;
    }
    MapObject_IncrementMovementStep(object);
    return TRUE;
}

static void MapObject_StartJumpByOffsetMovement(LocalMapObject *object, int dx, int dy, s16 dz, s8 direction, s16 duration, u16 animation) {
#if defined(SDK_PORT)
    if (SIM_Config_prj_GetConfig()->enable60fps) {
        duration *= 2;
    }
#endif
    fx32 time;
    JumpByOffsetWork *work;

    work = MapObject_InitMovementCmdData(object, sizeof(JumpByOffsetWork));
    work->data = Heap_AllocAtEnd(HEAP_ID_FIELD2, sizeof(JumpOffsetData));
    MI_CpuFill8(work->data, 0, sizeof(JumpOffsetData));
    work->direction = direction;
    work->timer = duration;
    work->animation = animation;
    time = FX32_CONST(duration);
    work->data->dx = dx;
    work->data->dy = dy;
    work->data->dz = dz;
    work->data->stepX = FX_Div(FX32_CONST(dx * 16), time);
    work->data->stepY = FX_Div(FX32_CONST(dy * 16), time);
    work->data->stepZ = FX_Div(FX32_CONST(dz * 16), time);
#if defined(SDK_PORT)
    work->data->angleStep = (180 * FX32_ONE) / duration;
#else
    work->data->angleStep = 180 / duration;
#endif
    MapObject_CopyPositionVector(object, &work->data->origin);
    work->height = work->data->origin.y;
    MapObject_SetPreviousCoordsToCurrent(object);
    MapObject_SetFlagsBits(object, (MapObjectFlagBits)(MAPOBJECTFLAG_UNK16 | MAPOBJECTFLAG_START_MOVEMENT));
    MapObject_SetOrQueueFacing(object, direction);
    MapObject_SetAnimation(object, animation);
    MapObject_IncrementMovementStep(object);
    if (!MapObject_CheckVisible(object)) {
        PlaySE(SEQ_SE_DP_DANSA);
    }
}

BOOL MapObjectMovementCmd108_Step1(LocalMapObject *object) {
    VecFx32 position;
    JumpByOffsetWork *work = MapObject_GetMovementCmdData(object);
    s64 progress;
    fx32 sine;
    fx32 targetY;
    float rounded;

    if (work->step != 0) {
        rounded = (float)(work->step << 12) + 0.5f;
    } else {
        rounded = (float)(work->step << 12) - 0.5f;
    }
    progress = (int)rounded;
    position.x = work->data->origin.x + FX_MUL(work->data->stepX, progress);
    position.y = 0;
    position.z = work->data->origin.z + FX_MUL(work->data->stepZ, progress);
    if (FieldSystem_SnapToTerrainHeightEx(MapObject_GetFieldSystem(object), &position, MapObject_CheckFlag29(object))) {
        work->height = position.y;
    } else {
        position.y = work->height;
    }
    MapObject_SetPositionVector(object, &position);
#if defined(SDK_PORT)
    sine = GF_SinDegNoWrap((u16)((work->data->angleStep * work->step) / FX32_ONE));
#else
    sine = GF_SinDegNoWrap(work->data->angleStep * work->step);
#endif
    targetY = work->data->origin.y + FX_MUL(work->data->stepY, progress);
    targetY += FX_MUL(sine, FX32_CONST(16));
    position.y = targetY - position.y;
    position.x = 0;
    position.z = 0;
    MapObject_SetDrawOffset(object, &position);
    work->timer--;
    work->step++;
    if (work->timer > 0) {
        return FALSE;
    }
    position.x = position.y = position.z = 0;
    MapObject_SetFacingVector(object, &position);
    MapObject_SetDrawOffset(object, &position);
    MapObject_AddCurrentX(object, work->data->dx);
    MapObject_AddCurrentY(object, work->data->dy);
    MapObject_AddCurrentZ(object, work->data->dz);
    if (work->data->dx << 4 > 0) {
        rounded = (float)(work->data->dx << 16) + 0.5f;
    } else {
        rounded = (float)(work->data->dx << 16) - 0.5f;
    }
    position.x = work->data->origin.x + (int)rounded;
    if (work->data->dy << 4 > 0) {
        rounded = (float)(work->data->dy << 16) + 0.5f;
    } else {
        rounded = (float)(work->data->dy << 16) - 0.5f;
    }
    position.y = work->data->origin.y + (int)rounded;
    if (work->data->dz << 4 > 0) {
        rounded = (float)(work->data->dz << 16) + 0.5f;
    } else {
        rounded = (float)(work->data->dz << 16) - 0.5f;
    }
    position.z = work->data->origin.z + (int)rounded;
    MapObject_SetPositionVector(object, &position);
    MapObject_UpdateHeight(object);
    MapObject_SetFlagsBits(object, (MapObjectFlagBits)(MAPOBJECTFLAG_UNK17 | MAPOBJECTFLAG_END_MOVEMENT));
    MapObject_SetPreviousCoordsToCurrent(object);
    MapObject_CallDrawUpdate(object);
    MapObject_SetAnimation(object, MAP_OBJECT_ANIM_STAND);
    MapObject_IncrementMovementStep(object);
    if (!MapObject_CheckVisible(object)) {
        PlaySE(SEQ_SE_DP_SUTYA2);
    }
    Heap_Free(work->data);
    return FALSE;
}

BOOL MapObjectMovementCmd105_Step0(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, 0, 1, 5, DIR_SOUTH, 15, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd105_Step2(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, 4, 0, 0, DIR_EAST, 12, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd105_Step4(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, 0, 0, -5, DIR_NORTH, 15, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd105_Step6(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, -2, 0, -3, DIR_NORTH, 9, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd105_Step8(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, -4, 1, -4, DIR_WEST, 12, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd106_Step0(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, 2, 1, 0, DIR_EAST, 6, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd106_Step2(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, -1, 0, 5, DIR_SOUTH, 12, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd106_Step4(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, -3, 0, 0, DIR_WEST, 6, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd106_Step6(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, -3, 0, 0, DIR_WEST, 9, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd107_Step0(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, 3, 1, -1, DIR_EAST, 6, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd107_Step2(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, 0, 0, 4, DIR_SOUTH, 9, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd107_Step4(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, -4, 0, 0, DIR_WEST, 12, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd107_Step6(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, 0, -1, -4, DIR_NORTH, 6, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd107_Step8(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, 1, 1, -3, DIR_NORTH, 9, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd107_Step10(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, 3, 0, 0, DIR_EAST, 9, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd107_Step12(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, 0, 0, 4, DIR_SOUTH, 12, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd109_Step12(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, 0, 0, 5, DIR_SOUTH, 12, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd108_Step0(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, 2, 1, 5, DIR_SOUTH, 9, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd110_Step0(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, 2, 1, 4, DIR_SOUTH, 9, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd108_Step2(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, 1, 0, 5, DIR_SOUTH, 12, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd111_Step0(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, 0, 0, 2, DIR_SOUTH, 6, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd111_Step2(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, 2, 0, 0, DIR_EAST, 6, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd111_Step4(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, 3, 0, 0, DIR_EAST, 9, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd111_Step6(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, 0, 0, 2, DIR_SOUTH, 6, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd111_Step10(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, -3, 0, 0, DIR_WEST, 9, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd111_Step14(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, 0, 0, -2, DIR_NORTH, 6, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd111_Step16(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, 0, 0, -3, DIR_NORTH, 9, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd111_Step18(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, 3, 0, 1, DIR_SOUTH, 9, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}

BOOL MapObjectMovementCmd112_Step0(LocalMapObject *object) {
    MapObject_StartJumpByOffsetMovement(object, 4, 0, 0, DIR_EAST, 9, MAP_OBJECT_ANIM_WALK_NORMAL);
    return TRUE;
}
#if defined(SDK_PORT)

/* Native pointer tables from asm/map_object_movement_tables.s. */
extern const MovementCommandFunc gMovementCmdSteps_000[];
extern const MovementCommandFunc gMovementCmdSteps_001[];
extern const MovementCommandFunc gMovementCmdSteps_002[];
extern const MovementCommandFunc gMovementCmdSteps_003[];
extern const MovementCommandFunc gMovementCmdSteps_004[];
extern const MovementCommandFunc gMovementCmdSteps_005[];
extern const MovementCommandFunc gMovementCmdSteps_006[];
extern const MovementCommandFunc gMovementCmdSteps_007[];
extern const MovementCommandFunc gMovementCmdSteps_008[];
extern const MovementCommandFunc gMovementCmdSteps_009[];
extern const MovementCommandFunc gMovementCmdSteps_010[];
extern const MovementCommandFunc gMovementCmdSteps_011[];
extern const MovementCommandFunc gMovementCmdSteps_012[];
extern const MovementCommandFunc gMovementCmdSteps_013[];
extern const MovementCommandFunc gMovementCmdSteps_014[];
extern const MovementCommandFunc gMovementCmdSteps_015[];
extern const MovementCommandFunc gMovementCmdSteps_016[];
extern const MovementCommandFunc gMovementCmdSteps_017[];
extern const MovementCommandFunc gMovementCmdSteps_018[];
extern const MovementCommandFunc gMovementCmdSteps_019[];
extern const MovementCommandFunc gMovementCmdSteps_020[];
extern const MovementCommandFunc gMovementCmdSteps_021[];
extern const MovementCommandFunc gMovementCmdSteps_022[];
extern const MovementCommandFunc gMovementCmdSteps_023[];
extern const MovementCommandFunc gMovementCmdSteps_024[];
extern const MovementCommandFunc gMovementCmdSteps_025[];
extern const MovementCommandFunc gMovementCmdSteps_026[];
extern const MovementCommandFunc gMovementCmdSteps_027[];
extern const MovementCommandFunc gMovementCmdSteps_028[];
extern const MovementCommandFunc gMovementCmdSteps_029[];
extern const MovementCommandFunc gMovementCmdSteps_030[];
extern const MovementCommandFunc gMovementCmdSteps_031[];
extern const MovementCommandFunc gMovementCmdSteps_032[];
extern const MovementCommandFunc gMovementCmdSteps_033[];
extern const MovementCommandFunc gMovementCmdSteps_034[];
extern const MovementCommandFunc gMovementCmdSteps_035[];
extern const MovementCommandFunc gMovementCmdSteps_036[];
extern const MovementCommandFunc gMovementCmdSteps_037[];
extern const MovementCommandFunc gMovementCmdSteps_038[];
extern const MovementCommandFunc gMovementCmdSteps_039[];
extern const MovementCommandFunc gMovementCmdSteps_040[];
extern const MovementCommandFunc gMovementCmdSteps_041[];
extern const MovementCommandFunc gMovementCmdSteps_042[];
extern const MovementCommandFunc gMovementCmdSteps_043[];
extern const MovementCommandFunc gMovementCmdSteps_044[];
extern const MovementCommandFunc gMovementCmdSteps_045[];
extern const MovementCommandFunc gMovementCmdSteps_046[];
extern const MovementCommandFunc gMovementCmdSteps_047[];
extern const MovementCommandFunc gMovementCmdSteps_048[];
extern const MovementCommandFunc gMovementCmdSteps_049[];
extern const MovementCommandFunc gMovementCmdSteps_050[];
extern const MovementCommandFunc gMovementCmdSteps_051[];
extern const MovementCommandFunc gMovementCmdSteps_052[];
extern const MovementCommandFunc gMovementCmdSteps_053[];
extern const MovementCommandFunc gMovementCmdSteps_054[];
extern const MovementCommandFunc gMovementCmdSteps_055[];
extern const MovementCommandFunc gMovementCmdSteps_056[];
extern const MovementCommandFunc gMovementCmdSteps_057[];
extern const MovementCommandFunc gMovementCmdSteps_058[];
extern const MovementCommandFunc gMovementCmdSteps_059[];
extern const MovementCommandFunc gMovementCmdSteps_060[];
extern const MovementCommandFunc gMovementCmdSteps_061[];
extern const MovementCommandFunc gMovementCmdSteps_062[];
extern const MovementCommandFunc gMovementCmdSteps_063[];
extern const MovementCommandFunc gMovementCmdSteps_064[];
extern const MovementCommandFunc gMovementCmdSteps_065[];
extern const MovementCommandFunc gMovementCmdSteps_066[];
extern const MovementCommandFunc gMovementCmdSteps_067[];
extern const MovementCommandFunc gMovementCmdSteps_068[];
extern const MovementCommandFunc gMovementCmdSteps_069[];
extern const MovementCommandFunc gMovementCmdSteps_070[];
extern const MovementCommandFunc gMovementCmdSteps_071[];
extern const MovementCommandFunc gMovementCmdSteps_072[];
extern const MovementCommandFunc gMovementCmdSteps_073[];
extern const MovementCommandFunc gMovementCmdSteps_074[];
extern const MovementCommandFunc gMovementCmdSteps_075[];
extern const MovementCommandFunc gMovementCmdSteps_076[];
extern const MovementCommandFunc gMovementCmdSteps_077[];
extern const MovementCommandFunc gMovementCmdSteps_078[];
extern const MovementCommandFunc gMovementCmdSteps_079[];
extern const MovementCommandFunc gMovementCmdSteps_080[];
extern const MovementCommandFunc gMovementCmdSteps_081[];
extern const MovementCommandFunc gMovementCmdSteps_082[];
extern const MovementCommandFunc gMovementCmdSteps_083[];
extern const MovementCommandFunc gMovementCmdSteps_084[];
extern const MovementCommandFunc gMovementCmdSteps_085[];
extern const MovementCommandFunc gMovementCmdSteps_086[];
extern const MovementCommandFunc gMovementCmdSteps_087[];
extern const MovementCommandFunc gMovementCmdSteps_088[];
extern const MovementCommandFunc gMovementCmdSteps_089[];
extern const MovementCommandFunc gMovementCmdSteps_090[];
extern const MovementCommandFunc gMovementCmdSteps_091[];
extern const MovementCommandFunc gMovementCmdSteps_092[];
extern const MovementCommandFunc gMovementCmdSteps_093[];
extern const MovementCommandFunc gMovementCmdSteps_094[];
extern const MovementCommandFunc gMovementCmdSteps_095[];
extern const MovementCommandFunc gMovementCmdSteps_096[];
extern const MovementCommandFunc gMovementCmdSteps_097[];
extern const MovementCommandFunc gMovementCmdSteps_098[];
extern const MovementCommandFunc gMovementCmdSteps_099[];
extern const MovementCommandFunc gMovementCmdSteps_100[];
extern const MovementCommandFunc gMovementCmdSteps_101[];
extern const MovementCommandFunc gMovementCmdSteps_102[];
extern const MovementCommandFunc gMovementCmdSteps_103[];
extern const MovementCommandFunc gMovementCmdSteps_104[];
extern const MovementCommandFunc gMovementCmdSteps_105[];
extern const MovementCommandFunc gMovementCmdSteps_106[];
extern const MovementCommandFunc gMovementCmdSteps_107[];
extern const MovementCommandFunc gMovementCmdSteps_108[];
extern const MovementCommandFunc gMovementCmdSteps_109[];
extern const MovementCommandFunc gMovementCmdSteps_110[];
extern const MovementCommandFunc gMovementCmdSteps_111[];
extern const MovementCommandFunc gMovementCmdSteps_112[];
const MovementCommandFunc *const gMovementCmdTable[] = {
    gMovementCmdSteps_000,
    gMovementCmdSteps_001,
    gMovementCmdSteps_002,
    gMovementCmdSteps_003,
    gMovementCmdSteps_004,
    gMovementCmdSteps_005,
    gMovementCmdSteps_006,
    gMovementCmdSteps_007,
    gMovementCmdSteps_008,
    gMovementCmdSteps_009,
    gMovementCmdSteps_010,
    gMovementCmdSteps_011,
    gMovementCmdSteps_012,
    gMovementCmdSteps_013,
    gMovementCmdSteps_014,
    gMovementCmdSteps_015,
    gMovementCmdSteps_016,
    gMovementCmdSteps_017,
    gMovementCmdSteps_018,
    gMovementCmdSteps_019,
    gMovementCmdSteps_020,
    gMovementCmdSteps_021,
    gMovementCmdSteps_022,
    gMovementCmdSteps_023,
    gMovementCmdSteps_024,
    gMovementCmdSteps_025,
    gMovementCmdSteps_026,
    gMovementCmdSteps_027,
    gMovementCmdSteps_028,
    gMovementCmdSteps_029,
    gMovementCmdSteps_030,
    gMovementCmdSteps_031,
    gMovementCmdSteps_032,
    gMovementCmdSteps_033,
    gMovementCmdSteps_034,
    gMovementCmdSteps_035,
    gMovementCmdSteps_036,
    gMovementCmdSteps_037,
    gMovementCmdSteps_038,
    gMovementCmdSteps_039,
    gMovementCmdSteps_040,
    gMovementCmdSteps_041,
    gMovementCmdSteps_042,
    gMovementCmdSteps_043,
    gMovementCmdSteps_044,
    gMovementCmdSteps_045,
    gMovementCmdSteps_046,
    gMovementCmdSteps_047,
    gMovementCmdSteps_048,
    gMovementCmdSteps_049,
    gMovementCmdSteps_050,
    gMovementCmdSteps_051,
    gMovementCmdSteps_052,
    gMovementCmdSteps_053,
    gMovementCmdSteps_054,
    gMovementCmdSteps_055,
    gMovementCmdSteps_056,
    gMovementCmdSteps_057,
    gMovementCmdSteps_058,
    gMovementCmdSteps_059,
    gMovementCmdSteps_060,
    gMovementCmdSteps_061,
    gMovementCmdSteps_062,
    gMovementCmdSteps_063,
    gMovementCmdSteps_064,
    gMovementCmdSteps_065,
    gMovementCmdSteps_066,
    gMovementCmdSteps_067,
    gMovementCmdSteps_068,
    gMovementCmdSteps_069,
    gMovementCmdSteps_070,
    gMovementCmdSteps_071,
    gMovementCmdSteps_072,
    gMovementCmdSteps_073,
    gMovementCmdSteps_074,
    gMovementCmdSteps_075,
    gMovementCmdSteps_076,
    gMovementCmdSteps_077,
    gMovementCmdSteps_078,
    gMovementCmdSteps_079,
    gMovementCmdSteps_080,
    gMovementCmdSteps_081,
    gMovementCmdSteps_082,
    gMovementCmdSteps_083,
    gMovementCmdSteps_084,
    gMovementCmdSteps_085,
    gMovementCmdSteps_086,
    gMovementCmdSteps_087,
    gMovementCmdSteps_088,
    gMovementCmdSteps_089,
    gMovementCmdSteps_090,
    gMovementCmdSteps_091,
    gMovementCmdSteps_092,
    gMovementCmdSteps_093,
    gMovementCmdSteps_094,
    gMovementCmdSteps_095,
    gMovementCmdSteps_096,
    gMovementCmdSteps_097,
    gMovementCmdSteps_098,
    gMovementCmdSteps_099,
    gMovementCmdSteps_100,
    gMovementCmdSteps_101,
    gMovementCmdSteps_102,
    gMovementCmdSteps_103,
    gMovementCmdSteps_104,
    gMovementCmdSteps_105,
    gMovementCmdSteps_106,
    gMovementCmdSteps_107,
    gMovementCmdSteps_108,
    gMovementCmdSteps_109,
    gMovementCmdSteps_110,
    gMovementCmdSteps_111,
    gMovementCmdSteps_112,
};
extern const u32 sFaceMovements[];
extern const u32 sWalkSlowerMovements[];
extern const u32 sWalkSlowMovements[];
extern const u32 sWalkNormalMovements[];
extern const u32 sWalkFastMovements[];
extern const u32 sWalkFasterMovements[];
extern const u32 sWalkOnSpotSlowerMovements[];
extern const u32 sWalkOnSpotSlowMovements[];
extern const u32 sWalkOnSpotNormalMovements[];
extern const u32 sWalkOnSpotFastMovements[];
extern const u32 sWalkOnSpotFasterMovements[];
extern const u32 sJumpOnSpotSlowMovements[];
extern const u32 sJumpOnSpotFastMovements[];
extern const u32 sJumpNearFastMovements[];
extern const u32 sJumpFarMovements[];
extern const u32 sWalkSlightlyFastMovements[];
extern const u32 sWalkSlightlyFasterMovements[];
extern const u32 sWalkFastestMovements[];
extern const u32 sRunMovements[];
extern const u32 sJumpNearSlowMovements[];
extern const u32 sJumpFartherMovements[];
extern const u32 sWalkEverSoSlightlyFastMovements[];
const u32 *const gMovementsByDirection[23] = {
    sFaceMovements,
    sWalkSlowerMovements,
    sWalkSlowMovements,
    sWalkNormalMovements,
    sWalkFastMovements,
    sWalkFasterMovements,
    sWalkOnSpotSlowerMovements,
    sWalkOnSpotSlowMovements,
    sWalkOnSpotNormalMovements,
    sWalkOnSpotFastMovements,
    sWalkOnSpotFasterMovements,
    sJumpOnSpotSlowMovements,
    sJumpOnSpotFastMovements,
    sJumpNearFastMovements,
    sJumpFarMovements,
    sWalkSlightlyFastMovements,
    sWalkSlightlyFasterMovements,
    sWalkFastestMovements,
    sRunMovements,
    sJumpNearSlowMovements,
    sJumpFartherMovements,
    sWalkEverSoSlightlyFastMovements,
    0,
};

/* Native tables from asm/unk_data_020FD978.s; adapters give differing originals the table's signature. */
const MovementCommandFunc gMovementCmdSteps_001[2] = {
    MapObjectMovementCmd001_Step0,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_003[2] = {
    MapObjectMovementCmd003_Step0,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_073[2] = {
    MapObjectMovementCmd073_Step0,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_074[2] = {
    MapObjectMovementCmd074_Step0,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_072[2] = {
    MapObjectMovementCmd072_Step0,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_070[2] = {
    MapObjectMovementCmd070_Step0,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_069[2] = {
    MapObjectMovementCmd069_Step0,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_000[2] = {
    MapObjectMovementCmd000_Step0,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_002[2] = {
    MapObjectMovementCmd002_Step0,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_071[2] = {
    MapObjectMovementCmd071_Step0,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_010[3] = {
    MapObjectMovementCmd010_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_089[3] = {
    MapObjectMovementCmd089_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_083[3] = {
    MapObjectMovementCmd083_Step0,
    MapObjectMovementCmd082_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_006[3] = {
    MapObjectMovementCmd006_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_091[3] = {
    MapObjectMovementCmd091_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_004[3] = {
    MapObjectMovementCmd004_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_011[3] = {
    MapObjectMovementCmd011_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};

/* Native tables from asm/map_object_movement_command_steps.s; adapters give differing originals the table's signature. */
const MovementCommandFunc gMovementCmdSteps_098[3] = {
    MapObjectMovementCmd098_Step0,
    MapObjectMovementCmd098_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_099[3] = {
    MapObjectMovementCmd099_Step0,
    MapObjectMovementCmd098_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_097[3] = {
    MapObjectMovementCmd097_Step0,
    MapObjectMovementCmd098_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_096[3] = {
    MapObjectMovementCmd096_Step0,
    MapObjectMovementCmd098_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_064[3] = {
    MapObjectMovementCmd064_Step0,
    MapObjectMovementCmd064_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_090[3] = {
    MapObjectMovementCmd090_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_060[3] = {
    MapObjectMovementCmd060_Step0,
    MapObjectMovementCmd064_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_088[3] = {
    MapObjectMovementCmd088_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_092[3] = {
    MapObjectMovementCmd092_Step0,
    MapObjectMovementCmd092_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_082[3] = {
    MapObjectMovementCmd082_Step0,
    MapObjectMovementCmd082_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_056[3] = {
    MapObjectMovementCmd056_Step0,
    MapObjectMovementCmd092_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_080[3] = {
    MapObjectMovementCmd080_Step0,
    MapObjectMovementCmd082_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_055[3] = {
    MapObjectMovementCmd055_Step0,
    MapObjectMovementCmd092_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_007[3] = {
    MapObjectMovementCmd007_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_100[3] = {
    MapObjectMovementCmd100_Step0,
    MapObjectMovementCmd100_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_075[3] = {
    MapObjectMovementCmd075_Step0,
    MapObjectMovementCmd075_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_103[3] = {
    MapObjectMovementCmd103_Step0,
    MapObjectMovementCmd075_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_101[3] = {
    MapObjectMovementCmd101_Step0,
    MapObjectMovementCmd101_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_040[3] = {
    MapObjectMovementCmd040_Step0,
    MapObjectMovementCmd040_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_038[3] = {
    MapObjectMovementCmd038_Step0,
    MapObjectMovementCmd040_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_036[3] = {
    MapObjectMovementCmd036_Step0,
    MapObjectMovementCmd040_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_034[3] = {
    MapObjectMovementCmd034_Step0,
    MapObjectMovementCmd040_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_032[3] = {
    MapObjectMovementCmd032_Step0,
    MapObjectMovementCmd040_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_030[3] = {
    MapObjectMovementCmd030_Step0,
    MapObjectMovementCmd040_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_068[3] = {
    MapObjectMovementCmd068_Step0,
    MapObjectMovementCmd068_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_067[3] = {
    MapObjectMovementCmd067_Step0,
    MapObjectMovementCmd067_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_066[3] = {
    MapObjectMovementCmd066_Step0,
    MapObjectMovementCmd064_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_065[3] = {
    MapObjectMovementCmd065_Step0,
    MapObjectMovementCmd064_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_084[3] = {
    MapObjectMovementCmd084_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_063[3] = {
    MapObjectMovementCmd063_Step0,
    MapObjectMovementCmd064_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_062[3] = {
    MapObjectMovementCmd062_Step0,
    MapObjectMovementCmd064_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_061[3] = {
    MapObjectMovementCmd061_Step0,
    MapObjectMovementCmd064_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_016[3] = {
    MapObjectMovementCmd016_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_095[3] = {
    MapObjectMovementCmd095_Step0,
    MapObjectMovementCmd092_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_094[3] = {
    MapObjectMovementCmd094_Step0,
    MapObjectMovementCmd092_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_093[3] = {
    MapObjectMovementCmd093_Step0,
    MapObjectMovementCmd092_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_008[3] = {
    MapObjectMovementCmd008_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_059[3] = {
    MapObjectMovementCmd059_Step0,
    MapObjectMovementCmd092_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_058[3] = {
    MapObjectMovementCmd058_Step0,
    MapObjectMovementCmd092_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_057[3] = {
    MapObjectMovementCmd057_Step0,
    MapObjectMovementCmd092_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_081[3] = {
    MapObjectMovementCmd081_Step0,
    MapObjectMovementCmd082_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_054[3] = {
    MapObjectMovementCmd054_Step0,
    MapObjectMovementCmd092_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_053[3] = {
    MapObjectMovementCmd053_Step0,
    MapObjectMovementCmd092_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_076[3] = {
    MapObjectMovementCmd076_Step0,
    MapObjectMovementCmd076_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_047[3] = {
    MapObjectMovementCmd047_Step0,
    MapObjectMovementCmd092_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_048[3] = {
    MapObjectMovementCmd048_Step0,
    MapObjectMovementCmd092_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_077[3] = {
    MapObjectMovementCmd077_Step0,
    MapObjectMovementCmd076_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_102[3] = {
    MapObjectMovementCmd102_Step0,
    MapObjectMovementCmd102_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_049[3] = {
    MapObjectMovementCmd049_Step0,
    MapObjectMovementCmd092_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_046[3] = {
    MapObjectMovementCmd046_Step0,
    MapObjectMovementCmd092_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_045[3] = {
    MapObjectMovementCmd045_Step0,
    MapObjectMovementCmd092_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_044[3] = {
    MapObjectMovementCmd044_Step0,
    MapObjectMovementCmd092_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_043[3] = {
    MapObjectMovementCmd043_Step0,
    MapObjectMovementCmd040_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_042[3] = {
    MapObjectMovementCmd042_Step0,
    MapObjectMovementCmd040_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_041[3] = {
    MapObjectMovementCmd041_Step0,
    MapObjectMovementCmd040_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_050[3] = {
    MapObjectMovementCmd050_Step0,
    MapObjectMovementCmd092_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_039[3] = {
    MapObjectMovementCmd039_Step0,
    MapObjectMovementCmd040_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_078[3] = {
    MapObjectMovementCmd078_Step0,
    MapObjectMovementCmd076_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_037[3] = {
    MapObjectMovementCmd037_Step0,
    MapObjectMovementCmd040_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_051[3] = {
    MapObjectMovementCmd051_Step0,
    MapObjectMovementCmd092_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_035[3] = {
    MapObjectMovementCmd035_Step0,
    MapObjectMovementCmd040_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_052[3] = {
    MapObjectMovementCmd052_Step0,
    MapObjectMovementCmd092_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_033[3] = {
    MapObjectMovementCmd033_Step0,
    MapObjectMovementCmd040_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_079[3] = {
    MapObjectMovementCmd079_Step0,
    MapObjectMovementCmd076_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_031[3] = {
    MapObjectMovementCmd031_Step0,
    MapObjectMovementCmd040_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_104[3] = {
    MapObjectMovementCmd104_Step0,
    MapObjectMovementCmd102_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_029[3] = {
    MapObjectMovementCmd029_Step0,
    MapObjectMovementCmd040_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_028[3] = {
    MapObjectMovementCmd028_Step0,
    MapObjectMovementCmd040_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_027[3] = {
    MapObjectMovementCmd027_Step0,
    MapObjectMovementCmd040_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_026[3] = {
    MapObjectMovementCmd026_Step0,
    MapObjectMovementCmd040_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_025[3] = {
    MapObjectMovementCmd025_Step0,
    MapObjectMovementCmd040_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_024[3] = {
    MapObjectMovementCmd024_Step0,
    MapObjectMovementCmd040_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_087[3] = {
    MapObjectMovementCmd087_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_086[3] = {
    MapObjectMovementCmd086_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_085[3] = {
    MapObjectMovementCmd085_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_005[3] = {
    MapObjectMovementCmd005_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_023[3] = {
    MapObjectMovementCmd023_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_022[3] = {
    MapObjectMovementCmd022_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_021[3] = {
    MapObjectMovementCmd021_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_020[3] = {
    MapObjectMovementCmd020_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_019[3] = {
    MapObjectMovementCmd019_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_018[3] = {
    MapObjectMovementCmd018_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_017[3] = {
    MapObjectMovementCmd017_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_009[3] = {
    MapObjectMovementCmd009_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_015[3] = {
    MapObjectMovementCmd015_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_014[3] = {
    MapObjectMovementCmd014_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_013[3] = {
    MapObjectMovementCmd013_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_012[3] = {
    MapObjectMovementCmd012_Step0,
    MapObjectMovementCmd090_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_108[5] = {
    MapObjectMovementCmd108_Step0,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd108_Step2,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_110[5] = {
    MapObjectMovementCmd110_Step0,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd108_Step2,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_112[7] = {
    MapObjectMovementCmd112_Step0,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd112_Step0,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd112_Step0,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_106[9] = {
    MapObjectMovementCmd106_Step0,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd106_Step2,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd106_Step4,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd106_Step6,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_105[11] = {
    MapObjectMovementCmd105_Step0,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd105_Step2,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd105_Step4,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd105_Step6,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd105_Step8,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_107[15] = {
    MapObjectMovementCmd107_Step0,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd107_Step2,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd107_Step4,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd107_Step6,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd107_Step8,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd107_Step10,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd107_Step12,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_109[15] = {
    MapObjectMovementCmd107_Step0,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd107_Step2,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd107_Step4,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd107_Step6,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd107_Step8,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd107_Step10,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd109_Step12,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd098_Step2,
};
const MovementCommandFunc gMovementCmdSteps_111[21] = {
    MapObjectMovementCmd111_Step0,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd111_Step2,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd111_Step4,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd111_Step6,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd111_Step6,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd111_Step10,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd111_Step10,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd111_Step14,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd111_Step16,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd111_Step18,
    MapObjectMovementCmd108_Step1,
    MapObjectMovementCmd098_Step2,
};

/* Native tables from asm/map_object_movement_commands_data.s; adapters give differing originals the table's signature. */
const MovementScriptStateFunc gMovementScriptMachineStateFuncs[6] = {
    MovementScriptMachineSub_Init,
    MovementScriptMachineSub_WaitReady,
    MovementScriptMachineSub_SetMovementCommand,
    MovementScriptMachineSub_WaitMovementCommand,
    MovementScriptMachineSub_LoopCheck,
    MovementScriptMachineSub_Done,
};

/* Native pointer table from asm/map_object_movement_commands_data.s. */
extern const fx32 gJumpHeightTable0[];
extern const fx32 gJumpHeightTable1[];
extern const fx32 gJumpHeightTable2[];
const fx32 *gJumpHeightTables[] = {
    gJumpHeightTable0,
    gJumpHeightTable1,
    gJumpHeightTable2,
};
#endif
