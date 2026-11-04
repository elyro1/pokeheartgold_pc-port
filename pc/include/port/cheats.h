#ifndef HG_PC_CHEATS_H
#define HG_PC_CHEATS_H
#include <nitro/types.h>
#ifdef __cplusplus
extern "C" {
#endif
struct FieldSystem;
struct FieldSystem *PC_Cheats_GetFieldSystem(void);
BOOL PC_Cheats_FieldReady(void);
BOOL PC_Cheats_GiveMon(int species, int level);
BOOL PC_Cheats_GiveItem(int item, int quantity);
BOOL PC_Cheats_Warp(int map, int x, int z);
BOOL PC_Cheats_Fly(int spawn);
void PC_Cheats_GetName(BOOL item, int id, char *dest, unsigned capacity);
#ifdef __cplusplus
}
#endif
#endif
