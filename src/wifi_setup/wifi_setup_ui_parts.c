#include "wifi_setup/wifi_setup_ui_parts.h"

#if defined(SDK_PORT)
#include <nitro/wm.h>
#else
#include <nitro/wm/wm_internal.h>
#endif
#include <string.h>

#include "global.h"

#include "wifi_setup/wifi_setup_access_points.h"
#include "wifi_setup/wifi_setup_app.h"
#include "wifi_setup/wifi_setup_keyboard.h"
#include "wifi_setup/wifi_setup_local_wireless.h"
#include "wifi_setup/wifi_setup_ui.h"

#include <nitro/code32.h>

extern s32 FX_DivS32(s32, s32);

static void WifiSetupButtonBar_Create(int type);
static void WifiSetupButtonBar_SetY(int y);
static void WifiSetupButtonBar_SlideInTask(WifiSetupTask *task, void *arg);
static void WifiSetupButtonBar_WaitTask(WifiSetupTask *task, void *arg);
static void WifiSetupButtonBar_InputTask(WifiSetupTask *task, void *arg);
static void WifiSetupButtonBar_SlideOutTask(WifiSetupTask *task, void *arg);
static void WifiSetupButtonBar_DestroyTask(WifiSetupTask *task, void *arg);
static void WifiSetupButtonBar_PressTask(WifiSetupTask *task, void *arg);
static void WifiSetupBackground_TransferTask(WifiSetupTask *task, void *arg);
static void WifiSetupSignalIcon_Task(WifiSetupTask *task, void *arg);

void WifiSetupBusyIcon_Task(WifiSetupTask *task, void *arg) {
    GXOamAttr *oam;
    u32 palette;
    gWifiSetupBusyIcon->frameCounter = FX_ModS32(gWifiSetupBusyIcon->frameCounter + 1, 40);
    {
        int templ = FX_DivS32(gWifiSetupBusyIcon->frameCounter, 5) + 71;
        WifiSetupOamTemplate_Copy(WIFI_SETUP_SCREEN_MAIN, templ, gWifiSetupBusyIcon->oam);
    }
    oam = gWifiSetupBusyIcon->oam;
    oam->attr2 = (oam->attr2 & ~0xc00) | 0x400;
    palette = gWifiSetupBusyIconPalettes[gWifiSetupBusyIcon->style];
    oam = gWifiSetupBusyIcon->oam;
    oam->attr01 &= ~0xc00;
    oam->attr2 = (oam->attr2 & ~0xf000) | (palette << 12);
    oam = gWifiSetupBusyIcon->oam;
    oam->attr01 = (oam->attr01 & 0xfe00ff00) | 0xe6008b;
}

static void WifiSetupButtonBar_Create(int type) {
    int count = gWifiSetupButtonBarButtonCounts[type], i;
    gWifiSetupButtonBar = WifiSetupHeap_AllocZeroed(sizeof(WifiSetupButtonBar), 4);
    gWifiSetupButtonBar->selected = WIFI_SETUP_SELECTED_NOT_SHOWN;
    gWifiSetupButtonBar->type = type;
    for (i = 0; i < count; i++) {
        gWifiSetupButtonBar->buttons[i] = WifiSetupOamTemplate_AllocBlock(WIFI_SETUP_SCREEN_MAIN, gWifiSetupButtonBarTemplates[type][i], TRUE);
        WifiSetupOamBlock_SetPriority(gWifiSetupButtonBar->buttons[i], -1, 1);
    }
    gWifiSetupButtonBar->bar = WifiSetupOamTemplate_AllocBlock(WIFI_SETUP_SCREEN_MAIN, 1, TRUE);
    WifiSetupOamBlock_SetPriority(gWifiSetupButtonBar->bar, -1, 1);
    WifiSetupButtonBar_SetY(192);
    gWifiSetupButtonBar->task = WifiSetupTask_Create(WIFI_SETUP_TASK_QUEUE_MAIN, WifiSetupButtonBar_SlideInTask, NULL, 120);
}

void WifiSetupButtonBar_Close(void) {
    gWifiSetupButtonBar->closing = 1;
    WifiSetupTask_SetFunc(gWifiSetupButtonBar->task, WifiSetupButtonBar_SlideOutTask);
}

int WifiSetupButtonBar_GetSelected(void) {
    return gWifiSetupButtonBar->selected;
}

void WifiSetupButtonBar_SelectIfNone(int button) {
    if (gWifiSetupButtonBar->selected == WIFI_SETUP_SELECTED_NONE) {
        gWifiSetupButtonBar->selected = button;
    }
}

void WifiSetupButtonBar_SetSelected(int button) {
    gWifiSetupButtonBar->selected = button;
}

BOOL WifiSetupButtonBar_IsNotClosing(void) {
    if (!gWifiSetupButtonBar) {
        return TRUE;
    }
    if (!gWifiSetupButtonBar->closing) {
        return TRUE;
    }
    return FALSE;
}

void WifiSetupButtonBar_Unlock(void) {
    gWifiSetupButtonBar->locked = FALSE;
}

void WifiSetupButtonBar_Lock(void) {
    gWifiSetupButtonBar->locked = TRUE;
}

static void WifiSetupButtonBar_SetY(int y) {
    int count = gWifiSetupButtonBarButtonCounts[gWifiSetupButtonBar->type], i;
    WifiSetupOamBlock_SetPosition(gWifiSetupButtonBar->bar, -1, 0, y);
    for (i = 0; i < count; i++) {
        WifiSetupOamBlock_SetPosition(gWifiSetupButtonBar->buttons[i], -1, gWifiSetupButtonBarButtonX[gWifiSetupButtonBarPositions[gWifiSetupButtonBar->type][i] * 2], y + 4);
    }
}

static void WifiSetupButtonBar_SlideInTask(WifiSetupTask *task, void *arg) {
    int x, y;
    WifiSetupOamBlock_GetPosition(gWifiSetupButtonBar->bar, 0, &x, &y);
    y -= 4;
    WifiSetupButtonBar_SetY(y);
    if (y > 168) {
        return;
    }
    WifiSetupButtonBar_SetY(168);
    WifiSetupTask_SetFunc(task, WifiSetupButtonBar_WaitTask);
}

static void WifiSetupButtonBar_WaitTask(WifiSetupTask *task, void *arg) {
    gWifiSetupButtonBar->selected = WIFI_SETUP_SELECTED_NONE;
    gWifiSetupButtonBar->timer++;
    if (gWifiSetupButtonBar->timer < 4) {
        return;
    }
    gWifiSetupButtonBar->timer = 0;
    WifiSetupTask_SetFunc(task, WifiSetupButtonBar_InputTask);
}

static void WifiSetupButtonBar_InputTask(WifiSetupTask *task, void *arg) {
    WifiSetupRect rect;
    int count = gWifiSetupButtonBarButtonCounts[gWifiSetupButtonBar->type], i;
    if (!gWifiSetupButtonBar->locked) {
        if (gWifiSetupButtonBar->selected != WIFI_SETUP_SELECTED_NONE) {
            return;
        }
        for (i = 0; i < count; i++) {
            WifiSetup_RectFromPosSize((WifiSetupPoint *)(gWifiSetupButtonBarButtonX + gWifiSetupButtonBarPositions[gWifiSetupButtonBar->type][i] * 2), &gWifiSetupButtonBarButtonSize, &rect);
            if (WifiSetupInput_IsTouchPressed(&rect)) {
                int templ;
                GXOamAttr *oam;
                if (gWifiSetupButtonBar->pressTask) {
                    break;
                }
                templ = gWifiSetupButtonBarTemplates[gWifiSetupButtonBar->type][i];
                oam = WifiSetupOamBlock_GetOam(gWifiSetupButtonBar->buttons[i], 0);
                WifiSetupOamTemplate_Copy(WIFI_SETUP_SCREEN_MAIN, templ + 1, oam);
                templ = gWifiSetupButtonBarPositions[gWifiSetupButtonBar->type][i] * 2;
                WifiSetupOamBlock_SetPosition(gWifiSetupButtonBar->buttons[i], -1, gWifiSetupButtonBarButtonX[templ], gWifiSetupButtonBarButtonY[templ]);
                WifiSetupOamBlock_SetPriority(gWifiSetupButtonBar->buttons[i], -1, 1);
                gWifiSetupButtonBar->pressTask = WifiSetupTask_Create(WIFI_SETUP_TASK_QUEUE_MAIN, WifiSetupButtonBar_PressTask, NULL, 110);
                gWifiSetupButtonBar->selected = i;
                return;
            }
        }
    }
    gWifiSetupButtonBar->selected = WIFI_SETUP_SELECTED_NONE;
}

static void WifiSetupButtonBar_SlideOutTask(WifiSetupTask *task, void *arg) {
    int x, y;
    WifiSetupOamBlock_GetPosition(gWifiSetupButtonBar->bar, 0, &x, &y);
    y += 4;
    WifiSetupButtonBar_SetY(y);
    if (y < GX_LCD_SIZE_Y) {
        return;
    }
    WifiSetupTask_SetFunc(task, WifiSetupButtonBar_DestroyTask);
}

static void WifiSetupButtonBar_DestroyTask(WifiSetupTask *task, void *arg) {
    int i;
    WifiSetupTask_DestroyLater(WIFI_SETUP_TASK_QUEUE_MAIN, task);
    if (gWifiSetupButtonBar->pressTask) {
        WifiSetupTask_DestroyLater(WIFI_SETUP_TASK_QUEUE_MAIN, gWifiSetupButtonBar->pressTask);
    }
    for (i = 0; i < 2; i++) {
        if (gWifiSetupButtonBar->buttons[i]) {
            WifiSetupOamBlock_Free(gWifiSetupButtonBar->buttons[i]);
        }
    }
    WifiSetupOamBlock_Free(gWifiSetupButtonBar->bar);
    WifiSetupHeap_FreeAndClear(&gWifiSetupMessageShown - 4);
}

// Restores the pressed button's normal graphics after 16 frames.
static void WifiSetupButtonBar_PressTask(WifiSetupTask *task, void *arg) {
    int count, i;
    gWifiSetupButtonBar->timer++;
    if (gWifiSetupButtonBar->timer < 16) {
        return;
    }
    count = gWifiSetupButtonBarButtonCounts[gWifiSetupButtonBar->type];
    for (i = 0; i < count; i++) {
        int templ = gWifiSetupButtonBarTemplates[gWifiSetupButtonBar->type][i];
        GXOamAttr *oam = WifiSetupOamBlock_GetOam(gWifiSetupButtonBar->buttons[i], 0);
        WifiSetupOamTemplate_Copy(WIFI_SETUP_SCREEN_MAIN, templ, oam);
        WifiSetupOamBlock_SetPriority(gWifiSetupButtonBar->buttons[i], -1, 1);
    }
    WifiSetupButtonBar_SetY(168);
    gWifiSetupButtonBar->timer = 0;
    gWifiSetupButtonBar->selected = WIFI_SETUP_SELECTED_NONE;
    if (!gWifiSetupButtonBar->pressTask) {
        return;
    }
    gWifiSetupButtonBar->pressTask = NULL;
    WifiSetupTask_DestroyLater(WIFI_SETUP_TASK_QUEUE_MAIN, task);
}

void WifiSetupMessage_Reset(void) {
    gWifiSetupMessageShown = 0;
}

// Shows message index of the message bank, with digit written at character pos when pos >= 0, in the window at
// the top of the top screen.
BOOL WifiSetupMessage_Show(int index, int pos, int digit) {
    WifiSetupCanvas *canvas;
    u16 *text;
    u32 flags;
    if (gWifiSetupMessageShown) {
        return FALSE;
    }
    WifiSetup_LoadGraphics(sStr_CharJtNullNscL, GXS_LoadBG0Scr);
    G2S_SetBG0Offset(0, 402);
    canvas = WifiSetupText_InitBgCanvas(WIFI_SETUP_SCREEN_SUB, 0);
    text = WifiSetupText_GetStringWithDigit(gWifiSetupTextBank, index, pos, digit);
    flags = WifiSetup_GetTextFlags();
    WifiSetupText_DrawInRect(canvas, 20, 0, 216, 64, 2, flags, text);
    WifiSetupText_MarkDirty(canvas);
    gWifiSetupMessageShown = 1;
    return TRUE;
}

BOOL WifiSetupMessage_Hide(void) {
    if (!gWifiSetupMessageShown) {
        return FALSE;
    }
    WifiSetupText_FreeBgCanvas(WIFI_SETUP_SCREEN_SUB);
    gWifiSetupMessageShown = 0;
    return TRUE;
}

// Language variant of a file name: character 5 is replaced by the language letter, unless it is 'x'
// (shared) or 'y' (shared except for language 0).
char *WifiSetup_LocalizePath(char *path) {
    int language;
    strncpy(gWifiSetupLocalizedPath, path, 63);
    if (((u8 *)path)[5] == 'x') {
        return gWifiSetupLocalizedPath;
    }
    language = WifiSetupApp_GetLanguage();
    if (((u8 *)path)[5] == 'y' && language != 0) {
        return gWifiSetupLocalizedPath;
    }
    gWifiSetupLocalizedPath[5] = gWifiSetupLanguageLetters[language];
    return gWifiSetupLocalizedPath;
}

void WifiSetup_LoadGraphics(char *path, void (*load)(const void *, u32, u32)) {
    u32 size;
    void *data;
    path = WifiSetup_LocalizePath(path);
    data = WifiSetupFile_Load(path, &size, 4);
    DC_FlushRange(data, size);
    load(data, 0, size);
    WifiSetupFile_Free(data);
}

void WifiSetupBanner_Show(int kind) {
    enum WifiSetupBannerMode mode;
    WifiSetupApp_GetTransition(&mode, NULL);
    if (mode == WIFI_SETUP_BANNER_OPEN) {
        WifiSetupBanner_Open(kind);
    } else if (mode == WIFI_SETUP_BANNER_CHANGE) {
        WifiSetupBanner_SetKind(kind);
    }
}

void WifiSetupButtonBar_Show(int type) {
    enum WifiSetupButtonBarMode mode;
    WifiSetupApp_GetTransition(NULL, &mode);
    if (mode == WIFI_SETUP_BUTTON_BAR_CREATE) {
        WifiSetupButtonBar_Create(type);
    }
}

void WifiSetup_PrintMessage(int message) {
    WifiSetupCanvas *canvas;
    u16 *text;
    u32 flags;
    canvas = WifiSetupText_InitBgCanvas(WIFI_SETUP_SCREEN_MAIN, 0);
    text = WifiSetupText_GetString(gWifiSetupTextBank, message);
    flags = WifiSetup_GetTextFlags();
    WifiSetupText_DrawInRect(canvas, 13, 40, 230, 112, 2, flags, text);
    WifiSetupText_MarkDirty(canvas);
}

void WifiSetup_PrintMessageWithText(const u16 *str, int message) {
    WifiSetupCanvas *canvas;
    u16 *text;
    u32 flags;
    int x, y;
    canvas = WifiSetupText_InitBgCanvas(WIFI_SETUP_SCREEN_MAIN, 0);
    text = WifiSetupText_GetString(gWifiSetupTextBank, message);
    flags = WifiSetup_GetTextFlags();
    WifiSetupText_DrawInRect(canvas, 13, 60, 230, 94, 2, flags, text);
    x = WifiSetupApp_GetLanguage();
    y = WifiSetupApp_GetLanguage();
    WifiSetupText_Draw(canvas, gWifiSetupMessageTextX[x * 2], gWifiSetupMessageTextY[y * 2], 2, 521, str);
    WifiSetupText_MarkDirty(canvas);
}

u32 WifiSetup_GetTextFlags(void) {
    return gWifiSetupTextFlagsByLanguage[WifiSetupApp_GetLanguage()];
}

void WifiSetupCursor_Init(void) {
    gWifiSetupCursorBlocks = WifiSetupHeap_AllocZeroed(16, 4);
}

void WifiSetupCursor_Free(void) {
    WifiSetupCursor_Hide();
    WifiSetupHeap_FreeAndClear(&gWifiSetupCursorBlocks);
}

// Two arrows (kind selects the template pair) at (x1, y) and (x2, y).
void WifiSetupCursor_ShowPair(int kind, int x1, int x2, int y) {
    int i;
    u8 *templ;
    WifiSetupCursor_Hide();
    templ = gWifiSetupCursorPairTemplates + kind * 2;
    for (i = 0; i < 2; i++) {
        gWifiSetupCursorBlocks[i] = WifiSetupOamTemplate_AllocBlock(WIFI_SETUP_SCREEN_MAIN, *templ++, TRUE);
        WifiSetupOamBlock_SetPriority(gWifiSetupCursorBlocks[i], -1, 1);
    }
    WifiSetupOamBlock_SetPosition(gWifiSetupCursorBlocks[0], -1, x1, y);
    WifiSetupOamBlock_SetPosition(gWifiSetupCursorBlocks[1], -1, x2, y);
}

// Four corner markers around the rectangle.
void WifiSetupCursor_ShowCorners(int left, int right, int top, int bottom) {
    int templ = 6, i;
    WifiSetupCursor_Hide();
    for (i = 0; i < 4; i++, templ++) {
        gWifiSetupCursorBlocks[i] = WifiSetupOamTemplate_AllocBlock(WIFI_SETUP_SCREEN_MAIN, templ, TRUE);
        WifiSetupOamBlock_SetPriority(gWifiSetupCursorBlocks[i], -1, 1);
    }
    WifiSetupOamBlock_SetPosition(gWifiSetupCursorBlocks[0], -1, left, top);
    WifiSetupOamBlock_SetPosition(gWifiSetupCursorBlocks[1], -1, right, top);
    WifiSetupOamBlock_SetPosition(gWifiSetupCursorBlocks[2], -1, left, bottom);
    WifiSetupOamBlock_SetPosition(gWifiSetupCursorBlocks[3], -1, right, bottom);
}

void WifiSetupCursor_Hide(void) {
    int i;
    for (i = 0; i < 4; i++) {
        if (gWifiSetupCursorBlocks[i]) {
            WifiSetupOamBlock_Free(gWifiSetupCursorBlocks[i]);
            gWifiSetupCursorBlocks[i] = NULL;
        }
    }
}

void WifiSetupBackground_Init(void) {
    gWifiSetupBackgroundScreen = WifiSetupHeap_AllocZeroed(192, 4);
    WifiSetup_LoadGraphics(sStr_CharJbBgHlNcgL, GX_LoadBG1Char);
    switch (WifiSetupApp_GetKind()) {
    case WIFI_SETUP_KIND_TOP_MENU:
        WifiSetup_LoadGraphics(gWifiSetupBackgroundScreenPaths[WIFI_SETUP_KIND_TOP_MENU], GX_LoadBG1Scr);
        break;
    case WIFI_SETUP_KIND_CONNECTION_LIST:
        WifiSetup_LoadGraphics(gWifiSetupBackgroundScreenPaths[WIFI_SETUP_KIND_CONNECTION_LIST], GX_LoadBG1Scr);
        break;
    }
}

void WifiSetupBackground_Free(void) {
    WifiSetupHeap_FreeAndClear(&gWifiSetupBackgroundScreen);
}

// Replaces the first 3 rows of the BG1 screen at the next vblank.
void WifiSetupBackground_LoadHeader(int index) {
    void *data;
    data = WifiSetupFile_Load(WifiSetup_LocalizePath(gWifiSetupBackgroundScreenPaths[index]), NULL, 4);
    MIi_CpuCopyFast(data, gWifiSetupBackgroundScreen, 192);
    WifiSetupFile_Free(data);
    WifiSetupTask_Create(WIFI_SETUP_TASK_QUEUE_VBLANK, WifiSetupBackground_TransferTask, NULL, 120);
}

void WifiSetupBackground_LoadSlotHeader(void) {
    WifiSetupBackground_LoadHeader(WifiSetupConnections_GetEdit()->index + 5);
}

void WifiSetupBackground_LoadSlotListHeader(void) {
    WifiSetupBackground_LoadHeader(WifiSetupConnections_GetEdit()->index + 2);
}

static void WifiSetupBackground_TransferTask(WifiSetupTask *task, void *arg) {
    DC_FlushRange(gWifiSetupBackgroundScreen, 192);
    GX_LoadBG1Scr(gWifiSetupBackgroundScreen, 0, 192);
    WifiSetupTask_DestroyLater(WIFI_SETUP_TASK_QUEUE_VBLANK, task);
}

void WifiSetupSignalIcon_Create(int type) {
    int templ;
    if (gWifiSetupSignalIcon) {
        return;
    }
    {
        WifiSetupSignalIcon *icon = WifiSetupHeap_AllocZeroed(sizeof(WifiSetupSignalIcon), 4);
        templ = gWifiSetupSignalIconTemplates[type][0];
        gWifiSetupSignalIcon = icon;
        icon->type = type;
    }
    gWifiSetupSignalIcon->oam = WifiSetupOamTemplate_AllocOam(WIFI_SETUP_SCREEN_MAIN, templ);
    gWifiSetupSignalIcon->oam->attr01 = (gWifiSetupSignalIcon->oam->attr01 & 0xfe00ff00) | 0x26 | 0xe50000;
    gWifiSetupSignalIcon->oam->attr2 = (gWifiSetupSignalIcon->oam->attr2 & ~0xc00) | 0x800;
    gWifiSetupSignalIcon->task = WifiSetupTask_Create(WIFI_SETUP_TASK_QUEUE_MAIN, WifiSetupSignalIcon_Task, NULL, 120);
}

WifiSetupSignalIcon *gWifiSetupSignalIcon;

void WifiSetupSignalIcon_Destroy(void) {
    if (gWifiSetupSignalIcon == NULL) {
        return;
    }
    WifiSetupTask_Destroy(WIFI_SETUP_TASK_QUEUE_MAIN, gWifiSetupSignalIcon->task);
    WifiSetupOam_Release(gWifiSetupSignalIcon->oam);
    WifiSetupHeap_FreeAndClear(&gWifiSetupSignalIcon);
}

// Shows the current link level (0 when no channel is allowed) at (229, 38).
static void WifiSetupSignalIcon_Task(WifiSetupTask *task, void *arg) {
    int level = 0;
    GXOamAttr *oam;
    BOOL enabled = OS_DisableIrq();
    if (WM_GetAllowedChannel() != 0x8000) {
        level = WM_GetLinkLevel();
    }
    OS_RestoreIrq(enabled);
    WifiSetupOamTemplate_Copy(WIFI_SETUP_SCREEN_MAIN, gWifiSetupSignalIconTemplates[gWifiSetupSignalIcon->type][level], gWifiSetupSignalIcon->oam);
    oam = gWifiSetupSignalIcon->oam;
    oam->attr01 = (oam->attr01 & 0xfe00ff00) | 0xe50026;
    oam = gWifiSetupSignalIcon->oam;
    oam->attr2 = (oam->attr2 & ~0xc00) | 0x800;
}

void WifiSetupKeyboard_Create(void) {
    WifiSetupTextBox box = { 0, 0, 0, 0 };
    u32 numObjs;

    int offset;
    u16 label[2];
    u8 *templ;
    int i, j, k;
    box.width = 17;
    box.height = 18;
    gWifiSetupKeyboard = WifiSetupHeap_AllocZeroed(sizeof(WifiSetupKeyboard), 4);
    gWifiSetupKeyboard->input = WIFI_SETUP_KEYBOARD_INPUT_BUSY;
    gWifiSetupKeyboard->cursorKey = 0;
    gWifiSetupKeyboard->canDelete = TRUE;
    gWifiSetupKeyboard->canType = TRUE;
    for (i = 0; i < 47; i++) {
        gWifiSetupKeyboard->keys[i] = WifiSetupOamTemplate_AllocOam(WIFI_SETUP_SCREEN_MAIN, 52);
        gWifiSetupKeyboard->keys[i]->attr01 = (gWifiSetupKeyboard->keys[i]->attr01 & 0xc1fffcff) | 512;
        gWifiSetupKeyboard->keys[i]->attr2 = (gWifiSetupKeyboard->keys[i]->attr2 & ~0xc00) | 0xc00;
    }
    templ = gWifiSetupKeyboardExtraKeyTemplates;
    for (i = 0; i < 4; i++) {
        gWifiSetupKeyboard->extraKeys[i] = WifiSetupOamTemplate_AllocOam(WIFI_SETUP_SCREEN_MAIN, *templ++);
        gWifiSetupKeyboard->extraKeys[i]->attr01 = (gWifiSetupKeyboard->extraKeys[i]->attr01 & 0xc1fffcff) | 512;
        gWifiSetupKeyboard->extraKeys[i]->attr2 = (gWifiSetupKeyboard->extraKeys[i]->attr2 & ~0xc00) | 0xc00;
    }
    {
        u8 *buttonTempl = gWifiSetupKeyboardButtonTemplates;
        for (i = 0; i < 2; i++) {
            gWifiSetupKeyboard->buttons[i] = WifiSetupOamTemplate_AllocBlock(WIFI_SETUP_SCREEN_MAIN, *buttonTempl++, TRUE);
            WifiSetupOamBlock_SetAffineMode(gWifiSetupKeyboard->buttons[i], -1, 512, 0);
            WifiSetupOamBlock_SetPriority(gWifiSetupKeyboard->buttons[i], -1, 3);
        }
    }
    i = 0;
    label[1] = i;
    for (; i < 3; i++) {
        offset = 0;
        for (j = 0; j < 4; j++) {
            gWifiSetupKeyboard->rows[i][j] = WifiSetupText_AllocObjCanvas(WIFI_SETUP_SCREEN_MAIN, 28, 2, FALSE, &numObjs, 0);
            box.x = 0;
            {
                int index = 0;
                for (k = offset; index < 12;) {
                    label[0] = gWifiSetupKeyboardPageLabels[i][k];
                    WifiSetupText_DrawInRect(gWifiSetupKeyboard->rows[i][j], box.x, box.y, box.width, box.height, 2, 1152, label);
                    index++;
                    box.x += 18;
                    k++;
                }
            }
            if (i == 0) {
                gWifiSetupKeyboard->rowBlocks[j] = WifiSetupOamBlock_Alloc(WIFI_SETUP_SCREEN_MAIN, numObjs, TRUE);
            }
            offset += 12;
        }
    }
    gWifiSetupKeyboard->cursor = WifiSetupOamTemplate_AllocBlock(WIFI_SETUP_SCREEN_MAIN, 64, TRUE);
    WifiSetupOamBlock_SetAffineMode(gWifiSetupKeyboard->cursor, -1, 512, 0);
    WifiSetupOamBlock_SetPriority(gWifiSetupKeyboard->cursor, -1, 2);
    gWifiSetupKeyboard->task = WifiSetupTask_Create(WIFI_SETUP_TASK_QUEUE_MAIN, WifiSetupKeyboard_SlideInRow0Task, NULL, 120);
    WifiSetupKeyboard_SetRowY(gWifiSetupKeyboard->page, 0, 192);
}

void WifiSetupKeyboard_Close(void) {
    WifiSetupOamBlock_Free(gWifiSetupKeyboard->cursor);
    WifiSetupTask_SetFunc(gWifiSetupKeyboard->task, WifiSetupKeyboard_SlideOutButtonsTask);
}

int WifiSetupKeyboard_GetInput(void) {
    return gWifiSetupKeyboard->input;
}

void WifiSetupKeyboard_SetCanDelete(BOOL canDelete) {
    gWifiSetupKeyboard->canDelete = canDelete;
}

void WifiSetupKeyboard_SetCanType(BOOL canType) {
    gWifiSetupKeyboard->canType = canType;
}

BOOL WifiSetupKeyboard_Exists(void) {
    return gWifiSetupKeyboard != NULL;
}
