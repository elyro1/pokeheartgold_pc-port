#ifndef POKEHEARTGOLD_FRONTIER_DATA_ACCESS_H
#define POKEHEARTGOLD_FRONTIER_DATA_ACCESS_H

#include "frontier_wifi_upload_record.h"
#include "scrcmd_9.h"

struct BattleRoomDownload;
typedef struct BattleTowerChallenge BattleTowerChallenge;

// One trainer of a downloaded Wi-Fi Battle Room.
typedef struct BattleRoomTrainer {
    u8 team[0xA8];
    u16 name[16];
    u8 useDefaultName : 1, gender : 1, pad : 6;
    u8 trainerClass;
    u8 messages[24];
    u16 score;
} BattleRoomTrainer;

// One leader of a downloaded Wi-Fi Battle Room.
typedef struct BattleRoomLeader {
    u16 name[PLAYER_NAME_LENGTH + 1];
    u8 padding_10[2];
    u8 country;
    u8 region;
    u8 padding_14[4];
    u8 greeting[8];
    u8 useDefaultName : 1;
    u8 gender : 1;
    u8 unk_20_2 : 6;
    u8 unk_21;
} BattleRoomLeader;

// The Wi-Fi download of one room: its trainers (BattleRoomDownload_SetTrainers) and leaders (_SetLeaders).
typedef struct BattleRoomDownloadData {
    BattleRoomTrainer trainers[7];
    BattleRoomLeader leaders[30];
} BattleRoomDownloadData;

enum BattleTowerChallengeField {
    TOWER_CHALLENGE_MODE,
    TOWER_CHALLENGE_BATTLE_NO,
    TOWER_CHALLENGE_TIMES_FAINTED,
    TOWER_CHALLENGE_TURNS,
    TOWER_CHALLENGE_DAMAGE,
    TOWER_CHALLENGE_PARTY_SLOTS,
    TOWER_CHALLENGE_ALLY_DATA,
    TOWER_CHALLENGE_ALLY_REPLACE_ITEM,
    TOWER_CHALLENGE_TRAINERS,
    TOWER_CHALLENGE_ALLY_ID,
    TOWER_CHALLENGE_RANDOM_STATE,
};

u32 BattleRoomTrainer_sizeof(void);
void BattleTowerChallenge_Clear(BattleTowerChallenge *challenge);
void FrontierData_Reset(void *data);
void TowerMessages_Init(void *data);
void BattleRoomDownload_Init(void *data);
u32 BattleTowerChallenge_GetField(BattleTowerChallenge *challenge, enum BattleTowerChallengeField field, void *out);
void BattleTowerChallenge_SetField(BattleTowerChallenge *challenge, enum BattleTowerChallengeField field, void *value);
void BattleTowerChallenge_AddWifiScoreCounters(BattleTowerChallenge *challenge, u8 timesFainted, u32 turns, u32 damage);
u32 BattleTowerChallenge_GetSaved(BattleTowerChallenge *challenge);
void BattleTowerChallenge_SetSaved(BattleTowerChallenge *challenge, u32 value);
u32 FrontierData_WifiStreakAction(FrontierData *frontierData, enum FrontierDataAction action);
u32 FrontierData_WifiRankAction(FrontierData *frontierData, enum FrontierDataAction action);
void FrontierData_SetWifiTeam(FrontierData *frontierData, u32 index, const FrontierMon *src);
void FrontierData_GetWifiTeam(FrontierData *frontierData, s32 index, FrontierWifiUploadRecord *dst);
u16 FrontierData_CalcWifiScore(FrontierData *frontierData, BattleTowerChallenge *challenge);
u16 FrontierData_GetWifiScore(FrontierData *frontierData);
u8 FrontierData_GetWifiWinCount(FrontierData *data);
u32 FrontierData_TowerRoundAction(FrontierData *data, u32 towerMode, enum FrontierDataAction action);
u32 FrontierData_SetTowerRound(FrontierData *frontierData, u32 towerMode, u16 round);
u32 FrontierData_FlagAction(FrontierData *frontierData, u16 flag, enum FrontierDataAction action);
void FrontierData_SetTowerSeed(FrontierData *data, u32 value);
u32 FrontierData_GetTowerSeed(FrontierData *data);
void Save_Frontier_SetTowerMessage(SaveData *saveData, int index, MailMessage *mailMessage);
MailMessage *Save_Frontier_GetTowerMessage(SaveData *save, u32 index);
void BattleRoomDownload_MarkRoom(struct BattleRoomDownload *download, u32 rank, u32 room, const RTCDate *date);
BOOL BattleRoomDownload_IsRoomMarked(struct BattleRoomDownload *download, u32 rank, u32 room, const RTCDate *date);
#if defined(SDK_PORT)
u32 BattleRoomDownload_HasTrainers(void *download);
u32 BattleRoomDownload_HasLeaders(void *download);
#else
u32 BattleRoomDownload_HasTrainers(u32 download);
u32 BattleRoomDownload_HasLeaders(u32 download);
#endif
void BattleRoomDownload_SetTrainers(struct BattleRoomDownload *download, const BattleRoomTrainer *trainers, u32 rank, u32 room);
void BattleRoomDownload_GetTrainersRoom(struct BattleRoomDownload *download, u8 *out);
#if defined(SDK_PORT)
void BattleRoomDownload_GetTrainer(void *addr, FrontierTrainer *trainer, int index);
#else
void BattleRoomDownload_GetTrainer(u32 addr, FrontierTrainer *trainer, int index);
#endif
void BattleRoomDownload_SetLeaders(struct BattleRoomDownload *download, const BattleRoomLeader *leaders, u32 rank, u32 room);
#if defined(SDK_PORT)
void BattleRoomDownload_GetLeadersRoom(void *addr, u8 *out);
BattleRoomLeader *BattleRoomDownload_CopyLeaders(void *addr, enum HeapID heapID);
#else
void BattleRoomDownload_GetLeadersRoom(u32 addr, u8 *out);
BattleRoomLeader *BattleRoomDownload_CopyLeaders(u32 addr, enum HeapID heapID);
#endif
BattleTowerChallenge *Save_BattleTowerChallenge_Get(SaveData *saveData);
#if defined(SDK_PORT)
void *Save_BattleRoomDownload_Get(SaveData *saveData);
#else
u32 Save_BattleRoomDownload_Get(SaveData *saveData);
#endif

#endif // POKEHEARTGOLD_FRONTIER_DATA_ACCESS_H
