#include "battle/battle_anim_emitter.h"

#include <string.h>

#include "global.h"

#include "constants/battle.h"

#include "battle/battle_anim_internal.h"
#include "library/spl_emitter.h"

#include "assert.h"
#include "camera.h"
#include "geometry_math.h"
#include "heap.h"
#include "obj_pltt_transfer.h"
#include "particle_system.h"
#include "particle_system_internal.h"
#include "pokepic.h"
#include "sprite.h"
#include "sprite_system.h"

static void BattleAnimUtil_ScaleVecComponents(int kind, int scale, VecFx32 *vec);
static void BattleAnimEmitter_ApplyConfiguredPosition(SPLEmitter *emitter, BattleAnimEmitterConfig *config);
static void BattleAnimEmitter_ApplyConfiguredAxis(SPLEmitter *emitter, BattleAnimEmitterConfig *config);
static void BattleAnimEmitter_ApplyConfiguredFlags(SPLEmitter *emitter, BattleAnimEmitterConfig *config);
static void BattleAnimEmitter_ApplyConfiguredCamera(SPLEmitter *emitter, BattleAnimEmitterConfig *config);
static void BattleAnimEmitter_ApplyConfiguredZ(SPLEmitter *emitter, BattleAnimEmitterConfig *config);
static void BattleAnimEmitter_AddOffsetForBattlerType(BattleAnimEmitterConfig *config, int battler, VecFx32 *pos);
static void BattleAnimEmitter_AddOffsetFromScript(BattleAnimEmitterConfig *config, VecFx32 *pos);
static void BattleAnimEmitter_RunFlagCallback(int index, SPLEmitter *emitter, BattleAnimEmitterConfig *config);
static int BattleAnimEmitter_GetFlagMask(int index);
static int BattleAnimEmitter_CountFlagMasks(void);
static void BattleAnimEmitter_RunCameraCallback(int index, SPLEmitter *emitter, BattleAnimEmitterConfig *config);

// Head of the motion structs: the current offset
typedef struct BattleAnimMotionOffset {
    s16 x;
    s16 y;
} BattleAnimMotionOffset;

enum BattleAnimEmitterTargetMode {
    BATTLE_ANIM_EMITTER_TARGET_MODE_NONE,
    BATTLE_ANIM_EMITTER_TARGET_MODE_CONVERGE,
    BATTLE_ANIM_EMITTER_TARGET_MODE_MAGNET,
};

enum BattleAnimEmitterTargetKind {
    BATTLE_ANIM_EMITTER_TARGET_ORIGIN,
    BATTLE_ANIM_EMITTER_TARGET_BY_SIDE,
    BATTLE_ANIM_EMITTER_TARGET_UP,
    BATTLE_ANIM_EMITTER_TARGET_BATTLER_B,
    BATTLE_ANIM_EMITTER_TARGET_BATTLER_A,
    BATTLE_ANIM_EMITTER_TARGET_FROM_SCRIPT,
};

enum BattleAnimScaleAxes {
    BATTLE_ANIM_SCALE_X = 1,
    BATTLE_ANIM_SCALE_Y,
    BATTLE_ANIM_SCALE_Z,
    BATTLE_ANIM_SCALE_XY,
    BATTLE_ANIM_SCALE_XZ,
    BATTLE_ANIM_SCALE_YZ,
    BATTLE_ANIM_SCALE_XYZ,
};

// Script arguments: a vector and the components BattleAnimUtil_ScaleVecComponents scales
typedef struct BattleAnimEmitterTargetArgs {
    VecFx32 pos;
    int scaleKind;
} BattleAnimEmitterTargetArgs;

// Script arguments: an offset, mirrored by BattleAnimEmitterConfig.sideSign unless ignoreSide
typedef struct BattleAnimEmitterOffsetArgs {
    BOOL ignoreSide;
    VecFx32 offset;
} BattleAnimEmitterOffsetArgs;

void BattleAnimEmitterCb_SetPositionAtFirstOpponent(SPLEmitter *emitter) {
    CameraAngle angle;
    VecFx32 pos;
    VecFx32 unusedPos;
    VecFx32 up;
    BattleAnimSystem *system;
    SPLEmitter *particleSystem;
    Camera *camera;

    system = ParticleSystem_GetEmitterParam();
    BattleAnimSystem_GetAttacker(system);
    BattleAnimSystem_GetDefender(system);
    particleSystem = BattleAnimSystem_GetCurrentParticleSystem(system);
    if (BattleAnimUtil_GetBattlerSide(system, BATTLER_ENEMY) == BATTLE_ANIM_SIDE_PLAYER) {
        BattleAnimUtil_GetBattlerEmitterPositionKind0(system, BATTLER_ENEMY, &pos);
    } else {
        int type = BattleAnimUtil_GetBattlerType(system, BATTLER_ENEMY);
        int opposingType = BattleAnimUtil_GetOpposingBattlerType(type);
        int perspective = ParticleSystem_GetPerspectiveType((ParticleSystem *)particleSystem);
        BOOL isContest = BattleAnimSystem_IsContest(system);
        BattleAnimUtil_GetEmitterPositionKind0(opposingType, &pos, isContest, perspective);
        BattleAnimUtil_GetEmitterPositionKind0(type, &unusedPos, isContest, perspective);
    }
    SPL_SetEmitterPosition(emitter, &pos);
    BattleAnimUtil_GetCameraUp(TRUE, &up);
    camera = ParticleSystem_GetCamera(particleSystem);
    angle = Camera_GetAngle(camera);
    Camera_SetAnglePos(&angle, camera);
}

void BattleAnimEmitterCb_SetPositionAtBattler0(SPLEmitter *emitter) {
    VecFx32 pos;
    VecFx32 up;
    BattleAnimSystem *system = ParticleSystem_GetEmitterParam();
    int defender = BattleAnimSystem_GetDefender(system);
    SPLEmitter *particleSystem = BattleAnimSystem_GetCurrentParticleSystem(system);
    BattleAnimUtil_GetBattlerEmitterPositionKind0(system, 0, &pos);
    SPL_SetEmitterPosition(emitter, &pos);
    BattleAnimUtil_GetCameraUp(defender, &up);
    ParticleSystem_SetCameraUp((ParticleSystem *)particleSystem, &up);
}

void BattleAnimEmitterCb_SetPositionAtDefender(SPLEmitter *emitter) {
    VecFx32 pos;
    BattleAnimSystem *system = ParticleSystem_GetEmitterParam();
    int defender = BattleAnimSystem_GetDefender(system);
    BattleAnimUtil_GetBattlerEmitterPositionKind0(system, defender, &pos);
    SPL_SetEmitterPosition(emitter, &pos);
}

void BattleAnimEmitterCb_SetPositionAtAttacker(SPLEmitter *emitter) {
    VecFx32 pos;
    BattleAnimSystem *system = ParticleSystem_GetEmitterParam();
    int attacker = BattleAnimSystem_GetAttacker(system);
    BattleAnimUtil_GetBattlerEmitterPositionKind0(system, attacker, &pos);
    SPL_SetEmitterPosition(emitter, &pos);
}

void BattleAnimEmitterCb_SetPositionAtAttacker2(SPLEmitter *emitter) {
    VecFx32 pos;
    BattleAnimSystem *system = ParticleSystem_GetEmitterParam();
    int attacker = BattleAnimSystem_GetAttacker(system);
    BattleAnimSystem_GetDefender(system);
    BattleAnimUtil_GetBattlerEmitterPositionKind0(system, attacker, &pos);
    SPL_SetEmitterPosition(emitter, &pos);
}

void BattleAnimEmitterCb_SetPositionAtAttackerSide(SPLEmitter *emitter) {
    VecFx32 pos;
    BattleAnimSystem *system = ParticleSystem_GetEmitterParam();
    SPLEmitter *particleSystem = BattleAnimSystem_GetCurrentParticleSystem(system);
    int perspective = ParticleSystem_GetPerspectiveType((ParticleSystem *)particleSystem);
    int attacker = BattleAnimSystem_GetAttacker(system);
    int type = BattleAnimUtil_GetBattlerSide(system, attacker) == BATTLE_ANIM_SIDE_PLAYER ? 0 : 1;
    BOOL isContest = BattleAnimSystem_IsContest(system);
    BattleAnimUtil_GetEmitterPositionKind0(type, &pos, isContest, perspective);
    SPL_SetEmitterPosition(emitter, &pos);
}

void BattleAnimEmitterCb_SetPositionAtDefenderSide(SPLEmitter *emitter) {
    VecFx32 pos;
    BattleAnimSystem *system = ParticleSystem_GetEmitterParam();
    SPLEmitter *particleSystem = BattleAnimSystem_GetCurrentParticleSystem(system);
    int perspective = ParticleSystem_GetPerspectiveType((ParticleSystem *)particleSystem);
    int defender = BattleAnimSystem_GetDefender(system);
    int type = BattleAnimUtil_GetBattlerSide(system, defender) == BATTLE_ANIM_SIDE_PLAYER ? 0 : 1;
    BOOL isContest = BattleAnimSystem_IsContest(system);
    BattleAnimUtil_GetEmitterPositionKind0(type, &pos, isContest, perspective);
    SPL_SetEmitterPosition(emitter, &pos);
}

void BattleAnimEmitterCb_SetPositionByBattlerTypes(SPLEmitter *emitter) {
    s16 unused[6] = { 0 };
    BattleAnimEmitterOffsetTable offsetsX;
    BattleAnimEmitterOffsetTable offsetsY;
    BattleAnimSystem *system;
    u32 attacker, defender;
    int attackerType, defenderType;
    u32 offsetX, offsetY;

    offsetsX = gBattleAnimEmitterOffsetsX;
    offsetsY = gBattleAnimEmitterOffsetsY;
    system = ParticleSystem_GetEmitterParam();
    attacker = BattleAnimSystem_GetAttacker(system);
    defender = BattleAnimSystem_GetDefender(system);
    attackerType = BattleAnimUtil_GetBattlerType(system, attacker);
    defenderType = BattleAnimUtil_GetBattlerType(system, defender);
    offsetX = offsetsX.values[attackerType][defenderType] * 172u;
    emitter->emtr_pos.x = offsetX + emitter->p_res->p_base->pos.x;
    offsetY = offsetsY.values[attackerType][defenderType] * 172u;
    emitter->emtr_pos.y = offsetY + emitter->p_res->p_base->pos.y;
    emitter->emtr_pos.z = emitter->p_res->p_base->pos.z;
}

// kind: 1 x, 2 y, 3 z, 4 xy, 5 xz, 6 yz, 7 xyz
static void BattleAnimUtil_ScaleVecComponents(int kind, int scale, VecFx32 *vec) {
    fx32 *z = &vec->z;

    switch (kind) {
    case BATTLE_ANIM_SCALE_X:
        vec->x *= scale;
        break;
    case BATTLE_ANIM_SCALE_Y:
        vec->y *= scale;
        break;
    case BATTLE_ANIM_SCALE_Z:
        *z *= scale;
        break;
    case BATTLE_ANIM_SCALE_XY:
        vec->x *= scale;
        vec->y *= scale;
        break;
    case BATTLE_ANIM_SCALE_XZ:
        vec->x *= scale;
        *z *= scale;
        break;
    case BATTLE_ANIM_SCALE_YZ:
        vec->y *= scale;
        *z *= scale;
        break;
    case BATTLE_ANIM_SCALE_XYZ:
        vec->x *= scale;
        vec->y *= scale;
        *z *= scale;
        break;
    }
}

// kind: 0 origin, 1 current convergence position mirrored by scale, 2 up, 3 battlerB, 4 battlerA, 5 script arguments;
// relative to the emitter position origin
static void BattleAnimEmitter_SetConvergenceTarget(BattleAnimSystem *system, SPLEmitter *emitter, int battlerA, int battlerB, int kind, s8 scale, VecFx32 *origin) {
    VecFx32 pos = { 0, 0, 0 };

    switch (kind) {
    case BATTLE_ANIM_EMITTER_TARGET_ORIGIN:
        break;
    case BATTLE_ANIM_EMITTER_TARGET_BY_SIDE:
        ParticleEmitter_GetConvergencePos(emitter, &pos);
        pos.x *= scale;
        pos.y *= scale;
        pos.z *= scale;
        break;
    case BATTLE_ANIM_EMITTER_TARGET_UP:
        pos.x = 0;
        pos.y = 3200 * scale;
        pos.z = 0;
        break;
    case BATTLE_ANIM_EMITTER_TARGET_BATTLER_B:
        BattleAnimUtil_GetBattlerEmitterPositionKind0(system, battlerB, &pos);
        break;
    case BATTLE_ANIM_EMITTER_TARGET_BATTLER_A:
        BattleAnimUtil_GetBattlerEmitterPositionKind0(system, battlerA, &pos);
        break;
    case BATTLE_ANIM_EMITTER_TARGET_FROM_SCRIPT: {
        BattleAnimEmitterTargetArgs args = { 0 };
        BattleAnimSystem_ReadScriptArgs(system, &args, 4);
        pos.x = args.pos.x;
        pos.y = args.pos.y;
        pos.z = args.pos.z;
        BattleAnimUtil_ScaleVecComponents(args.scaleKind, scale, &pos);
        break;
    }
    }
    pos.x -= origin->x;
    pos.y -= origin->y;
    pos.z -= origin->z;
    ParticleEmitter_SetConvergencePos(emitter, &pos);
}

// kind as in BattleAnimEmitter_SetConvergenceTarget (no origin kind)
static void BattleAnimEmitter_SetMagnetTarget(BattleAnimSystem *system, SPLEmitter *emitter, u32 battlerA, u32 battlerB, u32 kind, s8 scale, const VecFx32 *origin) {
    VecFx32 pos = { 0, 0, 0 };

    switch (kind) {
    case BATTLE_ANIM_EMITTER_TARGET_BY_SIDE:
        ParticleEmitter_GetMagnetPos(emitter, &pos);
        pos.x *= scale;
        pos.y *= scale;
        pos.z *= scale;
        break;
    case BATTLE_ANIM_EMITTER_TARGET_UP:
        pos.x = 0;
        pos.y = 3200 * scale;
        pos.z = 0;
        break;
    case BATTLE_ANIM_EMITTER_TARGET_BATTLER_B:
        BattleAnimUtil_GetBattlerEmitterPositionKind0(system, battlerB, &pos);
        break;
    case BATTLE_ANIM_EMITTER_TARGET_BATTLER_A:
        BattleAnimUtil_GetBattlerEmitterPositionKind0(system, battlerA, &pos);
        break;
    case BATTLE_ANIM_EMITTER_TARGET_FROM_SCRIPT: {
        s32 args[4] = { 0, 0, 0, 0 }; // BattleAnimEmitterTargetArgs
        BattleAnimSystem_ReadScriptArgs(system, args, 4);
        pos.x = args[0];
        pos.y = args[1];
        pos.z = args[2];
        BattleAnimUtil_ScaleVecComponents(args[3], scale, &pos);
    } break;
    }
    pos.x -= origin->x;
    pos.y -= origin->y;
    pos.z -= origin->z;
    ParticleEmitter_SetMagnetPos(emitter, &pos);
}

static inline void SetAxis(VecFx16 *axis, VecFx32 *src) {
    axis->x = src->x;
    axis->y = src->y;
    axis->z = src->z;
}

static inline void SetAxisNegated(VecFx16 *axis, VecFx32 *src) {
    axis->x = -src->x;
    axis->y = -src->y;
    axis->z = -src->z;
}

// Reads 8 VecFx32 script arguments: the axis for the pair of battler types (7), the position (0: at battlerA,
// mirrored by side). mode 1 / 2: then sets the convergence / magnet target of the given kind.
static void BattleAnimEmitter_SetTransformFromScript(BattleAnimSystem *system, SPLEmitter *emitter, int battlerA, int battlerB, int mode, int kind) {
    VecFx32 final, axis0, axis1, axis3, axis2, axis6, axis4, axis5;
    volatile VecFx32 posArg;
    VecFx32 pos;
    VecFx16 axis;
    int typeA, typeB;
    s8 sign;

    BattleAnimSystem_GetCurrentParticleSystem(system);
    typeA = BattleAnimUtil_GetBattlerType(system, battlerA);
    typeB = BattleAnimUtil_GetBattlerType(system, battlerB);
    BattleAnimSystem_ReadScriptArgs(system, &axis0, 3);
    BattleAnimSystem_ReadScriptArgs(system, &axis1, 3);
    BattleAnimSystem_ReadScriptArgs(system, &axis2, 3);
    BattleAnimSystem_ReadScriptArgs(system, &axis3, 3);
    BattleAnimSystem_ReadScriptArgs(system, &axis4, 3);
    BattleAnimSystem_ReadScriptArgs(system, &axis5, 3);
    BattleAnimSystem_ReadScriptArgs(system, &axis6, 3);
    switch (typeA) {
    default:
    case BATTLER_TYPE_SOLO_PLAYER:
        SetAxis(&axis, &axis0);
        break;
    case BATTLER_TYPE_SOLO_ENEMY:
        SetAxisNegated(&axis, &axis0);
        break;
    case BATTLER_TYPE_PLAYER_SIDE_SLOT_1:
        if (typeB == BATTLER_TYPE_ENEMY_SIDE_SLOT_1) {
            SetAxis(&axis, &axis1);
        } else if (typeB == BATTLER_TYPE_ENEMY_SIDE_SLOT_2) {
            SetAxis(&axis, &axis2);
        } else {
            SetAxis(&axis, &axis3);
        }
        break;
    case BATTLER_TYPE_ENEMY_SIDE_SLOT_1:
        if (typeB == BATTLER_TYPE_PLAYER_SIDE_SLOT_1) {
            SetAxisNegated(&axis, &axis1);
        } else if (typeB == BATTLER_TYPE_ENEMY_SIDE_SLOT_2) {
            SetAxisNegated(&axis, &axis3);
        } else {
            SetAxisNegated(&axis, &axis4);
        }
        break;
    case BATTLER_TYPE_PLAYER_SIDE_SLOT_2:
        if (typeB == BATTLER_TYPE_ENEMY_SIDE_SLOT_1) {
            SetAxis(&axis, &axis4);
        } else if (typeB == BATTLER_TYPE_ENEMY_SIDE_SLOT_2) {
            SetAxis(&axis, &axis5);
        } else {
            SetAxis(&axis, &axis6);
        }
        break;
    case BATTLER_TYPE_ENEMY_SIDE_SLOT_2:
        if (typeB == BATTLER_TYPE_ENEMY_SIDE_SLOT_1) {
            SetAxisNegated(&axis, &axis6);
        } else if (typeB == BATTLER_TYPE_PLAYER_SIDE_SLOT_1) {
            SetAxisNegated(&axis, &axis2);
        } else {
            SetAxisNegated(&axis, &axis5);
        }
        break;
    }
    sign = ((s8(*)(BattleAnimSystem *, int, int))BattleAnimSystem_GetSideSign)(system, battlerA, battlerB);
    memset(&posArg, 0, sizeof(posArg));
    BattleAnimSystem_ReadScriptArgs(system, &posArg, 3);
    BattleAnimUtil_GetBattlerEmitterPositionKind0(system, battlerA, &pos);
    if (posArg.x == 0 && posArg.y == 0 && posArg.z == 0) {
        final = pos;
        SPL_SetEmitterPosition(emitter, &pos);
    } else {
        final.x = posArg.x;
        final.y = posArg.y;
        final.z = posArg.z;
        if (final.x == 0 && final.y == 3200) {
            sign = 1;
        }
        final.x *= sign;
        final.y *= sign;
        final.z = pos.z * sign;
        SPL_SetEmitterPosition(emitter, &final);
    }
    switch (mode) {
    case BATTLE_ANIM_EMITTER_TARGET_MODE_NONE:
        break;
    case BATTLE_ANIM_EMITTER_TARGET_MODE_CONVERGE:
        BattleAnimEmitter_SetConvergenceTarget(system, emitter, battlerA, battlerB, kind, sign, &final);
        break;
    case BATTLE_ANIM_EMITTER_TARGET_MODE_MAGNET:
        BattleAnimEmitter_SetMagnetTarget(system, emitter, battlerA, battlerB, kind, sign, &final);
        break;
    }
    SPL_SetEmitterAxis(emitter, &axis);
}

static void BattleAnimEmitter_SetTransformAttackerToDefender(SPLEmitter *emitter, int mode, int kind) {
    BattleAnimSystem *system = ParticleSystem_GetEmitterParam();
    int attacker = BattleAnimSystem_GetAttacker(system);
    int defender = BattleAnimSystem_GetDefender(system);
    BattleAnimEmitter_SetTransformFromScript(system, emitter, attacker, defender, mode, kind);
}

void BattleAnimEmitterCb_AttackerToDefender(SPLEmitter *emitter) {
    BattleAnimEmitter_SetTransformAttackerToDefender(emitter, BATTLE_ANIM_EMITTER_TARGET_MODE_NONE, BATTLE_ANIM_EMITTER_TARGET_ORIGIN);
}

// Attacker to defender, then the convergence target (BattleAnimEmitter_SetConvergenceTarget kinds 1-5)
void BattleAnimEmitterCb_ConvergeBySide(SPLEmitter *emitter) {
    BattleAnimEmitter_SetTransformAttackerToDefender(emitter, BATTLE_ANIM_EMITTER_TARGET_MODE_CONVERGE, BATTLE_ANIM_EMITTER_TARGET_BY_SIDE);
}

void BattleAnimEmitterCb_ConvergeUp(SPLEmitter *emitter) {
    BattleAnimEmitter_SetTransformAttackerToDefender(emitter, BATTLE_ANIM_EMITTER_TARGET_MODE_CONVERGE, BATTLE_ANIM_EMITTER_TARGET_UP);
}

void BattleAnimEmitterCb_ConvergeToDefender(SPLEmitter *emitter) {
    BattleAnimEmitter_SetTransformAttackerToDefender(emitter, BATTLE_ANIM_EMITTER_TARGET_MODE_CONVERGE, BATTLE_ANIM_EMITTER_TARGET_BATTLER_B);
}

void BattleAnimEmitterCb_ConvergeToAttacker(SPLEmitter *emitter) {
    BattleAnimEmitter_SetTransformAttackerToDefender(emitter, BATTLE_ANIM_EMITTER_TARGET_MODE_CONVERGE, BATTLE_ANIM_EMITTER_TARGET_BATTLER_A);
}

void BattleAnimEmitterCb_ConvergeFromScript(SPLEmitter *emitter) {
    BattleAnimEmitter_SetTransformAttackerToDefender(emitter, BATTLE_ANIM_EMITTER_TARGET_MODE_CONVERGE, BATTLE_ANIM_EMITTER_TARGET_FROM_SCRIPT);
}

// Attacker to defender, then the magnet target (same kinds)
void BattleAnimEmitterCb_MagnetBySide(SPLEmitter *emitter) {
    BattleAnimEmitter_SetTransformAttackerToDefender(emitter, BATTLE_ANIM_EMITTER_TARGET_MODE_MAGNET, BATTLE_ANIM_EMITTER_TARGET_BY_SIDE);
}

void BattleAnimEmitterCb_MagnetUp(SPLEmitter *emitter) {
    BattleAnimEmitter_SetTransformAttackerToDefender(emitter, BATTLE_ANIM_EMITTER_TARGET_MODE_MAGNET, BATTLE_ANIM_EMITTER_TARGET_UP);
}

void BattleAnimEmitterCb_MagnetToDefender(SPLEmitter *emitter) {
    BattleAnimEmitter_SetTransformAttackerToDefender(emitter, BATTLE_ANIM_EMITTER_TARGET_MODE_MAGNET, BATTLE_ANIM_EMITTER_TARGET_BATTLER_B);
}

void BattleAnimEmitterCb_MagnetToAttacker(SPLEmitter *emitter) {
    BattleAnimEmitter_SetTransformAttackerToDefender(emitter, BATTLE_ANIM_EMITTER_TARGET_MODE_MAGNET, BATTLE_ANIM_EMITTER_TARGET_BATTLER_A);
}

void BattleAnimEmitterCb_MagnetFromScript(SPLEmitter *emitter) {
    BattleAnimEmitter_SetTransformAttackerToDefender(emitter, BATTLE_ANIM_EMITTER_TARGET_MODE_MAGNET, BATTLE_ANIM_EMITTER_TARGET_FROM_SCRIPT);
}

void BattleAnimEmitterCb_DefenderToAttacker(SPLEmitter *emitter) {
    BattleAnimSystem *system = ParticleSystem_GetEmitterParam();
    int attacker = BattleAnimSystem_GetAttacker(system);
    int defender = BattleAnimSystem_GetDefender(system);
    BattleAnimEmitter_SetTransformFromScript(system, emitter, defender, attacker, BATTLE_ANIM_EMITTER_TARGET_MODE_NONE, BATTLE_ANIM_EMITTER_TARGET_ORIGIN);
}

static void BattleAnimEmitter_UpdateConfigured(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    if (config->positionMode != 0) {
        BattleAnimEmitter_ApplyConfiguredPosition(emitter, config);
    }
    if (config->axisMode != 0) {
        BattleAnimEmitter_ApplyConfiguredAxis(emitter, config);
    }
    if (config->flags != 0) {
        BattleAnimEmitter_ApplyConfiguredFlags(emitter, config);
    }
    BattleAnimEmitter_ApplyConfiguredCamera(emitter, config);
    if (config->zMode != 0) {
        BattleAnimEmitter_ApplyConfiguredZ(emitter, config);
    }
}

static void BattleAnimEmitter_GetDefenderSidePosition(SPLEmitter *emitter, BattleAnimEmitterConfig *config, VecFx32 *pos) {
    BattleAnimSystem *system = config->system;
    int perspective = ParticleSystem_GetPerspectiveType((ParticleSystem *)BattleAnimSystem_GetCurrentParticleSystem(system));
    int defender = BattleAnimSystem_GetDefender(system);
    int type = BattleAnimUtil_GetBattlerSide(system, defender) != BATTLE_ANIM_SIDE_PLAYER;
    BOOL isContest = BattleAnimSystem_IsContest(system);
    BattleAnimUtil_GetEmitterPositionKind0(type, pos, isContest, perspective);
}

static void BattleAnimEmitter_GetAttackerSidePosition(SPLEmitter *emitter, BattleAnimEmitterConfig *config, VecFx32 *pos) {
    BattleAnimSystem *system = config->system;
    int perspective = ParticleSystem_GetPerspectiveType((ParticleSystem *)BattleAnimSystem_GetCurrentParticleSystem(system));
    int attacker = BattleAnimSystem_GetAttacker(system);
    int type = BattleAnimUtil_GetBattlerSide(system, attacker) != BATTLE_ANIM_SIDE_PLAYER;
    BOOL isContest = BattleAnimSystem_IsContest(system);
    BattleAnimUtil_GetEmitterPositionKind0(type, pos, isContest, perspective);
}

static void BattleAnimEmitter_ApplyConfiguredPosition(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    VecFx32 pos;
    BattleAnimEmitterOffsetArgs args;
    BattleAnimScreenPosTable6 positions6;
    BattleAnimScreenPosTable7 positions7;
    BattleAnimScreenPosTable7 positions7b;

    switch (config->positionMode) {
    case 1:
        BattleAnimUtil_GetBattlerEmitterPositionKind0(config->system, config->battlerA, &pos);
        break;
    case 2:
        BattleAnimUtil_GetBattlerEmitterPositionKind0(config->system, config->battlerB, &pos);
        break;
    case 6:
        BattleAnimUtil_GetBattlerEmitterPositionKind1(config->system, config->battlerA, &pos);
        break;
    case 7:
        BattleAnimUtil_GetBattlerEmitterPositionKind1(config->system, config->battlerB, &pos);
        break;
    case 10:
        BattleAnimUtil_GetBattlerEmitterPositionKind2(config->system, config->battlerA, &pos);
        break;
    case 11:
        BattleAnimUtil_GetBattlerEmitterPositionKind2(config->system, config->battlerB, &pos);
        break;
    case 14:
        BattleAnimUtil_GetBattlerEmitterPositionKind3(config->system, config->battlerA, &pos);
        break;
    case 15:
        BattleAnimUtil_GetBattlerEmitterPositionKind3(config->system, config->battlerB, &pos);
        break;
    case 16:
        BattleAnimUtil_GetBattlerEmitterPositionKind4(config->system, config->battlerA, &pos);
        break;
    case 17:
        BattleAnimUtil_GetBattlerEmitterPositionKind4(config->system, config->battlerB, &pos);
        break;
    case 18:
        BattleAnimUtil_GetBattlerEmitterPositionKind5(config->system, config->battlerA, &pos);
        break;
    case 19:
        BattleAnimUtil_GetBattlerEmitterPositionKind5(config->system, config->battlerB, &pos);
        break;
    case 20:
        BattleAnimUtil_GetBattlerEmitterPositionKind6(config->system, config->battlerA, &pos);
        break;
    case 21:
        BattleAnimUtil_GetBattlerEmitterPositionKind6(config->system, config->battlerB, &pos);
        break;
    case 22:
        BattleAnimUtil_GetBattlerEmitterPositionKind7(config->system, config->battlerA, &pos);
        break;
    case 23:
        BattleAnimUtil_GetBattlerEmitterPositionKind7(config->system, config->battlerB, &pos);
        break;
    case 24:
        BattleAnimUtil_GetBattlerEmitterPositionKind8(config->system, config->battlerA, &pos);
        break;
    case 25:
        BattleAnimUtil_GetBattlerEmitterPositionKind8(config->system, config->battlerB, &pos);
        break;
    case 26:
        BattleAnimUtil_GetBattlerEmitterPositionKind9(config->system, config->battlerA, &pos);
        break;
    case 27:
        BattleAnimUtil_GetBattlerEmitterPositionKind9(config->system, config->battlerB, &pos);
        break;
    case 8:
        BattleAnimUtil_GetBattlerEmitterPositionKind11(config->system, config->battlerA, &pos);
        break;
    case 9:
        BattleAnimUtil_GetBattlerEmitterPositionKind11(config->system, config->battlerB, &pos);
        break;
    case 3:
        BattleAnimSystem_ReadScriptArgs(config->system, &args, 4);
        config->sideSign = BattleAnimEmitter_GetConfiguredSideSign(config);
        pos.x = args.offset.x;
        pos.y = args.offset.y;
        pos.z = args.offset.z;
        if (args.ignoreSide == TRUE) {
            config->sideSign = 1;
        }
        pos.x *= config->sideSign;
        pos.y *= config->sideSign;
        break;
    case 4:
        BattleAnimUtil_GetBattlerEmitterPositionKind0(config->system, config->battlerA, &pos);
        BattleAnimEmitter_AddOffsetFromScript(config, &pos);
        break;
    case 5:
        BattleAnimUtil_GetBattlerEmitterPositionKind0(config->system, config->battlerB, &pos);
        BattleAnimEmitter_AddOffsetFromScript(config, &pos);
        break;
    case 100:
        BattleAnimUtil_GetBattlerEmitterPositionKind0(config->system, config->battlerA, &pos);
        BattleAnimEmitter_AddOffsetForBattlerType(config, config->battlerA, &pos);
        break;
    case 101:
        BattleAnimUtil_GetBattlerEmitterPositionKind0(config->system, config->battlerB, &pos);
        BattleAnimEmitter_AddOffsetForBattlerType(config, config->battlerB, &pos);
        break;
    case 12:
        BattleAnimEmitter_GetAttackerSidePosition(emitter, config, &pos);
        BattleAnimEmitter_AddOffsetFromScript(config, &pos);
        break;
    case 13:
        BattleAnimEmitter_GetDefenderSidePosition(emitter, config, &pos);
        BattleAnimEmitter_AddOffsetFromScript(config, &pos);
        break;
    case 28:
        pos.x = 0x2CE0;
        pos.y = 0;
        pos.z = 0;
        break;
    case 33:
        pos.x = -5000;
        pos.y = -6000;
        pos.z = 0;
        break;
    case 34:
        BattleAnimUtil_GetBattlerEmitterPositionKind0(config->system, config->battlerA, &pos);
        break;
    case 31:
        positions6 = gBattleAnimEmitterPositions_Mode31;
        {
            int type = BattleAnimUtil_GetBattlerType(config->system, config->battlerA);
            pos.x = positions6.pos[type].x;
            pos.y = positions6.pos[type].y;
        }
        pos.z = 0;
        break;
    case 30:
        positions7 = gBattleAnimEmitterPositions_Mode30;
        {
            int type;
            if (BattleAnimSystem_IsContest(config->system) == TRUE) {
                type = 6;
            } else {
                type = BattleAnimUtil_GetBattlerType(config->system, config->battlerA);
            }
            pos.x = positions7.pos[type].x;
            pos.y = positions7.pos[type].y;
        }
        pos.z = 0;
        break;
    case 32:
        positions7b = gBattleAnimEmitterPositions_Mode32;
        {
            int type;
            if (BattleAnimSystem_IsContest(config->system) == TRUE) {
                type = 6;
            } else {
                type = BattleAnimUtil_GetBattlerType(config->system, config->battlerA);
            }
            pos.x = positions7b.pos[type].x;
            pos.y = positions7b.pos[type].y;
        }
        pos.z = 0;
        break;
    default:
        break;
    }
    config->position = pos;
    SPL_SetEmitterPosition(emitter, &pos);
}

#define SET_AXIS(x_, y_, z_) \
    axis.x = (x_);           \
    axis.y = (y_);           \
    axis.z = (z_)
#define SET_AXIS_FROM_TABLES(tableX, tableYZ, divZ, signX) \
    SET_AXIS((signX) * tableX[typeA][typeB].v[0], tableYZ[typeA][typeB].v[1], tableYZ[typeA][typeB].v[2] / (divZ))
#define SET_AXIS_FROM_TABLE(table, divZ, signX) SET_AXIS_FROM_TABLES(table, table, divZ, signX)
#define SET_AXIS_FROM_ROW(row)                  SET_AXIS((row).v[0], (row).v[1], (row).v[2])
#define SET_AXIS_FROM_ROW_NEGATED(row)          SET_AXIS(-(row).v[0], -(row).v[1], -(row).v[2])

static void BattleAnimEmitter_ApplyConfiguredAxis(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    BattleAnimEmitterAxisRows table;
    BattleAnimEmitterAxisRow a, b, c, d, e, f, g, base;
    BattleAnimEmitterAxisPositions positions;
    VecFx16 axis;
    int typeA = BattleAnimUtil_GetBattlerType(config->system, config->battlerA);
    int typeB = BattleAnimUtil_GetBattlerType(config->system, config->battlerB);
    int i, type;

    switch (config->axisMode) {
    case 0:
        SET_AXIS(0, 0, 0);
        break;
    case 1:
    case 2:
        if (BattleAnimSystem_IsContest(config->system) == TRUE) {
            if (config->battlerOrder == 2) {
                SET_AXIS(-3336, 1840, 736);
            } else {
                SET_AXIS(2336, -1440, 736);
            }
        } else {
            SET_AXIS_FROM_TABLE(gBattleAnimEmitterAxes_Mode1, 2, 1);
        }
        break;
    case 4:
        if (BattleAnimSystem_IsContest(config->system) == TRUE) {
            SET_AXIS_FROM_TABLE(gBattleAnimEmitterAxes_Mode1, 2, -1);
        } else if (BattleAnimUtil_GetBattlerSide(config->system, config->battlerA) == BattleAnimUtil_GetBattlerSide(config->system, config->battlerB)) {
            SET_AXIS_FROM_TABLE(gBattleAnimEmitterAxes_Mode1, 2, 1);
        } else if (BattleAnimUtil_GetBattlerSide(config->system, config->battlerA) == BATTLE_ANIM_SIDE_PLAYER) {
            SET_AXIS(3776, 2112, 1532);
        } else {
            SET_AXIS(-4228, -2728, 1532);
        }
        break;
    case 5:
        if (BattleAnimSystem_IsContest(config->system) == TRUE) {
            SET_AXIS_FROM_TABLE(gBattleAnimEmitterAxes_Mode1, 2, -1);
        } else if (BattleAnimUtil_GetBattlerSide(config->system, config->battlerA) == BattleAnimUtil_GetBattlerSide(config->system, config->battlerB)) {
            SET_AXIS_FROM_TABLE(gBattleAnimEmitterAxes_Mode1, 2, 1);
        } else if (BattleAnimUtil_GetBattlerSide(config->system, config->battlerA) == BATTLE_ANIM_SIDE_PLAYER) {
            SET_AXIS(3776, 2112, 1532);
        } else {
            SET_AXIS(-4228, -2728, 1532);
        }
        break;
    case 8:
    case 9:
        if (BattleAnimSystem_IsContest(config->system) == TRUE) {
            if (config->battlerOrder == 2) {
                SET_AXIS(-2336, 1440, 736);
            } else {
                SET_AXIS(2336, -1440, 736);
            }
        } else {
            SET_AXIS_FROM_TABLE(gBattleAnimEmitterAxes_Mode8, 2, 1);
        }
        break;
    case 10:
    case 11:
        if (BattleAnimSystem_IsContest(config->system) == TRUE) {
            SET_AXIS(-2336, 1440, 736);
        } else {
            SET_AXIS_FROM_TABLES(gBattleAnimEmitterAxes_Mode10, gBattleAnimEmitterAxes_Mode8, 2, 1);
        }
        break;
    case 12:
    case 13:
        if (BattleAnimSystem_IsContest(config->system) == TRUE) {
            SET_AXIS(-2936, 1440, 736);
        } else {
            SET_AXIS_FROM_TABLE(gBattleAnimEmitterAxes_Mode12, 1, 1);
        }
        break;
    case 14:
    case 15:
        if (BattleAnimSystem_IsContest(config->system) == TRUE) {
            SET_AXIS(-2336, 1440, 736);
        } else {
            SET_AXIS_FROM_TABLE(gBattleAnimEmitterAxes_Mode14, 1, 1);
        }
        break;
    case 16:
    case 17:
        if (BattleAnimSystem_IsContest(config->system) == TRUE) {
            SET_AXIS(-2336, 1440, 736);
        } else {
            SET_AXIS_FROM_TABLE(gBattleAnimEmitterAxes_Mode16, 1, 1);
        }
        break;
    case 18:
    case 19:
        if (BattleAnimSystem_IsContest(config->system) == TRUE) {
            SET_AXIS(-2336, 1440, 736);
        } else {
            SET_AXIS_FROM_TABLE(gBattleAnimEmitterAxes_Mode18, 1, 1);
        }
        break;
    case 20:
    case 21:
        if (BattleAnimSystem_IsContest(config->system) == TRUE) {
            SET_AXIS(-4336, 1440, 736);
        } else {
            SET_AXIS_FROM_TABLE(gBattleAnimEmitterAxes_Mode20, 1, 1);
        }
        break;
    case 6:
    case 7:
        table = gBattleAnimEmitterAxisRows;
        base = gBattleAnimEmitterAxisBase;
        for (i = 0; i < 4; i++) {
            a.v[i] = table.rows[0].v[i];
            b.v[i] = table.rows[1].v[i];
            c.v[i] = table.rows[2].v[i];
            d.v[i] = table.rows[3].v[i];
            e.v[i] = table.rows[4].v[i];
            f.v[i] = table.rows[5].v[i];
            g.v[i] = table.rows[6].v[i];
        }
        if (BattleAnimSystem_IsContest(config->system) == TRUE) {
            for (i = 0; i < 4; i++) {
                a.v[i] = base.v[i];
                b.v[i] = base.v[i];
                c.v[i] = base.v[i];
                d.v[i] = base.v[i];
                e.v[i] = base.v[i];
                f.v[i] = base.v[i];
                g.v[i] = base.v[i];
            }
        }
        switch (typeA) {
        default:
        case BATTLER_TYPE_SOLO_PLAYER:
            SET_AXIS_FROM_ROW(a);
            break;
        case BATTLER_TYPE_SOLO_ENEMY:
            if (a.v[3] == 1) {
                SET_AXIS_FROM_ROW(a);
            } else {
                SET_AXIS_FROM_ROW_NEGATED(a);
            }
            break;
        case BATTLER_TYPE_PLAYER_SIDE_SLOT_1:
            if (typeB == BATTLER_TYPE_ENEMY_SIDE_SLOT_1) {
                SET_AXIS_FROM_ROW(b);
            } else if (typeB == BATTLER_TYPE_ENEMY_SIDE_SLOT_2) {
                SET_AXIS_FROM_ROW(d);
            } else {
                SET_AXIS_FROM_ROW(c);
            }
            break;
        case BATTLER_TYPE_ENEMY_SIDE_SLOT_1:
            // BUG: toward the partner the sign is taken from row c but the axis from row e.
            if (typeB == BATTLER_TYPE_PLAYER_SIDE_SLOT_1) {
                if (b.v[3] == 1) {
                    SET_AXIS_FROM_ROW(b);
                } else {
                    SET_AXIS_FROM_ROW_NEGATED(b);
                }
            } else if (typeB == BATTLER_TYPE_ENEMY_SIDE_SLOT_2) {
                if (c.v[3] == 1) {
                    SET_AXIS_FROM_ROW_NEGATED(e);
                } else {
                    SET_AXIS_FROM_ROW(e);
                }
            } else {
                if (f.v[3] == 1) {
                    SET_AXIS_FROM_ROW(f);
                } else {
                    SET_AXIS_FROM_ROW_NEGATED(f);
                }
            }
            break;
        case BATTLER_TYPE_PLAYER_SIDE_SLOT_2:
            if (typeB == BATTLER_TYPE_ENEMY_SIDE_SLOT_1) {
                SET_AXIS_FROM_ROW(f);
            } else if (typeB == BATTLER_TYPE_ENEMY_SIDE_SLOT_2) {
                SET_AXIS_FROM_ROW(g);
            } else {
                SET_AXIS_FROM_ROW(e);
            }
            break;
        case BATTLER_TYPE_ENEMY_SIDE_SLOT_2:
            // BUG: the sign of every target is taken from row b, not from the row the axis comes from.
            if (typeB == BATTLER_TYPE_ENEMY_SIDE_SLOT_1) {
                if (b.v[3] == 1) {
                    SET_AXIS_FROM_ROW(e);
                } else {
                    SET_AXIS_FROM_ROW_NEGATED(e);
                }
            } else if (typeB == BATTLER_TYPE_PLAYER_SIDE_SLOT_1) {
                if (b.v[3] == 1) {
                    SET_AXIS_FROM_ROW(d);
                } else {
                    SET_AXIS_FROM_ROW_NEGATED(d);
                }
            } else {
                if (b.v[3] == 1) {
                    SET_AXIS_FROM_ROW(g);
                } else {
                    SET_AXIS_FROM_ROW_NEGATED(g);
                }
            }
            break;
        }
        break;
    case 3:
        SET_AXIS(-800, 1200, 500);
        break;
    case 22:
        SET_AXIS(-3410, -2644, 0);
        break;
    case 25:
        SET_AXIS(-3440, 1952, 0);
        break;
    case 26:
        if (BattleAnimUtil_GetBattlerSide(config->system, config->battlerA) == BATTLE_ANIM_SIDE_PLAYER) {
            SET_AXIS(3776, 2112, 0);
        } else {
            SET_AXIS(-6000, -2200, 0);
        }
        break;
    case 24:
        positions = gBattleAnimEmitterAxisPositions;
        type = BattleAnimUtil_GetBattlerType(config->system, config->battlerA);
        SET_AXIS(positions.pos[type][0], positions.pos[type][1], 0);
        break;
    }
    SPL_SetEmitterAxis(emitter, &axis);
}

#undef SET_AXIS
#undef SET_AXIS_FROM_TABLE
#undef SET_AXIS_FROM_TABLES
#undef SET_AXIS_FROM_ROW
#undef SET_AXIS_FROM_ROW_NEGATED

static void BattleAnimEmitter_ApplyConfiguredFlags(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    int count = BattleAnimEmitter_CountFlagMasks();
    int i;

    for (i = 0; i < count; i++) {
        int mask = BattleAnimEmitter_GetFlagMask(i);
        if ((config->flags & mask) == mask) {
            BattleAnimEmitter_RunFlagCallback(i, emitter, config);
        }
    }
}

static void BattleAnimEmitter_ApplyConfiguredCamera(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    BattleAnimEmitter_RunCameraCallback(config->cameraMode, emitter, config);
}

static void BattleAnimEmitter_ApplyConfiguredZ(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    int typeB;
    fx32 z;

    BattleAnimUtil_GetBattlerType(config->system, config->battlerA);
    typeB = BattleAnimUtil_GetBattlerType(config->system, config->battlerB);
    switch (config->zMode) {
    case 0:
        break;
    case 1:
        z = typeB == BATTLER_TYPE_SOLO_PLAYER ? 256 : -5120;
        break;
    case 2:
        z = typeB == BATTLER_TYPE_SOLO_PLAYER ? 0 : -5376;
        break;
    case 3:
        break;
    }
    // BUG: z is never set for mode 3, so the emitter gets an undefined z (mode 0 does not reach this function).
    ParticleSystem_GetPerspectiveType((ParticleSystem *)config->particleSystem);
    SPL_SetEmitterPositionZ(emitter, z);
}

void BattleAnimEmitterCb_ConfigureFromScript(SPLEmitter *emitter) {
    BattleAnimSystem *system = ParticleSystem_GetEmitterParam();
    BattleAnimEmitterConfig *config = Heap_Alloc(BattleAnimSystem_GetHeapID(system), sizeof(BattleAnimEmitterConfig));

    config->system = system;
    config->particleSystem = BattleAnimSystem_GetCurrentParticleSystem(config->system);
    BattleAnimSystem_ReadScriptArgs(config->system, &config->zMode, 6);
    switch (config->battlerOrder) {
    default:
    case 0:
        config->battlerA = BattleAnimSystem_GetAttacker(config->system);
        config->battlerB = BattleAnimSystem_GetDefender(config->system);
        break;
    case 1:
        config->battlerA = BattleAnimSystem_GetDefender(config->system);
        config->battlerB = BattleAnimSystem_GetAttacker(config->system);
        break;
    case 2:
        config->battlerA = BattleAnimSystem_GetAttacker(config->system);
        config->battlerB = BattleAnimSystem_GetDefender(config->system);
        break;
    case 3:
        config->battlerA = BattleAnimSystem_GetDefender(config->system);
        config->battlerB = BattleAnimSystem_GetAttacker(config->system);
        break;
    case 4:
        config->battlerA = BattleAnimSystem_GetAttacker(config->system);
        config->battlerB = BattleAnimSystem_GetDefender(config->system);
        break;
    }
    BattleAnimEmitter_UpdateConfigured(emitter, config);
    Heap_Free(config);
}

// -1 when battlerA's type is 1, 3 or 5, else +1
int BattleAnimEmitter_GetConfiguredSideSign(BattleAnimEmitterConfig *config) {
    int sign = 1;
    int typeA = BattleAnimUtil_GetBattlerType(config->system, config->battlerA);
    BattleAnimUtil_GetBattlerType(config->system, config->battlerB);

    switch (typeA) {
    case BATTLER_TYPE_SOLO_PLAYER:
        break;
    case BATTLER_TYPE_SOLO_ENEMY:
        sign -= 2;
        break;
    case BATTLER_TYPE_PLAYER_SIDE_SLOT_1:
        break;
    case BATTLER_TYPE_ENEMY_SIDE_SLOT_1:
        sign -= 2;
        break;
    case BATTLER_TYPE_PLAYER_SIDE_SLOT_2:
        break;
    case BATTLER_TYPE_ENEMY_SIDE_SLOT_2:
        sign -= 2;
        break;
    }
    return sign;
}

// Reads one BattleAnimEmitterOffsetArgs per battler type (6) and adds the one of battler's type to pos
static void BattleAnimEmitter_AddOffsetForBattlerType(BattleAnimEmitterConfig *config, int battler, VecFx32 *pos) {
    VecFx32 offset = { 0 };
    BattleAnimEmitterOffsetArgs args;
    BattleAnimEmitterOffsetArgs skipped;
    int i;

    for (i = 0; i < 6; i++) {
        if (i == BattleAnimUtil_GetBattlerType(config->system, battler)) {
            BattleAnimSystem_ReadScriptArgs(config->system, &args, 4);
        } else {
            BattleAnimSystem_ReadScriptArgs(config->system, &skipped, 4);
        }
    }
    config->sideSign = BattleAnimEmitter_GetConfiguredSideSign(config);
    offset.x = args.offset.x;
    offset.y = args.offset.y;
    offset.z = args.offset.z;
    if (args.ignoreSide == TRUE) {
        config->sideSign = 1;
    }
    offset.x *= config->sideSign;
    offset.y *= config->sideSign;
    offset.z *= config->sideSign;
    pos->x += offset.x;
    pos->y += offset.y;
    pos->z += offset.z;
}

static void BattleAnimEmitter_AddOffsetFromScript(BattleAnimEmitterConfig *config, VecFx32 *pos) {
    BattleAnimEmitterOffsetArgs args;
    VecFx32 offset = { 0 };

    BattleAnimSystem_ReadScriptArgs(config->system, &args, 4);
    config->sideSign = BattleAnimEmitter_GetConfiguredSideSign(config);
    offset.x = args.offset.x;
    offset.y = args.offset.y;
    offset.z = args.offset.z;
    if (args.ignoreSide == TRUE) {
        config->sideSign = 1;
    }
    offset.x *= config->sideSign;
    offset.y *= config->sideSign;
    offset.z *= config->sideSign;
    pos->x += offset.x;
    pos->y += offset.y;
    pos->z += offset.z;
}

void BattleAnimEmitter_ReadFlagCbParams(BattleAnimEmitterConfig *config, BattleAnimEmitterFlagParams *params) {
    BattleAnimSystem_ReadScriptArgs(config->system, params, 5);
}

BOOL BattleAnimEmitterFlagCb_Nop0(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    return TRUE;
}

// mode 0: none (mirrored zero), 1: script value
BOOL BattleAnimEmitterFlagCb_SetGravity(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    BattleAnimEmitterFlagParams params;
    VecFx16 value = { 0, 0, 0 };

    BattleAnimEmitter_ReadFlagCbParams(config, &params);
    config->sideSign = BattleAnimEmitter_GetConfiguredSideSign(config);
    if (params.ignoreSide == TRUE) {
        config->sideSign = 1;
    }
    switch (params.mode) {
    case 0:
        ParticleEmitter_SetGravity(emitter, &value);
        value.x *= config->sideSign;
        value.y *= config->sideSign;
        value.z *= config->sideSign;
        break;
    case 1:
        value.x = params.value.x;
        value.y = params.value.y;
        value.z = params.value.z;
        value.x *= config->sideSign;
        value.y *= config->sideSign;
        value.z *= config->sideSign;
        ParticleEmitter_SetGravity(emitter, &value);
        return TRUE;
    case 2:
    case 3:
    case 255:
        break;
    }
    value.x -= config->position.x;
    value.y -= config->position.y;
    value.z -= config->position.z;
    ParticleEmitter_SetGravity(emitter, &value);
    return TRUE;
}

BOOL BattleAnimEmitterFlagCb_Nop2(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    return TRUE;
}

BOOL BattleAnimEmitterFlagCb_Nop3(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    return TRUE;
}

// mode 0: current position mirrored, 1: script value, 2: battlerA, 3: battlerB; relative to the configured position
BOOL BattleAnimEmitterFlagCb_SetMagnetPos(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    BattleAnimEmitterFlagParams params;
    VecFx32 value = { 0 };

    BattleAnimEmitter_ReadFlagCbParams(config, &params);
    config->sideSign = BattleAnimEmitter_GetConfiguredSideSign(config);
    if (params.ignoreSide == TRUE) {
        config->sideSign = 1;
    }
    switch (params.mode) {
    case 0:
        ParticleEmitter_GetMagnetPos(emitter, &value);
        value.x *= config->sideSign;
        value.y *= config->sideSign;
        value.z *= config->sideSign;
        break;
    case 1:
        value.x = params.value.x;
        value.y = params.value.y;
        value.z = params.value.z;
        value.x *= config->sideSign;
        value.y *= config->sideSign;
        value.z *= config->sideSign;
        break;
    case 2:
        BattleAnimUtil_GetBattlerEmitterPositionKind0(config->system, config->battlerA, &value);
        break;
    case 3:
        BattleAnimUtil_GetBattlerEmitterPositionKind0(config->system, config->battlerB, &value);
        break;
    case 0xFF:
        break;
    }
    value.x -= config->position.x;
    value.y -= config->position.y;
    ParticleEmitter_SetMagnetPos(emitter, &value);
    return TRUE;
}

BOOL BattleAnimEmitterFlagCb_SetMagnetForce(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    s16 value = 0;
    BattleAnimEmitterFlagParams params;

    BattleAnimEmitter_ReadFlagCbParams(config, &params);
    config->sideSign = BattleAnimEmitter_GetConfiguredSideSign(config);
    if (params.ignoreSide == TRUE) {
        config->sideSign = 1;
    }
    switch (params.mode) {
    case 0:
        ParticleEmitter_GetMagnetForce(emitter, &value);
        value *= config->sideSign;
        break;
    case 1:
        value = params.value.x;
        value *= config->sideSign;
        break;
    case 2:
    case 3:
    case 0xFF:
        break;
    }
    ParticleEmitter_SetMagnetForce(emitter, &value);
    return TRUE;
}

BOOL BattleAnimEmitterFlagCb_SetSpinAngle(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    u16 value = 0;
    BattleAnimEmitterFlagParams params;

    BattleAnimEmitter_ReadFlagCbParams(config, &params);
    config->sideSign = BattleAnimEmitter_GetConfiguredSideSign(config);
    if (params.ignoreSide == TRUE) {
        config->sideSign = 1;
    }
    switch (params.mode) {
    case 0:
        ParticleEmitter_GetSpinAngle(emitter, &value);
        value *= config->sideSign;
        break;
    case 1:
        value = params.value.x;
        value *= config->sideSign;
        break;
    case 2:
    case 3:
    case 0xFF:
        break;
    }
    ParticleEmitter_SetSpinAngle(emitter, &value);
    return TRUE;
}

BOOL BattleAnimEmitterFlagCb_SetSpinAxis(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    u16 value = 0;
    BattleAnimEmitterFlagParams params;

    BattleAnimEmitter_ReadFlagCbParams(config, &params);
    config->sideSign = BattleAnimEmitter_GetConfiguredSideSign(config);
    if (params.ignoreSide == TRUE) {
        config->sideSign = 1;
    }
    switch (params.mode) {
    case 0:
        ParticleEmitter_GetSpinAxis(emitter, &value);
        break;
    case 1:
        value = params.value.x;
        break;
    case 2:
    case 3:
    case 0xFF:
        break;
    }
    ParticleEmitter_SetSpinAxis(emitter, &value);
    return TRUE;
}

BOOL BattleAnimEmitterFlagCb_Nop8(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    return TRUE;
}

BOOL BattleAnimEmitterFlagCb_Nop9(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    return TRUE;
}

BOOL BattleAnimEmitterFlagCb_Nop10(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    return TRUE;
}

BOOL BattleAnimEmitterFlagCb_Nop11(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    return TRUE;
}

// mode as in SetMagnetPos; 4: battlerB position scaled by value.x / value.y
BOOL BattleAnimEmitterFlagCb_SetConvergencePos(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    BattleAnimEmitterFlagParams params;
    VecFx32 value = { 0, 0, 0 };

    BattleAnimEmitter_ReadFlagCbParams(config, &params);
    config->sideSign = BattleAnimEmitter_GetConfiguredSideSign(config);
    if (params.ignoreSide == TRUE) {
        config->sideSign = 1;
    }
    switch (params.mode) {
    case 0:
        ParticleEmitter_GetConvergencePos(emitter, &value);
        value.x *= config->sideSign;
        value.y *= config->sideSign;
        value.z *= config->sideSign;
        break;
    case 1:
        value.x = params.value.x;
        value.y = params.value.y;
        value.z = params.value.z;
        value.x *= config->sideSign;
        value.y *= config->sideSign;
        value.z *= config->sideSign;
        break;
    case 4:
        BattleAnimUtil_GetBattlerEmitterPositionKind0(config->system, config->battlerB, &value);
        {
            int num = params.value.x, den = params.value.y;
            value.x = value.x * num / den;
            value.y = value.y * num / den;
            value.z = value.z * num / den;
        }
        break;
    case 2:
        BattleAnimUtil_GetBattlerEmitterPositionKind0(config->system, config->battlerA, &value);
        break;
    case 3:
        BattleAnimUtil_GetBattlerEmitterPositionKind0(config->system, config->battlerB, &value);
        break;
    case 255:
        break;
    }
    value.x -= config->position.x;
    value.y -= config->position.y;
    value.z -= config->position.z;
    ParticleEmitter_SetConvergencePos(emitter, &value);
    return TRUE;
}

BOOL BattleAnimEmitterFlagCb_SetConvergenceForce(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    BattleAnimEmitterFlagParams params;
    s16 value = 0;

    BattleAnimEmitter_ReadFlagCbParams(config, &params);
    config->sideSign = BattleAnimEmitter_GetConfiguredSideSign(config);
    if (params.ignoreSide == TRUE) {
        config->sideSign = 1;
    }
    switch (params.mode) {
    case 0:
        ParticleEmitter_GetConvergenceForce(emitter, &value);
        value *= config->sideSign;
        break;
    case 1:
        value = params.value.x;
        value *= config->sideSign;
        break;
    case 2:
    case 3:
    case 255:
        break;
    }
    ParticleEmitter_SetConvergenceForce(emitter, &value);
    return TRUE;
}

static void BattleAnimEmitter_RunFlagCallback(int index, SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    gBattleAnimEmitterFlagCallbacks[index](emitter, config);
}

static int BattleAnimEmitter_GetFlagMask(int index) {
    BattleAnimEmitterFlagMasks table = gBattleAnimEmitterFlagMasks;
    return table.masks[index];
}

// Entries up to and including the 0xFEFE terminator (0 if there is none in the first 255)
static int BattleAnimEmitter_CountFlagMasks(void) {
    int index = 0;
    int count = 0;

    while (1) {
        int mask = BattleAnimEmitter_GetFlagMask(index);
        index++;
        if (mask == 0xFEFE) {
            break;
        }
        count++;
        if (count >= 0xFF) {
            break;
        }
    }
    if (count >= 0xFF) {
        return 0;
    }
    return index;
}

static void BattleAnimEmitter_ReadCameraCbParams(BattleAnimEmitterConfig *config, u32 *params) {
    BattleAnimSystem_ReadScriptArgs(config->system, params, 4);
}

BOOL BattleAnimEmitterCameraCb_ResetAngle(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    CameraAngle angle = { 0, 0, 0, 0 };
    Camera_SetAnglePos(&angle, ParticleSystem_GetCamera(config->particleSystem));
    return TRUE;
}

static inline void CopyCameraAngle(volatile CameraAngle *dst, const volatile CameraAngle *src) {
    u16 z, unused;

    dst->x = src->x;
    dst->y = src->y;
    z = src->z;
    unused = src->unused;
    dst->z = z;
    dst->unused = unused;
}

BOOL BattleAnimEmitterCameraCb_SetPresetAngle4(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    CameraAngle angle;
    Camera *camera;

    CopyCameraAngle(&angle, &gBattleAnimEmitterCameraAngles[4]);
    camera = ParticleSystem_GetCamera(config->particleSystem);
    ParticleSystem_SetPerspectiveType(config->particleSystem, CAMERA_PERSPECTIVE_TYPE_ORTHOGRAPHIC);
    Camera_SetAnglePos(&angle, camera);
    return TRUE;
}

BOOL BattleAnimEmitterCameraCb_SetAngleFromScript(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    CameraAngle angle = { 0, 0, 0, 0 };
    u32 params[4];

    BattleAnimEmitter_ReadCameraCbParams(config, params);
    angle.x = params[1];
    angle.y = params[2];
    angle.z = params[3];
    Camera_SetAnglePos(&angle, ParticleSystem_GetCamera(config->particleSystem));
    return TRUE;
}

BOOL BattleAnimEmitterCameraCb_SetPresetAngle3(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    CameraAngle angle;
    Camera *camera;

    CopyCameraAngle(&angle, &gBattleAnimEmitterCameraAngles[3]);
    camera = ParticleSystem_GetCamera(config->particleSystem);
    ParticleSystem_SetPerspectiveType(config->particleSystem, CAMERA_PERSPECTIVE_TYPE_ORTHOGRAPHIC);
    Camera_SetAnglePos(&angle, camera);
    return TRUE;
}

BOOL BattleAnimEmitterCameraCb_SetAngleByMode(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    CameraAngle angle1 = gBattleAnimEmitterCameraAngles[2];
    CameraAngle angle2 = gBattleAnimEmitterCameraAngles[5];
    Camera *camera = ParticleSystem_GetCamera(config->particleSystem);

    ParticleSystem_SetPerspectiveType(config->particleSystem, CAMERA_PERSPECTIVE_TYPE_ORTHOGRAPHIC);
    if (BattleAnimSystem_IsContest(config->system) == TRUE) {
        Camera_SetAnglePos(&angle1, camera);
    } else {
        Camera_SetAnglePos(&angle2, camera);
    }
    return TRUE;
}

BOOL BattleAnimEmitterCameraCb_SetPresetAngle0(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    CameraAngle angle;
    Camera *camera;

    CopyCameraAngle(&angle, &gBattleAnimEmitterCameraAngles[0]);
    camera = ParticleSystem_GetCamera(config->particleSystem);
    ParticleSystem_SetPerspectiveType(config->particleSystem, CAMERA_PERSPECTIVE_TYPE_ORTHOGRAPHIC);
    Camera_SetAnglePos(&angle, camera);
    return TRUE;
}

BOOL BattleAnimEmitterCameraCb_SetPresetAngle1(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    CameraAngle angle;
    Camera *camera;

    CopyCameraAngle(&angle, &gBattleAnimEmitterCameraAngles[1]);
    camera = ParticleSystem_GetCamera(config->particleSystem);
    ParticleSystem_SetPerspectiveType(config->particleSystem, CAMERA_PERSPECTIVE_TYPE_ORTHOGRAPHIC);
    Camera_SetAnglePos(&angle, camera);
    return TRUE;
}

BOOL BattleAnimEmitterCameraCb_LookAtBattlerA(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    VecFx32 target;
    Camera *camera = ParticleSystem_GetCamera(config->particleSystem);
    BattleAnimUtil_GetBattlerEmitterPositionKind10(config->system, config->battlerA, &target);
    Camera_SetLookAtCamTarget(&target, camera);
    return TRUE;
}

BOOL BattleAnimEmitterCameraCb_LookAtBattlerB(SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    VecFx32 target;
    Camera *camera = ParticleSystem_GetCamera(config->particleSystem);
    BattleAnimUtil_GetBattlerEmitterPositionKind10(config->system, config->battlerB, &target);
    Camera_SetLookAtCamTarget(&target, camera);
    return TRUE;
}

static void BattleAnimEmitter_RunCameraCallback(int index, SPLEmitter *emitter, BattleAnimEmitterConfig *config) {
    gBattleAnimEmitterCameraCallbacks[index](emitter, config);
}

void BattleAnimUtil_LookupScreenPos(int set, int index, s16 *x, s16 *y) {
    if (x != NULL) {
        *x = gBattleAnimBattlerScreenPositions[set][index].x;
    }
    if (y != NULL) {
        *y = gBattleAnimBattlerScreenPositions[set][index].y;
    }
}

void BattleAnimSystem_GetBattlerScreenPos(BattleAnimSystem *system, int index, s16 *x, s16 *y) {
    if (BattleAnimSystem_IsContest(system) == TRUE) {
        BattleAnimUtil_LookupScreenPos(0, index + 2, x, y);
    } else if (BattleAnimSystem_IsDoubleBattle(system) == TRUE) {
        BattleAnimUtil_LookupScreenPos(1, index - 2, x, y);
    } else {
        BattleAnimUtil_LookupScreenPos(0, index, x, y);
    }
}

// coord: 0 / 2 x, 1 / 3 y
s16 BattleAnimSystem_GetBattlerScreenCoord(BattleAnimSystem *system, int battler, enum BattleAnimCoord coord) {
    int index = BattleAnimUtil_GetBattlerType(system, battler);
    int set = BattleAnimSystem_IsDoubleBattle(system) == TRUE;

    if (BattleAnimSystem_IsContest(system) == TRUE) {
        index += 2;
    }
    switch (coord) {
    case BATTLE_ANIM_COORD_X:
    case 2:
        return gBattleAnimBattlerScreenPositions[set][index].x;
    case BATTLE_ANIM_COORD_Y:
    case 3:
        return gBattleAnimBattlerScreenPositions[set][index].y;
    default:
        GF_ASSERT(FALSE);
        return 0;
    }
}

u8 BattleAnimUtil_GetSpritePaletteSlot(ManagedSprite *sprite) {
    return ObjPlttTransfer_GetPaletteVramOffset(Sprite_GetPaletteProxy(sprite->sprite), NNS_G2D_VRAM_TYPE_2DMAIN);
}

int BattleAnimUtil_GetBattlerSideSignByMode(BattleAnimSystem *system, int battler) {
    int sign = 1;
    int side = BattleAnimUtil_GetBattlerSide(system, battler);

    if (BattleAnimSystem_IsContest(system)) {
        if (side == BATTLE_ANIM_SIDE_PLAYER) {
            sign -= 2;
        }
    } else if (side == BATTLE_ANIM_SIDE_ENEMY) {
        sign -= 2;
    }
    return sign;
}

// -1 for a battler on side 4, else +1
int BattleAnimUtil_GetBattlerSideSign(BattleAnimSystem *system, int battler) {
    int sign = 1;

    if (BattleAnimUtil_GetBattlerSide(system, battler) == BATTLE_ANIM_SIDE_ENEMY) {
        sign -= 2;
    }
    return sign;
}

fx32 BattleAnimUtil_CalcStep(fx32 start, fx32 end, int duration) {
    return FX_Div(end - start, duration << FX32_SHIFT);
}

// Number of steps (rounded up) from start to end
int BattleAnimUtil_CalcStepCount(fx32 start, fx32 end, fx32 step) {
    fx32 count = FX_Div(end - start, step);

    if (FX_Modf(count, &count) != 0) {
        count += FX32_ONE;
    }
    count = count < 0 ? -count : count;
    return count >> FX32_SHIFT;
}

void BattleAnimUtil_SetSpritePosWithOffset(void *position, ManagedSprite *sprite, s16 x, s16 y) {
    BattleAnimMotionOffset *offset = position;
    ManagedSprite_SetPositionXY(sprite, x + offset->x, y + offset->y);
}

void BattleAnimUtil_SetSpriteScale(BattleAnimLinearMotion *scale, ManagedSprite *sprite) {
    f32 x, y;

    BattleAnimUtil_GetScaleFloat(scale, &x, &y);
    ManagedSprite_SetAffineScale(sprite, x, y);
}

void BattleAnimUtil_SetPokepicPosWithOffset(void *position, Pokepic *pokepic, int x, int y) {
    BattleAnimMotionOffset *offset = position;
    Pokepic_SetAttr(pokepic, POKEPIC_X, x + offset->x);
    Pokepic_SetAttr(pokepic, POKEPIC_Y, y + offset->y);
}

void BattleAnimUtil_SetPokepicScale(BattleAnimLinearMotion *scale, Pokepic *pokepic) {
    Pokepic_SetAttr(pokepic, POKEPIC_AFFINEW, scale->x);
    Pokepic_SetAttr(pokepic, POKEPIC_AFFINEH, scale->y);
}

void BattleAnimUtil_InitSinMotion(BattleAnimSinMotion *motion, int startX, int endX, int startY, u16 endY, fx32 ampX, fx32 ampY, int duration) {
    motion->duration = duration;
    motion->angleX = startX;
    motion->ampX = ampX;
    motion->angleY = startY;
    motion->ampY = ampY;
    motion->stepX = (endX - startX) / duration;
    motion->stepY = ((u16)endY - startY) / duration;
}
#if defined(SDK_PORT)

/* Native tables from asm/battle_anim_data.s; adapters give differing originals the table's signature. */
BattleAnimEmitterConfigCb gBattleAnimEmitterFlagCallbacks[] = {
    BattleAnimEmitterFlagCb_Nop0,
    BattleAnimEmitterFlagCb_SetGravity,
    BattleAnimEmitterFlagCb_Nop2,
    BattleAnimEmitterFlagCb_Nop3,
    BattleAnimEmitterFlagCb_SetMagnetPos,
    BattleAnimEmitterFlagCb_SetMagnetForce,
    BattleAnimEmitterFlagCb_SetSpinAngle,
    BattleAnimEmitterFlagCb_SetSpinAxis,
    BattleAnimEmitterFlagCb_Nop8,
    BattleAnimEmitterFlagCb_Nop9,
    BattleAnimEmitterFlagCb_Nop10,
    BattleAnimEmitterFlagCb_Nop11,
    BattleAnimEmitterFlagCb_SetConvergencePos,
    BattleAnimEmitterFlagCb_SetConvergenceForce,
};
BattleAnimEmitterConfigCb gBattleAnimEmitterCameraCallbacks[] = {
    BattleAnimEmitterCameraCb_ResetAngle,
    BattleAnimEmitterCameraCb_SetPresetAngle4,
    BattleAnimEmitterCameraCb_SetAngleFromScript,
    BattleAnimEmitterCameraCb_SetPresetAngle3,
    BattleAnimEmitterCameraCb_SetAngleByMode,
    BattleAnimEmitterCameraCb_SetPresetAngle0,
    BattleAnimEmitterCameraCb_SetPresetAngle1,
    BattleAnimEmitterCameraCb_LookAtBattlerA,
    BattleAnimEmitterCameraCb_LookAtBattlerB,
};
#endif
