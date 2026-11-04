#ifndef POKEHEARTGOLD_RECORD_MIXING_LINK_INTERNAL_H
#define POKEHEARTGOLD_RECORD_MIXING_LINK_INTERNAL_H
#include "global.h"

#include "communication_command.h"
#include "field_comm_cmds.h"
#include "wireless_manager.h"
typedef struct RecordMixingJoinRequest {
    u8 sender, count, command, accepted;
} RecordMixingJoinRequest;
int RecordMixing_GetMemberCount(void);
#if defined(SDK_PORT)
#else
u32 MATH_CountPopulation(u32);
#endif
void CommState_SetMaxEntries(int);

#endif
