#ifndef HG_SIM_CONFIG_PRJ_H
#define HG_SIM_CONFIG_PRJ_H
#include <nitro/types.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum BattlePacing {
    BATTLE_PACING_ORIGINAL,
    BATTLE_PACING_SMOOTH,
    BATTLE_PACING_QUICK,
    BATTLE_PACING_COUNT
} BattlePacing;
typedef struct SIM_Config_prj_type {
    BOOL autosave;
    BOOL enable60fps;
    BattlePacing battlePacing;
    BOOL battleUiSpeedFix;
    BOOL walkThroughWalls;
    BOOL disableRandomEncounters;
    BOOL runFromTrainerBattles;
} SIM_Config_prj_type;
SIM_Config_prj_type *SIM_Config_prj_GetConfig(void);
void SIM_Config_prj_Load(void);
void SIM_Config_prj_Save(void);
#ifdef __cplusplus
}
#endif
#endif
