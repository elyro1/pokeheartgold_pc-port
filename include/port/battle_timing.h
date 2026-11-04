#ifndef POKEHEARTGOLD_PORT_BATTLE_TIMING_H
#define POKEHEARTGOLD_PORT_BATTLE_TIMING_H

#include <nitro/types.h>

// Ownership follows callbacks and their child tasks, never task priority.
typedef enum BattleTimingDomain {
    BATTLE_TIMING_NONE,
    BATTLE_TIMING_ANIMATION,
    BATTLE_TIMING_UI,
} BattleTimingDomain;

void BattleTiming_SetActive(BOOL active);
BOOL BattleTiming_IsActive(void);
void BattleTiming_BeginFrame(void);
u32 BattleTiming_GetFrame(void);
u32 BattleTiming_GetVisualEpoch(void);
BOOL BattleTiming_ShouldSmoothAnimations(void);
BOOL BattleTiming_ShouldTick(BattleTimingDomain domain);
// Convert a presentation-frame wait for a task running on the animation clock.
u32 BattleTiming_AnimationTicksForFrames(u32 frames);
BattleTimingDomain BattleTiming_GetDomain(void);
BattleTimingDomain BattleTiming_SetDomain(BattleTimingDomain domain);

#endif
