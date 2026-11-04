#include "pokepic_anim.h"

#include "global.h"

#include "filesystem.h"
#include "heap.h"
#include "pokepic.h"
#include "pokepic_anim_internal.h"
#include "sys_task_api.h"

// Animation scripts of Pokemon pictures: a word stream of commands that work on 8 variables and on a transform
// (position offsets, scale, rotation) applied to the Pokepic; up to 4 motions animate one transform field each.

// Argument tokens of the script commands
enum PokepicAnimTransformField {
    POKEPIC_ANIM_FIELD_X = 8,
    POKEPIC_ANIM_FIELD_Y,
    POKEPIC_ANIM_FIELD_DX,
    POKEPIC_ANIM_FIELD_DY,
    POKEPIC_ANIM_FIELD_SCALE_X,
    POKEPIC_ANIM_FIELD_SCALE_Y,
    POKEPIC_ANIM_FIELD_ROTATION,
};

enum PokepicAnimCompareResult {
    POKEPIC_ANIM_COMPARE_LESS = 15,
    POKEPIC_ANIM_COMPARE_GREATER,
    POKEPIC_ANIM_COMPARE_EQUAL,
};

enum PokepicAnimOperand {
    POKEPIC_ANIM_ARITH_CONST = 18, // second operand of a variable arithmetic command
    POKEPIC_ANIM_ARITH_VAR,
    POKEPIC_ANIM_OPERAND_CONST,    // operand of the other commands
    POKEPIC_ANIM_OPERAND_VAR,
};

enum PokepicAnimWriteOp {
    POKEPIC_ANIM_OP_SET = 22,
    POKEPIC_ANIM_OP_ADD,
    POKEPIC_MOTION_OP_SET,      // field = motion value
    POKEPIC_MOTION_OP_SET_BASE, // field = field at the start of the motion + motion value
    POKEPIC_MOTION_OP_ADD,      // field += motion value
};

enum PokepicAnimScaleMode {
    POKEPIC_SCALE_KEEP_FEET_WHEN_SHRINKING = 27,
    POKEPIC_SCALE_CENTERED,
    POKEPIC_SCALE_KEEP_FEET,
};

enum PokepicMotionWaveform {
    POKEPIC_WAVE_SIN = 30,
    POKEPIC_WAVE_COS,
    POKEPIC_WAVE_NEG_SIN,
    POKEPIC_WAVE_NEG_COS,
};

enum PokepicMotionTarget {
    POKEPIC_MOTION_TARGET_DX = 35,
    POKEPIC_MOTION_TARGET_DY,
    POKEPIC_MOTION_TARGET_SCALE_X,
    POKEPIC_MOTION_TARGET_SCALE_Y,
    POKEPIC_MOTION_TARGET_ROTATION,
};

enum PokepicMotionKind {
    POKEPIC_MOTION_WAVE,
    POKEPIC_MOTION_WAVE_OVER_DURATION,
    POKEPIC_MOTION_ACCEL,
    POKEPIC_MOTION_LINEAR,
    POKEPIC_MOTION_ACCEL_TO_LIMIT,
};

enum PokepicScriptAnimState {
    POKEPIC_SCRIPT_ANIM_RUNNING,
    POKEPIC_SCRIPT_ANIM_IDLE,
};

typedef struct PokepicScriptAnim PokepicScriptAnim;

typedef struct PokepicMotion {
    BOOL active;

    union { // read from the script; the argument count is per motion type (sPokepicMotionTypes)
        int args[7];

        struct {
            int waveform;
            int target;
            int amplitude;
            int angleStep; // angle range for PokepicMotion_UpdateWaveOverDuration
            int startAngle;
            int duration;
            int frame;
        } wave;

        struct {
            int target;
            int velocity;
            int acceleration;
            int durationOrLimit;
            int frame;
        } accel;

        struct {
            int target;
            int total;
            int duration;
            int frame;
        } linear;
    };

    u8 unk_20[4];
    int *value; // accumulator in values
    int *dest;  // the PokepicScriptAnim field the motion drives
    u8 op;      // enum PokepicAnimWriteOp
    u8 delay;   // frames before the first update
    u8 unk_2E[2];
    int base; // *dest when the motion started
    u8 unk_34[8];
    int values[5]; // one per target: dx, dy, scaleX, scaleY, rotation
    void (*update)(struct PokepicMotion *motion, PokepicScriptAnim *anim);
} PokepicMotion;
#if defined(SDK_PORT)

typedef u32 PokepicScriptWord;
#endif

struct PokepicScriptAnim {
    Pokepic *pic;
    SysTask *task;
    u32 *script;
    u32 *cursor; // current script word
    BOOL active;
    u32 scriptId; // NARC member
    BOOL yield;   // set by a command that ends the frame
    BOOL done;    // the script ended
    enum PokepicScriptAnimState state;
    s32 vars[8];
    s32 commandCount; // commands run this frame
    s32 loopCount;
    s32 loopIndex;
    u32 *loopStart;
    s32 delay; // frames to wait
    s32 baseX; // position of the picture when the animation started
    s32 baseY;
    s32 x;
    s32 y;
    s32 dx; // offsets, usually driven by motions
    s32 dy;
    s32 scaleX; // added to 0x100
    s32 scaleY;
    s32 rotation;
    PokepicMotion motions[4];
    u8 reverseX;
    u8 runMotions;
    u8 scaleMode;
    u8 waitPaletteFade;
};

struct PokepicAnimManager {
    PokepicScriptAnim *anims;
    enum HeapID heapId;
    u8 allowReverse;
    u8 count;
    u8 unk_0A[2];
};

typedef struct PokepicMotionType {
    void (*update)(PokepicMotion *motion, PokepicScriptAnim *anim);
    int argCount;
    int targetArg;
} PokepicMotionType;

typedef void (*PokepicScriptAnimCmd)(PokepicScriptAnim *anim);

static void PokepicScriptAnim_Task(SysTask *task, void *data);
static void PokepicScriptAnim_RunFrame(PokepicScriptAnim *anim);
static u32 PokepicScriptAnim_GetScriptWordAt(u32 *array, int index, int expected);
static u32 PokepicScriptAnim_GetScriptWordChecked(u32 *array, int expected);
static u32 PokepicScriptAnim_GetScriptWord(u32 *array);
static PokepicMotion *PokepicScriptAnim_AllocMotion(PokepicScriptAnim *anim, int type);
static void PokepicScriptAnim_ReadArg(PokepicScriptAnim *anim, s32 *out);
static void PokepicScriptAnim_ReadArgByte(PokepicScriptAnim *anim, u8 *out);
static void PokepicScriptAnim_ReadVarIndex(PokepicScriptAnim *anim, u8 *out);
static void PokepicScriptAnim_ReadVarIndexPair(PokepicScriptAnim *anim, u8 *first, u8 *second);
static void PokepicScriptAnim_ReadDestVarAndOperands(PokepicScriptAnim *anim, u8 *dest, s32 *left, s32 *right);
static void PokepicScriptAnim_ReadDestAndOperands(PokepicScriptAnim *anim, u8 *dest, s32 *left, s32 *right);
static void PokepicScriptAnim_ReadDestAngleAndAmplitude(PokepicScriptAnim *anim, u8 *dest, s32 *angle, s32 *amplitude);
static enum PokepicAnimCompareResult PokepicScriptAnim_Compare(const s32 *left, const s32 *right);
static void PokepicScriptAnim_AdjustYForScale(PokepicScriptAnim *anim);
static void PokepicMotion_ApplyValue(int op, int *base, int *value, int *dest);
static void PokepicMotion_SetTarget(u8 target, PokepicMotion *motion, PokepicScriptAnim *anim);
static void PokepicScriptAnim_StartMotion(PokepicScriptAnim *anim, int type);

static void PokepicScriptAnimCmd_End(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_Yield(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_ResetTransform(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_CopyVar(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_AddVar(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_MulVar(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_SubVar(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_DivVar(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_ModVar(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_SetVarIfCompare(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_SetVar(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_LoopStart(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_LoopEnd(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_SetAttrFromVar(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_AddAttrFromVar(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_SetOrAddAttr(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_Sin(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_Cos(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_SetPosFromVar(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_AddPosFromVar(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_SetOrAddTransform(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_ApplyPosition(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_ApplyTransform(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_SetOffsetFromVar(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_Wait(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_StartPaletteFade(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_WaitPaletteFade(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_RunMotions(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_SetScaleMode(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_StartWaveMotion(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_StartWaveMotionOverDuration(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_StartAccelMotion(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_StartLinearMotion(PokepicScriptAnim *anim);
static void PokepicScriptAnimCmd_StartAccelMotionToLimit(PokepicScriptAnim *anim);
static void PokepicMotion_UpdateWave(PokepicMotion *motion, PokepicScriptAnim *anim);
static void PokepicMotion_UpdateWaveOverDuration(PokepicMotion *motion, PokepicScriptAnim *anim);
static void PokepicMotion_UpdateAccel(PokepicMotion *motion, PokepicScriptAnim *anim);
static void PokepicMotion_UpdateLinear(PokepicMotion *motion, PokepicScriptAnim *anim);
static void PokepicMotion_UpdateAccelToLimit(PokepicMotion *motion, PokepicScriptAnim *anim);

static const PokepicMotionType sPokepicMotionTypes[] = {
    { PokepicMotion_UpdateWave,             6, 1 },
    { PokepicMotion_UpdateWaveOverDuration, 6, 1 },
    { PokepicMotion_UpdateAccel,            4, 0 },
    { PokepicMotion_UpdateLinear,           3, 0 },
    { PokepicMotion_UpdateAccelToLimit,     4, 0 },
};

static const PokepicScriptAnimCmd sPokepicAnimCommands[] = {
    PokepicScriptAnimCmd_End,
    PokepicScriptAnimCmd_Yield,
    PokepicScriptAnimCmd_ResetTransform,
    PokepicScriptAnimCmd_SetVarIfCompare,
    PokepicScriptAnimCmd_SetVar,
    PokepicScriptAnimCmd_CopyVar,
    PokepicScriptAnimCmd_AddVar,
    PokepicScriptAnimCmd_MulVar,
    PokepicScriptAnimCmd_SubVar,
    PokepicScriptAnimCmd_DivVar,
    PokepicScriptAnimCmd_ModVar,
    PokepicScriptAnimCmd_LoopStart,
    PokepicScriptAnimCmd_LoopEnd,
    PokepicScriptAnimCmd_SetAttrFromVar,
    PokepicScriptAnimCmd_AddAttrFromVar,
    PokepicScriptAnimCmd_SetOrAddAttr,
    PokepicScriptAnimCmd_Sin,
    PokepicScriptAnimCmd_Cos,
    PokepicScriptAnimCmd_SetPosFromVar,
    PokepicScriptAnimCmd_AddPosFromVar,
    PokepicScriptAnimCmd_SetOrAddTransform,
    PokepicScriptAnimCmd_ApplyPosition,
    PokepicScriptAnimCmd_ApplyTransform,
    PokepicScriptAnimCmd_SetOffsetFromVar,
    PokepicScriptAnimCmd_RunMotions,
    PokepicScriptAnimCmd_SetScaleMode,
    PokepicScriptAnimCmd_StartWaveMotion,
    PokepicScriptAnimCmd_StartWaveMotionOverDuration,
    PokepicScriptAnimCmd_StartAccelMotion,
    PokepicScriptAnimCmd_StartLinearMotion,
    PokepicScriptAnimCmd_StartAccelMotionToLimit,
    PokepicScriptAnimCmd_Wait,
    PokepicScriptAnimCmd_StartPaletteFade,
    PokepicScriptAnimCmd_WaitPaletteFade,
};

PokepicAnimManager *PokepicAnimManager_New(enum HeapID heapId, int count, BOOL allowReverse) {
    PokepicAnimManager *mgr = Heap_Alloc(heapId, sizeof(PokepicAnimManager));
    int i;
    mgr->allowReverse = allowReverse;
    mgr->count = count;
    mgr->heapId = heapId;
    mgr->anims = Heap_Alloc(heapId, count * sizeof(PokepicScriptAnim));
    MI_CpuFill8(mgr->anims, 0, count * sizeof(PokepicScriptAnim));
    for (i = 0; i < count; i++) {
        mgr->anims[i].state = POKEPIC_SCRIPT_ANIM_IDLE;
    }
    return mgr;
}

void PokepicAnimManager_Delete(PokepicAnimManager *mgr) {
    Heap_Free(mgr->anims);
    Heap_Free(mgr);
}

void PokepicAnimManager_StartAnim(PokepicAnimManager *mgr, Pokepic *pic, struct PokepicAnimParams *params, u8 index) {
    int scriptId = params->scriptId;
    int delay = params->delay;
    GF_ASSERT(index < mgr->count);
    GF_ASSERT(!mgr->anims[index].active);
    MI_CpuFill8(&mgr->anims[index], 0, sizeof(PokepicScriptAnim));
    mgr->anims[index].active = TRUE;
    mgr->anims[index].pic = pic;
    if (scriptId >= 143) {
        scriptId = 0;
        delay = 0;
    }
    mgr->anims[index].scriptId = scriptId;
    mgr->anims[index].reverseX = mgr->allowReverse ? params->reverseX : 0;
    mgr->anims[index].script = AllocAtEndAndReadWholeNarcMemberByIdPair(NARC_a_0_9_0, mgr->anims[index].scriptId, mgr->heapId);
    mgr->anims[index].cursor = mgr->anims[index].script;
    mgr->anims[index].done = FALSE;
    mgr->anims[index].state = POKEPIC_SCRIPT_ANIM_RUNNING;
    mgr->anims[index].runMotions = FALSE;
    mgr->anims[index].scaleMode = POKEPIC_SCALE_CENTERED;
    mgr->anims[index].waitPaletteFade = FALSE;
#if defined(SDK_PORT)
    mgr->anims[index].task = SysTask_CreateBattleAnimation(PokepicScriptAnim_Task, &mgr->anims[index], 0);
#else
    mgr->anims[index].task = SysTask_CreateOnMainQueue(PokepicScriptAnim_Task, &mgr->anims[index], 0);
#endif
    mgr->anims[index].delay = delay;
    mgr->anims[index].baseX = Pokepic_GetAttr(pic, POKEPIC_X);
    mgr->anims[index].baseY = Pokepic_GetAttr(pic, POKEPIC_Y);
    mgr->anims[index].x = 0;
    mgr->anims[index].y = 0;
    mgr->anims[index].dx = 0;
    mgr->anims[index].dy = 0;
    mgr->anims[index].scaleX = 0;
    mgr->anims[index].scaleY = 0;
    mgr->anims[index].rotation = 0;
}

u32 PokepicAnimManager_GetAnimState(PokepicAnimManager *mgr, u32 index) {
    GF_ASSERT(index < mgr->count);
    return mgr->anims[index].state;
}

void PokepicAnimManager_StopAnim(PokepicAnimManager *mgr, u32 index) {
    if (mgr->anims[index].task != NULL) {
        SysTask_Destroy(mgr->anims[index].task);
        mgr->anims[index].task = NULL;
        mgr->anims[index].state = POKEPIC_SCRIPT_ANIM_IDLE;
        mgr->anims[index].active = FALSE;
        Heap_Free(mgr->anims[index].script);
    }
}

static void PokepicScriptAnim_Task(SysTask *task, void *data) {
    PokepicScriptAnim *anim = data;
    if (anim->delay == 0) {
        PokepicScriptAnim_RunFrame(anim);
    } else {
        anim->delay--;
    }
    if (anim->done) {
        anim->state = POKEPIC_SCRIPT_ANIM_IDLE;
        anim->active = FALSE;
        SysTask_Destroy(task);
        anim->task = NULL;
        Heap_Free(anim->script);
    }
}

static void PokepicScriptAnim_RunFrame(PokepicScriptAnim *anim) {
    u8 ended;
    u8 i;

    anim->yield = FALSE;
    anim->commandCount = 0;
    ended = 0;
    for (i = 0; i < NELEMS(anim->motions); i++) {
        PokepicMotion *motion = &anim->motions[i];
        if (motion->active) {
            if (motion->delay == 0) {
                motion->update(motion, anim);
            } else {
                motion->delay--;
            }
        } else {
            ended++;
        }
    }
    if (ended == NELEMS(anim->motions)) {
        anim->runMotions = FALSE;
    }
    if (anim->runMotions) {
        PokepicScriptAnimCmd_ApplyPosition(anim);
        PokepicScriptAnimCmd_ApplyTransform(anim);
        return;
    }
    if (anim->waitPaletteFade) {
        if (!Pokepic_ResumePaletteFade(anim->pic)) {
            anim->waitPaletteFade = FALSE;
        } else {
            return;
        }
    }
    do {
        anim->commandCount++;
        GF_ASSERT(*anim->cursor < NELEMS(sPokepicAnimCommands));
        sPokepicAnimCommands[*anim->cursor](anim);
        if (anim->done) {
            return;
        }
        anim->cursor++;
        if (anim->yield) {
            return;
        }
        if (anim->runMotions) {
            PokepicScriptAnimCmd_ApplyPosition(anim);
            PokepicScriptAnimCmd_ApplyTransform(anim);
            return;
        }
    } while (anim->commandCount < 0x100);
    GF_ASSERT(FALSE);
    anim->done = TRUE;
}

static u32 PokepicScriptAnim_GetScriptWordAt(u32 *array, int index, int expected) {
    u32 value = array[index];
    GF_ASSERT(expected == 1);
    return value;
}

static u32 PokepicScriptAnim_GetScriptWordChecked(u32 *array, int expected) {
    return PokepicScriptAnim_GetScriptWordAt(array, 0, expected);
}

static u32 PokepicScriptAnim_GetScriptWord(u32 *array) {
    return PokepicScriptAnim_GetScriptWordChecked(array, 1);
}

static PokepicMotion *PokepicScriptAnim_AllocMotion(PokepicScriptAnim *anim, int type) {
    u8 i;

    for (i = 0; i < NELEMS(anim->motions); i++) {
        PokepicMotion *motion = &anim->motions[i];
        if (!motion->active) {
            MI_CpuFill8(motion, 0, sizeof(PokepicMotion));
            motion->active = 1;
            motion->update = sPokepicMotionTypes[type].update;
            return motion;
        }
    }
    GF_ASSERT(FALSE);
    return NULL;
}

static void PokepicScriptAnim_ReadArg(PokepicScriptAnim *anim, s32 *out) {
    anim->cursor++;
    *out = PokepicScriptAnim_GetScriptWord(anim->cursor);
}

static void PokepicScriptAnim_ReadArgByte(PokepicScriptAnim *anim, u8 *out) {
    anim->cursor++;
    *out = PokepicScriptAnim_GetScriptWord(anim->cursor);
}

static void PokepicScriptAnim_ReadVarIndex(PokepicScriptAnim *anim, u8 *out) {
    anim->cursor++;
    *out = PokepicScriptAnim_GetScriptWord(anim->cursor);
    GF_ASSERT(*out < NELEMS(anim->vars));
}

static void PokepicScriptAnim_ReadVarIndexPair(PokepicScriptAnim *anim, u8 *first, u8 *second) {
    PokepicScriptAnim_ReadVarIndex(anim, first);
    PokepicScriptAnim_ReadVarIndex(anim, second);
}

static void PokepicScriptAnim_ReadDestVarAndOperands(PokepicScriptAnim *anim, u8 *dest, s32 *left, s32 *right) {
    u8 args[4];
    PokepicScriptAnim_ReadVarIndex(anim, dest);
    PokepicScriptAnim_ReadArgByte(anim, &args[0]);
    if (args[0] == POKEPIC_ANIM_ARITH_CONST) {
        PokepicScriptAnim_ReadVarIndex(anim, &args[2]);
        *left = anim->vars[args[2]];
        PokepicScriptAnim_ReadArg(anim, right);
    } else if (args[0] == POKEPIC_ANIM_ARITH_VAR) {
        PokepicScriptAnim_ReadVarIndexPair(anim, &args[2], &args[1]);
        *left = anim->vars[args[2]];
        *right = anim->vars[args[1]];
    } else {
        GF_ASSERT(FALSE);
    }
}

static void PokepicScriptAnim_ReadDestAndOperands(PokepicScriptAnim *anim, u8 *dest, s32 *left, s32 *right) {
    u8 args[4];
    PokepicScriptAnim_ReadVarIndex(anim, dest);
    PokepicScriptAnim_ReadArgByte(anim, &args[1]);
    PokepicScriptAnim_ReadArgByte(anim, &args[0]);
    if (args[1] == POKEPIC_ANIM_ARITH_CONST) {
        PokepicScriptAnim_ReadArg(anim, left);
    } else if (args[1] == POKEPIC_ANIM_ARITH_VAR) {
        PokepicScriptAnim_ReadVarIndex(anim, &args[3]);
        *left = anim->vars[args[3]];
    } else {
        GF_ASSERT(FALSE);
    }
    if (args[0] == POKEPIC_ANIM_ARITH_CONST) {
        PokepicScriptAnim_ReadArg(anim, right);
    } else if (args[0] == POKEPIC_ANIM_ARITH_VAR) {
        PokepicScriptAnim_ReadVarIndex(anim, &args[2]);
        *right = anim->vars[args[2]];
    } else {
        GF_ASSERT(FALSE);
    }
}

static void PokepicScriptAnim_ReadDestAngleAndAmplitude(PokepicScriptAnim *anim, u8 *dest, s32 *angle, s32 *amplitude) {
    u8 angleVar, amplitudeVar, offsetVar, argType;
    s32 baseAngle, offset;

    PokepicScriptAnim_ReadVarIndexPair(anim, dest, &angleVar);
    baseAngle = anim->vars[angleVar];
    PokepicScriptAnim_ReadArgByte(anim, &argType);

    if (argType == POKEPIC_ANIM_OPERAND_CONST) {
        PokepicScriptAnim_ReadArg(anim, amplitude);
    } else if (argType == POKEPIC_ANIM_OPERAND_VAR) {
        PokepicScriptAnim_ReadVarIndex(anim, &amplitudeVar);
        *amplitude = anim->vars[amplitudeVar];
    } else {
        GF_ASSERT(FALSE);
    }

    PokepicScriptAnim_ReadArgByte(anim, &argType);

    if (argType == POKEPIC_ANIM_OPERAND_CONST) {
        PokepicScriptAnim_ReadArg(anim, &offset);
    } else if (argType == POKEPIC_ANIM_OPERAND_VAR) {
        PokepicScriptAnim_ReadVarIndex(anim, &offsetVar);
        offset = anim->vars[offsetVar];
    } else {
        GF_ASSERT(FALSE);
    }

    *angle = baseAngle + offset;
    *angle %= 0x10000;
}

static enum PokepicAnimCompareResult PokepicScriptAnim_Compare(const s32 *left, const s32 *right) {
    s32 difference = *left - *right;
    if (difference < 0) {
        return POKEPIC_ANIM_COMPARE_LESS;
    }
    if (difference > 0) {
        return POKEPIC_ANIM_COMPARE_GREATER;
    }
    return POKEPIC_ANIM_COMPARE_EQUAL;
}

static void PokepicScriptAnim_AdjustYForScale(PokepicScriptAnim *anim) {
    Pokepic_AddAttr(anim->pic, POKEPIC_Y, -anim->scaleY / 8);
}

static void PokepicScriptAnimCmd_End(PokepicScriptAnim *anim) {
    PokepicScriptAnimCmd_ResetTransform(anim);
    anim->yield = TRUE;
    anim->done = TRUE;
}

static void PokepicScriptAnimCmd_Yield(PokepicScriptAnim *anim) {
    anim->yield = TRUE;
}

static void PokepicScriptAnimCmd_ResetTransform(PokepicScriptAnim *anim) {
    Pokepic_SetAttr(anim->pic, POKEPIC_X, anim->baseX);
    Pokepic_SetAttr(anim->pic, POKEPIC_Y, anim->baseY);
    Pokepic_SetAttr(anim->pic, POKEPIC_ZROT, 0);
    Pokepic_SetAttr(anim->pic, POKEPIC_XPIVOT, 0);
    Pokepic_SetAttr(anim->pic, POKEPIC_AFFINEW, 0x100);
    Pokepic_SetAttr(anim->pic, POKEPIC_AFFINEH, 0x100);
}

static void PokepicScriptAnimCmd_CopyVar(PokepicScriptAnim *anim) {
    u8 dest, src;
    PokepicScriptAnim_ReadVarIndexPair(anim, &dest, &src);
    anim->vars[dest] = anim->vars[src];
}

static void PokepicScriptAnimCmd_AddVar(PokepicScriptAnim *anim) {
    u8 dest;
    s32 lhs, rhs;
    PokepicScriptAnim_ReadDestVarAndOperands(anim, &dest, &lhs, &rhs);
    anim->vars[dest] = lhs + rhs;
}

static void PokepicScriptAnimCmd_MulVar(PokepicScriptAnim *anim) {
    u8 dest;
    s32 lhs, rhs;
    PokepicScriptAnim_ReadDestVarAndOperands(anim, &dest, &lhs, &rhs);
    anim->vars[dest] = lhs * rhs;
}

static void PokepicScriptAnimCmd_SubVar(PokepicScriptAnim *anim) {
    u8 dest;
    s32 lhs, rhs;
    PokepicScriptAnim_ReadDestAndOperands(anim, &dest, &lhs, &rhs);
    anim->vars[dest] = lhs - rhs;
}

static void PokepicScriptAnimCmd_DivVar(PokepicScriptAnim *anim) {
    u8 dest;
    s32 lhs, rhs;
    PokepicScriptAnim_ReadDestAndOperands(anim, &dest, &lhs, &rhs);
    anim->vars[dest] = lhs / rhs;
}

static void PokepicScriptAnimCmd_ModVar(PokepicScriptAnim *anim) {
    u8 dest;
    s32 lhs, rhs;
    PokepicScriptAnim_ReadDestAndOperands(anim, &dest, &lhs, &rhs);
    anim->vars[dest] = lhs % rhs;
}

static void PokepicScriptAnimCmd_SetVarIfCompare(PokepicScriptAnim *anim) {
    s32 lhs;
    s32 rhs;
    s32 newValue;
    u8 args[4];
    int result;

    PokepicScriptAnim_ReadArgByte(anim, &args[0]);
    if (args[0] == POKEPIC_ANIM_OPERAND_CONST) {
        PokepicScriptAnim_ReadVarIndex(anim, &args[3]);
        lhs = anim->vars[args[3]];
        PokepicScriptAnim_ReadArg(anim, &rhs);
    } else if (args[0] == POKEPIC_ANIM_OPERAND_VAR) {
        PokepicScriptAnim_ReadVarIndexPair(anim, &args[3], &args[2]);
        lhs = anim->vars[args[3]];
        rhs = anim->vars[args[2]];
    } else {
        GF_ASSERT(FALSE);
    }
    PokepicScriptAnim_ReadArgByte(anim, &args[1]);
    GF_ASSERT(args[1] <= POKEPIC_ANIM_COMPARE_EQUAL);
    result = PokepicScriptAnim_Compare(&lhs, &rhs);
    PokepicScriptAnim_ReadArgByte(anim, &args[0]);
    if (args[0] == POKEPIC_ANIM_OPERAND_CONST) {
        PokepicScriptAnim_ReadVarIndex(anim, &args[3]);
        PokepicScriptAnim_ReadArg(anim, &newValue);
    } else if (args[0] == POKEPIC_ANIM_OPERAND_VAR) {
        PokepicScriptAnim_ReadVarIndexPair(anim, &args[3], &args[2]);
        newValue = anim->vars[args[2]];
    } else {
        GF_ASSERT(FALSE);
    }
    if (args[1] == result) {
        anim->vars[args[3]] = newValue;
    }
}

static void PokepicScriptAnimCmd_SetVar(PokepicScriptAnim *anim) {
    u8 dest;
    PokepicScriptAnim_ReadVarIndex(anim, &dest);
    anim->cursor++;
    anim->vars[dest] = PokepicScriptAnim_GetScriptWord(anim->cursor);
}

static void PokepicScriptAnimCmd_LoopStart(PokepicScriptAnim *anim) {
    GF_ASSERT(anim->loopStart == NULL);
    anim->cursor++;
    anim->loopStart = anim->cursor;
    anim->loopCount = PokepicScriptAnim_GetScriptWord(anim->cursor);
    anim->loopIndex = 0;
}

static void PokepicScriptAnimCmd_LoopEnd(PokepicScriptAnim *anim) {
    anim->loopIndex++;
    if (anim->loopIndex >= anim->loopCount) {
        anim->loopStart = NULL;
        anim->loopIndex = 0;
        anim->loopCount = 0;
    } else {
        anim->cursor = anim->loopStart;
    }
}

static void PokepicScriptAnimCmd_SetAttrFromVar(PokepicScriptAnim *anim) {
    s32 attr;
    u8 var;
    PokepicScriptAnim_ReadArg(anim, &attr);
    PokepicScriptAnim_ReadVarIndex(anim, &var);
    Pokepic_SetAttr(anim->pic, attr, anim->vars[var]);
}

static void PokepicScriptAnimCmd_AddAttrFromVar(PokepicScriptAnim *anim) {
    s32 attr;
    u8 var;
    PokepicScriptAnim_ReadArg(anim, &attr);
    PokepicScriptAnim_ReadVarIndex(anim, &var);
    Pokepic_AddAttr(anim->pic, attr, anim->vars[var]);
}

static void PokepicScriptAnimCmd_SetOrAddAttr(PokepicScriptAnim *anim) {
    struct {
        u8 command;
        u8 valueType;
        u8 var;
        u8 unk_3;
        s32 value;
        s32 attr;
    } args;

    PokepicScriptAnim_ReadArg(anim, &args.attr);
    PokepicScriptAnim_ReadArgByte(anim, &args.valueType);
    if (args.valueType == POKEPIC_ANIM_OPERAND_CONST) {
        PokepicScriptAnim_ReadArg(anim, &args.value);
    } else if (args.valueType == POKEPIC_ANIM_OPERAND_VAR) {
        PokepicScriptAnim_ReadVarIndex(anim, &args.var);
        args.value = anim->vars[args.var];
    } else {
        GF_ASSERT(FALSE);
    }
    PokepicScriptAnim_ReadArgByte(anim, &args.command);
    if (args.command == POKEPIC_ANIM_OP_SET) {
        Pokepic_SetAttr(anim->pic, args.attr, args.value);
    } else if (args.command == POKEPIC_ANIM_OP_ADD) {
        Pokepic_AddAttr(anim->pic, args.attr, args.value);
    } else {
        GF_ASSERT(FALSE);
    }
}

static void PokepicScriptAnimCmd_Sin(PokepicScriptAnim *anim) {
    u8 dest;
    s32 angle;
    s32 amplitude;
    PokepicScriptAnim_ReadDestAngleAndAmplitude(anim, &dest, &angle, &amplitude);
    anim->vars[dest] = (FX_SinIdx(angle) * amplitude) >> FX32_SHIFT;
}

static void PokepicScriptAnimCmd_Cos(PokepicScriptAnim *anim) {
    u8 dest;
    s32 angle;
    s32 amplitude;
    PokepicScriptAnim_ReadDestAngleAndAmplitude(anim, &dest, &angle, &amplitude);
    anim->vars[dest] = (FX_CosIdx(angle) * amplitude) >> FX32_SHIFT;
}

static void PokepicScriptAnimCmd_SetPosFromVar(PokepicScriptAnim *anim) {
    u8 args[2];
    PokepicScriptAnim_ReadVarIndex(anim, &args[1]);
    PokepicScriptAnim_ReadArgByte(anim, &args[0]);
    if (args[0] == POKEPIC_ANIM_FIELD_X) {
        anim->x = anim->vars[args[1]];
    } else if (args[0] == POKEPIC_ANIM_FIELD_Y) {
        anim->y = anim->vars[args[1]];
    } else {
        GF_ASSERT(FALSE);
    }
}

static void PokepicScriptAnimCmd_AddPosFromVar(PokepicScriptAnim *anim) {
    u8 args[2];
    PokepicScriptAnim_ReadVarIndex(anim, &args[1]);
    PokepicScriptAnim_ReadArgByte(anim, &args[0]);
    if (args[0] == POKEPIC_ANIM_FIELD_X) {
        anim->x += anim->vars[args[1]];
    } else if (args[0] == POKEPIC_ANIM_FIELD_Y) {
        anim->y += anim->vars[args[1]];
    } else {
        GF_ASSERT(FALSE);
    }
}

static void PokepicScriptAnimCmd_SetOrAddTransform(PokepicScriptAnim *anim) {
    s32 value;
    u8 args[4];
    s32 *target;
    PokepicScriptAnim_ReadArgByte(anim, &args[3]);
    if (args[3] == POKEPIC_ANIM_FIELD_X) {
        target = &anim->x;
    } else if (args[3] == POKEPIC_ANIM_FIELD_Y) {
        target = &anim->y;
    } else if (args[3] == POKEPIC_ANIM_FIELD_DX) {
        target = &anim->dx;
    } else if (args[3] == POKEPIC_ANIM_FIELD_DY) {
        target = &anim->dy;
    } else if (args[3] == POKEPIC_ANIM_FIELD_SCALE_X) {
        target = &anim->scaleX;
    } else if (args[3] == POKEPIC_ANIM_FIELD_SCALE_Y) {
        target = &anim->scaleY;
    } else if (args[3] == POKEPIC_ANIM_FIELD_ROTATION) {
        target = &anim->rotation;
    } else {
        GF_ASSERT(FALSE);
    }
    PokepicScriptAnim_ReadArgByte(anim, &args[1]);
    if (args[1] == POKEPIC_ANIM_OPERAND_CONST) {
        PokepicScriptAnim_ReadArg(anim, &value);
    } else if (args[1] == POKEPIC_ANIM_OPERAND_VAR) {
        PokepicScriptAnim_ReadVarIndex(anim, &args[2]);
        value = anim->vars[args[2]];
    } else {
        GF_ASSERT(FALSE);
    }
    PokepicScriptAnim_ReadArgByte(anim, &args[0]);
    if (args[0] == POKEPIC_ANIM_OP_SET) {
        *target = value;
    } else if (args[0] == POKEPIC_ANIM_OP_ADD) {
        *target += value;
    } else {
        GF_ASSERT(FALSE);
    }
}

static void PokepicScriptAnimCmd_ApplyPosition(PokepicScriptAnim *anim) {
    if (anim->reverseX) {
        Pokepic_SetAttr(anim->pic, POKEPIC_X, anim->baseX - (anim->x + anim->dx));
    } else {
        Pokepic_SetAttr(anim->pic, POKEPIC_X, anim->baseX + anim->x + anim->dx);
    }
    Pokepic_SetAttr(anim->pic, POKEPIC_Y, anim->baseY + anim->y + anim->dy);
}

static void PokepicScriptAnimCmd_ApplyTransform(PokepicScriptAnim *anim) {
    Pokepic_SetAttr(anim->pic, POKEPIC_AFFINEW, anim->scaleX + 0x100);
    Pokepic_SetAttr(anim->pic, POKEPIC_AFFINEH, anim->scaleY + 0x100);
    Pokepic_SetAttr(anim->pic, POKEPIC_ZROT, (u16)anim->rotation);
    if (anim->scaleMode == POKEPIC_SCALE_KEEP_FEET_WHEN_SHRINKING) {
        if (anim->scaleY < 0) {
            PokepicScriptAnim_AdjustYForScale(anim);
        }
    } else if (anim->scaleMode == POKEPIC_SCALE_KEEP_FEET) {
        if (anim->scaleY != 0) {
            PokepicScriptAnim_AdjustYForScale(anim);
        }
    } else {
        GF_ASSERT(anim->scaleMode == POKEPIC_SCALE_CENTERED);
    }
}

static void PokepicScriptAnimCmd_SetOffsetFromVar(PokepicScriptAnim *anim) {
    u8 var;
    u8 target;
    PokepicScriptAnim_ReadVarIndex(anim, &var);
    anim->cursor++;
    target = PokepicScriptAnim_GetScriptWord(anim->cursor);
    if (target == POKEPIC_ANIM_FIELD_X || target == POKEPIC_ANIM_FIELD_DX) {
        anim->dx = anim->vars[var];
    } else if (target == POKEPIC_ANIM_FIELD_Y || target == POKEPIC_ANIM_FIELD_DY) {
        anim->dy = anim->vars[var];
    } else {
        GF_ASSERT(FALSE);
    }
}

static void PokepicScriptAnimCmd_Wait(PokepicScriptAnim *anim) {
    PokepicScriptAnim_ReadArg(anim, &anim->delay);
    anim->yield = TRUE;
}

static void PokepicScriptAnimCmd_StartPaletteFade(PokepicScriptAnim *anim) {
    s32 duration;
    u8 args[4];
    PokepicScriptAnim_ReadArgByte(anim, &args[2]);
    PokepicScriptAnim_ReadArgByte(anim, &args[1]);
    PokepicScriptAnim_ReadArgByte(anim, &args[0]);
    PokepicScriptAnim_ReadArg(anim, &duration);
    Pokepic_StartPaletteFade(anim->pic, args[2], args[1], args[0], duration);
}

static void PokepicScriptAnimCmd_WaitPaletteFade(PokepicScriptAnim *anim) {
    if (Pokepic_ResumePaletteFade(anim->pic)) {
        anim->waitPaletteFade = TRUE;
        anim->yield = TRUE;
    }
}

static void PokepicScriptAnimCmd_RunMotions(PokepicScriptAnim *anim) {
    anim->runMotions = TRUE;
}

static void PokepicScriptAnimCmd_SetScaleMode(PokepicScriptAnim *anim) {
    u8 mode;
    BOOL valid;
    BOOL keepsFeet;
    PokepicScriptAnim_ReadArgByte(anim, &anim->scaleMode);
    mode = anim->scaleMode;
    valid = TRUE;
    keepsFeet = TRUE;
    if (mode != POKEPIC_SCALE_KEEP_FEET_WHEN_SHRINKING && mode != POKEPIC_SCALE_KEEP_FEET) {
        keepsFeet = FALSE;
    }
    if (!keepsFeet && mode != POKEPIC_SCALE_CENTERED) {
        valid = FALSE;
    }
    GF_ASSERT(valid);
}

static void PokepicScriptAnimCmd_StartWaveMotion(PokepicScriptAnim *anim) {
    PokepicScriptAnim_StartMotion(anim, POKEPIC_MOTION_WAVE);
}

static void PokepicScriptAnimCmd_StartWaveMotionOverDuration(PokepicScriptAnim *anim) {
    PokepicScriptAnim_StartMotion(anim, POKEPIC_MOTION_WAVE_OVER_DURATION);
}

static void PokepicScriptAnimCmd_StartAccelMotion(PokepicScriptAnim *anim) {
    PokepicScriptAnim_StartMotion(anim, POKEPIC_MOTION_ACCEL);
}

static void PokepicScriptAnimCmd_StartLinearMotion(PokepicScriptAnim *anim) {
    PokepicScriptAnim_StartMotion(anim, POKEPIC_MOTION_LINEAR);
}

static void PokepicScriptAnimCmd_StartAccelMotionToLimit(PokepicScriptAnim *anim) {
    PokepicScriptAnim_StartMotion(anim, POKEPIC_MOTION_ACCEL_TO_LIMIT);
}

static void PokepicMotion_ApplyValue(int op, int *base, int *value, int *dest) {
    if (op == POKEPIC_MOTION_OP_SET) {
        *dest = *value;
    } else if (op == POKEPIC_MOTION_OP_SET_BASE) {
        *dest = *base + *value;
    } else if (op == POKEPIC_MOTION_OP_ADD) {
        *dest += *value;
    } else {
        GF_ASSERT(FALSE);
    }
}

static void PokepicMotion_SetTarget(u8 target, PokepicMotion *motion, PokepicScriptAnim *anim) {
    switch (target) {
    case POKEPIC_MOTION_TARGET_DX:
        motion->value = &motion->values[0];
        motion->dest = &anim->dx;
        motion->base = anim->dx;
        break;
    case POKEPIC_MOTION_TARGET_DY:
        motion->value = &motion->values[1];
        motion->dest = &anim->dy;
        motion->base = anim->dy;
        break;
    case POKEPIC_MOTION_TARGET_SCALE_X:
        motion->value = &motion->values[2];
        motion->dest = &anim->scaleX;
        motion->base = anim->scaleX;
        break;
    case POKEPIC_MOTION_TARGET_SCALE_Y:
        motion->value = &motion->values[3];
        motion->dest = &anim->scaleY;
        motion->base = anim->scaleY;
        break;
    case POKEPIC_MOTION_TARGET_ROTATION:
        motion->value = &motion->values[4];
        motion->dest = &anim->rotation;
        motion->base = anim->rotation;
        break;
    default:
        GF_ASSERT(FALSE);
        break;
    }
}

static void PokepicScriptAnim_StartMotion(PokepicScriptAnim *anim, int type) {
    PokepicMotion *motion = PokepicScriptAnim_AllocMotion(anim, (u8)type);
    u8 i;
    PokepicScriptAnim_ReadArgByte(anim, &motion->op);
    PokepicScriptAnim_ReadArgByte(anim, &motion->delay);
    for (i = 0; i < sPokepicMotionTypes[type].argCount; i++) {
        PokepicScriptAnim_ReadArg(anim, &motion->args[i]);
    }
    PokepicMotion_SetTarget(motion->args[sPokepicMotionTypes[type].targetArg], motion, anim);
    if (motion->delay == 0) {
        motion->update(motion, anim);
    } else {
        motion->delay--;
    }
}

static void PokepicMotion_UpdateWave(PokepicMotion *motion, PokepicScriptAnim *anim) {
    u16 angle = motion->wave.startAngle + motion->wave.angleStep * (motion->wave.frame + 1);
    switch (motion->wave.waveform) {
    case POKEPIC_WAVE_SIN:
        *motion->value = (FX_SinIdx(angle) * motion->wave.amplitude) >> FX32_SHIFT;
        break;
    case POKEPIC_WAVE_COS:
        *motion->value = (FX_CosIdx(angle) * motion->wave.amplitude) >> FX32_SHIFT;
        break;
    case POKEPIC_WAVE_NEG_SIN:
        *motion->value = -((FX_SinIdx(angle) * motion->wave.amplitude) >> FX32_SHIFT);
        break;
    case POKEPIC_WAVE_NEG_COS:
        *motion->value = -((FX_CosIdx(angle) * motion->wave.amplitude) >> FX32_SHIFT);
        break;
    default:
        GF_ASSERT(FALSE);
        break;
    }
    PokepicMotion_ApplyValue(motion->op, &motion->base, motion->value, motion->dest);
    motion->wave.frame++;
    if (motion->wave.frame >= motion->wave.duration) {
        motion->active = 0;
    }
}

static void PokepicMotion_UpdateWaveOverDuration(PokepicMotion *motion, PokepicScriptAnim *anim) {
    u16 angle = motion->wave.startAngle + motion->wave.angleStep * (motion->wave.frame + 1) / motion->wave.duration;
    switch (motion->wave.waveform) {
    case POKEPIC_WAVE_SIN:
        *motion->value = (FX_SinIdx(angle) * motion->wave.amplitude) >> FX32_SHIFT;
        break;
    case POKEPIC_WAVE_COS:
        *motion->value = (FX_CosIdx(angle) * motion->wave.amplitude) >> FX32_SHIFT;
        break;
    case POKEPIC_WAVE_NEG_SIN:
        *motion->value = -((FX_SinIdx(angle) * motion->wave.amplitude) >> FX32_SHIFT);
        break;
    case POKEPIC_WAVE_NEG_COS:
        *motion->value = -((FX_CosIdx(angle) * motion->wave.amplitude) >> FX32_SHIFT);
        break;
    default:
        GF_ASSERT(FALSE);
        break;
    }
    PokepicMotion_ApplyValue(motion->op, &motion->base, motion->value, motion->dest);
    motion->wave.frame++;
    if (motion->wave.frame >= motion->wave.duration) {
        motion->active = 0;
    }
}

static void PokepicMotion_UpdateAccel(PokepicMotion *motion, PokepicScriptAnim *anim) {
    int delta = motion->accel.velocity + motion->accel.acceleration * motion->accel.frame;
    *motion->value += delta;
    PokepicMotion_ApplyValue(motion->op, &motion->base, motion->value, motion->dest);
    motion->accel.frame++;
    if (motion->accel.frame >= motion->accel.durationOrLimit) {
        motion->active = 0;
    }
}

static void PokepicMotion_UpdateLinear(PokepicMotion *motion, PokepicScriptAnim *anim) {
    *motion->value = (motion->linear.frame + 1) * motion->linear.total / motion->linear.duration;
    PokepicMotion_ApplyValue(motion->op, &motion->base, motion->value, motion->dest);
    motion->linear.frame++;
    if (motion->linear.frame >= motion->linear.duration) {
        motion->active = 0;
    }
}

static void PokepicMotion_UpdateAccelToLimit(PokepicMotion *motion, PokepicScriptAnim *anim) {
    int delta = motion->accel.velocity + motion->accel.acceleration * motion->accel.frame;
    *motion->value += delta;
    if (motion->op == POKEPIC_MOTION_OP_SET || motion->op == POKEPIC_MOTION_OP_ADD) {
        if (delta < 0) {
            int limit = motion->accel.durationOrLimit;
            int *value = motion->value;
            if (*value <= limit) {
                *value = limit;
                motion->active = 0;
            }
        } else {
            int limit = motion->accel.durationOrLimit;
            int *value = motion->value;
            if (*value >= limit) {
                *value = limit;
                motion->active = 0;
            }
        }
    } else if (motion->op == POKEPIC_MOTION_OP_SET_BASE) {
        int total = motion->base + *motion->value;
        if (delta < 0) {
            if (total <= motion->accel.durationOrLimit) {
                *motion->value += motion->accel.durationOrLimit - total;
                motion->active = 0;
            }
        } else {
            if (total >= motion->accel.durationOrLimit) {
                *motion->value -= total - motion->accel.durationOrLimit;
                motion->active = 0;
            }
        }
    } else {
        GF_ASSERT(FALSE);
    }
    PokepicMotion_ApplyValue(motion->op, &motion->base, motion->value, motion->dest);
    motion->accel.frame++;
}
