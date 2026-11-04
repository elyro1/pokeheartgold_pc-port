#include "record_mixing/record_mixing_exchange.h"

#include "global.h"
#if defined(SDK_PORT)
#include "frontier_data_access.h"
#endif

#include "record_mixing/record_mixing_records.h"

#include "assert.h"
#include "communication_core.h"
#include "communication_player_state.h"
#include "fashion_case.h"
#include "friend_group.h"
#include "frontier_wifi_upload_record.h"
#include "heap.h"
#include "save.h"
#include "save_arrays.h"
#include "save_block_23.h"
#include "save_fashion.h"
#include "save_rankings.h"

// The records exchanged by record mixing: per record type, its size, how it is loaded into the send buffer and how
// the copies received from the other members are merged into the save.

#define RECORD_MIXING_MAX_MEMBERS 5

void SaveBlock23_ClearArraysAndReset(SaveBlock23 *data);
void SaveBlock23_Reset(SaveBlock23 *data);

u32 RecordMixing_FriendGroup_GetSize(SaveData *saveData) {
    return RecordMixing_GetGroupRecordSize(Save_FriendGroup_Get(saveData));
}

void *RecordMixing_FriendGroup_Load(SaveData *saveData, enum HeapID heapID, u32 size) {
    SAV_FRIEND_GRP *friendGroup = Save_FriendGroup_Get(saveData);
    void *copy = Heap_AllocAtEnd(heapID, size);
    void *src = RecordMixing_GetGroupRecord(friendGroup);
    MIi_CpuCopyFast(src, copy, size);
    return copy;
}

void RecordMixing_FriendGroup_Merge(RecordMixingMergeArgs *args) {
    RecordMixing_MergeGroups(args->numPlayers, args->localId, Save_FriendGroup_Get(args->saveData)->groups, (FRIEND_GROUP **)args->records);
}

u32 RecordMixing_Fashion_GetSize(SaveData *saveData) {
    Save_FashionData_Get(saveData);
    return FashionEntry_sizeof();
}

void *RecordMixing_Fashion_Load(SaveData *saveData, enum HeapID heapID, u32 size) {
    SaveFashionData *fashionData = Save_FashionData_Get(saveData);
    void *copy = Heap_AllocAtEnd(heapID, size);
    void *src = Save_FashionData_GetEntry(fashionData, 0);
    MIi_CpuCopyFast(src, copy, size);
    return copy;
}

void RecordMixing_Fashion_Merge(RecordMixingMergeArgs *args) {
    SaveData *saveData = args->saveData;
    SaveFashionData *fashionData = Save_FashionData_Get(saveData);
    Save_FashionData_InsertEntries(args->numPlayers, args->localId, fashionData, (SaveFashionDataSub **)args->records);
}

void RecordMixing_Block23_Merge(RecordMixingMergeArgs *args) {
    SaveBlock23 *data = Save_Block23_Get(args->saveData);
    RecordMixing_MergeBlock23(data, args->numPlayers, args->localId, (RecordMixingEntry **)args->records, (RecordMixingEntry **)args->records2);
}

SaveRankingsEntry *RecordMixing_Rankings_Load(SaveData *saveData, enum HeapID heapID) {
    return Save_GetPlayerMixingRankingEntry(saveData, heapID);
}

void RecordMixing_Rankings_Merge(RecordMixingMergeArgs *args) {
    Save_Rankings_Get(args->saveData);
    // the local id is passed without truncation to u8
    ((void (*)(SaveData *, int, u8, SaveRankingsEntry **, enum HeapID))Save_UpdateRankingsFromMixing)(args->saveData, args->localId, args->numPlayers, (SaveRankingsEntry **)args->records, args->heapID);
}

void *RecordMixing_BattleRoomTrainer_Load(SaveData *saveData, enum HeapID heapID, u32 size) {
    void *dest = Heap_AllocAtEnd(heapID, size);
    MI_CpuFill8(dest, 0, size);
    Frontier_FillWifiUploadRecord(saveData, 0, dest);
    return dest;
}

void RecordMixing_BattleRoomTrainer_Merge(RecordMixingMergeArgs *args) {
}

// Copies the data of every record type into the send buffer.
void RecordMixing_BuildSendBuffer(SaveData *saveData, void *dest) {
    u32 remaining = RECORD_MIXING_SEND_SIZE;
    int i;

    for (i = 0; i < RECORD_MIXING_NUM_HANDLERS; i++) {
        u32 size = sRecordMixingHandlers[i].getSize(saveData);
        void *buffer = sRecordMixingHandlers[i].load(saveData, HEAP_ID_50, size);
        GF_ASSERT(remaining > size);
        MIi_CpuCopyFast(buffer, dest, size);
        Heap_Free(buffer);
        remaining -= size;
        dest = (u8 *)dest + size;
    }
}

// Merges the records received from the other members into the save, record type by record type.
void RecordMixing_MergeReceivedRecords(SaveData *saveData, u8 (*buffers)[RECORD_MIXING_BUFFER_SIZE]) {
    RecordMixingMergeArgs args;
    void *records[RECORD_MIXING_MAX_MEMBERS];
    void *records2[RECORD_MIXING_MAX_MEMBERS];
    u32 offsets[3];
    u32 size;
    SaveBlock23 *block23;
    u32 remaining;
    int i;
    u32 offset;
    int j;
    u32 *offsetOfEarlier;

    remaining = RECORD_MIXING_SEND_SIZE;
    args.heapID = HEAP_ID_50;
    offset = 0;
    args.saveData = saveData;
    args.localId = Comm_GetLocalNetId();
    args.numPlayers = RECORD_MIXING_MAX_MEMBERS;
    args.records = records;
    args.records2 = records2;
    // offsets of the records of types 2, 3 and 4 in the received buffers
    offsets[0] = 0;
    offsets[1] = 0;
    offsets[2] = 0;
    for (i = 0; i < 2; i++) {
        offsets[0] += sRecordMixingHandlers[i].getSize(saveData);
    }
    for (i = 0; i < 3; i++) {
        offsets[1] += sRecordMixingHandlers[i].getSize(saveData);
    }
    for (i = 0; i < 4; i++) {
        offsets[2] += sRecordMixingHandlers[i].getSize(saveData);
    }
    for (i = 0; (u32)i < RECORD_MIXING_NUM_HANDLERS; i++) {
        size = sRecordMixingHandlers[i].getSize(saveData);
        GF_ASSERT(remaining > size);
        for (j = 0; j < RECORD_MIXING_MAX_MEMBERS; j++) {
            if (CommPlayerInfo_GetProfile(j)) {
                args.records[j] = buffers[j] + offset;
            } else {
                args.records[j] = NULL;
            }
        }
        offsetOfEarlier = &offsets[i]; // only read for types 7-9: offsets[i - 7]
        switch (i) {
        case 7:
        case 8:
        case 9:
            // types 7, 8 and 9 also get the received records of types 2, 3 and 4
            for (j = 0; j < RECORD_MIXING_MAX_MEMBERS; j++) {
                if (CommPlayerInfo_GetProfile(j)) {
                    args.records2[j] = buffers[j] + offsetOfEarlier[-7];
                } else {
                    args.records2[j] = NULL;
                }
            }
            break;
        }
        if (sRecordMixingHandlers[i].merge != NULL) {
            sRecordMixingHandlers[i].merge(&args);
        }
        offset += size;
        remaining -= size;
    }
    block23 = Save_Block23_Get(saveData);
    SaveBlock23_ClearArraysAndReset(block23);
    SaveBlock23_Reset(block23);
}
#if defined(SDK_PORT)

static u32 Native_Save_Rankings_GetMixingSize(SaveData *saveData) { return Save_Rankings_GetMixingSize(); }
static void *Native_RecordMixing_Rankings_Load(SaveData *saveData, enum HeapID heapID, u32 size) { return RecordMixing_Rankings_Load(saveData, heapID); }
static u32 Native_BattleRoomTrainer_sizeof(SaveData *saveData) { return BattleRoomTrainer_sizeof(); }
const RecordMixingHandler sRecordMixingHandlers[RECORD_MIXING_NUM_HANDLERS] = {
    RecordMixing_FriendGroup_GetSize,
    RecordMixing_FriendGroup_Load,
    RecordMixing_FriendGroup_Merge,
    RecordMixing_Fashion_GetSize,
    RecordMixing_Fashion_Load,
    RecordMixing_Fashion_Merge,
    RecordMixing_Block23List0_GetSize,
    RecordMixing_Block23List0_Load,
    0,
    RecordMixing_Block23List1_GetSize,
    RecordMixing_Block23List1_Load,
    0,
    RecordMixing_Block23List2_GetSize,
    RecordMixing_Block23List2_Load,
    0,
    Native_Save_Rankings_GetMixingSize,
    Native_RecordMixing_Rankings_Load,
    RecordMixing_Rankings_Merge,
    Native_BattleRoomTrainer_sizeof,
    RecordMixing_BattleRoomTrainer_Load,
    RecordMixing_BattleRoomTrainer_Merge,
    RecordMixing_Block23List0_GetSize,
    RecordMixing_Block23List0_Load2,
    0,
    RecordMixing_Block23List1_GetSize,
    RecordMixing_Block23List1_Load2,
    0,
    RecordMixing_Block23List2_GetSize,
    RecordMixing_Block23List2_Load2,
    RecordMixing_Block23_Merge,
};
#endif
