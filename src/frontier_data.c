#include "frontier_data.h"

#include "global.h"

#include "msgdata/msg.naix"
#include "msgdata/msg/msg_0028.h"

#include "frontier_data_access.h"
#include "frontier_save_access.h"
#include "heap.h"
#include "mail_message.h"
#include "msgdata.h"

// The Battle Tower challenge in progress (fields addressed by enum BattleTowerChallengeField).
struct BattleTowerChallenge {
    u8 allyReplaceItem : 1;
    u8 saved : 1; // BattleTowerChallenge_GetSaved / SetSaved
    u8 towerMode : 3;
    u8 multiAllyId : 3;
    u8 unk1;
    u8 battleNumber;
    u8 totalTimesFainted; // 20 points each off the Wi-Fi score
    u16 totalTurns;       // 10 points each off the Wi-Fi score
    u16 totalDamage;      // the last part of the Wi-Fi score
    u8 partySlots[4];
    u8 trainers[28]; // 14 trainer ids
    u32 randomState;
    u8 multiAllyData[16];
};

struct FrontierData {
    u16 battlePoints;
    u8 wifiStreak;
    u8 wifiRank; // 1..10
    u32 towerSeed;

    union {
        u16 flags;

        struct {
            u16 pad : 4, wifiStreakActive : 1, pad2 : 11;
        } bits;
    } state;

    u16 towerRounds[6]; // per tower mode
    u16 wifiScore;
    FrontierMon wifiTeams[2][3];
};

typedef struct BattleRoomDownload {
    u32 markDate;      // year, month, day, hour of the last mark, one byte each
    u8 roomMarks[250]; // one bit per (rank 1..10, room 1..200)
    u8 hasTrainers, hasLeaders;
    u8 trainersRoom, trainersRank, leadersRoom, leadersRank;
    BattleRoomTrainer trainers[7];
    BattleRoomLeader leaders[30];
} BattleRoomDownload;

typedef struct BattleTowerOpponent {
    u32 id;
    u16 trainerClass;
    u8 pad6[2];
    u16 name[8];
    u8 messages[24];
    u8 team[0xA8];
} BattleTowerOpponent;

static void BattleRoomDownload_ClearRoomMarks(BattleRoomDownload *download);
static BOOL BattleRoomDownload_IsDateNewer(const RTCDate *date, const RTCDate *stored);

static const MailMessageTemplate sTowerMessageTemplates[4] = {
    { 0, 0, { 4, -1 }, { 7, 0 }  },
    { 1, 0, { 4, -1 }, { 33, 0 } },
    { 2, 0, { 8, -1 }, { 10, 0 } },
    { 1, 4, { 4, -1 }, { 1, 0 }  },
};

u32 BattleRoomTrainer_sizeof(void) {
    return sizeof(BattleRoomTrainer);
}

void BattleTowerChallenge_Clear(BattleTowerChallenge *challenge) {
    MI_CpuFill8(challenge, 0, sizeof(BattleTowerChallenge));
}

void FrontierData_Reset(void *data) {
    FrontierData *frontierData = data;

    MI_CpuFill8(frontierData, 0, sizeof(FrontierData));
    frontierData->wifiRank = 1;
}

void TowerMessages_Init(void *data) {
    MailMessage *messages = data;
    int i;
    for (i = 0; i < 4; i++) {
        MailMsg_Init_FromTemplate(&messages[i], &sTowerMessageTemplates[i]);
    }
}

void BattleRoomDownload_Init(void *data) {
    MI_CpuFill8(data, 0, sizeof(BattleRoomDownload));
}

u32 BattleTowerChallenge_GetField(BattleTowerChallenge *tower, enum BattleTowerChallengeField field, void *out) {
    switch (field) {
    case TOWER_CHALLENGE_MODE:
        return tower->towerMode;
    case TOWER_CHALLENGE_BATTLE_NO:
        return tower->battleNumber;
    case TOWER_CHALLENGE_TIMES_FAINTED:
        return tower->totalTimesFainted;
    case TOWER_CHALLENGE_TURNS:
        return tower->totalTurns;
    case TOWER_CHALLENGE_DAMAGE:
        return tower->totalDamage;
    case TOWER_CHALLENGE_PARTY_SLOTS:
        MI_CpuCopy8(tower->partySlots, out, sizeof(tower->partySlots));
        return 0;
    case TOWER_CHALLENGE_ALLY_DATA:
        MI_CpuCopy8(tower->multiAllyData, out, sizeof(tower->multiAllyData));
        return 0;
    case TOWER_CHALLENGE_ALLY_REPLACE_ITEM:
        return tower->allyReplaceItem;
    case TOWER_CHALLENGE_TRAINERS:
        MI_CpuCopy8(tower->trainers, out, sizeof(tower->trainers));
        return 0;
    case TOWER_CHALLENGE_ALLY_ID:
        return tower->multiAllyId;
    case TOWER_CHALLENGE_RANDOM_STATE:
        return tower->randomState;
    }
    return 0;
}

void BattleTowerChallenge_SetField(BattleTowerChallenge *tower, enum BattleTowerChallengeField field, void *value) {
    switch (field) {
    case TOWER_CHALLENGE_MODE:
        tower->towerMode = *(u8 *)value;
        break;
    case TOWER_CHALLENGE_BATTLE_NO:
        tower->battleNumber = *(u8 *)value;
        break;
    case TOWER_CHALLENGE_TIMES_FAINTED:
        tower->totalTimesFainted = *(u8 *)value;
        break;
    case TOWER_CHALLENGE_TURNS:
        tower->totalTurns = *(u16 *)value;
        break;
    case TOWER_CHALLENGE_DAMAGE:
        tower->totalDamage = *(u16 *)value;
        break;
    case TOWER_CHALLENGE_PARTY_SLOTS:
        MI_CpuCopy8(value, tower->partySlots, sizeof(tower->partySlots));
        break;
    case TOWER_CHALLENGE_ALLY_DATA:
        MI_CpuCopy8(value, tower->multiAllyData, sizeof(tower->multiAllyData));
        break;
    case TOWER_CHALLENGE_ALLY_REPLACE_ITEM:
        tower->allyReplaceItem = *(u8 *)value;
        break;
    case TOWER_CHALLENGE_TRAINERS:
        MI_CpuCopy8(value, tower->trainers, sizeof(tower->trainers));
        break;
    case TOWER_CHALLENGE_RANDOM_STATE:
        tower->randomState = *(u32 *)value;
        break;
    case TOWER_CHALLENGE_ALLY_ID:
        tower->multiAllyId = *(u8 *)value;
        break;
    }
}

// Adds to the three Wi-Fi score counters, each capped below its type's maximum.
void BattleTowerChallenge_AddWifiScoreCounters(BattleTowerChallenge *ch, u8 timesFainted, u32 turns, u32 damage) {
    int totalTimesFainted = ch->totalTimesFainted + timesFainted;
    int totalTurns;
    int totalDamage;
    if (totalTimesFainted < 255) {
        ch->totalTimesFainted = totalTimesFainted;
    }
    totalTurns = ch->totalTurns + turns;
    if (totalTurns < 65535) {
        ch->totalTurns = totalTurns;
    }
    totalDamage = ch->totalDamage + damage;
    if (totalDamage < 65535) {
        ch->totalDamage = totalDamage;
    }
}

u32 BattleTowerChallenge_GetSaved(BattleTowerChallenge *challenge) {
    return challenge->saved;
}

void BattleTowerChallenge_SetSaved(BattleTowerChallenge *challenge, u32 value) {
    challenge->saved = value;
}

// Capped at 9999. Returns the new amount.
u32 FrontierData_BattlePointAction(FrontierData *data, u32 param, enum FrontierDataAction action) {
    switch (action) {
    case FRONTIER_DATA_SET:
        if (param > 9999) {
            data->battlePoints = 9999;
        } else {
            data->battlePoints = param;
        }
        break;
    case FRONTIER_DATA_ADD: {
        int points = data->battlePoints + param;
        if (points > 9999) {
            data->battlePoints = 9999;
        } else {
            data->battlePoints = points;
        }
        break;
    }
    case FRONTIER_DATA_SUB:
        if (data->battlePoints < param) {
            data->battlePoints = 0;
        } else {
            data->battlePoints -= param;
        }
        break;
    case FRONTIER_DATA_GET:
    case FRONTIER_DATA_RESET:
    case FRONTIER_DATA_INC:
    case FRONTIER_DATA_DEC:
        break;
    }
    return data->battlePoints;
}

// Counting starts at 1 when the streak flag is not set. Returns the count.
u32 FrontierData_WifiStreakAction(FrontierData *data, enum FrontierDataAction action) {
    switch (action) {
    case FRONTIER_DATA_RESET:
        data->wifiStreak = 0;
        data->state.flags &= ~16;
        break;
    case FRONTIER_DATA_INC:
        if (data->state.bits.wifiStreakActive) {
            data->wifiStreak++;
        } else {
            data->wifiStreak = 1;
            data->state.flags |= 16;
        }
        break;
    }
    return data->wifiStreak;
}

// The rank resets to 1 and stays within 1-10. Returns the rank.
u32 FrontierData_WifiRankAction(FrontierData *data, enum FrontierDataAction action) {
    switch (action) {
    case FRONTIER_DATA_RESET:
        data->wifiRank = 1;
        break;
    case FRONTIER_DATA_INC:
        if (data->wifiRank < 10) {
            data->wifiRank++;
        }
        break;
    case FRONTIER_DATA_DEC:
        if (data->wifiRank > 1) {
            data->wifiRank--;
        }
        break;
    }
    return data->wifiRank;
}

void FrontierData_SetWifiTeam(FrontierData *data, u32 index, const FrontierMon *src) {
    if (index == 0) {
        MI_CpuCopy8(src, data->wifiTeams[1], sizeof(data->wifiTeams[1]));
    } else {
        MI_CpuCopy8(src, data->wifiTeams[0], sizeof(data->wifiTeams[0]));
    }
}

void FrontierData_GetWifiTeam(FrontierData *data, s32 index, FrontierWifiUploadRecord *dst) {
    if (index == 0) {
        MI_CpuCopy8(data->wifiTeams[1], dst, sizeof(data->wifiTeams[1]));
    } else {
        MI_CpuCopy8(data->wifiTeams[0], dst, sizeof(data->wifiTeams[0]));
    }
}

u16 FrontierData_CalcWifiScore(FrontierData *data, BattleTowerChallenge *tower) {
    u16 base = 1000 * (tower->battleNumber - 1);
    u16 turnsPenalty = 10 * tower->totalTurns;
    u16 faintPenalty = 20 * tower->totalTimesFainted;
    u16 score;
    u16 time;
    if (turnsPenalty + faintPenalty > 950) {
        score = 0;
    } else {
        score = 950 - (turnsPenalty + faintPenalty);
    }
    if (tower->totalDamage > 970) {
        time = 0;
    } else {
        time = (1000 - tower->totalDamage) / 30;
    }
    u16 result = base + score + time;
    data->wifiScore = result;
    return result;
}

u16 FrontierData_GetWifiScore(FrontierData *data) {
    return data->wifiScore;
}

u8 FrontierData_GetWifiWinCount(FrontierData *data) {
    return data->wifiScore / 1000;
}

// Counts up to 65534. The Wi-Fi practice mode has no counter, mode 6 uses its slot.
u32 FrontierData_TowerRoundAction(FrontierData *data, u32 towerMode, enum FrontierDataAction action) {
    if (towerMode == TOWER_MODE_WIFI_PRACTICE) {
        return 0;
    }
    if (towerMode == TOWER_MODE_6) {
        towerMode = TOWER_MODE_WIFI_PRACTICE;
    }
    switch (action) {
    case FRONTIER_DATA_RESET:
        data->towerRounds[towerMode] = 0;
        break;
    case FRONTIER_DATA_INC:
        if (data->towerRounds[towerMode] < 65534) {
            data->towerRounds[towerMode]++;
        }
        break;
    }
    return data->towerRounds[towerMode];
}

u32 FrontierData_SetTowerRound(FrontierData *data, u32 towerMode, u16 round) {
    if (towerMode == TOWER_MODE_WIFI_PRACTICE) {
        return 0;
    }
    if (towerMode == TOWER_MODE_6) {
        towerMode = TOWER_MODE_WIFI_PRACTICE;
    }
    data->towerRounds[towerMode] = round;
    return data->towerRounds[towerMode];
}

u32 FrontierData_FlagAction(FrontierData *data, u16 flag, enum FrontierDataAction action) {
    u16 mask = 1;
    u16 i;
    if (flag >= 16) {
        GF_ASSERT(FALSE);
        return 0;
    }
    for (i = 0; i < flag; i++) {
        mask <<= 1;
    }
    switch (action) {
    case FRONTIER_DATA_RESET:
        mask ^= 0xFFFF;
        data->state.flags = mask & data->state.flags;
        break;
    case FRONTIER_DATA_SET:
        data->state.flags |= mask;
        break;
    case FRONTIER_DATA_GET:
        return (data->state.flags >> flag) & 1;
    }
    return 0;
}

void FrontierData_SetTowerSeed(FrontierData *data, u32 value) {
    data->towerSeed = value;
}

u32 FrontierData_GetTowerSeed(FrontierData *data) {
    return data->towerSeed;
}

void Save_Frontier_SetTowerMessage(SaveData *save, int index, MailMessage *message) {
    MailMessage *messages = (MailMessage *)((u8 *)Save_Frontier_GetStatic(save) + 0xABC);
    MailMsg_Copy(&messages[index], message);
}

MailMessage *Save_Frontier_GetTowerMessage(SaveData *save, u32 index) {
    MailMessage *messages = (MailMessage *)((u8 *)Save_Frontier_GetStatic(save) + 0xABC);
    return &messages[index];
}

void BattleRoomDownload_MarkRoom(BattleRoomDownload *download, u32 rank, u32 room, const RTCDate *date) {
    u8 mask = 1;
    if (room != 0 && room <= 200 && rank != 0 && rank <= 10) {
        u16 index = (room - 1) + (rank - 1) * 200;
        u8 bit = index % 8;
        u8 byte = index / 8;
        mask <<= bit;
        download->roomMarks[byte] |= mask;
        download->markDate = (date->year << 24) | ((date->month << 24) >> 8) | ((date->day << 24) >> 16) | date->week;
    }
}

static void BattleRoomDownload_ClearRoomMarks(BattleRoomDownload *download) {
    MI_CpuFill8(download->roomMarks, 0, 250);
    MI_CpuFill8(&download->markDate, 0, 4);
}

// BUG: year, month and day are compared one by one, so a later month or day of an earlier year counts as newer.
static BOOL BattleRoomDownload_IsDateNewer(const RTCDate *date, const RTCDate *stored) {
    if (date->year > stored->year) {
        return TRUE;
    }
    if (date->month > stored->month) {
        return TRUE;
    }
    if (date->day > stored->day) {
        return TRUE;
    }
    return FALSE;
}

BOOL BattleRoomDownload_IsRoomMarked(BattleRoomDownload *download, u32 rank, u32 room, const RTCDate *date) {
    u8 mask = 1;
    RTCDate stored;
    u16 index;
    u8 bit;
    u8 byte;
    // BUG: unlike BattleRoomDownload_MarkRoom, rank 0 and room 0 are not rejected; the index then wraps and the
    // mark is read far past roomMarks.
    if (room > 200 || rank > 10) {
        return FALSE;
    }
    u32 markDate = download->markDate;
    stored.year = (u8)(markDate >> 24);
    stored.month = (u8)(markDate >> 16);
    stored.day = (u8)(markDate >> 8);
    stored.week = (RTCWeek)(u8)markDate;
    if (BattleRoomDownload_IsDateNewer(date, &stored)) {
        BattleRoomDownload_ClearRoomMarks(download);
        return FALSE;
    }
    index = (rank - 1) * 200 + (room - 1);
    bit = index % 8;
    byte = index / 8;
    mask <<= bit;
    if (download->roomMarks[byte] & mask) {
        return TRUE;
    }
    return FALSE;
}

#if defined(SDK_PORT)
u32 BattleRoomDownload_HasTrainers(void *download) {
#else
u32 BattleRoomDownload_HasTrainers(u32 download) {
#endif
    return ((BattleRoomDownload *)download)->hasTrainers;
}

#if defined(SDK_PORT)
u32 BattleRoomDownload_HasLeaders(void *download) {
#else
u32 BattleRoomDownload_HasLeaders(u32 download) {
#endif
    return ((BattleRoomDownload *)download)->hasLeaders;
}

void BattleRoomDownload_SetTrainers(BattleRoomDownload *download, const BattleRoomTrainer *trainers, u32 rank, u32 room) {
    MI_CpuCopy8(trainers, download->trainers, sizeof(download->trainers));
    download->trainersRank = rank;
    download->trainersRoom = room;
    download->hasTrainers = 1;
}

void BattleRoomDownload_GetTrainersRoom(BattleRoomDownload *download, u8 *out) {
    out[0] = download->trainersRank;
    out[1] = download->trainersRoom;
}

#if defined(SDK_PORT)
void BattleRoomDownload_GetTrainer(void *addr, FrontierTrainer *trainer, int index) {
#else
void BattleRoomDownload_GetTrainer(u32 addr, FrontierTrainer *trainer, int index) {
#endif
    BattleRoomDownload *download = (BattleRoomDownload *)addr;
    BattleTowerOpponent *out = (BattleTowerOpponent *)trainer;
    BattleRoomTrainer *roomTrainer = &download->trainers[index];
    u8 *team = out->team;
    out->id = 10000;
    out->trainerClass = roomTrainer->trainerClass;
    if (roomTrainer->useDefaultName) {
        MsgData *msg = NewMsgDataFromNarc(MSGDATA_LOAD_DIRECT, NARC_msgdata_msg, NARC_msg_msg_0028_bin, HEAP_ID_FIELD2);
        ReadMsgDataIntoU16Array(msg, roomTrainer->gender + msg_0028_00033, out->name);
        DestroyMsgData(msg);
    } else {
        MI_CpuCopy8(roomTrainer->name, out->name, 16);
    }
    MI_CpuCopy8(roomTrainer->messages, out->messages, 8);
    MI_CpuCopy8(roomTrainer->messages + 8, out->messages + 8, 8);
    MI_CpuCopy8(roomTrainer->messages + 16, out->messages + 16, 8);
    MI_CpuCopy8(roomTrainer->team, team, 0xA8);
}

void BattleRoomDownload_SetLeaders(BattleRoomDownload *download, const BattleRoomLeader *leaders, u32 rank, u32 room) {
    MI_CpuCopy8(leaders, download->leaders, sizeof(download->leaders));
    download->leadersRank = rank;
    download->leadersRoom = room;
    download->hasLeaders = 1;
}

#if defined(SDK_PORT)
void BattleRoomDownload_GetLeadersRoom(void *addr, u8 *out) {
#else
void BattleRoomDownload_GetLeadersRoom(u32 addr, u8 *out) {
#endif
    BattleRoomDownload *download = (BattleRoomDownload *)addr;
    out[0] = download->leadersRank;
    out[1] = download->leadersRoom;
}

#if defined(SDK_PORT)
BattleRoomLeader *BattleRoomDownload_CopyLeaders(void *addr, enum HeapID heapID) {
#else
BattleRoomLeader *BattleRoomDownload_CopyLeaders(u32 addr, enum HeapID heapID) {
#endif
    BattleRoomDownload *download = (BattleRoomDownload *)addr;
    BattleRoomLeader *out = Heap_Alloc(heapID, sizeof(download->leaders));
    MI_CpuCopy8(download->leaders, out, sizeof(download->leaders));
    return out;
}

BattleTowerChallenge *Save_BattleTowerChallenge_Get(SaveData *save) {
    return (BattleTowerChallenge *)((u8 *)Save_Frontier_GetStatic(save) + 0x8E0);
}

FrontierData *Save_FrontierData_Get(SaveData *save) {
    return (FrontierData *)((u8 *)Save_Frontier_GetStatic(save) + 0x954);
}

#if defined(SDK_PORT)
void *Save_BattleRoomDownload_Get(SaveData *save) {
    return (u8 *)Save_Frontier_GetStatic(save) + 0xADC;
#else
u32 Save_BattleRoomDownload_Get(SaveData *save) {
    return (u32)Save_Frontier_GetStatic(save) + 0xADC;
#endif
}
