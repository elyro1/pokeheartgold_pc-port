#include <stddef.h>

#include "global.h"

#include "constants/items.h"
#include "constants/pokemon.h"
#include "constants/species.h"

#include "application/pc_box/pc_box_internal.h"
#include "msgdata/msg.naix"
#include "msgdata/msg/msg_0024.h"
#include "msgdata/msg/msg_0025.h"

#include "bg_cell_manager.h"
#include "bg_window.h"
#include "dpad_menu_box.h"
#include "font.h"
#include "font_sprite.h"
#include "gf_gfx_loader.h"
#include "gf_gfx_planes.h"
#include "grid_input_handler.h"
#include "heap.h"
#include "item.h"
#include "message_format.h"
#include "message_printer.h"
#include "msgdata.h"
#include "obj_char_transfer.h"
#include "palette.h"
#include "pm_string.h"
#include "pokedex_util.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "render_window.h"
#include "sprite_system.h"
#include "string.h"
#include "text.h"
#include "touchscreen.h"
#include "yes_no_prompt.h"

static void PCBox_PlaceSpriteOnTop(PCBoxView *view, u32 i, s16 x, s16 y, u32 visible);
static void PCBox_CopyWindowToTextObj(PCBoxView *view, Window *window, u32 index);
static void PCBox_PrintAligned(Window *window, String *str, s32 x, s32 y, u32 font, u32 color, u32 align);
static void PCBox_PrintMsg(PCBoxView *view, MsgData *msgData, u32 win, u32 msgId, s32 x, s32 y, u32 font, u32 color, u32 align);
static void PCBox_PrintExpandedMsg(PCBoxView *view, MsgData *msgData, u32 win, u32 msgId, s32 x, s32 y, u32 font, u32 color, u32 align);
static void PCBox_PrintSpeciesName(PCBoxView *view, PCBoxMonInfo *monInfo, u32 win);
static void PCBox_PrintNickname(PCBoxView *view, PCBoxMonInfo *monInfo, u32 win);
static void PCBox_PrintLevel(PCBoxView *view, PCBoxMonInfo *monInfo, u32 win);
static void PCBox_PrintGender(PCBoxView *view, PCBoxMonInfo *monInfo, u32 win);
static void PCBox_PrintDexNumber(PCBoxWork *work, PCBoxMonInfo *monInfo, u32 win);
static void PCBox_PrintNature(PCBoxView *view, PCBoxMonInfo *monInfo, u32 win);
static void PCBox_PrintAbility(PCBoxView *view, PCBoxMonInfo *monInfo, u32 win);
static void PCBox_PrintHeldItem(PCBoxView *view, PCBoxMonInfo *monInfo, u32 win);
static void PCBox_DrawButtonWindow(PCBoxWork *work, u32 win, u32 msgId, u32 style);
static void PCBox_PrintWindowMessage(PCBoxWork *work, u32 win, u32 msgId, u32 style, u8 align);
static void PCBox_DrawTabWindow(PCBoxWork *work, u32 win, u32 msgId);
static void PCBox_DrawMenuExitButton(PCBoxWork *work, u32 style);
static void PCBox_DrawChangeBoxTitle(PCBoxWork *work);
static void PCBox_SetPanelWindowFrame(BgCellManager *mgr, int id, Window *win);
static void PCBox_AttachWindowToPanel(BgCellManager *mgr, int id, Window *window);
static void PCBox_PrintCenteredLabel(PCBoxView *view, u32 windowIndex, u32 messageId, u32 unused);
static MsgData *PCBox_LoadBoxMessages(void);
static void PCBox_DrawMessageFrame(Window *window);
static void PCBox_PrintMessage(PCBoxWork *work, int msgNo, int windowId);

void PCBox_LoadTrayPageThumbnails(PCBoxWork *work) {
    u32 index;
    u32 offset;
    u32 i;

    offset = work->trayBox / 6 * 6;
    index = 15;
    i = 0;
    offset = offset << 10;
    for (; i < PC_BOX_TRAY_BOX_COUNT; i++) {
        PCBox_LoadSpriteCharsMain(work->view, index, (u8 *)work->view->boxThumbnails + offset, 0x400);
        index++;
        offset += 0x400;
    }
}

void PCBox_RefreshTrayBoxThumbnail(PCBoxWork *work, u32 box) {
    if (work->trayBox / 6 == box / 6) {
        PCBox_LoadSpriteCharsMain(work->view, box % 6 + 15, work->view->boxThumbnails[box], 0x400);
    }
}

#if defined(SDK_PORT)
#else
typedef union BoxTilePtrSlot {
    void *ptr;
    u32 raw;
} BoxTilePtrSlot;

#endif
void PCBox_DrawBoxThumbnailMons(PCBoxWork *work, u32 box, void *dst) {
#if defined(SDK_PORT)
    BoxPokemon *mon;
#else
    BoxTilePtrSlot mon;
#endif
    u32 lock;
    u8 row;
    u8 col;
    u8 x;
    u32 rowbase;
    u32 y;
    u16 tile;
    u32 yy;
    u32 type;
    u32 species;
    u16 form;

    tile = 0xb;
    y = (u8)tile;
    for (row = 0; row < 5; row++) {
        x = 0xa;
        col = 0;
        rowbase = row * 6;
        for (; col < 6; col++) {
#if defined(SDK_PORT)
            mon = PCStorage_GetMonByIndexPair(work->storage, box, col + rowbase);
            lock = AcquireBoxMonLock(mon);
            species = GetBoxMonData(mon, MON_DATA_SPECIES, NULL);
            if (GetBoxMonData(mon, MON_DATA_SPECIES_EXISTS, NULL)) {
                if (!GetBoxMonData(mon, MON_DATA_IS_EGG, NULL)) {
                    form = (u16)GetBoxMonData(mon, MON_DATA_FORM, NULL);
#else
            mon.ptr = PCStorage_GetMonByIndexPair(work->storage, box, col + rowbase);
            lock = AcquireBoxMonLock(mon.ptr);
            species = GetBoxMonData((void *)mon.raw, MON_DATA_SPECIES, NULL);
            if (GetBoxMonData(mon.ptr, MON_DATA_SPECIES_EXISTS, NULL)) {
                if (!GetBoxMonData((void *)mon.raw, MON_DATA_IS_EGG, NULL)) {
                    form = (u16)GetBoxMonData(mon.ptr, MON_DATA_FORM, NULL);
#endif
                    type = (u16)GetMonBaseStatEx_HandleAlternateForm(work->view->personalNarc, species, form, 0x1b);
                } else if (species == SPECIES_MANAPHY) {
                    type = 1;
                } else {
                    type = 8;
                }
                tile = (u16)(gPCBoxThumbnailTiles[type] + 0x20);
                tile = (u16)((tile << 8) | tile);
                for (yy = y; (s32)yy < (s32)(y + 2); yy = (u8)(yy + 1)) {
#if defined(SDK_PORT)
                    MIi_CpuClear16(tile, (u16 *)((u8 *)dst + ((x & 7) + ((yy & 7) * 8 + (((s32)yy >> 3) * 4 + (x >> 3)) * 64))), 2);
#else
                    MIi_CpuClear16(tile, (u16 *)((u8 *)dst + ((x & 7) + ((yy & 7) * 8 + (u32)((u8 *)(((s32)yy >> 3) * 4) + (x >> 3)) * 64))), 2);
#endif
                }
            }
#if defined(SDK_PORT)
            ReleaseBoxMonLock(mon, lock);
#else
            ReleaseBoxMonLock((void *)mon.raw, lock);
#endif
            x += 2;
        }
        y = (u8)(y + 2);
    }
}

static void PCBox_PlaceSpriteOnTop(PCBoxView *view, u32 i, s16 x, s16 y, u32 visible) {
    ManagedSprite_SetPositionXY(view->sprites[i], x, y);
    PCBox_SetSpriteVisible(view, i, visible);
    PCBox_SetSpritePriority(view, i, 0);
}

void PCBox_SetupDepositBoxSprites(PCBoxWork *work) {
    u32 i;
    s16 x;
    s16 y;

    PCBox_PlaceSpriteOnTop(work->view, PC_BOX_SPRITE_TRAY_PREV_ARROW, 12, 84, 1);
    PCBox_PlaceSpriteOnTop(work->view, PC_BOX_SPRITE_TRAY_NEXT_ARROW, 244, 84, 1);
    PCBox_PlaceSpriteOnTop(work->view, PC_BOX_SPRITE_TRAY_MARKER, 43, 84, 1);
    PCBox_PlaceSpriteOnTop(work->view, PC_BOX_SPRITE_TRAY_CURSOR, 128, 65, 1);
    PCBox_PlaceSpriteOnTop(work->view, PC_BOX_SPRITE_TRAY_LABEL, 128, 77, 1);
    for (i = 0; i < PC_BOX_TRAY_BOX_COUNT; i++) {
        ManagedSprite_GetPositionXY(work->view->sprites[PC_BOX_SPRITE_TRAY_BOXES + i], &x, &y);
        PCBox_PlaceSpriteOnTop(work->view, PC_BOX_SPRITE_TRAY_BOXES + i, x, 84, 1);
    }
    PCBox_LoadTrayPageThumbnails(work);
    PCBox_SetSpriteAnim(work->view, PC_BOX_SPRITE_TRAY_CURSOR, 5);
    PCBox_PlaceTrayTextObjs(work->view);
    TextOBJ_SetSpritesDrawFlag(work->view->textObjs[0].textObj, TRUE);
    TextOBJ_SetSpritesDrawFlag(work->view->textObjs[1].textObj, TRUE);
    TextOBJ_SetPriority(work->view->textObjs[0].textObj, 0);
    TextOBJ_SetPriority(work->view->textObjs[1].textObj, 0);
}

void PCBox_HideBoxTraySprites(PCBoxWork *work) {
    u32 i;

    PCBox_SetSpriteVisible(work->view, PC_BOX_SPRITE_TRAY_PREV_ARROW, FALSE);
    PCBox_SetSpriteVisible(work->view, PC_BOX_SPRITE_TRAY_NEXT_ARROW, FALSE);
    PCBox_SetSpriteVisible(work->view, PC_BOX_SPRITE_TRAY_MARKER, FALSE);
    PCBox_SetSpriteVisible(work->view, PC_BOX_SPRITE_TRAY_CURSOR, FALSE);
    PCBox_SetSpriteVisible(work->view, PC_BOX_SPRITE_TRAY_LABEL, FALSE);
    for (i = 0; i < PC_BOX_TRAY_BOX_COUNT; i++) {
        PCBox_SetSpriteVisible(work->view, PC_BOX_SPRITE_TRAY_BOXES + i, FALSE);
    }
    TextOBJ_SetSpritesDrawFlag(work->view->textObjs[0].textObj, FALSE);
    TextOBJ_SetSpritesDrawFlag(work->view->textObjs[1].textObj, FALSE);
}

void PCBox_CreateTrayTextObjs(PCBoxView *view) {
    Window window;
    TextOBJTemplate template;
    PCBoxTextObj *textObj;

    view->fontSystem = FontSystem_NewInit(2, HEAP_ID_10);
    textObj = &view->textObjs[0];
    InitWindow(&window);
    AddTextWindowTopLeftCorner(view->bgConfig, &window, 12, 2, 0, 2);
    ObjCharTransfer_ReserveCharSpace(FontOAM_GetCharSize(&window, NNS_G2D_VRAM_TYPE_2DMAIN, HEAP_ID_10), TRUE, NNS_G2D_VRAM_TYPE_2DMAIN, &textObj->charTransfer);
    template.fontSystem = view->fontSystem;
    template.window = &window;
    template.spriteList = SpriteManager_GetSpriteList(view->spriteManager);
    template.plttResourceProxy = SpriteManager_FindPlttResourceProxy(view->spriteManager, 0xc101);
    template.sprite = view->sprites[PC_BOX_SPRITE_TRAY_CURSOR]->sprite;
    template.offset = textObj->charTransfer.offset;
    template.x = 128;
    template.y = -28;
    template.bgPriority = 1;
    template.drawPriority = 4;
    template.vram = NNS_G2D_VRAM_TYPE_2DMAIN;
    template.heapID = HEAP_ID_10;
    view->textObjs[0].textObj = FontOAM_Create(&template);
    TextOBJ_SetPalOffset(view->textObjs[0].textObj, 1);
    TextOBJ_SetPalOffsetRespectVramOffset(view->textObjs[0].textObj, 1);
    RemoveWindow(&window);

    textObj = &view->textObjs[1];
    InitWindow(&window);
    AddTextWindowTopLeftCorner(view->bgConfig, &window, 5, 2, 0, 2);
    ObjCharTransfer_ReserveCharSpace(FontOAM_GetCharSize(&window, NNS_G2D_VRAM_TYPE_2DMAIN, HEAP_ID_10), TRUE, NNS_G2D_VRAM_TYPE_2DMAIN, &textObj->charTransfer);
    template.fontSystem = view->fontSystem;
    template.window = &window;
    template.spriteList = SpriteManager_GetSpriteList(view->spriteManager);
    template.plttResourceProxy = SpriteManager_FindPlttResourceProxy(view->spriteManager, 0xc101);
    template.sprite = view->sprites[PC_BOX_SPRITE_TRAY_CURSOR]->sprite;
    template.offset = textObj->charTransfer.offset;
    template.x = 128;
    template.y = -28;
    template.bgPriority = 1;
    template.drawPriority = 4;
    template.vram = NNS_G2D_VRAM_TYPE_2DMAIN;
    template.heapID = HEAP_ID_10;
    textObj->textObj = FontOAM_Create(&template);
    TextOBJ_SetPalOffset(textObj->textObj, 1);
    TextOBJ_SetPalOffsetRespectVramOffset(textObj->textObj, 1);
    RemoveWindow(&window);
}

void PCBox_DestroyTrayTextObjs(PCBoxView *view) {
    u32 i;
    u8 *charTransfer;
    u8 *slot;

    i = 0;
    charTransfer = (u8 *)&view->textObjs[0].charTransfer;
    slot = (u8 *)view;
    for (; i < 2; i++) {
        ObjCharTransfer_FreeCharSpace((ObjCharSpaceReservation *)charTransfer);
        FontOAM_Delete(*(TextOBJ **)(slot + offsetof(PCBoxView, textObjs[0].textObj)));
        charTransfer += sizeof(PCBoxTextObj);
        slot += sizeof(PCBoxTextObj);
    }
    FontSystem_Delete(view->fontSystem);
}

static void PCBox_CopyWindowToTextObj(PCBoxView *view, Window *window, u32 index) {
    TextOBJLayout *layout;

    layout = TextOBJLayout_New(window, HEAP_ID_10);
    TextOBJ_CopyFromBGWindow(view->textObjs[index].textObj, layout, window, HEAP_ID_10);
    TextOBJLayout_Delete(layout);
}

void PCBox_AddWindows(PCBoxWork *work) {
    u32 i;

    FontID_Alloc(4, HEAP_ID_10);
    for (i = 0; i < 44; i++) {
        AddWindow(work->view->bgConfig, &work->view->windows[i], &gPCBoxWindowTemplates[i]);
    }
}

void PCBox_RemoveWindows(PCBoxWork *work) {
    u16 i;

    for (i = 0; i < 0x2c; i++) {
        RemoveWindow(&work->view->windows[i]);
    }
    FontID_Release(4);
}

static void PCBox_PrintAligned(Window *window, String *str, s32 x, s32 y, u32 font, u32 color, u32 align) {
    if (align == 1) {
        x -= FontID_String_GetWidth(font, str, 0);
    } else if (align == 2) {
        x -= FontID_String_GetWidth(font, str, 0) / 2;
    } else if (align == 3) {
        x -= FontID_String_GetWidthMultiline(font, str, 0) / 2;
    }
    ((u8(*)(Window *, u32, String *, u32, u32, u32, u32, PrinterCallback_t))AddTextPrinterParameterizedWithColor)(window, font, str, x, y, TEXT_SPEED_NOTRANSFER, color, NULL);
}

static void PCBox_PrintMsg(PCBoxView *view, MsgData *msgData, u32 win, u32 msgId, s32 x, s32 y, u32 font, u32 color, u32 align) {
    String *str;

    str = NewString_ReadMsgData(msgData, msgId);
    PCBox_PrintAligned(&view->windows[win], str, x, y, font, color, align);
    String_Delete(str);
}

static void PCBox_PrintExpandedMsg(PCBoxView *view, MsgData *msgData, u32 win, u32 msgId, s32 x, s32 y, u32 font, u32 color, u32 align) {
    String *str;

    str = NewString_ReadMsgData(msgData, msgId);
    StringExpandPlaceholders(view->msgFormat, view->string, str);
    PCBox_PrintAligned(&view->windows[win], view->string, x, y, font, color, align);
    String_Delete(str);
}

static void PCBox_PrintSpeciesName(PCBoxView *view, PCBoxMonInfo *monInfo, u32 win) {
    FillWindowPixelBuffer(&view->windows[win], 0);
    if (!monInfo->isEgg) {
        BufferBoxMonSpeciesName(view->msgFormat, 0, monInfo->boxMon);
        PCBox_PrintExpandedMsg(view, view->msgData, win, msg_0024_00000, 0, 0, 0, 0x10200, 0);
    }
    ScheduleWindowCopyToVram(&view->windows[win]);
}

static void PCBox_PrintNickname(PCBoxView *view, PCBoxMonInfo *monInfo, u32 win) {
    FillWindowPixelBuffer(&view->windows[win], 0);
    BufferBoxMonNickname(view->msgFormat, 0, monInfo->boxMon);
    PCBox_PrintExpandedMsg(view, view->msgData, win, msg_0024_00001, 0, 0, 0, 0x10200, 0);
    ScheduleWindowCopyToVram(&view->windows[win]);
}

static void PCBox_PrintLevel(PCBoxView *view, PCBoxMonInfo *monInfo, u32 win) {
    FillWindowPixelBuffer(&view->windows[win], 0);
    if (!monInfo->isEgg) {
        MessagePrinter_PrintGlyph(view->msgPrinter, 1, &view->windows[win], 0, 5);
        BufferIntegerAsString(view->msgFormat, 0, monInfo->level, 3, PRINTING_MODE_LEFT_ALIGN, TRUE);
        PCBox_PrintExpandedMsg(view, view->msgData, win, msg_0024_00090, 0x10, 0, 0, 0x10200, 0);
    }
    ScheduleWindowCopyToVram(&view->windows[win]);
}

static void PCBox_PrintGender(PCBoxView *view, PCBoxMonInfo *monInfo, u32 win) {
    FillWindowPixelBuffer(&view->windows[win], 0);
    if (!monInfo->isEgg) {
        if (monInfo->showGender == 1) {
            if (monInfo->gender == 0) {
                PCBox_PrintMsg(view, view->msgData, win, msg_0024_00082, 0, 0, 0, 0x70800, 0);
            } else if (monInfo->gender == 1) {
                PCBox_PrintMsg(view, view->msgData, win, msg_0024_00083, 0, 0, 0, 0x30400, 0);
            }
        }
    }
    ScheduleWindowCopyToVram(&view->windows[win]);
}

static void PCBox_PrintDexNumber(PCBoxWork *work, PCBoxMonInfo *monInfo, u32 win) {
    u32 dexno;

    FillWindowPixelBuffer(&work->view->windows[win], 0);
    if (!monInfo->isEgg) {
        dexno = Pokedex_ConvertToCurrentDexNo(SaveArray_IsNatDexEnabled(work->args->saveData), monInfo->species);
        if (dexno) {
            MessagePrinter_PrintGlyph(work->view->msgPrinter, 2, &work->view->windows[win], 0, 5);
            BufferIntegerAsString(work->view->msgFormat, 0, dexno, 3, PRINTING_MODE_LEADING_ZEROS, TRUE);
            PCBox_PrintExpandedMsg(work->view, work->view->msgData, win, msg_0024_00091, 0x10, 0, 0, 0x10200, 0);
        }
    }
    ScheduleWindowCopyToVram(&work->view->windows[win]);
}

static void PCBox_PrintNature(PCBoxView *view, PCBoxMonInfo *monInfo, u32 win) {
    FillWindowPixelBuffer(&view->windows[win], 0);
    if (!monInfo->isEgg) {
        BufferNatureName(view->msgFormat, 0, monInfo->nature);
        PCBox_PrintExpandedMsg(view, view->msgData, win, msg_0024_00085, 0, 0, 0, 0x10200, 0);
    } else {
        PCBox_PrintMsg(view, view->msgData, win, msg_0024_00093, 0, 0, 0, 0x10200, 0);
    }
    ScheduleWindowCopyToVram(&view->windows[win]);
}

static void PCBox_PrintAbility(PCBoxView *view, PCBoxMonInfo *monInfo, u32 win) {
    FillWindowPixelBuffer(&view->windows[win], 0);
    if (!monInfo->isEgg) {
        BufferAbilityName(view->msgFormat, 0, monInfo->ability);
        PCBox_PrintExpandedMsg(view, view->msgData, win, msg_0024_00084, 0, 0, 0, 0x10200, 0);
    } else {
        PCBox_PrintMsg(view, view->msgData, win, msg_0024_00093, 0, 0, 0, 0x10200, 0);
    }
    ScheduleWindowCopyToVram(&view->windows[win]);
}

static void PCBox_PrintHeldItem(PCBoxView *view, PCBoxMonInfo *monInfo, u32 win) {
    FillWindowPixelBuffer(&view->windows[win], 0);
    if (monInfo->heldItem) {
        BufferItemName(view->msgFormat, 0, monInfo->heldItem);
        PCBox_PrintExpandedMsg(view, view->msgData, win, msg_0024_00086, 0, 0, 0, 0x10200, 0);
    } else {
        PCBox_PrintMsg(view, view->msgData, win, msg_0024_00092, 0, 0, 0, 0x10200, 0);
    }
    ScheduleWindowCopyToVram(&view->windows[win]);
}

void PCBox_PrintMonInfo(PCBoxWork *work, PCBoxMonInfo *monInfo) {
    PCBox_PrintSpeciesName(work->view, monInfo, 0);
    PCBox_PrintNickname(work->view, monInfo, 1);
    PCBox_PrintLevel(work->view, monInfo, 2);
    PCBox_PrintGender(work->view, monInfo, 4);
    PCBox_PrintDexNumber(work, monInfo, 5);
    PCBox_PrintNature(work->view, monInfo, 6);
    PCBox_PrintAbility(work->view, monInfo, 7);
    PCBox_PrintHeldItem(work->view, monInfo, 8);
}

void PCBox_ClearMonInfoWindows(PCBoxView *view) {
    ClearWindowTilemapAndScheduleTransfer(&view->windows[0]);
    ClearWindowTilemapAndScheduleTransfer(&view->windows[1]);
    ClearWindowTilemapAndScheduleTransfer(&view->windows[2]);
    ClearWindowTilemapAndScheduleTransfer(&view->windows[4]);
    ClearWindowTilemapAndScheduleTransfer(&view->windows[5]);
    ClearWindowTilemapAndScheduleTransfer(&view->windows[6]);
    ClearWindowTilemapAndScheduleTransfer(&view->windows[7]);
    ClearWindowTilemapAndScheduleTransfer(&view->windows[8]);
}

#pragma opt_common_subs off

// Draws the four moves of the mon into the move windows of the back buffer (9-12 or 13-16) and returns the
// first window id.
int PCBox_DrawMoveWindows(PCBoxWork *work, PCBoxMonInfo *monInfo) {
    NNSG2dCharacterData *charData;
    const u8 *second;
    void *allocation, *pixels;
    MsgData *msgData;
    u16 i, j;
    int index, start;
    PCBoxView *view = work->view;
    u8 next;

    if (view->infoWindowToggle == 0) {
        start = 9;
    } else {
        start = 13;
    }
    next = view->infoWindowToggle ^ 1;
    view->infoWindowToggle = next;
    allocation = GfGfxLoader_GetCharData(NARC_a_0_1_9, 6, TRUE, &charData, HEAP_ID_10);
    pixels = charData->pRawData;
    msgData = NewMsgDataFromNarc(MSGDATA_LOAD_DIRECT, NARC_msgdata_msg, NARC_msg_msg_0750_bin, HEAP_ID_10);
    if (!monInfo->isEgg) {
        for (i = 0; i < 4; i++) {
            index = start + i;
            second = pixels;
            second += 32;
            for (j = 0; j < 11; j++) {
                int x = j * 8;
                BlitBitmapRect(&work->view->windows[index], (u8 *)pixels + 0x160, 0, 0, 8, 8, x, 0, 8, 8, 255);
                BlitBitmapRect(&work->view->windows[index], (void *)second, 0, 0, 8, 8, x, 8, 8, 8, 255);
            }
            PCBox_PrintMsg(work->view, msgData, index, monInfo->moves[i], 0, 0, 0, 0x10200, 0);
            CopyWindowPixelsToVram_TextMode(&work->view->windows[index]);
        }
        work->view->movesDrawn = 1;
    } else {
        work->view->movesDrawn = 0;
    }
    DestroyMsgData(msgData);
    Heap_Free(allocation);
    return start;
}

#pragma opt_common_subs reset

// Prints the name and description of the held item into the back buffer windows (20-21 or 22-23).
u32 PCBox_PrintItemNameAndDesc(PCBoxWork *work, u16 itemId) {
    u32 windowId;
    Window *window;
    PCBoxView *view = work->view;

    if (view->infoWindowToggle == 0) {
        windowId = 20;
    } else {
        windowId = 22;
    }
    window = &view->windows[windowId];
    view->infoWindowToggle ^= 1;
    FillWindowPixelBuffer(window, 13);
    FillWindowPixelBuffer(window + 1, 13);
    if (itemId != ITEM_NONE) {
        GetItemNameIntoString(work->view->string, itemId, HEAP_ID_10);
        PCBox_PrintAligned(window, work->view->string, 0, 0, 0, 0x10200, 0);
        CopyWindowPixelsToVram_TextMode(window);
        GetItemDescIntoString(work->view->string, itemId, HEAP_ID_10);
        PCBox_PrintAligned(window + 1, work->view->string, 0, 0, 0, 0x10200, 0);
        CopyWindowPixelsToVram_TextMode(window + 1);
    }
    return windowId;
}

void PCBox_PrintModeTitle(PCBoxWork *work) {
    MsgData *msg;

    msg = PCBox_LoadBoxMessages();
    FillWindowPixelBuffer(&work->view->windows[3], 0);
    PCBox_PrintMsg(work->view, msg, 3, work->args->mode + 0x32, 0, 8, 0, 0x10200, 0);
    DestroyMsgData(msg);
    CopyWindowToVram(&work->view->windows[3]);
}

void PCBox_PrintInfoLabels(PCBoxWork *work) {
    FillWindowPixelBuffer(&work->view->windows[0x11], 0);
    FillWindowPixelBuffer(&work->view->windows[0x12], 0);
    FillWindowPixelBuffer(&work->view->windows[0x13], 0);
    PCBox_PrintMsg(work->view, work->view->msgData, 0x11, msg_0024_00087, 0, 0, 0, 0x10200, 0);
    PCBox_PrintMsg(work->view, work->view->msgData, 0x12, msg_0024_00088, 0, 0, 0, 0x10200, 0);
    PCBox_PrintMsg(work->view, work->view->msgData, 0x13, msg_0024_00089, 0, 0, 0, 0x10200, 0);
    CopyWindowToVram(&work->view->windows[0x11]);
    CopyWindowToVram(&work->view->windows[0x12]);
    CopyWindowToVram(&work->view->windows[0x13]);
}

void PCBox_DrawBoxNameOnWallpaper(PCBoxWork *work, void *src, u32 baseTile, u32 width, u32 height) {
    Window *window;
    String *string;

    window = Heap_AllocAtEnd(HEAP_ID_10, sizeof(Window));
    AddWindowParameterized(work->view->bgConfig, window, GF_BG_LYR_MAIN_3, 0, 0, width, height, 0, baseTile);
    MIi_CpuCopy32(src, window->pixelBuffer, width * height * 32);
    string = String_New(20, HEAP_ID_10);
    PCStorage_GetBoxName(work->storage, work->activeBox, string);
    PCBox_PrintAligned(window, string, width * 8 / 2, height * 8 / 2 - 8, 0, 0x20100, 2);
    String_Delete(string);
    CopyWindowPixelsToVram_TextMode(window);
    RemoveWindow(window);
    Heap_Free(window);
}

void PCBox_DrawTrayBoxNameAndCount(PCBoxWork *work) {
    Window window;
    String *str;

    InitWindow(&window);
    AddTextWindowTopLeftCorner(work->view->bgConfig, &window, 0xc, 2, 0, 0);
    str = String_New(0x14, HEAP_ID_10);
    PCStorage_GetBoxName(work->storage, work->trayBox, str);
    PCBox_PrintAligned(&window, str, 0x30, 0, 0, 0x10200, 2);
    String_Delete(str);
    PCBox_CopyWindowToTextObj(work->view, &window, 0);
    RemoveWindow(&window);
    InitWindow(&window);
    AddTextWindowTopLeftCorner(work->view->bgConfig, &window, 5, 2, 0, 0);
    str = NewString_ReadMsgData(work->view->msgData, msg_0024_00024);
    BufferIntegerAsString(work->view->msgFormat, 0, PCStorage_CountMonsAndEggsInBox(work->storage, work->trayBox), 2, PRINTING_MODE_LEFT_ALIGN, TRUE);
    BufferIntegerAsString(work->view->msgFormat, 1, 0x1e, 2, PRINTING_MODE_LEFT_ALIGN, TRUE);
    StringExpandPlaceholders(work->view->msgFormat, work->view->string, str);
    PCBox_PrintAligned(&window, work->view->string, 0x14, 0, 0, 0x10200, 2);
    String_Delete(str);
    PCBox_CopyWindowToTextObj(work->view, &window, 1);
    RemoveWindow(&window);
}

void PCBox_DrawWallpaperName(PCBoxWork *work) {
    Window window;
    String *str;

    InitWindow(&window);
    AddTextWindowTopLeftCorner(work->view->bgConfig, &window, 0xc, 2, 0, 0);
    if (work->view->wallpaperSelection >= 0x10) {
        if (!PCStorage_IsBonusWallpaperUnlocked(work->storage, work->view->wallpaperSelection - 0x10)) {
            str = NewString_ReadMsgData(work->view->msgData, msg_0024_00059);
        } else {
            str = NewString_ReadMsgData(work->view->msgData, work->view->wallpaperSelection + 0x23);
        }
    } else {
        str = NewString_ReadMsgData(work->view->msgData, work->view->wallpaperSelection + 0x23);
    }
    PCBox_PrintAligned(&window, str, 0x30, 0, 0, 0x10200, 2);
    String_Delete(str);
    PCBox_CopyWindowToTextObj(work->view, &window, 0);
    RemoveWindow(&window);
}

static void PCBox_DrawButtonWindow(PCBoxWork *work, u32 win, u32 msgId, u32 style) {
    Window *window = &work->view->windows[win];
    void *buf;
    u32 color;
    u16 offset;
    u8 bg, x, y, width, height;
    NNSG2dCharacterData *chr;
    u8 *pix;
    u8 i;
    String *str;
    bg = GetWindowBgId(window);
    x = GetWindowX(window);
    y = GetWindowY(window);
    width = GetWindowWidth(window);
    height = GetWindowHeight(window);
    if (style == 1) {
        FillWindowPixelBuffer(window, 11);
        color = MAKE_TEXT_COLOR(14, 15, 0);
        offset = 0x180;
    } else {
        FillWindowPixelBuffer(window, 4);
        color = MAKE_TEXT_COLOR(14, 15, 0);
        offset = 0;
    }
    buf = GfGfxLoader_GetCharData(NARC_a_0_1_9, 64, TRUE, &chr, HEAP_ID_10);
    pix = chr->pRawData;
    BlitBitmapRect(window, pix + offset, 0, 0, 8, 8, 0, 0, 8, 8, 0xFF);
    BlitBitmapRect(window, pix + (offset + 0x40), 0, 0, 8, 8, (width - 1) * 8, 0, 8, 8, 0xFF);
    BlitBitmapRect(window, pix + (offset + 0x120), 0, 0, 8, 8, 0, (height - 1) * 8, 8, 8, 0xFF);
    BlitBitmapRect(window, pix + (offset + 0x160), 0, 0, 8, 8, (width - 1) * 8, (height - 1) * 8, 8, 8, 0xFF);
    for (i = 1; i < height - 1; i++) {
        BlitBitmapRect(window, pix + (offset + 0x60), 0, 0, 8, 8, 0, i * 8, 8, 8, 0xFF);
        BlitBitmapRect(window, pix + (offset + 0xA0), 0, 0, 8, 8, (width - 1) * 8, i * 8, 8, 8, 0xFF);
    }
    for (i = 1; i < width - 1; i++) {
        BlitBitmapRect(window, pix + (offset + 0x20), 0, 0, 8, 8, i * 8, 0, 8, 8, 0xFF);
        BlitBitmapRect(window, pix + (offset + 0x140), 0, 0, 8, 8, i * 8, (height - 1) * 8, 8, 8, 0xFF);
    }
    Heap_Free(buf);
    str = NewString_ReadMsgData(work->view->msgData, msgId);
    PCBox_PrintAligned(window, str, width * 8 / 2, 4, 4, color, 2);
    String_Delete(str);
    CopyWindowPixelsToVram_TextMode(window);
}

static void PCBox_PrintWindowMessage(PCBoxWork *work, u32 win, u32 msgId, u32 style, u8 align) {
    Window *window = &work->view->windows[win];
    u8 bg, x, y, width, height;
    String *str;
    u32 color;
    bg = GetWindowBgId(window);
    x = GetWindowX(window);
    y = GetWindowY(window);
    width = GetWindowWidth(window);
    height = GetWindowHeight(window);
    if (style == 1) {
        FillWindowPixelBuffer(window, 11);
        color = MAKE_TEXT_COLOR(14, 15, 0);
    } else {
        FillWindowPixelBuffer(window, 4);
        color = MAKE_TEXT_COLOR(14, 15, 0);
    }
    str = NewString_ReadMsgData(work->view->msgData, msgId);
    if (align == 2 || align == 3) {
        PCBox_PrintAligned(window, str, width * 8 / 2, 0, 4, color, align);
    } else {
        PCBox_PrintAligned(window, str, 0, 0, 4, color, align);
    }
    String_Delete(str);
    CopyWindowPixelsToVram_TextMode(window);
}

void PCBox_DrawPartyButton(PCBoxWork *work, u32 style) {
    PCBox_DrawButtonWindow(work, 0x18, msg_0024_00060, style);
}

static void PCBox_DrawTabWindow(PCBoxWork *work, u32 win, u32 msgId) {
    Window *window = &work->view->windows[win];
    u8 width;
    NNSG2dCharacterData *chr;
    void *buf;
    u8 *pix;
    u8 i;
    String *str;
    width = GetWindowWidth(window);
    buf = GfGfxLoader_GetCharData(NARC_a_0_1_9, 14, TRUE, &chr, HEAP_ID_10);
    pix = chr->pRawData;
    BlitBitmapRect(window, pix + 0xAE0, 0, 0, 24, 8, 0, 0, 24, 8, 0xFF);
    BlitBitmapRect(window, pix + 0xB60, 0, 0, 24, 8, 0, 8, 24, 8, 0xFF);
    BlitBitmapRect(window, pix + 0xB20, 0, 0, 24, 8, 0, 16, 24, 8, 0xFF);
    BlitBitmapRect(window, pix + 0xD60, 0, 0, 8, 8, (width - 1) * 8, 0, 8, 8, 0xFF);
    BlitBitmapRect(window, pix + 0xD20, 0, 0, 8, 8, (width - 1) * 8, 8, 8, 8, 0xFF);
    BlitBitmapRect(window, pix + 0xD40, 0, 0, 8, 8, (width - 1) * 8, 16, 8, 8, 0xFF);
    for (i = 1; i < width - 1; i++) {
        BlitBitmapRect(window, pix + 0xB00, 0, 0, 8, 8, i * 8, 0, 8, 8, 0xFF);
        BlitBitmapRect(window, pix + 0xB80, 0, 0, 8, 8, i * 8, 8, 8, 8, 0xFF);
        BlitBitmapRect(window, pix + 0xB40, 0, 0, 8, 8, i * 8, 16, 8, 8, 0xFF);
    }
    Heap_Free(buf);
    str = NewString_ReadMsgData(work->view->msgData, msgId);
    PCBox_PrintAligned(window, str, width * 8 / 2, 4, 4, 0x90A00, 2);
    String_Delete(str);
    CopyWindowPixelsToVram_TextMode(window);
}

void PCBox_DrawTrayTab(PCBoxWork *work, u32 unused) {
    if (work->args->mode == PC_BOX_MODE_MOVE_ITEMS) {
        PCBox_DrawTabWindow(work, 0x19, msg_0024_00064);
    } else {
        PCBox_DrawTabWindow(work, 0x19, msg_0024_00061);
    }
}

void PCBox_DrawReturnButton(PCBoxWork *work, u32 style) {
    PCBox_DrawButtonWindow(work, 0x1a, msg_0024_00062, style);
}

void PCBox_DrawExitButton(PCBoxWork *work, u32 style) {
    PCBox_DrawButtonWindow(work, 0x1b, msg_0024_00076, style);
}

static void PCBox_DrawMenuExitButton(PCBoxWork *work, u32 style) {
    PCBox_DrawButtonWindow(work, 0x1c, msg_0024_00071, style);
}

void PCBox_DrawActionMenu(PCBoxWork *work, const PCBoxMenuEntry *list, u32 count) {
    u16 size[2];
    u32 i;

    BgCellManager_GetCellSize(work->view->panels, PC_BOX_PANEL_MENU_ROW_0, &size[1], &size[0]);
    for (i = 0; i < count; i++) {
        const PCBoxMenuEntry *entry = &list[count - 1 - i];
        if (entry->kind == 0) {
            PCBox_DrawButtonWindow(work, 0x21 - i, entry->msgId, 0);
            BgCellManager_SetCellPalette(work->view->panels, PC_BOX_PANEL_MENU_ROW_4 - i, 0, 0, size[1], size[0], 0xc);
        } else {
            PCBox_DrawTabWindow(work, 0x21 - i, entry->msgId);
            BgCellManager_SetCellPalette(work->view->panels, PC_BOX_PANEL_MENU_ROW_4 - i, 0, 0, size[1], size[0], 2);
        }
    }
    for (; count < 5; count++) {
        FillWindowPixelBuffer(&work->view->windows[0x21 - count], 0);
        CopyWindowPixelsToVram_TextMode(&work->view->windows[0x21 - count]);
    }
}

void PCBox_DrawBagGiveButton(PCBoxWork *work, u32 which) {
    FillWindowPixelBuffer(&work->view->windows[0x1d], 0);
    FillWindowPixelBuffer(&work->view->windows[0x1e], 0);
    FillWindowPixelBuffer(&work->view->windows[0x1f], 0);
    FillWindowPixelBuffer(&work->view->windows[0x20], 0);
    CopyWindowPixelsToVram_TextMode(&work->view->windows[0x1d]);
    CopyWindowPixelsToVram_TextMode(&work->view->windows[0x1e]);
    CopyWindowPixelsToVram_TextMode(&work->view->windows[0x1f]);
    CopyWindowPixelsToVram_TextMode(&work->view->windows[0x20]);
    if (which == 0) {
        PCBox_DrawButtonWindow(work, 0x21, msg_0024_00081, 0);
    } else {
        PCBox_DrawButtonWindow(work, 0x21, msg_0024_00080, 0);
    }
}

void PCBox_DrawChangeBoxButton(PCBoxWork *work) {
    PCBox_DrawButtonWindow(work, 0x2a, msg_0024_00063, 0);
}

static void PCBox_DrawChangeBoxTitle(PCBoxWork *work) {
    PCBox_PrintWindowMessage(work, 0x29, msg_0024_00077, 0, 3);
}

void PCBox_DrawBoxMenu(PCBoxWork *work) {
    PCBox_DrawChangeBoxTitle(work);
    PCBox_DrawButtonWindow(work, 0x20, msg_0024_00026, 0);
    PCBox_DrawButtonWindow(work, 0x21, msg_0024_00027, 0);
}

void PCBox_DrawChangeWallpaperTitle(PCBoxWork *work) {
    PCBox_PrintWindowMessage(work, 0x29, msg_0024_00078, 0, 3);
}

void PCBox_DrawDepositTitle(PCBoxWork *work) {
    PCBox_PrintWindowMessage(work, 0x29, msg_0024_00079, 0, 3);
}

static void PCBox_SetPanelWindowFrame(BgCellManager *mgr, int id, Window *win) {
    u16 size[2];
    u16 *map;
    u16 i;
    u16 j;
    u16 tile;
    u16 pal;
    u16 x;

    map = BgCellManager_GetCellTilemap(mgr, id);
    BgCellManager_GetCellSize(mgr, id, &size[1], &size[0]);
    tile = GetWindowBaseTile(win);
    pal = win->paletteNum << 12;
    map[0] = pal + 1000;
    map[size[1] - 1] = pal + 1002;
    map[size[1] * (size[0] - 1)] = pal + 1009;
    map[size[1] * size[0] - 1] = pal + 1011;
    for (i = 0; i < size[1] - 2; i++) {
        map[i + 1] = pal + 1001;
        map[i + size[1] * (size[0] - 1) + 1] = pal + 1010;
    }
    for (i = 0; i < size[0] - 2; i++) {
        map[size[1] * (i + 1)] = pal + 1003;
        map[size[1] * (i + 2) - 1] = pal + 1005;
    }
    for (j = 0; j < size[0] - 2; j++) {
        for (x = 0; x < size[1] - 2; x++) {
            map[x + (size[1] * (j + 1) + 1)] = pal + tile;
            tile++;
        }
    }
}

static void PCBox_AttachWindowToPanel(BgCellManager *mgr, int id, Window *window) {
    u32 width;
    u32 height;
    u32 bg = GetWindowBgId(window);
    width = GetWindowWidth(window);
    height = GetWindowHeight(window);
    BgCellManager_InitCell(mgr, id, bg, width, height);
    BgCellManager_PutWindowTiles(mgr, id, window);
}

void PCBox_AttachPanelWindows(PCBoxView *view) {
    PCBox_AttachWindowToPanel(view->panels, PC_BOX_PANEL_MENU_ROW_0, &view->windows[29]);
    PCBox_AttachWindowToPanel(view->panels, PC_BOX_PANEL_MENU_ROW_1, &view->windows[30]);
    PCBox_AttachWindowToPanel(view->panels, PC_BOX_PANEL_MENU_ROW_2, &view->windows[31]);
    PCBox_AttachWindowToPanel(view->panels, PC_BOX_PANEL_MENU_ROW_3, &view->windows[32]);
    PCBox_AttachWindowToPanel(view->panels, PC_BOX_PANEL_MENU_ROW_4, &view->windows[33]);
    PCBox_SetPanelWindowFrame(view->panels, 0xc, &view->windows[41]);
    PCBox_AttachWindowToPanel(view->panels, PC_BOX_PANEL_CHANGE_BOX_BUTTON, &view->windows[42]);
    PCBox_AttachWindowToPanel(view->panels, PC_BOX_PANEL_SUMMARY_BUTTON, &view->windows[43]);
}

void PCBox_AttachPartyButtonWindow(PCBoxView *view) {
    PCBox_AttachWindowToPanel(view->panels, PC_BOX_PANEL_PARTY_BUTTON, &view->windows[24]);
}

void PCBox_AttachTrayTabWindow(PCBoxView *view) {
    PCBox_AttachWindowToPanel(view->panels, PC_BOX_PANEL_TRAY_TAB, &view->windows[25]);
}

void PCBox_AttachReturnButtonWindow(PCBoxView *view) {
    PCBox_AttachWindowToPanel(view->panels, PC_BOX_PANEL_RETURN_BUTTON, &view->windows[26]);
}

void PCBox_AttachMenuExitButtonWindow(PCBoxWork *work) {
    PCBoxView *view = work->view;

    PCBox_AttachWindowToPanel(view->panels, PC_BOX_PANEL_MENU_EXIT_BUTTON, &view->windows[28]);
    PCBox_DrawMenuExitButton(work, 0);
}

static void PCBox_PrintCenteredLabel(PCBoxView *view, u32 windowIndex, u32 messageId, u32 unused) {
    Window *window = &view->windows[windowIndex];
    u8 x;

    FillWindowPixelBuffer(window, 0xD);
    x = GetWindowWidth(window) * 8 / 2;
    PCBox_PrintMsg(view, view->msgData, windowIndex, messageId, x, 0, 4, 0x00090A0D, 2);
    CopyWindowPixelsToVram_TextMode(window);
}

void PCBox_DrawConfirmCancel(PCBoxView *view) {
    PCBox_PrintCenteredLabel(view, 0x22, msg_0024_00072, 0);
    PCBox_PrintCenteredLabel(view, 0x23, msg_0024_00073, 0);
}

void PCBox_PrintPartyPanelSwitchLabel(PCBoxView *view) {
    PCBox_PrintCenteredLabel(view, 0x24, msg_0024_00074, 0);
}

void PCBox_PrintPartyPanelExitLabel(PCBoxView *view) {
    PCBox_PrintCenteredLabel(view, 0x24, msg_0024_00075, 0);
}

void PCBox_AttachMarkingMenuWindows(PCBoxView *view) {
    BgCellManager_PutWindowTiles(view->panels, PC_BOX_PANEL_MARKING_MENU, &view->windows[34]);
    BgCellManager_PutWindowTiles(view->panels, PC_BOX_PANEL_MARKING_MENU, &view->windows[35]);
}

void PCBox_AttachPartyPanelLabelWindow(PCBoxView *view) {
    BgCellManager_PutWindowTiles(view->panels, PC_BOX_PANEL_PARTY, &view->windows[36]);
}

void PCBox_DrawSummaryButton(PCBoxWork *work, u32 unused) {
    PCBoxView *view = work->view;
    void *allocation;
    int limit;
    int width = GetWindowWidth(&view->windows[43]);
    NNSG2dCharacterData *chars;
    u8 *pixels;
    int last;
    int edge;
    u8 i;
    String *string;

    allocation = GfGfxLoader_GetCharData(NARC_a_0_1_9, 14, TRUE, &chars, HEAP_ID_10);
    pixels = chars->pRawData;
    BlitBitmapRect(&view->windows[43], pixels + 0x7a0, 0, 0, 24, 8, 0, 0, 24, 8, 255);
    BlitBitmapRect(&view->windows[43], pixels + 0xba0, 0, 0, 24, 8, 0, 8, 24, 8, 255);
    BlitBitmapRect(&view->windows[43], pixels + 0xfa0, 0, 0, 24, 8, 0, 16, 24, 8, 255);
    last = width - 1;
    edge = last * 8;
    BlitBitmapRect(&view->windows[43], pixels + 0xd00, 0, 0, 8, 8, edge, 0, 8, 8, 255);
    BlitBitmapRect(&view->windows[43], pixels + 0xce0, 0, 0, 8, 8, edge, 8, 8, 8, 255);
    BlitBitmapRect(&view->windows[43], pixels + 0xcc0, 0, 0, 8, 8, edge, 16, 8, 8, 255);
    i = 3;
    if (last > 3) {
        limit = width - 1;
        do {
            int x = i * 8;
            BlitBitmapRect(&view->windows[43], pixels + 0xc20, 0, 0, 8, 8, x, 0, 8, 8, 255);
            BlitBitmapRect(&view->windows[43], pixels + 0xca0, 0, 0, 8, 8, x, 8, 8, 8, 255);
            BlitBitmapRect(&view->windows[43], pixels + 0xc80, 0, 0, 8, 8, x, 16, 8, 8, 255);
            i++;
        } while (i < limit);
    }
    Heap_Free(allocation);
    string = NewString_ReadMsgData(work->view->msgData, msg_0024_00065);
    PCBox_PrintAligned(&view->windows[43], string, width * 8 / 2, 4, 4, 0x90a00, 2);
    String_Delete(string);
    CopyWindowPixelsToVram_TextMode(&view->windows[43]);
}

static MsgData *PCBox_LoadBoxMessages(void) {
    return NewMsgDataFromNarc(MSGDATA_LOAD_LAZY, NARC_msgdata_msg, NARC_msg_msg_0025_bin, HEAP_ID_10);
}

static void PCBox_DrawMessageFrame(Window *window) {
    FillWindowPixelBuffer(window, 15);
    DrawFrame2AroundWindow(window, 910, 10);
}

void PCBox_CloseMessageWindow(PCBoxView *view, int windowId) {
    ClearFrameAndWindow2(&view->windows[windowId], TRUE);
    ScheduleBgTilemapBufferTransfer(view->bgConfig, GetWindowBgId(&view->windows[windowId]));
}

void PCBox_ClearWindowFrameNoTransfer(PCBoxView *view, u32 idx) {
    ClearFrameAndWindow2(&view->windows[idx], TRUE);
}

void PCBox_ClearWindowFrame(PCBoxView *view, u32 idx) {
    ClearFrameAndWindow2(&view->windows[idx], FALSE);
}

// Prints box message MSGNO (msg_0025) into window WINDOWID with a frame.
static void PCBox_PrintMessage(PCBoxWork *work, int msgNo, int windowId) {
    MsgData *msgData = PCBox_LoadBoxMessages();

    PCBox_DrawMessageFrame(&work->view->windows[windowId]);
    PCBox_PrintExpandedMsg(work->view, msgData, windowId, msgNo, 0, 0, 1, 0x1020f, 0);
    ScheduleWindowCopyToVram(&work->view->windows[windowId]);
    DestroyMsgData(msgData);
}

void PCBox_PrintExitPrompt(PCBoxWork *work, int windowId, int kind) {
    if (kind == 1) {
        PCBox_PrintMessage(work, msg_0025_00011, windowId);
    } else {
        PCBox_PrintMessage(work, msg_0025_00012, windowId);
    }
}

void PCBox_PrintTakeItemPrompt(PCBoxWork *work, int item, int windowId) {
    PCBoxView *view = work->view;
    BufferItemName(view->msgFormat, 0, item);
    PCBox_PrintMessage(work, msg_0025_00023, windowId);
}

void PCBox_PrintMailCantBeTaken(PCBoxWork *work, int windowId) {
    PCBox_PrintMessage(work, msg_0025_00024, windowId);
}

void PCBox_PrintMailCantBeMoved(PCBoxWork *work, int windowId) {
    PCBox_PrintMessage(work, msg_0025_00054, windowId);
}

void PCBox_PrintTookItem(PCBoxWork *work, int item, int windowId) {
    PCBoxView *view = work->view;
    BufferItemName(view->msgFormat, 0, item);
    PCBox_PrintMessage(work, msg_0025_00015, windowId);
}

void PCBox_PrintBagFull(PCBoxWork *work, int windowId) {
    PCBox_PrintMessage(work, msg_0025_00014, windowId);
}

void PCBox_PrintItemHeld(PCBoxWork *work, int item, int windowId) {
    if (item == 0) {
        BufferItemName(work->view->msgFormat, 0, ITEM_GRISEOUS_ORB);
        PCBox_PrintMessage(work, msg_0025_00055, windowId);
    } else {
        BufferItemName(work->view->msgFormat, 0, item);
        PCBox_PrintMessage(work, msg_0025_00016, windowId);
    }
}

void PCBox_PrintTouchTheMark(PCBoxWork *work, int windowId) {
    PCBox_PrintMessage(work, msg_0025_00035, windowId);
}

void PCBox_PrintReleaseMessage(PCBoxWork *work, int mode, int windowId) {
    int msgNo;

    switch (mode) {
    case PC_BOX_RELEASE_MSG_CONFIRM:
        msgNo = msg_0025_00002;
        break;
    case PC_BOX_RELEASE_MSG_RELEASED:
        msgNo = msg_0025_00003;
        BufferBoxMonNickname(work->view->msgFormat, 0, PCBox_GetSlotBoxMon(work, work->activeBox, work->selectedSlot));
        break;
    case PC_BOX_RELEASE_MSG_BYE_BYE:
        msgNo = msg_0025_00004;
        BufferBoxMonNickname(work->view->msgFormat, 0, PCBox_GetSlotBoxMon(work, work->activeBox, work->selectedSlot));
        break;
    case PC_BOX_RELEASE_MSG_EGG:
        msgNo = msg_0025_00031;
        break;
    case PC_BOX_RELEASE_MSG_CAME_BACK:
        msgNo = msg_0025_00032;
        BufferBoxMonNickname(work->view->msgFormat, 0, PCBox_GetSlotBoxMon(work, work->activeBox, work->selectedSlot));
        break;
    case PC_BOX_RELEASE_MSG_WORRIED:
        msgNo = msg_0025_00033;
        break;
    case PC_BOX_RELEASE_MSG_LAST_MON:
        msgNo = msg_0025_00006;
        break;
    default:
        break;
    }
    PCBox_PrintMessage(work, msgNo, windowId);
}

void PCBox_PrintMenuPrompt(PCBoxWork *work, u32 which, u32 windowId) {
    switch (which) {
    case 0:
        which = msg_0025_00007;
        break;
    case 1:
        which = msg_0025_00010;
        break;
    }
    PCBox_PrintMessage(work, which, windowId);
}

void PCBox_PrintMonActionMessage(PCBoxWork *work, u32 unused, u32 kind, u32 windowId) {
    u32 id;

    switch (kind) {
    case PC_BOX_MON_MSG_WHICH_ONE:
        id = msg_0025_00018;
        break;
    case PC_BOX_MON_MSG_SELECTED:
        id = msg_0025_00000;
        BufferBoxMonNickname(work->view->msgFormat, 0, PCBox_GetSlotBoxMon(work, work->activeBox, work->selectedSlot));
        break;
    case PC_BOX_MON_MSG_PARTY_FULL:
        id = msg_0025_00005;
        break;
    case PC_BOX_MON_MSG_DEPOSIT_WHERE:
        id = msg_0025_00019;
        break;
    case PC_BOX_MON_MSG_BOX_FULL:
        id = msg_0025_00013;
        break;
    case PC_BOX_MON_MSG_DETACH_CAPSULE:
        id = msg_0025_00029;
        break;
    case PC_BOX_MON_MSG_REMOVE_MAIL:
        id = msg_0025_00030;
        break;
    }

    PCBox_PrintMessage(work, id, windowId);
}

void PCBox_PrintItemActionMessage(PCBoxWork *work, u32 kind, u32 windowId) {
    u32 id;

    switch (kind) {
    case 0:
        break;
    case 1:
        break;
    case PC_BOX_ITEM_MSG_PUT_AWAY:
        id = msg_0025_00026;
        BufferItemName(work->view->msgFormat, 0, work->view->heldItem);
        break;
    case PC_BOX_ITEM_MSG_PLACED_IN_BAG:
        id = msg_0025_00027;
        BufferItemName(work->view->msgFormat, 0, work->view->heldItem);
        break;
    case PC_BOX_ITEM_MSG_MAIL_CANT_BE_TAKEN:
        id = msg_0025_00024;
        break;
    case PC_BOX_ITEM_MSG_EGG_CANT_HOLD:
        id = msg_0025_00034;
        break;
    case PC_BOX_ITEM_MSG_BAG_FULL:
        id = msg_0025_00014;
        break;
    case PC_BOX_ITEM_MSG_MAIL_CANT_BE_MOVED:
        id = msg_0025_00059;
        break;
    }

    PCBox_PrintMessage(work, id, windowId);
}

void PCBox_PrintPanelHint(PCBoxWork *work, u32 idx, u32 kind) {
    MsgData *msgData = PCBox_LoadBoxMessages();
    String *str;

    switch (kind) {
    case 0:
        kind = msg_0025_00036;
        break;
    case 1:
        kind = msg_0025_00037;
        break;
    case 2:
        kind = msg_0025_00038;
        break;
    case 3:
        kind = msg_0025_00039;
        break;
    case 4:
        kind = msg_0025_00028;
        break;
    case 5:
        kind = msg_0025_00025;
        BufferItemName(work->view->msgFormat, 0, work->view->heldItem);
        break;
    case 6:
        kind = msg_0025_00056;
        break;
    case 7:
        kind = msg_0025_00057;
        break;
    case 8:
        kind = msg_0025_00058;
        break;
    case 9:
        kind = msg_0025_00059;
        break;
    case 10:
        kind = msg_0025_00060;
        break;
    }

    str = NewString_ReadMsgData(msgData, kind);
    StringExpandPlaceholders(work->view->msgFormat, work->view->string, str);
    String_Delete(str);
    FillWindowPixelBuffer(&work->view->windows[idx], 0xf);
    AddTextPrinterParameterizedWithColor(&work->view->windows[idx], 1, work->view->string, 0, 0, 0xff, 0x1020f, NULL);
    DestroyMsgData(msgData);
}

void PCBox_DrawWindowFrame(PCBoxWork *work, int windowId) {
    DrawFrame2AroundWindow(&work->view->windows[windowId], 910, 10);
    ScheduleWindowCopyToVram(&work->view->windows[windowId]);
}

s32 PCBox_FindTouchedBoxSlot(void) {
    return TouchscreenHitbox_FindRectAtTouchNew(gPCBoxSlotHitboxes);
}

s32 PCBox_FindTouchedPartySlot(void) {
    return TouchscreenHitbox_FindRectAtTouchNew(gPCBoxPartyHitboxes);
}

s32 PCBox_FindTouchedTrayPartySlot(void) {
    return TouchscreenHitbox_FindRectAtTouchNew(gPCBoxTrayPartyHitboxes);
}

void PCBox_CreateModeGridInput(PCBoxWork *work) {
    int layout;

    switch (work->args->mode) {
    case PC_BOX_MODE_DEPOSIT:
        layout = 0;
        break;
    case PC_BOX_MODE_WITHDRAW:
        layout = 2;
        break;
    case PC_BOX_MODE_MOVE_POKEMON:
        layout = 3;
        break;
    case PC_BOX_MODE_MOVE_ITEMS:
        layout = 6;
        break;
    }
    PCBox_FreeGridInput(work);
    work->view->gridInput = GridInputHandler_Create(gPCBoxGridLayouts[layout].hitboxes, gPCBoxGridLayouts[layout].dpadBoxes, gPCBoxGridLayouts[layout].callbacks, work, TRUE, work->returnCursorPos, HEAP_ID_10);
    PCBox_PlaceCursorOnGridInput(work, work->returnCursorPos);
}

void PCBox_CreateGridInput(PCBoxWork *work, int layout, int initialPos) {
    PCBox_FreeGridInput(work);
    work->view->gridInput = GridInputHandler_Create(gPCBoxGridLayouts[layout].hitboxes, gPCBoxGridLayouts[layout].dpadBoxes, gPCBoxGridLayouts[layout].callbacks, work, TRUE, initialPos, HEAP_ID_10);
    PCBox_PlaceCursorOnGridInput(work, initialPos);
}

void PCBox_FreeGridInput(PCBoxWork *work) {
    GridInputHandler *gridInput = work->view->gridInput;

    if (gridInput != NULL) {
        GridInputHandler_Free(gridInput);
        work->view->gridInput = NULL;
    }
}

void PCBox_PlaceCursorOnGridInput(PCBoxWork *work, int target) {
    const DpadMenuBox *box = GridInputHandler_GetDpadBox(work->view->gridInput, target);

    struct {
        u8 y, x;
    } pos;

    PCBox_SetSpriteVisible(work->view, PC_BOX_SPRITE_CURSOR, TRUE);
    DpadMenuBox_GetPosition(box, &pos.x, &pos.y);
    ManagedSprite_SetPositionXY(work->view->sprites[PC_BOX_SPRITE_CURSOR], pos.x, pos.y);
}

void PCBox_GridOnButton(PCBoxWork *work, int target) {
    GridInputHandler_SetNextInput(work->view->gridInput, (u8)target);
    PCBox_PlaceCursorOnGridInput(work, target);
}

void PCBox_GridOnSwitchToTouchMode(PCBoxWork *work) {
    PCBox_SetSpriteVisible(work->view, PC_BOX_SPRITE_CURSOR, FALSE);
}
#if defined(SDK_PORT)

/* Native tables from asm/overlay_14_state_dispatch_rodata_67.s; adapters give differing originals the field signatures. */
void PCBox_WallpaperGridOnButton(PCBoxWork *work, u32 newTarget);
void PCBox_GridOnSwitchToTouchMode(PCBoxWork *work);
void PCBox_WallpaperGridOnKeyMove(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_WallpaperGridOnTouch(PCBoxWork *work, u32 newTarget);
void PCBox_WithdrawGridOnButton(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_GridOnKeyMove(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_WithdrawGridOnTouch(PCBoxWork *work, u32 newTarget);
void PCBox_DepositGridOnButton(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_GridOnTouch(PCBoxWork *work, u32 newTarget);
void PCBox_DepositBoxGridOnButton(PCBoxWork *work, u32 newTarget);
void PCBox_DepositBoxGridOnKeyMove(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_DepositBoxGridOnTouch(PCBoxWork *work, u32 newTarget);
void PCBox_BoxMenuGridOnButton(PCBoxWork *work, u32 newTarget);
void PCBox_BoxMenuGridOnKeyMove(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_BoxMenuGridOnTouch(PCBoxWork *work, u32 newTarget);
void PCBox_MoveModePartyGridOnButton(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_MoveModePartyGridOnKeyMove(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_TrayGridOnButton(PCBoxWork *work, u32 newTarget);
void PCBox_TrayGridOnKeyMove(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_TrayGridOnTouch(PCBoxWork *work, u32 newTarget);
void PCBox_MoveModeGridOnButton(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_MoveModeGridOnKeyMove(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_MoveModeGridOnTouch(PCBoxWork *work, u32 newTarget);
void PCBox_GridOnButton(PCBoxWork *work, int target);
void PCBox_ItemModePartyGridOnButton(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_ItemModePartyGridOnKeyMove(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_ItemModeGridOnButton(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_ItemModeGridOnKeyMove(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_ItemModeGridOnTouch(PCBoxWork *work, u32 newTarget);
static void Native_PCBox_WallpaperGridOnButton_2016(void * a0, int a1, int a2) { PCBox_WallpaperGridOnButton((PCBoxWork *)a0, a1); }
static void Native_PCBox_GridOnSwitchToTouchMode_2016(void * a0, int a1, int a2) { PCBox_GridOnSwitchToTouchMode((PCBoxWork *)a0); }
static void Native_PCBox_WallpaperGridOnKeyMove_2016(void * a0, int a1, int a2) { PCBox_WallpaperGridOnKeyMove((PCBoxWork *)a0, a1, a2); }
static void Native_PCBox_WallpaperGridOnTouch_2016(void * a0, int a1, int a2) { PCBox_WallpaperGridOnTouch((PCBoxWork *)a0, a1); }
const GridCallbacks sPCBoxWallpaperGridCallbacks = {
    Native_PCBox_WallpaperGridOnButton_2016,
    Native_PCBox_GridOnSwitchToTouchMode_2016,
    Native_PCBox_WallpaperGridOnKeyMove_2016,
    Native_PCBox_WallpaperGridOnTouch_2016,
};
static void Native_PCBox_WithdrawGridOnButton_2016(void * a0, int a1, int a2) { PCBox_WithdrawGridOnButton((PCBoxWork *)a0, a1, a2); }
static void Native_PCBox_GridOnKeyMove_2016(void * a0, int a1, int a2) { PCBox_GridOnKeyMove((PCBoxWork *)a0, a1, a2); }
static void Native_PCBox_WithdrawGridOnTouch_2016(void * a0, int a1, int a2) { PCBox_WithdrawGridOnTouch((PCBoxWork *)a0, a1); }
const GridCallbacks sPCBoxWithdrawGridCallbacks = {
    Native_PCBox_WithdrawGridOnButton_2016,
    Native_PCBox_GridOnSwitchToTouchMode_2016,
    Native_PCBox_GridOnKeyMove_2016,
    Native_PCBox_WithdrawGridOnTouch_2016,
};
static void Native_PCBox_DepositGridOnButton_2016(void * a0, int a1, int a2) { PCBox_DepositGridOnButton((PCBoxWork *)a0, a1, a2); }
static void Native_PCBox_GridOnTouch_2016(void * a0, int a1, int a2) { PCBox_GridOnTouch((PCBoxWork *)a0, a1); }
const GridCallbacks sPCBoxDepositGridCallbacks = {
    Native_PCBox_DepositGridOnButton_2016,
    Native_PCBox_GridOnSwitchToTouchMode_2016,
    Native_PCBox_GridOnKeyMove_2016,
    Native_PCBox_GridOnTouch_2016,
};
static void Native_PCBox_DepositBoxGridOnButton_2016(void * a0, int a1, int a2) { PCBox_DepositBoxGridOnButton((PCBoxWork *)a0, a1); }
static void Native_PCBox_DepositBoxGridOnKeyMove_2016(void * a0, int a1, int a2) { PCBox_DepositBoxGridOnKeyMove((PCBoxWork *)a0, a1, a2); }
static void Native_PCBox_DepositBoxGridOnTouch_2016(void * a0, int a1, int a2) { PCBox_DepositBoxGridOnTouch((PCBoxWork *)a0, a1); }
const GridCallbacks sPCBoxDepositBoxGridCallbacks = {
    Native_PCBox_DepositBoxGridOnButton_2016,
    Native_PCBox_GridOnSwitchToTouchMode_2016,
    Native_PCBox_DepositBoxGridOnKeyMove_2016,
    Native_PCBox_DepositBoxGridOnTouch_2016,
};
static void Native_PCBox_BoxMenuGridOnButton_2016(void * a0, int a1, int a2) { PCBox_BoxMenuGridOnButton((PCBoxWork *)a0, a1); }
static void Native_PCBox_BoxMenuGridOnKeyMove_2016(void * a0, int a1, int a2) { PCBox_BoxMenuGridOnKeyMove((PCBoxWork *)a0, a1, a2); }
static void Native_PCBox_BoxMenuGridOnTouch_2016(void * a0, int a1, int a2) { PCBox_BoxMenuGridOnTouch((PCBoxWork *)a0, a1); }
const GridCallbacks sPCBoxBoxMenuGridCallbacks = {
    Native_PCBox_BoxMenuGridOnButton_2016,
    Native_PCBox_GridOnSwitchToTouchMode_2016,
    Native_PCBox_BoxMenuGridOnKeyMove_2016,
    Native_PCBox_BoxMenuGridOnTouch_2016,
};
static void Native_PCBox_MoveModePartyGridOnButton_2016(void * a0, int a1, int a2) { PCBox_MoveModePartyGridOnButton((PCBoxWork *)a0, a1, a2); }
static void Native_PCBox_MoveModePartyGridOnKeyMove_2016(void * a0, int a1, int a2) { PCBox_MoveModePartyGridOnKeyMove((PCBoxWork *)a0, a1, a2); }
const GridCallbacks sPCBoxMoveModePartyGridCallbacks = {
    Native_PCBox_MoveModePartyGridOnButton_2016,
    Native_PCBox_GridOnSwitchToTouchMode_2016,
    Native_PCBox_MoveModePartyGridOnKeyMove_2016,
    Native_PCBox_GridOnTouch_2016,
};
static void Native_PCBox_TrayGridOnButton_2016(void * a0, int a1, int a2) { PCBox_TrayGridOnButton((PCBoxWork *)a0, a1); }
static void Native_PCBox_TrayGridOnKeyMove_2016(void * a0, int a1, int a2) { PCBox_TrayGridOnKeyMove((PCBoxWork *)a0, a1, a2); }
static void Native_PCBox_TrayGridOnTouch_2016(void * a0, int a1, int a2) { PCBox_TrayGridOnTouch((PCBoxWork *)a0, a1); }
const GridCallbacks sPCBoxTrayGridCallbacks = {
    Native_PCBox_TrayGridOnButton_2016,
    Native_PCBox_GridOnSwitchToTouchMode_2016,
    Native_PCBox_TrayGridOnKeyMove_2016,
    Native_PCBox_TrayGridOnTouch_2016,
};
static void Native_PCBox_MoveModeGridOnButton_2016(void * a0, int a1, int a2) { PCBox_MoveModeGridOnButton((PCBoxWork *)a0, a1, a2); }
static void Native_PCBox_MoveModeGridOnKeyMove_2016(void * a0, int a1, int a2) { PCBox_MoveModeGridOnKeyMove((PCBoxWork *)a0, a1, a2); }
static void Native_PCBox_MoveModeGridOnTouch_2016(void * a0, int a1, int a2) { PCBox_MoveModeGridOnTouch((PCBoxWork *)a0, a1); }
const GridCallbacks sPCBoxMoveModeGridCallbacks = {
    Native_PCBox_MoveModeGridOnButton_2016,
    Native_PCBox_GridOnSwitchToTouchMode_2016,
    Native_PCBox_MoveModeGridOnKeyMove_2016,
    Native_PCBox_MoveModeGridOnTouch_2016,
};
static void Native_PCBox_GridOnButton_2016(void * a0, int a1, int a2) { PCBox_GridOnButton((PCBoxWork *)a0, a1); }
const GridCallbacks sPCBoxMarkingGridCallbacks = {
    Native_PCBox_GridOnButton_2016,
    Native_PCBox_GridOnSwitchToTouchMode_2016,
    Native_PCBox_GridOnKeyMove_2016,
    Native_PCBox_GridOnTouch_2016,
};
static void Native_PCBox_ItemModePartyGridOnButton_2016(void * a0, int a1, int a2) { PCBox_ItemModePartyGridOnButton((PCBoxWork *)a0, a1, a2); }
static void Native_PCBox_ItemModePartyGridOnKeyMove_2016(void * a0, int a1, int a2) { PCBox_ItemModePartyGridOnKeyMove((PCBoxWork *)a0, a1, a2); }
const GridCallbacks sPCBoxItemModePartyGridCallbacks = {
    Native_PCBox_ItemModePartyGridOnButton_2016,
    Native_PCBox_GridOnSwitchToTouchMode_2016,
    Native_PCBox_ItemModePartyGridOnKeyMove_2016,
    Native_PCBox_GridOnTouch_2016,
};
static void Native_PCBox_ItemModeGridOnButton_2016(void * a0, int a1, int a2) { PCBox_ItemModeGridOnButton((PCBoxWork *)a0, a1, a2); }
static void Native_PCBox_ItemModeGridOnKeyMove_2016(void * a0, int a1, int a2) { PCBox_ItemModeGridOnKeyMove((PCBoxWork *)a0, a1, a2); }
static void Native_PCBox_ItemModeGridOnTouch_2016(void * a0, int a1, int a2) { PCBox_ItemModeGridOnTouch((PCBoxWork *)a0, a1); }
const GridCallbacks sPCBoxItemModeGridCallbacks = {
    Native_PCBox_ItemModeGridOnButton_2016,
    Native_PCBox_GridOnSwitchToTouchMode_2016,
    Native_PCBox_ItemModeGridOnKeyMove_2016,
    Native_PCBox_ItemModeGridOnTouch_2016,
};

/* Native grid layouts from asm/overlay_14_state_dispatch_rodata_67.s. */
extern const TouchscreenHitbox sPCBoxBoxMenuGridHitboxes[];
extern const TouchscreenHitbox sPCBoxDepositBoxGridHitboxes[];
extern const TouchscreenHitbox sPCBoxDepositGridHitboxes[];
extern const TouchscreenHitbox sPCBoxItemModeGridHitboxes[];
extern const TouchscreenHitbox sPCBoxItemModePartyGridHitboxes[];
extern const TouchscreenHitbox sPCBoxMarkingGridHitboxes[];
extern const TouchscreenHitbox sPCBoxMoveModeGridHitboxes[];
extern const TouchscreenHitbox sPCBoxMoveModePartyGridHitboxes[];
extern const TouchscreenHitbox sPCBoxTrayGridHitboxes[];
extern const TouchscreenHitbox sPCBoxWallpaperGridHitboxes[];
extern const TouchscreenHitbox sPCBoxWithdrawGridHitboxes[];
extern const DpadMenuBox sPCBoxBoxMenuGridDpadBoxes[];
extern const DpadMenuBox sPCBoxDepositBoxGridDpadBoxes[];
extern const DpadMenuBox sPCBoxDepositGridDpadBoxes[];
extern const DpadMenuBox sPCBoxItemModeGridDpadBoxes[];
extern const DpadMenuBox sPCBoxItemModePartyGridDpadBoxes[];
extern const DpadMenuBox sPCBoxMarkingGridDpadBoxes[];
extern const DpadMenuBox sPCBoxMoveModeGridDpadBoxes[];
extern const DpadMenuBox sPCBoxMoveModePartyGridDpadBoxes[];
extern const DpadMenuBox sPCBoxTrayGridDpadBoxes[];
extern const DpadMenuBox sPCBoxWallpaperGridDpadBoxes[];
extern const DpadMenuBox sPCBoxWithdrawGridDpadBoxes[];
const PCBoxGridLayout gPCBoxGridLayouts[] = {
    sPCBoxDepositGridHitboxes,
    sPCBoxDepositGridDpadBoxes,
    &sPCBoxDepositGridCallbacks,
    sPCBoxDepositBoxGridHitboxes,
    sPCBoxDepositBoxGridDpadBoxes,
    &sPCBoxDepositBoxGridCallbacks,
    sPCBoxWithdrawGridHitboxes,
    sPCBoxWithdrawGridDpadBoxes,
    &sPCBoxWithdrawGridCallbacks,
    sPCBoxMoveModeGridHitboxes,
    sPCBoxMoveModeGridDpadBoxes,
    &sPCBoxMoveModeGridCallbacks,
    sPCBoxTrayGridHitboxes,
    sPCBoxTrayGridDpadBoxes,
    &sPCBoxTrayGridCallbacks,
    sPCBoxMoveModePartyGridHitboxes,
    sPCBoxMoveModePartyGridDpadBoxes,
    &sPCBoxMoveModePartyGridCallbacks,
    sPCBoxItemModeGridHitboxes,
    sPCBoxItemModeGridDpadBoxes,
    &sPCBoxItemModeGridCallbacks,
    sPCBoxItemModePartyGridHitboxes,
    sPCBoxItemModePartyGridDpadBoxes,
    &sPCBoxItemModePartyGridCallbacks,
    sPCBoxMarkingGridHitboxes,
    sPCBoxMarkingGridDpadBoxes,
    &sPCBoxMarkingGridCallbacks,
    sPCBoxBoxMenuGridHitboxes,
    sPCBoxBoxMenuGridDpadBoxes,
    &sPCBoxBoxMenuGridCallbacks,
    sPCBoxWallpaperGridHitboxes,
    sPCBoxWallpaperGridDpadBoxes,
    &sPCBoxWallpaperGridCallbacks,
};
#endif
