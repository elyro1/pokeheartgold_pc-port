#include "sys_task_api.h"

#include "global.h"

#include "system.h"

SysTask *SysTask_CreateOnMainQueue(SysTaskFunc func, void *data, u32 priority) {
    return SysTaskQueue_InsertTask(gSystem.mainTaskQueue, func, data, priority);
}

SysTask *SysTask_CreateOnVBlankQueue(SysTaskFunc func, void *data, u32 priority) {
#if defined(SDK_PORT)
    BattleTimingDomain previous = BattleTiming_SetDomain(BATTLE_TIMING_NONE);
    SysTask *task = SysTaskQueue_InsertTask(gSystem.vblankTaskQueue, func, data, priority);
    BattleTiming_SetDomain(previous);
    return task;
#else
    return SysTaskQueue_InsertTask(gSystem.vblankTaskQueue, func, data, priority);
#endif
}

SysTask *SysTask_CreateOnPrintQueue(SysTaskFunc func, void *data, u32 priority) {
    return SysTaskQueue_InsertTask(gSystem.printTaskQueue, func, data, priority);
}

SysTask *SysTask_CreateOnVWaitQueue(SysTaskFunc func, void *data, u32 priority) {
#if defined(SDK_PORT)
    BattleTimingDomain previous = BattleTiming_SetDomain(BATTLE_TIMING_NONE);
    SysTask *task = SysTaskQueue_InsertTask(gSystem.vwaitTaskQueue, func, data, priority);
    BattleTiming_SetDomain(previous);
    return task;
#else
    return SysTaskQueue_InsertTask(gSystem.vwaitTaskQueue, func, data, priority);
#endif
}

void SysTask_Destroy(SysTask *task) {
    SysTask_Unlink(task);
}
#if defined(SDK_PORT)

SysTask *SysTask_CreateBattleAnimation(SysTaskFunc func, void *data, u32 priority) {
    BattleTimingDomain previous = BattleTiming_SetDomain(BATTLE_TIMING_ANIMATION);
    SysTask *task = SysTask_CreateOnMainQueue(func, data, priority);
    BattleTiming_SetDomain(previous);
    return task;
}

SysTask *SysTask_CreateBattleUI(SysTaskFunc func, void *data, u32 priority) {
    BattleTimingDomain previous = BattleTiming_SetDomain(BATTLE_TIMING_UI);
    SysTask *task = SysTask_CreateOnMainQueue(func, data, priority);
    BattleTiming_SetDomain(previous);
    return task;
}

SysTask *SysTask_CreateUnscaled(SysTaskFunc func, void *data, u32 priority) {
    BattleTimingDomain previous = BattleTiming_SetDomain(BATTLE_TIMING_NONE);
    SysTask *task = SysTask_CreateOnMainQueue(func, data, priority);
    BattleTiming_SetDomain(previous);
    return task;
}

#endif
