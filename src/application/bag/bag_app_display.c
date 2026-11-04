#include "global.h"

#include "constants/items.h"
#include "constants/sndseq.h"

#include "application/bag/bag_app.h"
#include "msgdata/msg/msg_0010.h"

#include "bag.h"
#include "bg_window.h"
#include "font.h"
#include "gf_gfx_loader.h"
#include "gf_gfx_planes.h"
#include "heap.h"
#include "item.h"
#include "message_format.h"
#include "move.h"
#include "msgdata.h"
#include "options.h"
#include "player_data.h"
#include "render_text.h"
#include "render_window.h"
#include "sound.h"
#include "sound_playback.h"
#include "sprite_system.h"
#include "string_util.h"
#include "text.h"
#include "type_icon_resources.h"
#include "vram_transfer_manager.h"
#include "yes_no_prompt.h"

// Bag application: item list, message boxes, quantity and money windows, sprites (cursor, item icons,
// pocket tabs, buttons).

static void BagApp_DrawRegisteredItemIcon(BagApp *app, Window *window, int y, BOOL secondSlot);
static BOOL BagApp_TextPrinterCallback(TextPrinterTemplate *printer, u16 event);
static int BagApp_GetNthItemSlotIndex(BagViewPocket *pocket, int pocketId, int n);
static void BagApp_PrintItemListEntry(BagApp *app, Window *window, String *name, BagViewPocket *pocket, int index);
static void BagApp_PrintItemQuantity(MessageFormat *msgFormat, MsgData *msgData, Window *window, int quantity);
static void BagApp_SetItemIconSprite(BagApp *app, int slot, u16 itemId);
static void BagApp_ShowTMHMTypeIcons(BagApp *app, u16 itemId, BOOL show);
static void BagApp_CreateSpriteSystem(BagApp *app);
static void BagApp_LoadSpriteResources(BagApp *app);
static void BagApp_CreateSprites(BagApp *app);
static void BagApp_ClearField648(BagApp *app);
static void BagApp_UpdatePageArrows(BagApp *app);

static void BagApp_DrawRegisteredItemIcon(BagApp *app, Window *window, int y, BOOL secondSlot) {
    NNSG2dCharacterData *charData;
    void *buffer = BagApp_LoadListIconChars(app, &charData);
    if (!secondSlot) {
        BlitBitmapRectToWindow(window, charData->pRawData, 24, 0, 104, 16, 0, y, 40, 16);
    } else {
        BlitBitmapRectToWindow(window, charData->pRawData, 64, 0, 104, 16, 0, y, 40, 16);
    }
    Heap_FreeExplicit(HEAP_ID_6, buffer);
}

void BagApp_LoadItemMenuStrings(BagApp *app) {
    app->menuStrings[BAG_ITEM_ACTION_USE] = NewString_ReadMsgData(app->msgData, msg_0010_00000);
    app->menuStrings[BAG_ITEM_ACTION_WALK] = NewString_ReadMsgData(app->msgData, msg_0010_00006);
    app->menuStrings[BAG_ITEM_ACTION_CHECK] = NewString_ReadMsgData(app->msgData, msg_0010_00016);
    app->menuStrings[BAG_ITEM_ACTION_PLANT_BERRY] = NewString_ReadMsgData(app->msgData, msg_0010_00098);
    app->menuStrings[BAG_ITEM_ACTION_POFFIN_CASE] = NewString_ReadMsgData(app->msgData, msg_0010_00099);
    app->menuStrings[BAG_ITEM_ACTION_TRASH] = NewString_ReadMsgData(app->msgData, msg_0010_00001);
    app->menuStrings[BAG_ITEM_ACTION_REGISTER] = NewString_ReadMsgData(app->msgData, msg_0010_00002);
    app->menuStrings[BAG_ITEM_ACTION_DESELECT] = NewString_ReadMsgData(app->msgData, msg_0010_00018);
    app->menuStrings[BAG_ITEM_ACTION_GIVE] = NewString_ReadMsgData(app->msgData, msg_0010_00003);
    app->menuStrings[9] = NewString_ReadMsgData(app->msgData, msg_0010_00004);
    app->menuStrings[BAG_ITEM_ACTION_CONFIRM] = NewString_ReadMsgData(app->msgData, msg_0010_00005);
    app->menuStrings[BAG_ITEM_ACTION_CANCEL] = NewString_ReadMsgData(app->msgData, msg_0010_00008);
    app->menuStrings[BAG_ITEM_ACTION_MOVE] = NewString_ReadMsgData(app->msgData, msg_0010_00075);
    app->menuStrings[BAG_ITEM_ACTION_SELL] = NewString_ReadMsgData(app->msgData, msg_0010_00086);
    app->menuStrings[BAG_ITEM_ACTION_CHOOSE_FOR_BERRY_POT] = NewString_ReadMsgData(app->msgData, msg_0010_00000);
    app->menuStrings[BAG_ITEM_ACTION_STOP] = NewString_ReadMsgData(app->msgData, msg_0010_00128);
}

void BagApp_FreeItemMenuStrings(BagApp *app) {
    u16 i;
    for (i = 0; i < NELEMS(app->menuStrings); i++) {
        String_Delete(app->menuStrings[i]);
    }
}

void BagApp_PrintItemSelectedMessage(BagApp *app, const u8 *actions, int count) {
    BagViewPocket *pocket;
    String *template, *string;

    if (app->bagView->pockets[app->bagView->pocketIndex].pocketId == POCKET_TMHMS) {
        FillWindowPixelBuffer(&app->windows[1], 0);
        BagApp_PrintTMHMMoveInfo(app, app->bagView->itemId);
        ScheduleWindowCopyToVram(&app->windows[0]);
        BagApp_ShowTMHMTypeIcons(app, app->bagView->itemId, TRUE);
        BagApp_LoadInfoPanelTilemap(app, 0);
    }
    DrawFrameAndWindow2(&app->windows[2], TRUE, 0x3E2, 12);
    FillWindowPixelBuffer(&app->windows[2], 15);
    pocket = &app->bagView->pockets[app->bagView->pocketIndex];
    if (app->bagView->mode == 6 && !BagApp_IsBerryOrMulch(pocket->pocketId, app->bagView->itemId)) {
        template = NewString_ReadMsgData(app->msgData, msg_0010_00106);
    } else {
        template = NewString_ReadMsgData(app->msgData, msg_0010_00043);
    }
    string = String_New(108, HEAP_ID_6);
    BagApp_BufferSlotItemName(app, pocket->scroll + app->cursorButton - BAG_BUTTON_FIRST_SLOT, 0);
    StringExpandPlaceholders(app->msgFormat, string, template);
    AddTextPrinterParameterized(&app->windows[2], 1, string, 0, 0, TEXT_SPEED_NOTRANSFER, NULL);
    String_Delete(string);
    String_Delete(template);
    ScheduleWindowCopyToVram(&app->windows[2]);
}

void BagApp_ShowItemDescription(BagApp *app, Window *window, u32 itemId) {
    FillWindowPixelBuffer(window, 0);
    BagApp_PrintItemDescription(app, itemId);
    ScheduleWindowCopyToVram(window);
}

void BagApp_ClearDescriptionWindow(BagApp *app, Window *window) {
    FillWindowPixelBuffer(window, 0);
    ScheduleWindowCopyToVram(window);
}

void BagApp_ShowPocketDescription(BagApp *app, Window *window, u32 pocket) {
    String *string = NewString_ReadMsgData(app->msgData, pocket + msg_0010_00120);
    FillWindowPixelBuffer(window, 0);
    AddTextPrinterParameterizedWithColor(window, 0, string, 20, 0, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(15, 14, 0), NULL);
    String_Delete(string);
    ScheduleWindowCopyToVram(window);
}

void BagApp_CloseItemPromptWindow(BagApp *app) {
    ClearFrameAndWindow2(&app->windows[2], TRUE);
    ClearWindowTilemapAndScheduleTransfer(&app->windows[2]);
}

void BagApp_CloseItemMenuWindows(BagApp *app) {
    BagApp_CloseItemPromptWindow(app);
    BagApp_RemoveItemActionWindows(app);
    BagApp_ShowTMHMTypeIcons(app, ITEM_NONE, FALSE);
}

void BagApp_CloseItemActionWindows(BagApp *app) {
    BagApp_RemoveItemActionWindows(app);
}

void BagApp_PrintMoveItemPrompt(BagApp *app) {
    FillWindowPixelBuffer(&app->windows[2], 0xFF);
    String *template = NewString_ReadMsgData(app->msgData, msg_0010_00046);
    String *string = String_New(0x82, HEAP_ID_6);
    BagApp_BufferSlotItemName(app, app->moveSource, 0);
    StringExpandPlaceholders(app->msgFormat, string, template);
    DrawFrameAndWindow2(&app->windows[2], TRUE, 0x3E2, 12);
    AddTextPrinterParameterizedWithColor(&app->windows[2], 1, string, 0, 0, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(1, 2, 0), NULL);
    ScheduleWindowCopyToVram(&app->windows[2]);
    String_Delete(string);
    String_Delete(template);
}

void BagApp_PrintQuantityDigits(BagApp *app, u32 numDigits) {
    String *string;
    u32 divisor, value, i;

    GF_ASSERT(app->quantity < 1000);
    string = String_New(2, HEAP_ID_6);
    if (numDigits == 2) {
        divisor = 10;
    } else {
        divisor = 100;
    }
    value = (u32)app->quantity;
    for (i = 0; i < numDigits; i++) {
        u32 digit = value / divisor;
        Window *window;
        String16_FormatInteger(string, digit, 1, PRINTING_MODE_LEFT_ALIGN, TRUE);
        value -= digit * divisor;
        divisor /= 10;
        window = &app->dynWindows[i + 17];
        FillWindowPixelBuffer(window, 0);
        AddTextPrinterParameterizedWithColor(window, 0, string, 0, 4, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(1, 2, 0), NULL);
        ScheduleWindowCopyToVram(window);
    }
    String_Delete(string);
}

void BagApp_PrintTrashConfirmMessage(BagApp *app) {
    String *template = NewString_ReadMsgData(app->msgData, msg_0010_00055);
    BagView *bagView = app->bagView;
    BagViewPocket *pocket = &bagView->pockets[bagView->pocketIndex];
    if (app->quantity > 1) {
        BagApp_BufferSlotItemNamePlural(app, pocket->scroll + app->cursorButton - BAG_BUTTON_FIRST_SLOT, 0);
    } else {
        BagApp_BufferSlotItemName(app, pocket->scroll + app->cursorButton - BAG_BUTTON_FIRST_SLOT, 0);
    }
    BufferIntegerAsString(app->msgFormat, 1, app->quantity, 3, PRINTING_MODE_LEFT_ALIGN, TRUE);
    StringExpandPlaceholders(app->msgFormat, app->msgBuf, template);
    String_Delete(template);
    app->printerId = (u8)BagApp_PrintMessage(app, FALSE);
}

u8 BagApp_PrintMessage(BagApp *app, BOOL wide) {
    Window *window;
    if (!wide) {
        window = &app->windows[3];
    } else {
        GF_ASSERT(app->dynWindows[21].bgConfig != NULL);
        window = &app->dynWindows[21];
    }
    FillWindowPixelBuffer(window, 15);
    DrawFrameAndWindow2(window, TRUE, 0x3E2, 12);
    ScheduleWindowCopyToVram(window);
    TextFlags_SetCanABSpeedUpPrint(TRUE);
    TextFlags_SetAutoScrollParam(0);
    u8 speed = Options_GetTextFrameDelay(app->options);
    return AddTextPrinterParameterized(window, 1, app->msgBuf, 0, 0, speed, BagApp_TextPrinterCallback);
}

static BOOL BagApp_TextPrinterCallback(TextPrinterTemplate *printer, u16 event) {
    switch (event) {
    case 1:
        return GF_IsAnySEPlaying();
    case 2:
        return IsFanfarePlaying();
    case 3:
        PlaySE(SEQ_SE_DP_PC_LOGIN);
        break;
    case 4:
        return IsSEPlaying(SEQ_SE_DP_PC_LOGIN);
    }
    return FALSE;
}

void BagApp_CreateYesNoPrompt(BagApp *app) {
    YesNoPromptTemplate template;
    template.bgConfig = app->bgConfig;
    template.bgId = GF_BG_LYR_SUB_1;
    template.tileStart = 129;
    template.plttSlot = 9;
    template.x = 25;
    template.y = 6;
    template.ignoreTouchFlag = FALSE;
    template.initialCursorPos = 0;
    template.initialCursorPos = 0;
    template.shapeParam = 0;
    app->yesNoPrompt = YesNoPrompt_Create(HEAP_ID_6);
    YesNoPrompt_InitFromTemplate(app->yesNoPrompt, &template);
}

void BagApp_DestroyYesNoPrompt(BagApp *app) {
    YesNoPrompt_Destroy(app->yesNoPrompt);
}

void BagApp_PrintSellPrice(BagApp *app) {
    String *template;
    Window *window = &app->dynWindows[23];

    FillWindowPixelBuffer(window, 0);
    template = NewString_ReadMsgData(app->msgData, msg_0010_00083);
    BufferIntegerAsString(app->msgFormat, 0, app->sellPrice * app->quantity, 6, PRINTING_MODE_RIGHT_ALIGN, TRUE);
    StringExpandPlaceholders(app->msgFormat, app->msgBuf, template);
    FontID_String_GetWidth(0, app->msgBuf, 0);
    AddTextPrinterParameterizedWithColor(window, 0, app->msgBuf, 0, 4, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(1, 2, 0), NULL);
    ScheduleWindowCopyToVram(window);
    String_Delete(template);
}

void BagApp_PrintMoney(BagApp *app, BOOL skipLabel) {
    String *string = String_New(0x100, HEAP_ID_6);
    Window *window = &app->dynWindows[22];
    if (!skipLabel) {
        FillWindowPixelBuffer(window, 0);
        String *label = NewString_ReadMsgData(app->msgData, msg_0010_00080);
        AddTextPrinterParameterizedWithColor(window, 0, label, 4, 0, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(1, 2, 0), NULL);
        String_Delete(label);
    } else {
        FillWindowPixelRect(window, 0, 0, 16, 72, 16);
    }
    String *template = NewString_ReadMsgData(app->msgData, msg_0010_00081);
    u32 money = PlayerProfile_GetMoney(app->profile);
    BufferIntegerAsString(app->msgFormat, 0, money, 6, PRINTING_MODE_RIGHT_ALIGN, TRUE);
    StringExpandPlaceholders(app->msgFormat, string, template);
    u32 width = FontID_String_GetWidth(0, string, 0);
    AddTextPrinterParameterizedWithColor(window, 0, string, 68 - (width + 8), 16, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(1, 2, 0), NULL);
    ScheduleWindowCopyToVram(window);
    String_Delete(template);
    String_Delete(string);
}

void BagApp_PrintMsg115And116(BagApp *app) {
    Window *window = &app->windows[5];
    FillWindowPixelBuffer(window, 15);
    DrawFrameAndWindow1(window, TRUE, 0x3F7, 14);
    String *string = NewString_ReadMsgData(app->msgData, msg_0010_00115);
    AddTextPrinterParameterized(window, 0, string, 0, 0, TEXT_SPEED_NOTRANSFER, NULL);
    String_Delete(string);
    string = NewString_ReadMsgData(app->msgData, msg_0010_00116);
    BufferIntegerAsString(app->msgFormat, 0, 0, 3, PRINTING_MODE_RIGHT_ALIGN, TRUE);
    StringExpandPlaceholders(app->msgFormat, app->msgBuf, string);
    String_Delete(string);
    u32 width = FontID_String_GetWidth(0, app->msgBuf, 0);
    AddTextPrinterParameterized(window, 0, app->msgBuf, 88 - width, 16, TEXT_SPEED_NOTRANSFER, NULL);
    ScheduleWindowCopyToVram(window);
}

void BagApp_PrintCancelButton(BagApp *app, BOOL left) {
    String *string = NewString_ReadMsgData(app->msgData, msg_0010_00008);
    FillWindowPixelBuffer(&app->windows[7], 0);
    if (!left) {
        u32 width = FontID_String_GetWidth(0, string, 0);
        AddTextPrinterParameterizedWithColor(&app->windows[7], 0, string, ((48 - width) / 2) + 8, 0, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(15, 14, 0), NULL);
    } else {
        AddTextPrinterParameterizedWithColor(&app->windows[7], 0, string, 5, 0, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(15, 14, 0), NULL);
    }
    ScheduleWindowCopyToVram(&app->windows[7]);
    String_Delete(string);
}

static int BagApp_GetNthItemSlotIndex(BagViewPocket *pocket, int pocketId, int n) {
    int i;
    int count;
    count = 0;
    for (i = 0; i < gListPocketCapacities[pocketId]; i++) {
        if (pocket->slots[i].id != ITEM_NONE && pocket->slots[i].quantity != 0) {
            count++;
            if (count == n + 1) {
                break;
            }
        }
    }
    return i;
}

// The item list alternates between two banks of six windows, so a page is drawn before the old one is cleared.
void BagApp_DrawItemList(BagApp *app, int scroll, int unused, BOOL dim) {
    int i, k, count;
    BagViewPocket *pocket;
    int limit;
    u8 first, last;

    pocket = &app->bagView->pockets[app->bagView->pocketIndex];
    limit = pocket->itemCount - pocket->scroll;
    if (limit > BAG_ITEMS_PER_PAGE) {
        limit = BAG_ITEMS_PER_PAGE;
    }
    if (app->listBank == 0) {
        first = 0;
        last = BAG_ITEMS_PER_PAGE;
    } else {
        first = BAG_ITEMS_PER_PAGE;
        last = 0;
    }
    app->listBank ^= 1;
    BagApp_AddItemListWindows(app);
    for (i = 0; i < BAG_ITEMS_PER_PAGE; i++) {
        FillWindowPixelBuffer(&app->dynWindows[first + i], 0);
        ClearWindowTilemapAndScheduleTransfer(&app->dynWindows[last + i]);
    }
    count = 0;
    k = BagApp_GetNthItemSlotIndex(pocket, app->bagView->pocketIndex, scroll);
    for (; k < gListPocketCapacities[app->bagView->pocketIndex]; k++) {
        if (pocket->slots[k].id != ITEM_NONE && pocket->slots[k].quantity != 0) {
            if (!dim) {
                BagApp_PrintItemListEntry(app, &app->dynWindows[first + count], app->itemNames[k], pocket, k);
            } else if (k == app->moveSource) {
                BagApp_PrintItemListEntry(app, &app->dynWindows[first + count], app->itemNames[k], pocket, k);
            } else {
                AddTextPrinterParameterizedWithColor(&app->dynWindows[first + count], 0, app->itemNames[k], 0, 16, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(1, 2, 0), NULL);
            }
            if (++count >= limit) {
                break;
            }
        }
    }
    for (i = 0; i < BAG_ITEMS_PER_PAGE; i++) {
        ScheduleWindowCopyToVram(&app->dynWindows[first + i]);
    }
}

void BagApp_DrawSelectedItemEntry(BagApp *app, s32 scroll, s32 slot) {
    u32 index = slot;
    BagView *bagView = app->bagView;
    BagViewPocket *pocket = &bagView->pockets[bagView->pocketIndex];

    index += scroll;

    s32 i = 0;
    Window *window = &app->dynWindows[0];

    for (; i < BAG_ITEMS_PER_PAGE; i++) {
        ClearWindowTilemapAndScheduleTransfer(window);
        window++;
    }
    BagApp_RemoveItemListWindows(app);
    ClearWindowTilemapAndScheduleTransfer(&app->windows[6]);
    BagApp_AddItemActionWindows(app);
    BagApp_PrintItemListEntry(app, &app->dynWindows[12], app->itemNames[index], pocket, index);
    ScheduleWindowCopyToVram(&app->dynWindows[12]);
}

void BagApp_ClearSelectedItemEntry(BagApp *app) {
    ClearWindowTilemapAndScheduleTransfer(&app->dynWindows[12]);
}

static void BagApp_PrintItemListEntry(BagApp *app, Window *window, String *name, BagViewPocket *pocket, int index) {
    switch (pocket->pocketId) {
    case POCKET_TMHMS:
        AddTextPrinterParameterizedWithColor(window, 0, name, 0, 0, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(1, 2, 0), NULL);
        BagApp_PrintTMHMNumber(app, window, &pocket->slots[index], 16);
        if (pocket->slots[index].id >= ITEM_TM01 && pocket->slots[index].id <= ITEM_TM92) {
            BagApp_PrintItemQuantity(app->msgFormat, app->msgData, window, pocket->slots[index].quantity);
        }
        break;
    case POCKET_KEY_ITEMS:
        AddTextPrinterParameterizedWithColor(window, 0, name, 0, 0, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(1, 2, 0), NULL);
        if (pocket->slots[index].id == Bag_GetRegisteredItem1(app->bag)) {
            BagApp_DrawRegisteredItemIcon(app, window, 16, FALSE);
        }
        if (pocket->slots[index].id == Bag_GetRegisteredItem2(app->bag)) {
            BagApp_DrawRegisteredItemIcon(app, window, 16, TRUE);
        }
        break;
    default:
        AddTextPrinterParameterizedWithColor(window, 0, name, 0, 0, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(1, 2, 0), NULL);
        BagApp_PrintItemQuantity(app->msgFormat, app->msgData, window, pocket->slots[index].quantity);
        break;
    }
}

static void BagApp_PrintItemQuantity(MessageFormat *msgFormat, MsgData *msgData, Window *window, int quantity) {
    BufferIntegerAsString(msgFormat, 0, quantity, 3, PRINTING_MODE_LEFT_ALIGN, TRUE);
    String *string = ReadMsgData_ExpandPlaceholders(msgFormat, msgData, 87, HEAP_ID_6);
    AddTextPrinterParameterizedWithColor(window, 0, string, 48, 16, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(1, 2, 0), NULL);
    String_Delete(string);
}

void BagApp_PrintPageNumber(BagApp *app, s32 itemCount, s32 scroll, s32 slot) {
    s32 page = (scroll + slot) / BAG_ITEMS_PER_PAGE;
    s32 numPages;
    if (itemCount == 0) {
        numPages = 1;
    } else {
        numPages = (itemCount + BAG_ITEMS_PER_PAGE - 1) / BAG_ITEMS_PER_PAGE;
    }
    FillWindowPixelBuffer(&app->windows[6], 0);
    BufferIntegerAsString(app->msgFormat, 0, page + 1, 3, PRINTING_MODE_RIGHT_ALIGN, TRUE);
    BufferIntegerAsString(app->msgFormat, 1, numPages, 3, PRINTING_MODE_RIGHT_ALIGN, TRUE);
    String *string = ReadMsgData_ExpandPlaceholders(app->msgFormat, app->msgData, msg_0010_00022, HEAP_ID_6);
    AddTextPrinterParameterizedWithColor(&app->windows[6], 0, string, 0, 0, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(15, 1, 0), NULL);
    ScheduleWindowCopyToVram(&app->windows[6]);
    String_Delete(string);
}

void BagApp_PrintItemMenuLabel(Window *window, String **strings, u8 action) {
    FillWindowPixelBuffer(window, 0);
    if (action != BAG_ITEM_ACTION_NONE) {
        String **string = &strings[action];
        u32 width;
        width = GetWindowWidth(window);
        u32 textWidth = FontID_String_GetWidth(0, *string, 0);
        AddTextPrinterParameterizedWithColor(window, 0, *string, ((width * 8) - textWidth) / 2, 0, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(15, 14, 0), NULL);
    }
    ScheduleWindowCopyToVram(window);
}

void BagApp_ClearItemMenuLabels(Window *windows) {
    s32 i;
    for (i = 0; i < 4; i++) {
        ClearWindowTilemapAndScheduleTransfer(&windows[i]);
    }
}

void BagApp_PrintTrashLabel(BagApp *app) {
    AddTextPrinterParameterizedWithColor(&app->dynWindows[20], 0, app->menuStrings[BAG_ITEM_ACTION_TRASH], 5, 0, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(15, 14, 0), NULL);
    ScheduleWindowCopyToVram(&app->dynWindows[20]);
}

void BagApp_PrintSellLabel(BagApp *app) {
    AddTextPrinterParameterizedWithColor(&app->dynWindows[20], 0, app->menuStrings[BAG_ITEM_ACTION_SELL], 5, 0, TEXT_SPEED_NOTRANSFER, MAKE_TEXT_COLOR(15, 14, 0), NULL);
    ScheduleWindowCopyToVram(&app->dynWindows[20]);
}

void BagApp_ClearQuantityLabel(BagApp *app) {
    ClearWindowTilemapAndScheduleTransfer(&app->dynWindows[20]);
}

void BagApp_ClearCancelButtonText(BagApp *app) {
    ClearWindowTilemapAndScheduleTransfer(&app->windows[7]);
}

void BagApp_InitSprites(BagApp *app) {
    GfGfx_EngineATogglePlanes(GX_PLANEMASK_OBJ, GF_PLANE_TOGGLE_ON);
    GfGfx_EngineBTogglePlanes(GX_PLANEMASK_OBJ, GF_PLANE_TOGGLE_ON);
    GF_CreateVramTransferManager(32, HEAP_ID_6);
    BagApp_CreateSpriteSystem(app);
    BagApp_LoadSpriteResources(app);
    BagApp_CreateSprites(app);
    G2dRenderer_SetSubSurfaceCoords(SpriteSystem_GetRenderer(app->spriteSystem), 0, FX32_CONST(256));
}

void BagApp_FreeSprites(BagApp *app) {
    u32 i;
    for (i = 0; i < NELEMS(app->sprites); i++) {
        Sprite_DeleteAndFreeResources(app->sprites[i]);
    }
    SpriteSystem_FreeResourcesAndManager(app->spriteSystem, app->spriteManager);
    SpriteSystem_Free(app->spriteSystem);
    Heap_Free(app->objPaletteBuffer);
}

void BagApp_TickSprites(BagApp *app) {
    u32 i;
    for (i = 0; i < NELEMS(app->sprites); i++) {
        ManagedSprite_TickFrame(app->sprites[i]);
    }
}

static void BagApp_SetItemIconSprite(BagApp *app, int slot, u16 itemId) {
    SpriteSystem_ReplaceCharResObj(app->spriteSystem, app->spriteManager, NARC_itemtool_itemdata_item_icon, GetItemIndexMapping(itemId, ITEMNARC_NCGR), FALSE, slot + 0xC0FC);
    SpriteSystem_ReplacePlttResObj(app->spriteSystem, app->spriteManager, NARC_itemtool_itemdata_item_icon, GetItemIndexMapping(itemId, ITEMNARC_NCLR), FALSE, slot + 0xC0FB);
}

void BagApp_SetField648(BagApp *app) {
    app->unk_64B = 0;
    app->unk_648 = 1;
}

void BagApp_UpdateField648(BagApp *app) {
    switch (app->unk_648) {
    case 1:
        BagApp_ClearField648(app);
        break;
    case 0:
        break;
    }
}

static void BagApp_ShowTMHMTypeIcons(BagApp *app, u16 itemId, BOOL show) {
    u16 move;
    u16 type;
    u16 category;
    NarcId narc;

    ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_TM_TYPE], show);
    ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_TM_CATEGORY], show);
    if (show) {
        move = TMHMGetMove(itemId);
        type = GetMoveAttr(move, MOVEATTR_TYPE);
        category = GetMoveAttr(move, MOVEATTR_CLASS);
        narc = TypeIcon_GetNarc();
        SpriteSystem_ReplaceCharResObj(app->spriteSystem, app->spriteManager, narc, TypeIcon_GetCharFile(type), TRUE, 0xC103);
        ManagedSprite_SetPaletteOverride(app->sprites[BAG_SPRITE_TM_TYPE], TypeIcon_GetPalette(type) + 4);
        narc = MoveCategoryIcon_GetNarc();
        SpriteSystem_ReplaceCharResObj(app->spriteSystem, app->spriteManager, narc, MoveCategoryIcon_GetCharFile(category), TRUE, 0xC104);
        ManagedSprite_SetPaletteOverride(app->sprites[BAG_SPRITE_TM_CATEGORY], MoveCategoryIcon_GetPalette(category) + 4);
    }
}

static void BagApp_CreateSpriteSystem(BagApp *app) {
    SpriteResourceCountsListUnion counts = gBagSpriteResourceCounts;
    app->spriteSystem = SpriteSystem_Alloc(HEAP_ID_6);
    app->spriteManager = SpriteManager_New(app->spriteSystem);
    OamManagerParam oamManagerParam = gBagOamManagerParam;
    OamCharTransferParam oamCharTransferParam = gBagOamCharTransferParam;
    SpriteSystem_Init(app->spriteSystem, &oamManagerParam, &oamCharTransferParam, 32);
    SpriteSystem_InitSprites(app->spriteSystem, app->spriteManager, NELEMS(app->sprites));
    SpriteSystem_InitManagerWithCapacities(app->spriteSystem, app->spriteManager, &counts);
}

static void BagApp_LoadSpriteResources(BagApp *app) {
    int i;

    SpriteSystem_LoadCharResObj(app->spriteSystem, app->spriteManager, NARC_a_0_1_5, 26, FALSE, NNS_G2D_VRAM_TYPE_2DMAIN, 0xC0F9);
    SpriteSystem_LoadCharResObj(app->spriteSystem, app->spriteManager, NARC_a_0_1_5, 6, FALSE, NNS_G2D_VRAM_TYPE_2DMAIN, 0xC0FA);
    SpriteSystem_LoadCharResObj(app->spriteSystem, app->spriteManager, NARC_a_0_1_5, 51, FALSE, NNS_G2D_VRAM_TYPE_2DSUB, 0xC0FB);
    SpriteSystem_LoadCharResObj(app->spriteSystem, app->spriteManager, NARC_a_0_6_0, 4, FALSE, NNS_G2D_VRAM_TYPE_2DMAIN, 0xC102);
    for (i = 0; i < BAG_ITEMS_PER_PAGE; i++) {
        SpriteSystem_LoadCharResObj(app->spriteSystem, app->spriteManager, NARC_itemtool_itemdata_item_icon, GetItemIndexMapping(ITEM_NONE, ITEMNARC_NCGR), FALSE, NNS_G2D_VRAM_TYPE_2DSUB, 0xC0FC + i);
    }
    TypeIcon_LoadChar(app->spriteSystem, app->spriteManager, NNS_G2D_VRAM_TYPE_2DMAIN, 0, 0xC103);
    MoveCategoryIcon_LoadChar(app->spriteSystem, app->spriteManager, NNS_G2D_VRAM_TYPE_2DMAIN, 0, 0xC104);
    SpriteSystem_LoadPlttResObj(app->spriteSystem, app->spriteManager, NARC_a_0_1_5, 15, FALSE, 2, NNS_G2D_VRAM_TYPE_2DMAIN, 0xC0F9);
    SpriteSystem_LoadPlttResObj(app->spriteSystem, app->spriteManager, NARC_a_0_6_0, 10, FALSE, 2, NNS_G2D_VRAM_TYPE_2DMAIN, 0xC101);
    TypeIcon_LoadPltt(app->spriteSystem, app->spriteManager, NNS_G2D_VRAM_TYPE_2DMAIN, 0xC102);
    SpriteSystem_LoadPlttResObj(app->spriteSystem, app->spriteManager, NARC_a_0_1_5, 47, FALSE, 10, NNS_G2D_VRAM_TYPE_2DSUB, 0xC0FA);
    for (i = 0; i < BAG_ITEMS_PER_PAGE; i++) {
        SpriteSystem_LoadPlttResObj(app->spriteSystem, app->spriteManager, NARC_itemtool_itemdata_item_icon, GetItemIndexMapping(ITEM_NONE, ITEMNARC_NCLR), FALSE, 1, NNS_G2D_VRAM_TYPE_2DSUB, 0xC0FB + i);
    }
    SpriteSystem_LoadCellResObj(app->spriteSystem, app->spriteManager, NARC_a_0_1_5, 25, FALSE, 0xC0F9);
    SpriteSystem_LoadCellResObj(app->spriteSystem, app->spriteManager, NARC_a_0_1_5, 5, FALSE, 0xC0FA);
    SpriteSystem_LoadCellResObj(app->spriteSystem, app->spriteManager, NARC_a_0_1_5, 49, FALSE, 0xC0FB);
    SpriteSystem_LoadCellResObj(app->spriteSystem, app->spriteManager, NARC_itemtool_itemdata_item_icon, GetItemIconCell(), FALSE, 0xC0FC);
    SpriteSystem_LoadCellResObj(app->spriteSystem, app->spriteManager, NARC_a_0_6_0, 5, FALSE, 0xC0FD);
    SpriteSystem_LoadAnimResObj(app->spriteSystem, app->spriteManager, NARC_a_0_1_5, 21, FALSE, 0xC0F9);
    SpriteSystem_LoadAnimResObj(app->spriteSystem, app->spriteManager, NARC_a_0_1_5, 24, FALSE, 0xC0FA);
    SpriteSystem_LoadAnimResObj(app->spriteSystem, app->spriteManager, NARC_a_0_1_5, 4, FALSE, 0xC0FB);
    SpriteSystem_LoadAnimResObj(app->spriteSystem, app->spriteManager, NARC_a_0_1_5, 50, FALSE, 0xC0FC);
    SpriteSystem_LoadAnimResObj(app->spriteSystem, app->spriteManager, NARC_itemtool_itemdata_item_icon, GetItemIconAnim(), FALSE, 0xC0FD);
    SpriteSystem_LoadAnimResObj(app->spriteSystem, app->spriteManager, NARC_a_0_6_0, 6, FALSE, 0xC0FE);
    TypeIcon_LoadCellAnim(app->spriteSystem, app->spriteManager, 0xC0FE, 0xC0FF);
    app->objPaletteBuffer = GfGfxLoader_GetPlttData(NARC_a_0_1_5, 48, &app->objPalette, HEAP_ID_6);
#if defined(SDK_PORT)
    GF_ASSERT(app->objPalette->szByte <= sizeof(app->pocketObjColors));
    memcpy(app->pocketObjColors, app->objPalette->pRawData, app->objPalette->szByte);
    app->objPalette->pRawData = app->pocketObjColors;
#endif
}

static void BagApp_CreateSprites(BagApp *app) {
    const ManagedSpriteTemplate *template = gBagSpriteTemplates;
    u32 i;

    for (i = 0; i < NELEMS(app->sprites); i++) {
        app->sprites[i] = SpriteSystem_NewSpriteWithYOffset(app->spriteSystem, app->spriteManager, template, FX32_CONST(256));
        template ++;
    }
    ManagedSprite_SetPriority(app->sprites[BAG_SPRITE_CANCEL], 1);
    for (i = 0; i < 4; i++) {
        ManagedSprite_SetPriority(app->sprites[BAG_SPRITE_ITEM_MENU_ROWS + i], 1);
    }
    for (i = 0; i < POCKETS_COUNT; i++) {
        ManagedSprite_SetPriority(app->sprites[BAG_SPRITE_POCKET_TABS + i], 1);
    }
    BagApp_ShowPocketTabSprites(app, TRUE);
    ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_MOVE_MARKER], FALSE);
    ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_TM_TYPE], FALSE);
    ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_TM_CATEGORY], FALSE);
    for (i = 0; i < 4; i++) {
        ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_ITEM_MENU_ROWS + i], FALSE);
    }
    for (i = 0; i < 6; i++) {
        ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_QUANTITY_UP + i], FALSE);
    }
    ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_CONFIRM], FALSE);
    ManagedSprite_SetPriority(app->sprites[BAG_SPRITE_CONFIRM], 1);
}

static void BagApp_ClearField648(BagApp *app) {
    app->unk_648 = 0;
}

void BagApp_SetCursorPosition(BagApp *app, s32 button) {
    ManagedSprite_SetPositionXYWithSubscreenOffset(app->sprites[BAG_SPRITE_CURSOR], gBagAppCursorPositions[button].x, gBagAppCursorPositions[button].y, FX32_CONST(256));
    ManagedSprite_SetAnim(app->sprites[BAG_SPRITE_CURSOR], gBagAppCursorPositions[button].anim);
    ManagedSprite_SetPaletteOverride(app->sprites[BAG_SPRITE_CURSOR], gBagAppCursorPositions[button].palette);
    ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_CURSOR], TRUE);
}

void BagApp_HideCursor(BagApp *app) {
    ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_CURSOR], FALSE);
}

void BagApp_SetMoveCursorPosition(BagApp *app, int slot) {
    GF_ASSERT(slot < 9);
    if (slot == 8) {
        ManagedSprite_SetAnim(app->sprites[BAG_SPRITE_CURSOR], gBagAppMoveCursorPositions[slot].anim);
    } else {
        BagView *bagView = app->bagView;
        BagViewPocket *pocket = &bagView->pockets[bagView->pocketIndex];
        int index = pocket->scroll + slot;
        if (index == app->moveSource) {
            ManagedSprite_SetAnim(app->sprites[BAG_SPRITE_CURSOR], 10);
        } else if (index >= pocket->itemCount) {
            ManagedSprite_SetAnim(app->sprites[BAG_SPRITE_CURSOR], 40);
        } else {
            ManagedSprite_SetAnim(app->sprites[BAG_SPRITE_CURSOR], 20);
        }
    }
    ManagedSprite_SetPositionXYWithSubscreenOffset(app->sprites[BAG_SPRITE_CURSOR], gBagAppMoveCursorPositions[slot].x, gBagAppMoveCursorPositions[slot].y, FX32_CONST(256));
    ManagedSprite_SetPaletteOverride(app->sprites[BAG_SPRITE_CURSOR], gBagAppMoveCursorPositions[slot].palette);
}

void BagApp_SetQuantityCursorPosition(BagApp *app, s32 index) {
    GF_ASSERT(index < 8);
    ManagedSprite_SetPositionXYWithSubscreenOffset(app->sprites[BAG_SPRITE_CURSOR], gBagAppQuantityCursorPositions[index].x, gBagAppQuantityCursorPositions[index].y, FX32_CONST(256));
    ManagedSprite_SetAnim(app->sprites[BAG_SPRITE_CURSOR], gBagAppQuantityCursorPositions[index].anim);
    ManagedSprite_SetPaletteOverride(app->sprites[BAG_SPRITE_CURSOR], gBagAppQuantityCursorPositions[index].palette);
}

void BagApp_LoadPocketObjPalette(BagApp *app, int pocket) {
    if (pocket <= POCKET_KEY_ITEMS) {
        u16 *palette = app->objPalette->pRawData;
        GXS_LoadOBJPltt(&palette[0x80], 0, 0x100);
        GXS_LoadOBJPltt(&palette[pocket * 16], pocket * 32, 32);
    }
}

void BagApp_ShowMoveSlotSprites(BagApp *app, int count, int movingSlot, BOOL hideLast) {
    int i;
    if (count == 0) {
        for (i = 0; i < BAG_ITEMS_PER_PAGE; i++) {
            ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_SLOT_FRAMES + i], FALSE);
        }
        ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_SLOT_FRAMES + BAG_ITEMS_PER_PAGE], FALSE);
        return;
    }
    for (i = 0; i < BAG_ITEMS_PER_PAGE; i++) {
        if (i < count) {
            ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_SLOT_FRAMES + i], TRUE);
        } else {
            ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_SLOT_FRAMES + i], FALSE);
        }
    }
    if (movingSlot >= 0) {
        movingSlot += 21;
        ManagedSprite_SetDrawFlag(app->sprites[movingSlot], FALSE);
    }
    if (hideLast) {
        ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_SLOT_FRAMES + BAG_ITEMS_PER_PAGE], FALSE);
    }
}

static void BagApp_UpdatePageArrows(BagApp *app) {
    BagView *bagView = app->bagView;
    if (bagView->pockets[bagView->pocketIndex].itemCount <= BAG_ITEMS_PER_PAGE) {
        ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_PREV_PAGE], FALSE);
        ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_NEXT_PAGE], FALSE);
    } else {
        ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_PREV_PAGE], TRUE);
        ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_NEXT_PAGE], TRUE);
    }
}

void BagApp_DrawItemIcons(BagApp *app, BagViewPocket *pocket, int count, BOOL update) {
    int i;
    for (i = 0; i < BAG_ITEMS_PER_PAGE; i++) {
        ManagedSprite_SetPositionXYWithSubscreenOffset(app->sprites[BAG_SPRITE_ITEM_ICONS + i], gBagSpriteTemplates[BAG_SPRITE_ITEM_ICONS + i].x, gBagSpriteTemplates[BAG_SPRITE_ITEM_ICONS + i].y, FX32_CONST(256));
        if (i < count) {
            if (update) {
                BagApp_SetItemIconSprite(app, i, app->itemIds[pocket->scroll + i]);
            }
            ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_ITEM_ICONS + i], TRUE);
        } else {
            ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_ITEM_ICONS + i], FALSE);
        }
    }
    BagApp_UpdatePageArrows(app);
}

void BagApp_ShowMovingItemIcon(BagApp *app, BagViewPocket *pocket, int index) {
    int slot = -1;
    int i;
    if (pocket->scroll == (index / BAG_ITEMS_PER_PAGE) * BAG_ITEMS_PER_PAGE) {
        slot = index % BAG_ITEMS_PER_PAGE;
    }
    for (i = 0; i < BAG_ITEMS_PER_PAGE; i++) {
        ManagedSprite_SetPositionXYWithSubscreenOffset(app->sprites[BAG_SPRITE_ITEM_ICONS + i], gBagSpriteTemplates[BAG_SPRITE_ITEM_ICONS + i].x, gBagSpriteTemplates[BAG_SPRITE_ITEM_ICONS + i].y, FX32_CONST(256));
        if (i == slot) {
            ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_ITEM_ICONS + i], TRUE);
        } else {
            ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_ITEM_ICONS + i], FALSE);
        }
    }
    BagApp_UpdatePageArrows(app);
}

void BagApp_ShowItemMenuSprites(BagApp *app, const u8 *actions) {
    s32 i;
    ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_CURSOR], TRUE);
    for (i = 0; i < 4; i++) {
        if (actions[i] != BAG_ITEM_ACTION_NONE) {
            ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_ITEM_MENU_ROWS + i], TRUE);
        } else {
            ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_ITEM_MENU_ROWS + i], FALSE);
        }
    }
    ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_PREV_PAGE], FALSE);
    ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_NEXT_PAGE], FALSE);
}

void BagApp_HideItemMenuSprites(BagApp *app) {
    s32 i;
    for (i = 0; i < 4; i++) {
        ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_ITEM_MENU_ROWS + i], FALSE);
    }
}

void BagApp_ShowSelectedItemIcon(BagApp *app, int slot) {
    int i;
    for (i = 0; i < BAG_ITEMS_PER_PAGE; i++) {
        if (slot != i) {
            ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_ITEM_ICONS + i], FALSE);
        } else {
            ManagedSprite_SetPositionXYWithSubscreenOffset(app->sprites[BAG_SPRITE_ITEM_ICONS + i], 86, 76, FX32_CONST(256));
        }
    }
}

// 0: all three digits can be stepped, 1: max is below 100 (hide the hundreds), 2: max is below 10 (ones only)
s32 BagApp_GetUnusedDigitCount(s32 max) {
    s32 ret = 0;
    if (max < 100) {
        ret = 1;
    }
    if (max < 10) {
        ret = 2;
    }
    return ret;
}

// numDigits 2: sell, 3: trash
void BagApp_ShowQuantityButtons(BagApp *app, int numDigits, int max) {
    int i;
    int unusedDigits;

    if (numDigits == 2 && max > 99) {
        max = 99;
    }
    for (i = 0; i < gQuantityButtonCounts[numDigits - 2]; i++) {
        ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_QUANTITY_UP + gQuantityButtonSprites[numDigits - 2][i]], TRUE);
        ManagedSprite_SetAnim(app->sprites[BAG_SPRITE_QUANTITY_UP + gQuantityButtonSprites[numDigits - 2][i]], gQuantityButtonAnims[numDigits - 2][i]);
    }
    unusedDigits = BagApp_GetUnusedDigitCount(max);
    if (unusedDigits != 0) {
        if (numDigits - 2 == 0 && unusedDigits == 2) {
            ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_QUANTITY_UP], FALSE);
            ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_QUANTITY_DOWN], FALSE);
        } else if (numDigits - 2 == 1) {
            for (i = 0; i < gHiddenQuantityButtonCounts[unusedDigits - 1]; i++) {
                ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_QUANTITY_UP + gHiddenQuantityButtons[unusedDigits - 1][i]], FALSE);
            }
        }
    }
    ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_CONFIRM], TRUE);
    ManagedSprite_SetAnimationFrame(app->sprites[BAG_SPRITE_CONFIRM], 0);
    ManagedSprite_SetAnim(app->sprites[BAG_SPRITE_CONFIRM], 37);
    ManagedSprite_SetAnimationFrame(app->sprites[BAG_SPRITE_CANCEL], 0);
    ManagedSprite_SetAnim(app->sprites[BAG_SPRITE_CANCEL], 39);
}

void BagApp_HideQuantityButtons(BagApp *app) {
    s32 i;
    for (i = 0; i < 6; i++) {
        ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_QUANTITY_UP + i], FALSE);
    }
    ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_CONFIRM], FALSE);
}

void BagApp_ShowPocketTabSprites(BagApp *app, BOOL show) {
    u8 hasPocket[8];
    int i;

    GF_ASSERT(show == TRUE || show == FALSE);
    MI_CpuFill8(hasPocket, 0, sizeof(hasPocket));
    for (i = 0; i < POCKETS_COUNT; i++) {
        GF_ASSERT(app->bagView->pockets[i].pocketId < POCKETS_COUNT);
        if (app->bagView->pockets[i].slots != NULL) {
            hasPocket[app->bagView->pockets[i].pocketId] = TRUE;
        }
    }
    for (i = 0; i < POCKETS_COUNT; i++) {
        if (hasPocket[i]) {
            ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_POCKET_TABS + i], show);
        } else {
            ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_POCKET_TABS + i], FALSE);
        }
    }
}

void BagApp_SetCancelSpriteVisible(BagApp *app, BOOL show) {
    ManagedSprite_SetDrawFlag(app->sprites[BAG_SPRITE_CANCEL], show);
}
