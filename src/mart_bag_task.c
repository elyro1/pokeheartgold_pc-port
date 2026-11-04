#include "mart_bag_task.h"

#include "global.h"

#include "field/fieldmap.h"

#include "bag_view.h"
#include "heap.h"
#include "overlay_03.h"
#include "task.h"

#if defined(SDK_PORT)
static BOOL MartBag_WaitBagViewExit(FieldSystem *fieldSystem, MartData *env);
#else
typedef struct MartReturnEnv {
    u8 unk_00[4];
    BagView *bagView;
    u8 unk_08[0x26A];
    u8 state;
    u8 unk_273[0x2D];
    u16 *result;
} MartReturnEnv;

static BOOL MartBag_WaitBagViewExit(FieldSystem *fieldSystem, MartReturnEnv *env);
#endif
static void MartBag_ReturnToMart(TaskManager *taskManager);

BOOL Task_MartBag(TaskManager *taskManager) {
    FieldSystem *fieldSystem = TaskManager_GetFieldSystem(taskManager);
#if defined(SDK_PORT)
    MartData *env = TaskManager_GetEnvironment(taskManager);
#else
    MartReturnEnv *env = TaskManager_GetEnvironment(taskManager);
#endif
    switch (env->state) {
    case 24:
        env->state = MartBag_WaitBagViewExit(fieldSystem, env);
        break;
    case 25:
        MartBag_ReturnToMart(taskManager);
        break;
    }
    return FALSE;
}

#if defined(SDK_PORT)
static BOOL MartBag_WaitBagViewExit(FieldSystem *fieldSystem, MartData *env) {
#else
static BOOL MartBag_WaitBagViewExit(FieldSystem *fieldSystem, MartReturnEnv *env) {
#endif
    if (FieldSystem_ApplicationIsRunning(fieldSystem)) {
        return 24;
    }
#if defined(SDK_PORT)
    if (env->sellResult != NULL) {
        env->sellResult[1] = BagView_GetSellCount(env->bagView);
#else
    if (env->result != NULL) {
        env->result[1] = BagView_GetSellCount(env->bagView);
#endif
    }
    Heap_Free(env->bagView);
    FieldSystem_LoadFieldOverlay(fieldSystem);
    return 25;
}

static void MartBag_ReturnToMart(TaskManager *taskManager) {
    FieldSystem *fieldSystem = TaskManager_GetFieldSystem(taskManager);
#if defined(SDK_PORT)
    MartData *env = TaskManager_GetEnvironment(taskManager);
#else
    MartReturnEnv *env = TaskManager_GetEnvironment(taskManager);
#endif
    if (FieldSystem_IsOverworldRestored(fieldSystem)) {
        FieldMap_FadeScreen(1);
        TaskManager_Jump(taskManager, Task_Mart, env);
        env->state = 26;
    }
}
