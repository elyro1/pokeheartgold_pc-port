#include "frontier/frontier_system.h"

#include "global.h"

#include "frontier/frontier_map.h"
#include "frontier/frontier_map_params.h"
#include "frontier/frontier_scrcmd.h"
#include "frontier/frontier_script_context.h"

#include "filesystem.h"
#include "heap.h"
#include "message_format.h"
#include "msgdata.h"
#include "pm_string.h"

static BOOL FrontierSystem_InsertTask(FrontierSystem *system, FrontierScriptContext *task);
static void FrontierSystem_FreeTask(FrontierSystem *system, FrontierScriptContext *task);
static void FrontierSystem_InitDummy(FrontierSystem *system, u16 mapId, u32 a2, u32 a3);
static void FrontierSystem_LoadMapScripts(u8 **scripts, MsgData **msgData, u32 mapId, enum HeapID heapId);
static void FrontierScriptContext_GoToScript(FrontierScriptContext *ctx, u32 scriptIndex);

FrontierSystem *FrontierSystem_Create(FrontierWork *frontierWork, enum HeapID heapId, u32 mapId) {
    FrontierSystem *system = Heap_Alloc(heapId, sizeof(FrontierSystem));
    MI_CpuFill8(system, 0, sizeof(FrontierSystem));
    system->frontierWork = frontierWork;
    system->heapId = heapId;
    system->mapId = mapId;
    FrontierSystem_LoadMapScripts(&system->scripts, &system->msgData, mapId, heapId);
    system->msgFormat = MessageFormat_New_Custom(8, 64, heapId);
    system->msgBuffer = String_New(1024, heapId);
    system->msgTemplate = String_New(1024, heapId);
    FrontierSystem_InitDummy(system, mapId, 0, 0);
    return system;
}

// Runs every task one step; finished tasks are freed. Returns TRUE once no task is left.
BOOL FrontierSystem_Main(FrontierSystem *system) {
    s32 i;
    if (system->taskCount == 0) {
        return TRUE;
    }
    if (system->unk_38 == 0) {
        for (i = 0; i < 8; i++) {
            FrontierScriptContext *task = system->tasks[i];
            if (task != NULL && !FrontierScriptContext_Run(task)) {
                FrontierSystem_FreeTask(system, task);
                system->tasks[i] = NULL;
                system->taskCount--;
            }
        }
        FrontierMap_RunObjectMovements(system->frontierWork);
    }
    return system->taskCount == 0;
}

void FrontierSystem_Delete(FrontierSystem *system) {
    GF_ASSERT(system->taskCount == 0);
    MessageFormat_Delete(system->msgFormat);
    String_Delete(system->msgBuffer);
    String_Delete(system->msgTemplate);
    Heap_Free(system->scripts);
    DestroyMsgData(system->msgData);
    Heap_Free(system);
}

// Starts script scriptIndex of map mapId (0xFFFF: the current map) in a new task.
void FrontierSystem_AddTask(FrontierSystem *system, u16 mapId, u32 scriptIndex) {
    FrontierScriptContext *task = Heap_Alloc(system->heapId, sizeof(FrontierScriptContext));
    MI_CpuFill8(task, 0, sizeof(FrontierScriptContext));
    FrontierScriptContext_Init(task, gFrontierCommandTable, gFrontierCommandTableSize);
    task->frontierSystem = system;
    if (mapId == 0xFFFF || system->mapId == mapId) {
        task->scripts = system->scripts;
        task->msgLoader = system->msgData;
    } else {
        FrontierSystem_LoadMapScripts(&task->scripts, &task->msgLoader, mapId, system->heapId);
    }
    FrontierScriptContext_Start(task, task->scripts);
    FrontierScriptContext_GoToScript(task, scriptIndex);
    FrontierSystem_InsertTask(system, task);
}

static BOOL FrontierSystem_InsertTask(FrontierSystem *system, FrontierScriptContext *task) {
    s32 i;
    for (i = 0; i < 8; i++) {
        if (system->tasks[i] == NULL) {
            system->tasks[i] = task;
            system->taskCount++;
            return TRUE;
        }
    }
    GF_ASSERT(FALSE);
    return FALSE;
}

// Frees a task and the script data / messages it loaded for another map.
static void FrontierSystem_FreeTask(FrontierSystem *system, FrontierScriptContext *task) {
    if (task->msgLoader != system->msgData) {
        DestroyMsgData(task->msgLoader);
    }
    if (task->scripts != system->scripts) {
        Heap_Free(task->scripts);
    }
    Heap_Free(task);
}

static void FrontierSystem_InitDummy(FrontierSystem *system, u16 mapId, u32 a2, u32 a3) {
}

static void FrontierSystem_LoadMapScripts(u8 **scripts, MsgData **msgData, u32 mapId, enum HeapID heapId) {
    u32 member = FrontierMap_GetParam(mapId, MAP_PARAM_SCRIPT_ID);
    u32 bank = FrontierMap_GetParam(mapId, MAP_PARAM_MSG_BANK);
    *scripts = AllocAndReadWholeNarcMemberByIdPair(NARC_a_1_8_2, member, heapId);
    *msgData = NewMsgDataFromNarc(MSGDATA_LOAD_LAZY, NARC_msgdata_msg, bank, heapId);
}

// Switches the system's messages (and the tasks using them) to the message bank of map mapId.
void FrontierSystem_ChangeMsgBank(FrontierSystem *system, u16 mapId, enum HeapID heapId) {
    s32 i;
    u32 oldBank = FrontierMap_GetParam(system->mapId, MAP_PARAM_MSG_BANK);
    u32 newBank = FrontierMap_GetParam(mapId, MAP_PARAM_MSG_BANK);
    if (oldBank != newBank) {
        MsgData *msgData = NewMsgDataFromNarc(MSGDATA_LOAD_LAZY, NARC_msgdata_msg, newBank, heapId);
        for (i = 0; i < 8; i++) {
            if (system->tasks[i] != NULL && system->tasks[i]->msgLoader == system->msgData) {
                system->tasks[i]->msgLoader = msgData;
            }
        }
        DestroyMsgData(system->msgData);
        system->msgData = msgData;
    }
}

u16 *FrontierSystem_SaveVars(FrontierSystem *system, enum HeapID heapId) {
    u16 *saved = Heap_Alloc(heapId, sizeof(system->vars));
    u16 *src = system->vars;
    u16 *dest = saved;
    u32 count = 8;
    do {
        *dest++ = *src++;
    } while (--count);
    return saved;
}

void FrontierSystem_RestoreVars(FrontierSystem *system, u16 *saved) {
    u16 *src = saved;
    u16 *dest = system->vars;
    u32 count = 8;
    do {
        *dest++ = *src++;
    } while (--count);
    Heap_Free(saved);
}

// The script data starts with a table of relative offsets, one word per script.
static void FrontierScriptContext_GoToScript(FrontierScriptContext *ctx, u32 scriptIndex) {
    ctx->scriptPtr += scriptIndex * 4;
    ctx->scriptPtr += FrontierScriptContext_ReadWord(ctx);
}

u16 *FrontierSystem_GetVarPtr(FrontierSystem *system, u32 index) {
    return &system->vars[index];
}

FrontierMap *FrontierSystem_GetFrontierMap(FrontierSystem *system) {
    return Frontier_GetMap(system->frontierWork);
}
#if defined(SDK_PORT)

/* Native tables from asm/frontier_script_commands_data.s; adapters give differing originals the table's signature. */
BOOL FrtCmd_Nop(FrontierScriptContext *ctx);
BOOL FrtCmd_End(FrontierScriptContext *ctx);
BOOL FrtCmd_Exit(FrontierScriptContext *ctx);
BOOL FrtCmd_ChangeMode(FrontierScriptContext *ctx);
BOOL FrtCmd_ChangeModeWithTask(FrontierScriptContext *ctx);
BOOL FrtCmd_Wait(FrontierScriptContext *ctx);
BOOL FrtCmd_SetVar(FrontierScriptContext *ctx);
BOOL FrtCmd_CopyVar(FrontierScriptContext *ctx);
BOOL FrtCmd_AddVar(FrontierScriptContext *ctx);
BOOL FrtCmd_SubVar(FrontierScriptContext *ctx);
BOOL FrtCmd_GoTo(FrontierScriptContext *ctx);
BOOL FrtCmd_GoToIf(FrontierScriptContext *ctx);
BOOL FrtCmd_Call(FrontierScriptContext *ctx);
BOOL FrtCmd_Return(FrontierScriptContext *ctx);
BOOL FrtCmd_CallIf(FrontierScriptContext *ctx);
BOOL FrtCmd_PrintMessageInstant(FrontierScriptContext *ctx);
BOOL FrtCmd_PrintMessage(FrontierScriptContext *ctx);
BOOL FrtCmd_PrintMessageCanSpeedUp(FrontierScriptContext *ctx);
BOOL FrtCmd_CloseMessage(FrontierScriptContext *ctx);
BOOL FrtCmd_FadeScreen(FrontierScriptContext *ctx);
BOOL FrtCmd_WaitFade(FrontierScriptContext *ctx);
BOOL FrtCmd_MenuInitStdGmm(FrontierScriptContext *ctx);
BOOL FrtCmd_MenuInit(FrontierScriptContext *ctx);
BOOL FrtCmd_MenuItemAdd(FrontierScriptContext *ctx);
BOOL FrtCmd_MenuItemAddWithDescription(FrontierScriptContext *ctx);
BOOL FrtCmd_MenuExec(FrontierScriptContext *ctx);
BOOL FrtCmd_ListMenuInitStdGmm(FrontierScriptContext *ctx);
BOOL FrtCmd_ListMenuInit(FrontierScriptContext *ctx);
BOOL FrtCmd_ListMenuItemAdd(FrontierScriptContext *ctx);
BOOL FrtCmd_ListMenuExec(FrontierScriptContext *ctx);
BOOL FrtCmd_ListMenuCancel(FrontierScriptContext *ctx);
BOOL FrtCmd_YesNo(FrontierScriptContext *ctx);
BOOL FrtCmd_CompareVarToValue(FrontierScriptContext *ctx);
BOOL FrtCmd_CompareVarToVar(FrontierScriptContext *ctx);
BOOL FrtCmd_LoadObjectGfx(FrontierScriptContext *ctx);
BOOL FrtCmd_RemoveObjectGfx(FrontierScriptContext *ctx);
BOOL FrtCmd_CreateObjects(FrontierScriptContext *ctx);
BOOL FrtCmd_DeleteObject(FrontierScriptContext *ctx);
BOOL FrtCmd_SetObjectVisible(FrontierScriptContext *ctx);
BOOL FrtCmd_Dummy039(FrontierScriptContext *ctx);
BOOL FrtCmd_ApplyMovement(FrontierScriptContext *ctx);
BOOL FrtCmd_WaitMovement(FrontierScriptContext *ctx);
BOOL FrtCmd_LoadSpriteResources(FrontierScriptContext *ctx);
BOOL FrtCmd_UnloadSpriteResources(FrontierScriptContext *ctx);
BOOL FrtCmd_CreateSprites(FrontierScriptContext *ctx);
BOOL FrtCmd_DeleteSprite(FrontierScriptContext *ctx);
BOOL FrtCmd_SetSpriteDrawFlag(FrontierScriptContext *ctx);
BOOL FrtCmd_SetSpriteFlip(FrontierScriptContext *ctx);
BOOL FrtCmd_SetSpriteTranslucent(FrontierScriptContext *ctx);
BOOL FrtCmd_StartSpriteAnim(FrontierScriptContext *ctx);
BOOL FrtCmd_StopSpriteAnim(FrontierScriptContext *ctx);
BOOL FrtCmd_WaitSpriteAnim(FrontierScriptContext *ctx);
BOOL FrtCmd_LaunchNamingScreen(FrontierScriptContext *ctx);
BOOL FrtCmd_LinkSync(FrontierScriptContext *ctx);
BOOL FrtCmd_ClearLinkSyncPackets(FrontierScriptContext *ctx);
BOOL FrtCmd_ExitClubLink(FrontierScriptContext *ctx);
BOOL FrtCmd_Random(FrontierScriptContext *ctx);
BOOL FrtCmd_HealParty(FrontierScriptContext *ctx);
BOOL FrtCmd_WaitButton(FrontierScriptContext *ctx);
BOOL FrtCmd_WaitButtonOrDelay(FrontierScriptContext *ctx);
BOOL FrtCmd_Dummy060(FrontierScriptContext *ctx);
BOOL FrtCmd_SetSaveVar(FrontierScriptContext *ctx);
BOOL FrtCmd_GetSaveVar(FrontierScriptContext *ctx);
BOOL FrtCmd_BattleTransition(FrontierScriptContext *ctx);
BOOL FrtCmd_GetPlayerSpriteId(FrontierScriptContext *ctx);
BOOL FrtCmd_CreateParticleSystem(FrontierScriptContext *ctx);
BOOL FrtCmd_DestroyParticleSystem(FrontierScriptContext *ctx);
BOOL FrtCmd_CreateParticleEmitter(FrontierScriptContext *ctx);
BOOL FrtCmd_WaitParticles(FrontierScriptContext *ctx);
BOOL FrtCmd_CreatePartyMonSprite(FrontierScriptContext *ctx);
BOOL FrtCmd_DeleteMonSprite(FrontierScriptContext *ctx);
BOOL FrtCmd_BrainIntro(FrontierScriptContext *ctx);
BOOL FrtCmd_IncGameStat(FrontierScriptContext *ctx);
BOOL FrtCmd_AddGameStat(FrontierScriptContext *ctx);
BOOL FrtCmd_AddGameScore(FrontierScriptContext *ctx);
BOOL FrtCmd_CheckPlayerOrPartnerVersion0(FrontierScriptContext *ctx);
BOOL FrtCmd_ShakeMap(FrontierScriptContext *ctx);
BOOL FrtCmd_WaitShakeMap(FrontierScriptContext *ctx);
BOOL FrtCmd_SetWindowDelayed(FrontierScriptContext *ctx);
BOOL FrtCmd_WaitWindowDelayed(FrontierScriptContext *ctx);
BOOL FrtCmd_CastleShowPoints(FrontierScriptContext *ctx);
BOOL FrtCmd_CastleHidePoints(FrontierScriptContext *ctx);
BOOL FrtCmd_CastlePrintPoints(FrontierScriptContext *ctx);
BOOL FrtCmd_ClearObjectMovement(FrontierScriptContext *ctx);
BOOL FrtCmd_SetObjectLookAround(FrontierScriptContext *ctx);
BOOL FrtCmd_PlaySE(FrontierScriptContext *ctx);
BOOL FrtCmd_StopSE(FrontierScriptContext *ctx);
BOOL FrtCmd_WaitSE(FrontierScriptContext *ctx);
BOOL FrtCmd_PlayFanfare(FrontierScriptContext *ctx);
BOOL FrtCmd_WaitFanfare(FrontierScriptContext *ctx);
BOOL FrtCmd_PlayBGM(FrontierScriptContext *ctx);
BOOL FrtCmd_StopBGM(FrontierScriptContext *ctx);
BOOL FrtCmd_FactoryAlloc(FrontierScriptContext *ctx);
BOOL FrtCmd_FactoryInit(FrontierScriptContext *ctx);
BOOL FrtCmd_FactoryFree(FrontierScriptContext *ctx);
BOOL FrtCmd_FactoryLaunchRentalApp(FrontierScriptContext *ctx);
BOOL FrtCmd_FactoryEndBattle(FrontierScriptContext *ctx);
BOOL FrtCmd_FactoryStartBattle(FrontierScriptContext *ctx);
BOOL FrtCmd_FactoryLaunchTradeApp(FrontierScriptContext *ctx);
BOOL FrtCmd_FactorySetRentalParty(FrontierScriptContext *ctx);
BOOL FrtCmd_FactoryGenerateOpponent(FrontierScriptContext *ctx);
BOOL FrtCmd_FactoryTradeMon(FrontierScriptContext *ctx);
BOOL FrtCmd_FactorySetOpponentParty(FrontierScriptContext *ctx);
BOOL FrtCmd_FactoryAction(FrontierScriptContext *ctx);
BOOL FrtCmd_FactoryGetBattleResult(FrontierScriptContext *ctx);
BOOL FrtCmd_FactorySendBuffer(FrontierScriptContext *ctx);
BOOL FrtCmd_FactoryReceiveBuffer(FrontierScriptContext *ctx);
BOOL FrtCmd_FactoryPrintMsg(FrontierScriptContext *ctx);
BOOL FrtCmd_CreateBattleRecord(FrontierScriptContext *ctx);
BOOL FrtCmd_SaveBattleRecord(FrontierScriptContext *ctx);
BOOL FrtCmd_PlayBattleRecord(FrontierScriptContext *ctx);
BOOL FrtCmd_DeleteBattleRecord(FrontierScriptContext *ctx);
BOOL FrtCmd_CheckBattleRecordSlotValid(FrontierScriptContext *ctx);
BOOL FrtCmd_CheckBattleRecordAiFlagsValid(FrontierScriptContext *ctx);
BOOL FrtCmd_SaveGameNormal(FrontierScriptContext *ctx);
BOOL FrtCmd_SaveGameAsync(FrontierScriptContext *ctx);
BOOL FrtCmd_SaveWipeExtraChunks(FrontierScriptContext *ctx);
BOOL FrtCmd_CheckExtraSaveChunksExist(FrontierScriptContext *ctx);
BOOL FrtCmd_HallUpdateSpeciesRecord(FrontierScriptContext *ctx);
BOOL FrtCmd_AddWaitingIcon(FrontierScriptContext *ctx);
BOOL FrtCmd_RemoveWaitingIcon(FrontierScriptContext *ctx);
BOOL FrtCmd_BufferItemName(FrontierScriptContext *ctx);
BOOL FrtCmd_BufferInt(FrontierScriptContext *ctx);
BOOL FrtCmd_BufferPlayersName(FrontierScriptContext *ctx);
BOOL FrtCmd_BufferPartnersName(FrontierScriptContext *ctx);
BOOL FrtCmd_BufferMoveName(FrontierScriptContext *ctx);
BOOL FrtCmd_BufferSpeciesName(FrontierScriptContext *ctx);
BOOL FrtCmd_BufferTypeName(FrontierScriptContext *ctx);
BOOL FrtCmd_BufferRivalsName(FrontierScriptContext *ctx);
BOOL FrtCmd_GetBattlePoints(FrontierScriptContext *ctx);
BOOL FrtCmd_GiveBattlePoints(FrontierScriptContext *ctx);
BOOL FrtCmd_TakeBattlePoints(FrontierScriptContext *ctx);
BOOL FrtCmd_TowerAction(FrontierScriptContext *ctx);
BOOL FrtCmd_TowerPrintTrainerMessage(FrontierScriptContext *ctx);
BOOL FrtCmd_TowerFreeData(FrontierScriptContext *ctx);
BOOL FrtCmd_TowerStartBattle(FrontierScriptContext *ctx);
BOOL FrtCmd_TowerEndBattle(FrontierScriptContext *ctx);
BOOL FrtCmd_TowerSendLinkMessage(FrontierScriptContext *ctx);
BOOL FrtCmd_TowerWaitLink(FrontierScriptContext *ctx);
BOOL FrtCmd_HallAlloc(FrontierScriptContext *ctx);
BOOL FrtCmd_HallInit(FrontierScriptContext *ctx);
BOOL FrtCmd_HallFree(FrontierScriptContext *ctx);
BOOL FrtCmd_HallLaunchApp(FrontierScriptContext *ctx);
BOOL FrtCmd_HallEndBattle(FrontierScriptContext *ctx);
BOOL FrtCmd_HallStartBattle(FrontierScriptContext *ctx);
BOOL FrtCmd_HallAction(FrontierScriptContext *ctx);
BOOL FrtCmd_HallGetBattleResult(FrontierScriptContext *ctx);
BOOL FrtCmd_HallSendLinkMessage(FrontierScriptContext *ctx);
BOOL FrtCmd_HallWaitLink(FrontierScriptContext *ctx);
BOOL FrtCmd_HallPrintTrainerMessage(FrontierScriptContext *ctx);
BOOL FrtCmd_Dummy150(FrontierScriptContext *ctx);
BOOL FrtCmd_CastleAlloc(FrontierScriptContext *ctx);
BOOL FrtCmd_CastleInit(FrontierScriptContext *ctx);
BOOL FrtCmd_CastleFree(FrontierScriptContext *ctx);
BOOL FrtCmd_CastleLaunchPartyApp(FrontierScriptContext *ctx);
BOOL FrtCmd_CastleEndBattle(FrontierScriptContext *ctx);
BOOL FrtCmd_CastleStartBattle(FrontierScriptContext *ctx);
BOOL FrtCmd_CastleLaunchOpponentApp(FrontierScriptContext *ctx);
BOOL FrtCmd_CastleSetPartyBeforeBattle(FrontierScriptContext *ctx);
BOOL FrtCmd_CastleGenerateOpponents(FrontierScriptContext *ctx);
BOOL FrtCmd_CastleAction(FrontierScriptContext *ctx);
BOOL FrtCmd_CastleGetBattleResult(FrontierScriptContext *ctx);
BOOL FrtCmd_CastleSendLinkMessage(FrontierScriptContext *ctx);
BOOL FrtCmd_CastleWaitLink(FrontierScriptContext *ctx);
BOOL FrtCmd_CastleLaunchEmptyApp(FrontierScriptContext *ctx);
BOOL FrtCmd_CastleWaitLinkChoice(FrontierScriptContext *ctx);
BOOL FrtCmd_CastleGetPoints(FrontierScriptContext *ctx);
BOOL FrtCmd_CastleSpendPoints(FrontierScriptContext *ctx);
BOOL FrtCmd_CastleAddPoints(FrontierScriptContext *ctx);
BOOL FrtCmd_CastlePrintTrainerMessage(FrontierScriptContext *ctx);
BOOL FrtCmd_LinkLobbyAlloc(FrontierScriptContext *ctx);
BOOL FrtCmd_LinkLobbyFree(FrontierScriptContext *ctx);
BOOL FrtCmd_LinkLobbySend(FrontierScriptContext *ctx);
BOOL FrtCmd_LinkLobbyWaitReceive(FrontierScriptContext *ctx);
BOOL FrtCmd_LinkLobbyCheckPartnerFacility(FrontierScriptContext *ctx);
BOOL FrtCmd_LinkLobbyRunPartyApp(FrontierScriptContext *ctx);
BOOL FrtCmd_LinkLobbyGetPartyMenuResult(FrontierScriptContext *ctx);
BOOL FrtCmd_LinkLobbyGetPartnerChoice(FrontierScriptContext *ctx);
BOOL FrtCmd_LinkLobbyAction(FrontierScriptContext *ctx);
BOOL FrtCmd_LinkLobbyStartTower(FrontierScriptContext *ctx);
BOOL FrtCmd_TowerLinkSendTrainers(FrontierScriptContext *ctx);
BOOL FrtCmd_TowerLinkWaitReceive(FrontierScriptContext *ctx);
BOOL FrtCmd_TowerFree(FrontierScriptContext *ctx);
BOOL FrtCmd_UpdateWifiFriendDate(FrontierScriptContext *ctx);
BOOL FrtCmd_ArcadeAlloc(FrontierScriptContext *ctx);
BOOL FrtCmd_ArcadeInit(FrontierScriptContext *ctx);
BOOL FrtCmd_ArcadeFree(FrontierScriptContext *ctx);
BOOL FrtCmd_ArcadeEndBattle(FrontierScriptContext *ctx);
BOOL FrtCmd_ArcadeStartBattle(FrontierScriptContext *ctx);
BOOL FrtCmd_ArcadeSetPartyBeforeBattle(FrontierScriptContext *ctx);
BOOL FrtCmd_ArcadeSetPartyAfterBattle(FrontierScriptContext *ctx);
BOOL FrtCmd_ArcadeAction(FrontierScriptContext *ctx);
BOOL FrtCmd_ArcadeGetBattleResult(FrontierScriptContext *ctx);
BOOL FrtCmd_ArcadeSendBuffer(FrontierScriptContext *ctx);
BOOL FrtCmd_ArcadeReceiveBuffer(FrontierScriptContext *ctx);
BOOL FrtCmd_LaunchGameBoard(FrontierScriptContext *ctx);
BOOL FrtCmd_ArcadePrintMsg(FrontierScriptContext *ctx);
BOOL FrtCmd_ArcadeSetEvent(FrontierScriptContext *ctx);
BOOL FrtCmd_ArcadeSetItemIconVisible(FrontierScriptContext *ctx);
BOOL FrtCmd_Dummy199(FrontierScriptContext *ctx);
BOOL FrtCmd_200(FrontierScriptContext *ctx);
BOOL FrtCmd_201(FrontierScriptContext *ctx);
BOOL FrtCmd_LinkLobbyWaitReceiveOrAbort(FrontierScriptContext *ctx);
BOOL FrtCmd_LinkSyncOrAbort(FrontierScriptContext *ctx);
BOOL FrtCmd_GetSyncAborted(FrontierScriptContext *ctx);
BOOL FrtCmd_ClearSyncAborted(FrontierScriptContext *ctx);
BOOL FrtCmd_MenuSetAlignRight(FrontierScriptContext *ctx);
BOOL FrtCmd_MenuSetAlignBottom(FrontierScriptContext *ctx);

#endif
