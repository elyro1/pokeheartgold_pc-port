#include "battle_link_comm.h"

#include "battle_record.h"
#include "communication_command.h"
#include "communication_core.h"
#include "link_synchronization.h"

static void BattleLink_RecvData(int sender, int size, u8 *data, BattleSystem *battle);
static void BattleLink_RecvAiFlags(int sender, int unused, u32 *data, BattleLinkWork *work);
static void BattleLink_RecvProfile(int sender, int unused, void *data, BattleLinkWork *work);
static void BattleLink_RecvTrainer(int sender, int unused, void *data, BattleLinkWork *work);
static void BattleLink_RecvParty(int sender, int unused, void *data, BattleLinkWork *work);
static void BattleLink_RecvChatot(int sender, int unused, void *data, BattleLinkWork *work);
static void BattleLink_RecvPartnerTrainer(int sender, int unused, void *data, BattleLinkWork *work);
static void BattleLink_RecvPartnerParty(int sender, int unused, void *data, BattleLinkWork *work);
static void BattleLink_SendTask(SysTask *task, void *ptr);
static void BattleLink_RecvTask(SysTask *task, void *ptr);
static void BattleLink_RecvEnd(int sender, int unused, void *data, BattleSystem *battle);
static void BattleLink_BuildPalPad(PlayerProfile *profile, SavePalPad *friends, SavePalPad *out);
static void BattleLink_RecvPalPad(int sender, int unused, SavePalPad *data, BattleLinkWork *work);
static u32 BattleLink_GetPalPadSize(void);

enum BattleLinkTaskState {
    BATTLE_LINK_STATE_RUN,
    BATTLE_LINK_STATE_END = 255,
};

// Link command ids of gBattleLinkCommands, in table order after the communication core's own commands
enum BattleLinkCommandId {
    BATTLE_LINK_CMD_END = COMM_BASE_COMMAND_COUNT,
    BATTLE_LINK_CMD_DATA,
    BATTLE_LINK_CMD_AI_FLAGS,
    BATTLE_LINK_CMD_PROFILE,
    BATTLE_LINK_CMD_TRAINER,
    BATTLE_LINK_CMD_PARTY,
    BATTLE_LINK_CMD_CHATOT,
    BATTLE_LINK_CMD_PARTNER_TRAINER_1,
    BATTLE_LINK_CMD_PARTNER_TRAINER_3,
    BATTLE_LINK_CMD_PARTNER_PARTY_1,
    BATTLE_LINK_CMD_PARTNER_PARTY_3,
    BATTLE_LINK_CMD_PAL_PAD,
};

void BattleLink_Start(BattleSystem *battle) {
    BattleLinkTask *send, *recv;
    if (BattleSystem_GetBattleSpecial(battle) & BATTLE_SPECIAL_RECORDING) {
        return;
    }
    send = Heap_Alloc(HEAP_ID_BATTLE, sizeof(BattleLinkTask));
    recv = Heap_Alloc(HEAP_ID_BATTLE, sizeof(BattleLinkTask));
    Comm_RegisterCommandTable(gBattleLinkCommands, 12, battle);
    send->battle = battle;
    send->state = BATTLE_LINK_STATE_RUN;
    recv->battle = battle;
    recv->state = BATTLE_LINK_STATE_RUN;
    BattleSystem_SetLinkSendStatePtr(battle, &send->state);
    BattleSystem_SetLinkRecvStatePtr(battle, &recv->state);
    SysTask_CreateOnMainQueue(BattleLink_SendTask, send, 0);
    SysTask_CreateOnMainQueue(BattleLink_RecvTask, recv, 0);
}

void BattleLink_RegisterSetupCommands(BattleLinkWork *work) {
    Comm_RegisterCommandTable(gBattleLinkCommands, 12, work);
}

static u32 BattleLink_GetAiFlagsSize(void) {
    return sizeof(u32);
}

static u32 BattleLink_GetProfileSize(void) {
    return PlayerProfile_sizeof();
}

static u32 BattleLink_GetPartySize(void) {
    return PartyCore_sizeof();
}

static u32 BattleLink_GetChatotSize(void) {
    return 1000;
}

static u32 BattleLink_GetTrainerSize(void) {
    return sizeof(Trainer);
}

static void *BattleLink_GetProfileRecvBuffer(int sender, BattleLinkWork *work) {
    BattleSetup *setup = work->setup;
    if (setup->battleType & BATTLE_TYPE_FRONTIER) {
        return setup->profile[sender * 2];
    }
    return setup->profile[sender];
}

static void *BattleLink_GetTrainerRecvBuffer(int sender, BattleLinkWork *work) {
    BattleSetup *setup = work->setup;
    if (setup->battleType & BATTLE_TYPE_FRONTIER) {
        return &setup->trainer[sender * 2];
    }
    return &setup->trainer[sender];
}

static void *BattleLink_GetPartyRecvBuffer(int sender, BattleLinkWork *work) {
    BattleSetup *setup = work->setup;
    if (setup->battleType & BATTLE_TYPE_FRONTIER) {
        return setup->party[sender * 2];
    }
    return setup->party[sender];
}

static void *BattleLink_GetChatotRecvBuffer(int sender, BattleLinkWork *work) {
    BattleSetup *setup = work->setup;
    if (setup->battleType & BATTLE_TYPE_FRONTIER) {
        return setup->chatot[sender * 2];
    }
    return setup->chatot[sender];
}

static void *BattleLink_GetTrainer1RecvBuffer(int sender, BattleLinkWork *work) {
    return &work->setup->trainer[1];
}

static void *BattleLink_GetTrainer3RecvBuffer(int sender, BattleLinkWork *work) {
    return &work->setup->trainer[3];
}

static void *BattleLink_GetParty1RecvBuffer(int sender, BattleLinkWork *work) {
    return work->setup->party[1];
}

static void *BattleLink_GetParty3RecvBuffer(int sender, BattleLinkWork *work) {
    return work->setup->party[3];
}

static void *BattleLink_GetPalPadRecvBuffer(int sender, BattleLinkWork *work) {
    return work->friends[sender];
}

const BattleLinkCommand gBattleLinkCommands[] = {
    { BattleLink_RecvEnd,            CommCmd_GetSizeVariable,   NULL                             },
    { BattleLink_RecvData,           CommCmd_GetSizeVariable,   NULL                             },
    { BattleLink_RecvAiFlags,        BattleLink_GetAiFlagsSize, NULL                             },
    { BattleLink_RecvProfile,        BattleLink_GetProfileSize, BattleLink_GetProfileRecvBuffer  },
    { BattleLink_RecvTrainer,        BattleLink_GetTrainerSize, BattleLink_GetTrainerRecvBuffer  },
    { BattleLink_RecvParty,          BattleLink_GetPartySize,   BattleLink_GetPartyRecvBuffer    },
    { BattleLink_RecvChatot,         BattleLink_GetChatotSize,  BattleLink_GetChatotRecvBuffer   },
    { BattleLink_RecvPartnerTrainer, BattleLink_GetTrainerSize, BattleLink_GetTrainer1RecvBuffer },
    { BattleLink_RecvPartnerTrainer, BattleLink_GetTrainerSize, BattleLink_GetTrainer3RecvBuffer },
    { BattleLink_RecvPartnerParty,   BattleLink_GetPartySize,   BattleLink_GetParty1RecvBuffer   },
    { BattleLink_RecvPartnerParty,   BattleLink_GetPartySize,   BattleLink_GetParty3RecvBuffer   },
    { BattleLink_RecvPalPad,         BattleLink_GetPalPadSize,  BattleLink_GetPalPadRecvBuffer   },
};
#if defined(SDK_PORT)
_Static_assert(NELEMS(gBattleLinkCommands) == 12, "Battle command IDs must retain their order");
#endif

void BattleLink_QueueSendData(BattleSystem *battle, int command, int battler, const u8 *data, u8 size) {
    u8 *header;
    u8 *buffer;
    u16 *write;
    u16 *end;
    int i;

    header = Heap_Alloc(HEAP_ID_BATTLE, sizeof(BattleLinkHeader));
    buffer = BattleSystem_GetSendBufferPtr(battle);
    write = BattleSystem_GetSendBufferWritePos(battle);
    end = BattleSystem_GetSendBufferEndPos(battle);
    if ((u32)(*write + 5 + size) > 0x1000) {
        *end = *write;
        *write = 0;
    }
    header[0] = command;
    header[1] = battler;
    *(u16 *)&header[2] = size;
    for (i = 0; i < sizeof(BattleLinkHeader); i++) {
        buffer[*write] = header[i];
        (*write)++;
    }
    for (i = 0; i < size; i++) {
        buffer[*write] = data[i];
        (*write)++;
    }
    Heap_Free(header);
}

static void BattleLink_RecvData(int sender, int size, u8 *data, BattleSystem *battle) {
    int i;
    u8 *buffer = BattleSystem_GetRecvBufferPtr(battle);
    u16 *write = BattleSystem_GetRecvBufferWritePos(battle);
    u16 *end = BattleSystem_GetRecvBufferEndPos(battle);
    if (*write + size + 1 > 4096) {
        *end = *write;
        *write = 0;
    }
    for (i = 0; i < size; i++) {
        buffer[*write] = data[i];
        (*write)++;
    }
}

BOOL BattleLink_SendAiFlags(BattleLinkWork *work, u32 value, ...) {
    if (Comm_GetSendBufferFreeSpace() != 264) {
        return FALSE;
    }
    if (LinkSync_IsSynced(BATTLE_LINK_SYNC_AI_FLAGS) == FALSE) {
        return FALSE;
    }
    return Comm_SendCommand(BATTLE_LINK_CMD_AI_FLAGS, &value, sizeof(value));
}

static void BattleLink_RecvAiFlags(int sender, int unused, u32 *data, BattleLinkWork *work) {
    work->setup->aiFlags[sender] = *data;
    BattleRecord_SetAiFlags(sender, work->setup->aiFlags[sender]);
    work->received++;
}

BOOL BattleLink_PrepareProfile(BattleLinkWork *work) {
    if (Comm_GetSendBufferFreeSpace() != 264) {
        return FALSE;
    }
    PlayerProfile_Copy(work->setup->profile[0], (PlayerProfile *)&work->buffer);
    return TRUE;
}

BOOL BattleLink_SendProfile(BattleLinkWork *work) {
    if (Comm_GetSendBufferFreeSpace() != 264) {
        return FALSE;
    }
    if (LinkSync_IsSynced(BATTLE_LINK_SYNC_PROFILE) == FALSE) {
        return FALSE;
    }
    return Comm_SendCommandNoCopy(BATTLE_LINK_CMD_PROFILE, &work->buffer, PlayerProfile_sizeof());
}

static void BattleLink_RecvProfile(int sender, int unused, void *data, BattleLinkWork *work) {
    work->received++;
}

BOOL BattleLink_PrepareTrainer(BattleLinkWork *work) {
    if (Comm_GetSendBufferFreeSpace() != 264) {
        return FALSE;
    }
    work->buffer.trainer = work->setup->trainer[0];
    return TRUE;
}

BOOL BattleLink_SendTrainer(BattleLinkWork *work) {
    if (Comm_GetSendBufferFreeSpace() != 264) {
        return FALSE;
    }
    if (LinkSync_IsSynced(BATTLE_LINK_SYNC_TRAINER) == FALSE) {
        return FALSE;
    }
    return Comm_SendCommandNoCopy(BATTLE_LINK_CMD_TRAINER, &work->buffer, sizeof(Trainer));
}

static void BattleLink_RecvTrainer(int sender, int unused, void *data, BattleLinkWork *work) {
    work->received++;
}

BOOL BattleLink_PrepareParty(BattleLinkWork *work) {
    if (Comm_GetSendBufferFreeSpace() != 264) {
        return FALSE;
    }
    Party_Copy(work->setup->party[0], (Party *)&work->buffer);
    return TRUE;
}

BOOL BattleLink_SendParty(BattleLinkWork *work) {
    if (Comm_GetSendBufferFreeSpace() != 264) {
        return FALSE;
    }
    if (LinkSync_IsSynced(BATTLE_LINK_SYNC_PARTY) == FALSE) {
        return FALSE;
    }
    return Comm_SendCommandNoCopy(BATTLE_LINK_CMD_PARTY, &work->buffer, PartyCore_sizeof());
}

static void BattleLink_RecvParty(int sender, int unused, void *data, BattleLinkWork *work) {
    work->received++;
}

BOOL BattleLink_PrepareChatot(BattleLinkWork *work) {
    if (Comm_GetSendBufferFreeSpace() != 264) {
        return FALSE;
    }
    Chatot_Copy(&work->buffer.chatot, work->setup->chatot[0]);
    return TRUE;
}

BOOL BattleLink_SendChatot(BattleLinkWork *work) {
    if (Comm_GetSendBufferFreeSpace() != 264) {
        return FALSE;
    }
    if (LinkSync_IsSynced(BATTLE_LINK_SYNC_CHATOT) == FALSE) {
        return FALSE;
    }
    return Comm_SendCommandNoCopy(BATTLE_LINK_CMD_CHATOT, &work->buffer, 1000);
}

BOOL BattleLink_PreparePalPad(BattleLinkWork *work) {
    BattleSetup *setup;
    int index;
    int i;
    if (Comm_GetSendBufferFreeSpace() != 264) {
        return FALSE;
    }
    setup = work->setup;
    if (setup->battleType & BATTLE_TYPE_FRONTIER) {
        index = Comm_GetLocalNetId() * 8;
    } else {
        index = Comm_GetLocalNetId() * 4;
    }
    BattleLink_BuildPalPad(*(PlayerProfile **)((u8 *)setup->profile + index), setup->palPad, (SavePalPad *)&work->buffer);
    for (i = 0; i < 4; i++) {
        work->friends[i] = Heap_Alloc(HEAP_ID_BATTLE, sizeof(SavePalPad));
    }
    return TRUE;
}

BOOL BattleLink_SendPalPad(BattleLinkWork *work) {
    if (Comm_GetSendBufferFreeSpace() != 264) {
        return FALSE;
    }
    if (LinkSync_IsSynced(BATTLE_LINK_SYNC_PAL_PAD) == FALSE) {
        return FALSE;
    }
    return Comm_SendCommandNoCopy(BATTLE_LINK_CMD_PAL_PAD, &work->buffer, 1000);
}

static void BattleLink_RecvChatot(int sender, int unused, void *data, BattleLinkWork *work) {
    work->received++;
}

BOOL BattleLink_PreparePartnerTrainer(BattleLinkWork *work, int index) {
    if (Comm_GetSendBufferFreeSpace() != 264) {
        return FALSE;
    }
    work->buffer.trainer = work->setup->trainer[index];
    return TRUE;
}

BOOL BattleLink_SendPartnerTrainer(BattleLinkWork *work, int index, int syncId) {
    if (Comm_GetSendBufferFreeSpace() != 264) {
        return FALSE;
    }
    if (!LinkSync_IsSynced(syncId)) {
        return FALSE;
    }
    if (index == 1) {
        return Comm_SendCommandNoCopy(BATTLE_LINK_CMD_PARTNER_TRAINER_1, &work->buffer, sizeof(Trainer));
    }
    return Comm_SendCommandNoCopy(BATTLE_LINK_CMD_PARTNER_TRAINER_3, &work->buffer, sizeof(Trainer));
}

static void BattleLink_RecvPartnerTrainer(int sender, int unused, void *data, BattleLinkWork *work) {
    work->received++;
}

BOOL BattleLink_PreparePartnerParty(BattleLinkWork *work, int index) {
    if (Comm_GetSendBufferFreeSpace() != 264) {
        return FALSE;
    }
    Party_Copy(work->setup->party[index], (Party *)&work->buffer);
    return TRUE;
}

BOOL BattleLink_SendPartnerParty(BattleLinkWork *work, int index, int syncId) {
    if (Comm_GetSendBufferFreeSpace() != 264) {
        return FALSE;
    }
    if (!LinkSync_IsSynced(syncId)) {
        return FALSE;
    }
    if (index == 1) {
        return Comm_SendCommandNoCopy(BATTLE_LINK_CMD_PARTNER_PARTY_1, &work->buffer, PartyCore_sizeof());
    }
    return Comm_SendCommandNoCopy(BATTLE_LINK_CMD_PARTNER_PARTY_3, &work->buffer, PartyCore_sizeof());
}

static void BattleLink_RecvPartnerParty(int sender, int unused, void *data, BattleLinkWork *work) {
    work->received++;
}

static void BattleLink_SendTask(SysTask *task, void *ptr) {
    BattleLinkTask *work = ptr;
    u8 *buffer = BattleSystem_GetSendBufferPtr(work->battle);
    u16 *read = BattleSystem_GetSendBufferReadPos(work->battle);
    u16 *write = BattleSystem_GetSendBufferWritePos(work->battle);
    u16 *end = BattleSystem_GetSendBufferEndPos(work->battle);
    switch (work->state) {
    case BATTLE_LINK_STATE_RUN: {
        int size;
        if (Comm_GetSendBufferFreeSpace() != 264) {
            break;
        }
        if (*read == *write) {
            break;
        }
        if (*read == *end) {
            *read = 0;
            *end = 0;
        }
        size = (buffer[*read + 2] | (buffer[*read + 3] << 8)) + sizeof(BattleLinkHeader);
        if (Comm_SendCommand(BATTLE_LINK_CMD_DATA, &buffer[*read], size) == TRUE) {
            *read += size;
        }
        break;
    }
    case BATTLE_LINK_STATE_END:
    default:
        Heap_Free(work);
        SysTask_Destroy(task);
        break;
    }
}

static void BattleLink_RecvTask(SysTask *task, void *ptr) {
    BattleLinkTask *work = ptr;
    u8 *buffer = BattleSystem_GetRecvBufferPtr(work->battle);
    u16 *read = BattleSystem_GetRecvBufferReadPos(work->battle);
    u16 *write = BattleSystem_GetRecvBufferWritePos(work->battle);
    u16 *end = BattleSystem_GetRecvBufferEndPos(work->battle);
    switch (work->state) {
    case BATTLE_LINK_STATE_RUN:
        if (*read == *write) {
            break;
        }
        if (*read == *end) {
            *read = 0;
            *end = 0;
        }
        if (BattleController_RecvLinkData(work->battle, &buffer[*read]) == TRUE) {
            *read += (buffer[*read + 2] | (buffer[*read + 3] << 8)) + sizeof(BattleLinkHeader);
        }
        break;
    case BATTLE_LINK_STATE_END:
    default:
        Heap_Free(work);
        SysTask_Destroy(task);
        break;
    }
}

static void BattleLink_RecvEnd(int sender, int unused, void *data, BattleSystem *battle) {
    BattleSystem_SetLinkSendState(battle, BATTLE_LINK_STATE_END);
    BattleSystem_SetLinkRecvState(battle, BATTLE_LINK_STATE_END);
    BattleSystem_SetBattleFinished(battle, 1);
}

static void BattleLink_BuildPalPad(PlayerProfile *profile, SavePalPad *friends, SavePalPad *out) {
    int i;
    CopyU16StringArray(out->name, PlayerProfile_GetNamePtr(profile));
    out->otid = PlayerProfile_GetTrainerID(profile);
    out->language = PlayerProfile_GetLanguage(profile);
    out->version = PlayerProfile_GetVersion(profile);
    out->gender = PlayerProfile_GetTrainerGender(profile);
    for (i = 0; i < 16; i++) {
        out->friendOtIds[i] = friends[i].otid;
        out->friendVersions[i] = friends[i].version;
        out->friendLanguages[i] = friends[i].language;
        out->friendGenders[i] = friends[i].gender;
    }
}

static void BattleLink_RecvPalPad(int sender, int unused, SavePalPad *data, BattleLinkWork *work) {
    if (sender != Comm_GetLocalNetId()) {
        SavePalPad_Merge(work->setup->palPad, data, 1, HEAP_ID_BATTLE);
    }
    work->received++;
}

static u32 BattleLink_GetPalPadSize(void) {
    return sizeof(SavePalPad);
}
