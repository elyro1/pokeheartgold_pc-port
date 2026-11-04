#include "pokeathlon/pokeathlon_event_points.h"

#include "global.h"

#include "pokeathlon/pokeathlon_course_internal.h"

#include "assert.h"
#include "math_util.h"

// The course points (at most 200) every participant gets for an event: events 0-8 convert each participant's
// event score (gPokeathlonEventScorePointsFuncs[event]), event 9 (Goal Roll) gives points by rank.

void PokeathlonEvent_CalcPoints(PokeathlonCourseScoreList *list, int event) {
    gPokeathlonEventPointsCalcFuncs[event](list, event);
}

void PokeathlonEvent_CalcPointsFromScores(PokeathlonCourseScoreList *list, int event) {
    PokeathlonEventPointsFunc func = gPokeathlonEventScorePointsFuncs[event];
    u8 i;
    if (func != NULL) {
        for (i = 0; i < POKEATHLON_PARTICIPANTS; i++) {
            PokeathlonCourseScore *entry = PokeathlonCourseScoreList_GetRanked(list, i);
            entry->points = func((u16)entry->score);
        }
    } else {
        GF_ASSERT(FALSE);
    }
}

void PokeathlonEvent_CalcPointsFromRanks(PokeathlonCourseScoreList *list, int event) {
    if (gPokeathlonEventScorePointsFuncs[event] == NULL) {
        PokeathlonEvent_CalcRankPoints(list);
    } else {
        GF_ASSERT(FALSE);
    }
}

// 30 points at 0, approaching 150.
static float PokeathlonEvent_Saturate(u32 score, float rate) {
    return 150.0 - 120.0 / (1.0 + (float)score * rate);
}

static u8 PokeathlonEvent_ClampPoints(float points) {
    u32 clamped = points;
    if ((double)clamped > 200.0) {
        clamped = 200;
    }
    return clamped;
}

// Event 0 (Hurdle Dash): the score is the time in frames.
u8 PokeathlonEvent_HurdleDashPoints(u32 frames) {
    return PokeathlonEvent_ClampPoints(11500.0 / (double)((float)frames / 30.0f));
}

// Event 1 (Pennant Capture)
u8 PokeathlonEvent_PennantCapturePoints(int score) {
    return PokeathlonEvent_ClampPoints(score * 3);
}

// Event 2 (Circle Push)
u8 PokeathlonEvent_CirclePushPoints(int score) {
    return PokeathlonEvent_ClampPoints(score * 3);
}

// Event 3 (Block Smash)
u8 PokeathlonEvent_BlockSmashPoints(u32 score) {
    return PokeathlonEvent_ClampPoints(score);
}

// Event 4 (Disc Catch)
u8 PokeathlonEvent_DiscCatchPoints(u32 score) {
    return PokeathlonEvent_ClampPoints(PokeathlonEvent_Saturate(score, 0.08f));
}

// Event 5 (Lamp Jump)
u8 PokeathlonEvent_LampJumpPoints(u32 score) {
    return PokeathlonEvent_ClampPoints((double)(float)score / 3.5);
}

u8 PokeathlonEvent_RelayRunPoints(u32 score) {
    return PokeathlonEvent_ClampPoints(((double)(float)score / 1024.0) * 10.0);
}

// Event 7 (Ring Drop)
u8 PokeathlonEvent_RingDropPoints(u32 score) {
    double points = (float)score;
    points *= 1.5;
    return PokeathlonEvent_ClampPoints(points);
}

u8 PokeathlonEvent_SnowThrowPoints(int score) {
    return PokeathlonEvent_ClampPoints(score * 3);
}
#if defined(SDK_PORT)

/* Native tables from asm/pokeathlon_event_data.s. */
void (*const gPokeathlonEventPointsCalcFuncs[])(PokeathlonCourseScoreList *list, int event) = {
    PokeathlonEvent_CalcPointsFromScores,
    PokeathlonEvent_CalcPointsFromScores,
    PokeathlonEvent_CalcPointsFromScores,
    PokeathlonEvent_CalcPointsFromScores,
    PokeathlonEvent_CalcPointsFromScores,
    PokeathlonEvent_CalcPointsFromScores,
    PokeathlonEvent_CalcPointsFromScores,
    PokeathlonEvent_CalcPointsFromScores,
    PokeathlonEvent_CalcPointsFromScores,
    PokeathlonEvent_CalcPointsFromRanks,
};

static u32 Native_PokeathlonEvent_HurdleDashPoints(u32 score) { return PokeathlonEvent_HurdleDashPoints(score); }
static u32 Native_PokeathlonEvent_PennantCapturePoints(u32 score) { return PokeathlonEvent_PennantCapturePoints(score); }
static u32 Native_PokeathlonEvent_CirclePushPoints(u32 score) { return PokeathlonEvent_CirclePushPoints(score); }
static u32 Native_PokeathlonEvent_BlockSmashPoints(u32 score) { return PokeathlonEvent_BlockSmashPoints(score); }
static u32 Native_PokeathlonEvent_DiscCatchPoints(u32 score) {
    return PokeathlonEvent_DiscCatchPoints((float)score);
}
static u32 Native_PokeathlonEvent_LampJumpPoints(u32 score) { return PokeathlonEvent_LampJumpPoints(score); }
static u32 Native_PokeathlonEvent_RelayRunPoints(u32 score) { return PokeathlonEvent_RelayRunPoints(score); }
static u32 Native_PokeathlonEvent_RingDropPoints(u32 score) { return PokeathlonEvent_RingDropPoints(score); }
static u32 Native_PokeathlonEvent_SnowThrowPoints(u32 score) { return PokeathlonEvent_SnowThrowPoints(score); }
const PokeathlonEventPointsFunc gPokeathlonEventScorePointsFuncs[] = {
    Native_PokeathlonEvent_HurdleDashPoints,
    Native_PokeathlonEvent_PennantCapturePoints,
    Native_PokeathlonEvent_CirclePushPoints,
    Native_PokeathlonEvent_BlockSmashPoints,
    Native_PokeathlonEvent_DiscCatchPoints,
    Native_PokeathlonEvent_LampJumpPoints,
    Native_PokeathlonEvent_RelayRunPoints,
    Native_PokeathlonEvent_RingDropPoints,
    Native_PokeathlonEvent_SnowThrowPoints,
    0,
};
#endif
