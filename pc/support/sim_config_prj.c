#include "port/sim_config_prj.h"

#include <simulator/config/ini.h>
#include <stdio.h>
#include <string.h>
static SIM_Config_prj_type sConfig = { .autosave = TRUE, .enable60fps = TRUE, .battlePacing = BATTLE_PACING_SMOOTH, .battleUiSpeedFix = TRUE };
static const char *sPacing[] = { "Original", "Smooth", "Quick" };
static int Handler(void *user, const char *section, const char *name, const char *value) {
    SIM_Config_prj_type *c = user;
    if (!strcmp(section, "Developer") && !strcmp(name, "Autosave")) {
        if (!strcmp(value, "true") || !strcmp(value, "1")) c->autosave = TRUE;
        if (!strcmp(value, "false") || !strcmp(value, "0")) c->autosave = FALSE;
    }
    if (!strcmp(section, "Cheats")) {
        BOOL enabled = !strcmp(value, "true") || !strcmp(value, "1");
        if (!strcmp(name, "WalkThroughWalls")) c->walkThroughWalls = enabled;
        if (!strcmp(name, "DisableRandomEncounters")) c->disableRandomEncounters = enabled;
        if (!strcmp(name, "RunFromTrainerBattles")) c->runFromTrainerBattles = enabled;
    }
    if (strcmp(section, "General")) {
        return 1;
    }
    if (!strcmp(name, "60Fps")) {
        c->enable60fps = !strcmp(value, "true") || !strcmp(value, "1");
    }
    if (!strcmp(name, "BattleUiSpeedFix")) {
        c->battleUiSpeedFix = !strcmp(value, "true") || !strcmp(value, "1");
    }
    if (!strcmp(name, "BattlePacing")) {
        for (int i = 0; i < BATTLE_PACING_COUNT; i++) {
            if (!strcmp(value, sPacing[i])) {
                c->battlePacing = i;
            }
        }
    }
    return 1;
}
SIM_Config_prj_type *SIM_Config_prj_GetConfig(void) {
    return &sConfig;
}
void SIM_Config_prj_Load(void) {
    ini_parse("sim_config_prj.ini", Handler, &sConfig);
}
void SIM_Config_prj_Save(void) {
    FILE *f = fopen("sim_config_prj.ini", "w");
    if (!f) {
        return;
    }
    fprintf(f, "[General]\n60Fps=%s\nBattlePacing=%s\nBattleUiSpeedFix=%s\n", sConfig.enable60fps ? "true" : "false", sPacing[sConfig.battlePacing], sConfig.battleUiSpeedFix ? "true" : "false");
    fprintf(f, "\n[Cheats]\nWalkThroughWalls=%s\nDisableRandomEncounters=%s\nRunFromTrainerBattles=%s\n",
        sConfig.walkThroughWalls ? "true" : "false",
        sConfig.disableRandomEncounters ? "true" : "false",
        sConfig.runFromTrainerBattles ? "true" : "false");
    fprintf(f, "\n[Developer]\nAutosave=%s\n", sConfig.autosave ? "true" : "false");
    fclose(f);
}
