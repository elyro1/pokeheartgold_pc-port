#ifndef POKEHEARTGOLD_OVERLAY_TRAINER_CARD_H
#define POKEHEARTGOLD_OVERLAY_TRAINER_CARD_H

#include "menu_input_state.h"
#include "overlay_manager.h"
#include "save.h"
#if defined(SDK_PORT)
#include "trainer_card_types.h"
#endif

#define HEAP_ID_TRAINER_CARD ((enum HeapID)94)

typedef struct TrainerCardAppSettings {
    MenuInputStateMgr *menuInputStatePtr;
    SaveData *saveData;
    int showBack;
    BOOL gameCleared;
} TrainerCardAppSettings;

typedef struct TrainerCardAppArgs {
#if defined(SDK_PORT)
    union {
        u8 card[sizeof(TrainerCardData)];
        TrainerCardData nativeCard;
    };
#else
    u8 card[0x66c];
#endif
    TrainerCardAppSettings settings;
} TrainerCardAppArgs;

typedef struct {
    u8 unk0[4];
    u8 unk4a : 4;
    u8 signatureExists : 1;
    u8 unk5[0x63];
    u8 signature[0x600];
    u8 unk668[6];
    SaveData *saveData;
    u32 reqUpdateSignature;
} Ov50_021E5A3C;

typedef struct {
    enum HeapID heapID;
    u8 unk4[4];
    OverlayManager *ov_mgr;
#if defined(SDK_PORT)
    TrainerCardAppArgs *parentData;
#else
    Ov50_021E5A3C *parentData;
#endif
    void *unk10;
} TrainerCardAppState;

BOOL TrainerCard_Init(OverlayManager *man, int *state);
BOOL TrainerCard_Main(OverlayManager *man, int *state);
BOOL TrainerCard_Exit(OverlayManager *man, int *state);

#endif // POKEHEARTGOLD_OVERLAY_TRAINER_CARD_H
