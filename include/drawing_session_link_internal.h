#ifndef POKEHEARTGOLD_DRAWING_SESSION_LINK_INTERNAL_H
#define POKEHEARTGOLD_DRAWING_SESSION_LINK_INTERNAL_H
#if defined(SDK_PORT)

#endif

#include "global.h"

#include "communication_command.h"
#include "field_comm_cmds.h"

typedef struct FourBytes {
    u8 data[4];
} FourBytes;

int Wireless_GetConnectedBitmap(void);
#if defined(SDK_PORT)
#else
int MATH_CountPopulation(u32);
#endif
void CommState_SetMaxEntries(int);
void GF_AssertFail(void);
void CommState_SetErrorWatch(BOOL, BOOL);

void DrawingSessionLink_RegisterCommands(void *arg);

#endif
