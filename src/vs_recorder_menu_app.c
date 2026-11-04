#include "constants/flypoints.h"
#include "constants/trainer_class.h"

#include "global_terminal/global_terminal_app_internal.h"

#include "assert.h"
#include "field_bgm.h"
#include "heap.h"
#include "orbit_cursor.h"
#include "poke_overlay.h"
#include "save_misc_data.h"
#include "sound_playback.h"
#include "sound_system.h"
#include "sys_flags.h"
#include "system.h"
#include "vs_recorder_app.h"
#include "vs_recorder_app_internal.h"

enum VsRecorderMenuExitState {
    VS_RECORDER_MENU_EXIT_DESTROY_TASKS,
    VS_RECORDER_MENU_EXIT_DELETE_SPRITES,
    VS_RECORDER_MENU_EXIT_DELETE_CURSOR_1,
    VS_RECORDER_MENU_EXIT_DELETE_CURSOR_2,
};

static BOOL VsRecorderMenu_InitCommon(OverlayManager *manager, int *state, BOOL fromTerminal);
static int VsRecorderMenu_Init(OverlayManager *manager, int *state);
static int VsRecorderMenu_InitFromTerminal(OverlayManager *manager, int *state);
static BOOL VsRecorderMenu_Main(OverlayManager *manager, int *state);
static BOOL VsRecorderMenu_Exit(OverlayManager *manager, int *state);
static const VsRecorderMenuEntry *VsRecorderApp_GetRootMenu(int mode);

FS_EXTERN_OVERLAY(OVY_40);

const OverlayManagerTemplate gOverlayTemplate_VsRecorderApp = { VsRecorderApp_InitMode0, VsRecorderApp_Main, VsRecorderApp_Exit, FS_OVERLAY_ID_NONE };
static const OverlayManagerTemplate sOverlayTemplate_VsRecorderAppMode1 = { VsRecorderApp_InitMode1, VsRecorderApp_Main, VsRecorderApp_Exit, FS_OVERLAY_ID_NONE };
static const OverlayManagerTemplate sOverlayTemplate_VsRecorderAppMode2 = { VsRecorderApp_InitMode2, VsRecorderApp_Main, VsRecorderApp_Exit, FS_OVERLAY_ID_NONE };
static const OverlayManagerTemplate sOverlayTemplate_VsRecorderAppMode3 = { VsRecorderApp_InitMode3, VsRecorderApp_Main, VsRecorderApp_Exit, FS_OVERLAY_ID_NONE };
static const OverlayManagerTemplate sOverlayTemplate_VsRecorderAppMode4 = { VsRecorderApp_InitMode4, VsRecorderApp_Main, VsRecorderApp_Exit, FS_OVERLAY_ID_NONE };
static const OverlayManagerTemplate sOverlayTemplate_VsRecorderAppMode5 = { VsRecorderApp_InitMode5, VsRecorderApp_Main, VsRecorderApp_Exit, FS_OVERLAY_ID_NONE };
static const OverlayManagerTemplate sOverlayTemplate_VsRecorderAppMode6 = { VsRecorderApp_InitMode6, VsRecorderApp_Main, VsRecorderApp_Exit, FS_OVERLAY_ID_NONE };

static const FrontierBrainClasses sFrontierBrainTrainerClasses = {
    { TRAINERCLASS_TOWER_TYCOON, TRAINERCLASS_HALL_MATRON, TRAINERCLASS_FACTORY_HEAD, TRAINERCLASS_ARCADE_STAR, TRAINERCLASS_CASTLE_VALET }
};

static const OverlayManagerTemplate *sVsRecorderAppTemplates[] = {
    &gOverlayTemplate_VsRecorderApp,
    &sOverlayTemplate_VsRecorderAppMode1,
    &sOverlayTemplate_VsRecorderAppMode2,
    &sOverlayTemplate_VsRecorderAppMode3,
    &sOverlayTemplate_VsRecorderAppMode4,
    &sOverlayTemplate_VsRecorderAppMode5,
    &sOverlayTemplate_VsRecorderAppMode6,
};

const OverlayManagerTemplate *VsRecorderApp_GetTemplate(u8 mode) {
    return sVsRecorderAppTemplates[mode];
}

// Fly point flag 27 (FLYPOINT_UNION_CAVE) doubles as the Battle Frontier's visited flag: the Frontier Access gatehouse
// sets it on the first visit, and the Pokegear map shows the Battle Frontier once it is set. Until then the VS Recorder
// leaves out its Battle Frontier parts.
BOOL VsRecorderApp_HasVisitedBattleFrontier(VsRecorderWork *work) {
    SaveVarsFlags *varsFlags = Save_VarsFlags_Get(work->saveData);
    return Save_VarsFlags_FlypointFlagAction(varsFlags, 2, FLYPOINT_UNION_CAVE);
}

BOOL VsRecorderApp_IsFrontierBrainClass(u32 trainerClass) {
    FrontierBrainClasses classes = sFrontierBrainTrainerClasses;
    u32 i;
    for (i = 0; i < 5; i++) {
        if (trainerClass == classes.trainerClass[i]) {
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL VsRecorderMenu_InitCommon(OverlayManager *manager, int *state, BOOL fromTerminal) {
    VsRecorderWork *recorder;

    StopAllSE(0);
    Heap_Create(HEAP_ID_3, HEAP_ID_109, 0x55000);
    recorder = VsRecorderApp_GetRecorderWork(manager);
    VsRecorder_MenuPop(recorder);
    GF_SndHandleSetPlayerVolume(1, 0x2A);
    GF_SndHandleSetPlayerVolume(7, 0x2A);
    if (fromTerminal) {
        FieldBGM_Play(NULL, SEQ_GS_WIFITOWER);
    }
    if (!fromTerminal) {
        SaveMisc_GetVsRecorderColorScheme(Save_Misc_Get(recorder->saveData), &recorder->colorScheme);
        if (recorder->colorScheme >= 7) {
            recorder->colorScheme = 0;
        }
        recorder->blendColor = VsRecorder_GetBlendColor(recorder);
    } else {
        recorder->blendColor = 0x7FDD;
    }
    VsRecorder_InitResources(recorder);
    return TRUE;
}

static int VsRecorderMenu_Init(OverlayManager *manager, int *state) {
    return VsRecorderMenu_InitCommon(manager, state, FALSE);
}

static int VsRecorderMenu_InitFromTerminal(OverlayManager *manager, int *state) {
    return VsRecorderMenu_InitCommon(manager, state, TRUE);
}

static BOOL VsRecorderMenu_Main(OverlayManager *manager, int *state) {
    VsRecorderWork *recorder = VsRecorderApp_GetRecorderWork(manager);
    recorder->screenPtr = state;
    if (VsRecorder_Main(recorder, state)) {
        return TRUE;
    }
    return FALSE;
}

static BOOL VsRecorderMenu_Exit(OverlayManager *manager, int *state) {
    VsRecorderWork *recorder = VsRecorderApp_GetRecorderWork(manager);

    switch (*state) {
    case VS_RECORDER_MENU_EXIT_DESTROY_TASKS:
        VsRecorder_DestroyDecorTasks(recorder);
        (*state)++;
        break;
    case VS_RECORDER_MENU_EXIT_DELETE_SPRITES:
        VsRecorder_DeleteDecorSprites(recorder);
        VsRecorder_DeleteMenuRowSprites(recorder);
        VsRecorder_FreeMenuRowText(recorder);
        (*state)++;
        break;
    case VS_RECORDER_MENU_EXIT_DELETE_CURSOR_1:
        if (!OrbitCursor_Delete(recorder->orbitCursors[0])) {
            (*state)++;
        }
        break;
    case VS_RECORDER_MENU_EXIT_DELETE_CURSOR_2:
        if (!OrbitCursor_Delete(recorder->orbitCursors[1])) {
            (*state)++;
        }
        break;
    default:
        VsRecorder_FreeResources(recorder);
        Heap_Destroy(HEAP_ID_109);
        UnloadOverlayByID(FS_OVERLAY_ID(OVY_40));
        gSystem.screensFlipped = FALSE;
        GfGfx_SwapDisplay();
        Field_ForceEnvironmentSoundRecheck();
        return TRUE;
    }
    return FALSE;
}

const OverlayManagerTemplate gOverlayTemplate_VsRecorderMenuFromTerminal = { VsRecorderMenu_InitFromTerminal, VsRecorderMenu_Main, VsRecorderMenu_Exit, FS_OVERLAY_ID(OVY_40) };
const OverlayManagerTemplate gOverlayTemplate_VsRecorderMenu = { VsRecorderMenu_Init, VsRecorderMenu_Main, VsRecorderMenu_Exit, FS_OVERLAY_ID(OVY_40) };

static const VsRecorderMenuEntry *VsRecorderApp_GetRootMenu(int mode) {
    if (mode >= 7) {
        GF_ASSERT(mode < 7);
        // falls back to the first mode's menu (sVsRecorderRootMenu[0].child)
#if defined(SDK_PORT)
        return sVsRecorderRootMenu[0].child;
#else
        return *(const VsRecorderMenuEntry **)(sVsRecorderMode6MenuTail + 0x64);
#endif
    }
    return sVsRecorderRootMenu[mode].child;
}

const VsRecorderMenuEntry *VsRecorderApp_GetStartMenu(VsRecorderWork *recorder, int mode) {
    if (mode == 0 && !VsRecorderApp_HasVisitedBattleFrontier(recorder)) {
        return sVsRecorderMode0MenuNoFrontier;
    }
    if (mode == 3 && !VsRecorderApp_HasVisitedBattleFrontier(recorder)) {
        return sVsRecorderMode3MenuNoFrontier;
    }
    return VsRecorderApp_GetRootMenu(mode);
}

void VsRecorderApp_CountMenuChoices(VsRecorderWork *recorder) {
    int i = 0;
    const VsRecorderMenuEntry *entry = recorder->menu;

    recorder->choiceCount = 0;
    while (i < 5) {
        if (entry->enabled) {
            recorder->choiceCount++;
        }
        i++;
        entry++;
    }
}
#if defined(SDK_PORT)

/* Native menu entries retain master's messages and tree links; child pointers use the host width. */
static const VsRecorderMenuEntry sVsRecorderQuitMenu[5];
static const VsRecorderMenuEntry sVsRecorderMode3QuitMenu[5];
static const VsRecorderMenuEntry sVsRecorderMode2QuitMenu[5];
static const VsRecorderMenuEntry sVsRecorderMode4QuitMenu[5];
static const VsRecorderMenuEntry sVsRecorderMode2Menu[5];
static const VsRecorderMenuEntry sVsRecorderSendVideoMenu[5];
static const VsRecorderMenuEntry sVsRecorderMode3Menu[5];
const VsRecorderMenuEntry sVsRecorderMode3MenuNoFrontier[5];
static const VsRecorderMenuEntry sVsRecorderMode4Menu[5];
static const VsRecorderMenuEntry sVsRecorderViewVideosMenu[5];
static const VsRecorderMenuEntry sVsRecorderMode5Menu[5];
static const VsRecorderMenuEntry sVsRecorderSearchMenu[5];
static const VsRecorderMenuEntry sVsRecorderMode1SubMenu[5];
static const VsRecorderMenuEntry sVsRecorderMode1Menu[5];
static const VsRecorderMenuEntry sVsRecorderDeleteOtherVideoMenu[5];
static const VsRecorderMenuEntry sVsRecorderDeleteVideosMenu[5];
static const VsRecorderMenuEntry sVsRecorderOtherVideosMenu[5];
static const VsRecorderMenuEntry sVsRecorderBattleVideosMenu[5];
const VsRecorderMenuEntry sVsRecorderMode0MenuNoFrontier[5];
static const VsRecorderMenuEntry sVsRecorderMode0Menu[5];
static const VsRecorderMenuEntry sVsRecorderMode6Menu[5];
const VsRecorderMenuEntry sVsRecorderRootMenu[7];
static const VsRecorderMenuEntry sVsRecorderQuitMenu[5] = {
    { 1, 1, 130, 4, 2, 2, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 1, 1, 131, 3, 2, 4, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
};
static const VsRecorderMenuEntry sVsRecorderMode3QuitMenu[5] = {
    { 1, 1, 130, 5, 2, 2, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 1, 1, 131, 4, 2, 4, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
};
static const VsRecorderMenuEntry sVsRecorderMode2QuitMenu[5] = {
    { 1, 1, 130, 4, 2, 2, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 1, 1, 131, 3, 2, 4, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
};
static const VsRecorderMenuEntry sVsRecorderMode4QuitMenu[5] = {
    { 1, 1, 130, 5, 2, 2, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 1, 1, 131, 4, 2, 4, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
};
static const VsRecorderMenuEntry sVsRecorderMode2Menu[5] = {
    { 1, 1, 103, 2, 1, 3, 0xFBFBFBFB, (int)0xFBFBFBFB, sVsRecorderViewVideosMenu },
    { 1, 1, 104, 6, 1, 3, 9999, (int)0xFBFBFBFB, sVsRecorderSendVideoMenu },
    { 1, 1, 2, 9, 2, 3, 999, (int)0xFBFBFBFB, sVsRecorderMode2QuitMenu },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
};
static const VsRecorderMenuEntry sVsRecorderSendVideoMenu[5] = {
    { 1, 1, 130, 4, 19, 3, 100, (int)0xFBFBFBFB, 0 },
    { 1, 1, 3, 3, 3, 4, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
};
static const VsRecorderMenuEntry sVsRecorderMode3Menu[5] = {
    { 1, 1, 92, 0, 14, 3, 300, (int)0xFBFBFBFB, 0 },
    { 1, 1, 93, 1, 15, 3, 301, (int)0xFBFBFBFB, 0 },
    { 1, 1, 2, 2, 2, 3, 999, (int)0xFBFBFBFB, sVsRecorderMode3QuitMenu },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
};
const VsRecorderMenuEntry sVsRecorderMode3MenuNoFrontier[5] = {
    { 1, 1, 92, 0, 14, 3, 300, (int)0xFBFBFBFB, 0 },
    { 1, 1, 2, 2, 2, 3, 999, (int)0xFBFBFBFB, sVsRecorderMode3QuitMenu },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
};
static const VsRecorderMenuEntry sVsRecorderMode4Menu[5] = {
    { 1, 1, 78, 0, 12, 3, 0, (int)0xFBFBFBFB, 0 },
    { 1, 1, 79, 1, 13, 3, 1, (int)0xFBFBFBFB, 0 },
    { 1, 1, 2, 2, 2, 3, 999, (int)0xFBFBFBFB, sVsRecorderMode4QuitMenu },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
};
static const VsRecorderMenuEntry sVsRecorderViewVideosMenu[5] = {
    { 1, 1, 105, 7, 16, 3, 200, (int)0xFBFBFBFB, 0 },
    { 1, 1, 106, 0, 17, 3, 210, (int)0xFBFBFBFB, 0 },
    { 1, 1, 107, 8, 18, 3, 220, (int)0xFBFBFBFB, 0 },
    { 1, 1, 3, 3, 3, 4, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
};
static const VsRecorderMenuEntry sVsRecorderMode5Menu[5] = {
    { 1, 1, 52, 0, 9, 3, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 1, 1, 51, 0, 8, 3, 0, (int)0xFBFBFBFB, 0 },
    { 1, 1, 2, 2, 2, 3, 999, (int)0xFBFBFBFB, sVsRecorderQuitMenu },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
};
static const VsRecorderMenuEntry sVsRecorderSearchMenu[5] = {
    { 1, 1, 48, 1, 20, 6, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 1, 1, 49, 0, 20, 6, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 1, 1, 50, 0, 20, 6, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 1, 1, 3, 5, 3, 4, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
};
static const VsRecorderMenuEntry sVsRecorderMode1SubMenu[5] = {
    { 1, 1, 45, 10, 20, 6, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 1, 1, 46, 0, 1, 3, 0xFBFBFBFB, (int)0xFBFBFBFB, sVsRecorderSearchMenu },
    { 1, 1, 47, 11, 20, 6, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 1, 1, 3, 5, 3, 4, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
};
static const VsRecorderMenuEntry sVsRecorderMode1Menu[5] = {
    { 1, 1, 43, 2, 1, 3, 0xFBFBFBFB, (int)0xFBFBFBFB, sVsRecorderMode1SubMenu },
    { 1, 1, 44, 9, 20, 6, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 1, 1, 2, 14, 2, 2, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
};
static const VsRecorderMenuEntry sVsRecorderDeleteOtherVideoMenu[5] = {
    { 1, 1, 41, 7, 7, 3, 1, (int)0xFBFBFBFB, 0 },
    { 1, 1, 41, 7, 7, 3, 2, (int)0xFBFBFBFB, 0 },
    { 1, 1, 41, 7, 7, 3, 3, (int)0xFBFBFBFB, 0 },
    { 1, 1, 3, 5, 3, 4, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
};
static const VsRecorderMenuEntry sVsRecorderDeleteVideosMenu[5] = {
    { 1, 1, 39, 6, 7, 3, 0, (int)0xFBFBFBFB, 0 },
    { 1, 1, 40, 7, 0, 3, 0xFBFBFBFB, (int)0xFBFBFBFB, sVsRecorderDeleteOtherVideoMenu },
    { 1, 1, 3, 5, 3, 4, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
};
static const VsRecorderMenuEntry sVsRecorderOtherVideosMenu[5] = {
    { 1, 1, 8, 7, 4, 3, 1, (int)0xFBFBFBFB, 0 },
    { 1, 1, 8, 7, 4, 3, 2, (int)0xFBFBFBFB, 0 },
    { 1, 1, 8, 7, 4, 3, 3, (int)0xFBFBFBFB, 0 },
    { 1, 1, 3, 5, 3, 4, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
};
static const VsRecorderMenuEntry sVsRecorderBattleVideosMenu[5] = {
    { 1, 1, 4, 6, 4, 3, 0, (int)0xFBFBFBFB, 0 },
    { 1, 1, 5, 7, 1, 3, 0xFBFBFBFB, (int)0xFBFBFBFB, sVsRecorderOtherVideosMenu },
    { 1, 1, 6, 1, 1, 3, 0xFBFBFBFB, (int)0xFBFBFBFB, sVsRecorderDeleteVideosMenu },
    { 1, 1, 3, 5, 3, 4, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
};
const VsRecorderMenuEntry sVsRecorderMode0MenuNoFrontier[5] = {
    { 1, 1, 0, 0, 1, 3, 0xFBFBFBFB, (int)0xFBFBFBFB, sVsRecorderBattleVideosMenu },
    { 1, 1, 2, 4, 2, 2, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
};
static const VsRecorderMenuEntry sVsRecorderMode0Menu[5] = {
    { 1, 1, 0, 0, 1, 3, 0xFBFBFBFB, (int)0xFBFBFBFB, sVsRecorderBattleVideosMenu },
    { 1, 1, 1, 3, 5, 3, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 1, 1, 2, 4, 2, 2, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
};
static const VsRecorderMenuEntry sVsRecorderMode6Menu[5] = {
    { 1, 1, 59, 1, 11, 3, 0xFBFBFBFB, (int)0xFBFBFBFB, 0 },
    { 1, 1, 58, 1, 10, 3, 100, (int)0xFBFBFBFB, 0 },
    { 1, 1, 2, 2, 2, 3, 999, (int)0xFBFBFBFB, sVsRecorderQuitMenu },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, (int)0, 0 },
};
const VsRecorderMenuEntry sVsRecorderRootMenu[7] = {
    { 1, 1, 255, 65535, 0, 9, 0xFBFBFBFB, (int)0xFBFBFBFB, sVsRecorderMode0Menu },
    { 1, 1, 255, 65535, 0, 9, 0xFBFBFBFB, (int)0xFBFBFBFB, sVsRecorderMode1Menu },
    { 1, 1, 255, 65535, 0, 9, 0xFBFBFBFB, (int)0xFBFBFBFB, sVsRecorderMode2Menu },
    { 1, 1, 255, 65535, 0, 9, 0xFBFBFBFB, (int)0xFBFBFBFB, sVsRecorderMode3Menu },
    { 1, 1, 255, 65535, 0, 9, 0xFBFBFBFB, (int)0xFBFBFBFB, sVsRecorderMode4Menu },
    { 1, 1, 255, 65535, 0, 9, 0xFBFBFBFB, (int)0xFBFBFBFB, sVsRecorderMode5Menu },
    { 1, 1, 255, 65535, 0, 9, 0xFBFBFBFB, (int)0xFBFBFBFB, sVsRecorderMode6Menu },
};
#endif
