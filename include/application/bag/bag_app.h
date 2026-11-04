#ifndef POKEHEARTGOLD_APPLICATION_BAG_BAG_APP_H
#define POKEHEARTGOLD_APPLICATION_BAG_BAG_APP_H

#include "bag_types_def.h"
#include "bg_window.h"
#include "camera.h"
#include "filesystem.h"
#include "message_format.h"
#include "message_printer.h"
#include "msgdata.h"
#include "options.h"
#include "overlay_manager.h"
#include "player_data.h"
#include "pm_string.h"
#include "sprite_system.h"
#include "yes_no_prompt.h"

// Work of the Bag application: the touch-screen bag with the 3D bag model on the top screen.
// Launched with a BagView (BagView.pocketIndex: shown pocket, mode: launch mode, selectedAction: exit action).

#define BAG_ITEMS_PER_PAGE 6

// BagApp.cursorButton: the pocket tabs, the item slots of the shown page, the page arrows and CANCEL
enum BagAppButton {
    BAG_BUTTON_LAST_POCKET = 7,
    BAG_BUTTON_FIRST_SLOT,
    BAG_BUTTON_LAST_SLOT = BAG_BUTTON_FIRST_SLOT + BAG_ITEMS_PER_PAGE - 1,
    BAG_BUTTON_PREV_PAGE,
    BAG_BUTTON_NEXT_PAGE,
    BAG_BUTTON_CANCEL,
    BAG_BUTTON_CURRENT_POCKET, // navigation target: the shown pocket's tab
};

enum BagAppSprite {
    BAG_SPRITE_MOVE_MARKER,
    BAG_SPRITE_ITEM_ICONS,
    BAG_SPRITE_TM_TYPE = BAG_SPRITE_ITEM_ICONS + BAG_ITEMS_PER_PAGE,
    BAG_SPRITE_TM_CATEGORY,
    BAG_SPRITE_POCKET_TABS,
    BAG_SPRITE_PREV_PAGE = BAG_SPRITE_POCKET_TABS + 8,
    BAG_SPRITE_NEXT_PAGE,
    BAG_SPRITE_CANCEL,
    BAG_SPRITE_CURSOR,
    BAG_SPRITE_SLOT_FRAMES,
    BAG_SPRITE_ITEM_MENU_ROWS = BAG_SPRITE_SLOT_FRAMES + BAG_ITEMS_PER_PAGE + 1,
    BAG_SPRITE_QUANTITY_UP = BAG_SPRITE_ITEM_MENU_ROWS + 4, // hundreds, tens, ones
    BAG_SPRITE_QUANTITY_DOWN = BAG_SPRITE_QUANTITY_UP + 3,
    BAG_SPRITE_CONFIRM = BAG_SPRITE_QUANTITY_DOWN + 3,
    BAG_SPRITE_COUNT,
};

// Item menu entries: index into gItemActionFuncs and BagApp.menuStrings
enum BagAppItemAction {
    BAG_ITEM_ACTION_USE,
    BAG_ITEM_ACTION_WALK,
    BAG_ITEM_ACTION_CHECK,
    BAG_ITEM_ACTION_PLANT_BERRY,
    BAG_ITEM_ACTION_POFFIN_CASE,
    BAG_ITEM_ACTION_TRASH,
    BAG_ITEM_ACTION_REGISTER,
    BAG_ITEM_ACTION_DESELECT,
    BAG_ITEM_ACTION_GIVE,
    BAG_ITEM_ACTION_CONFIRM = 10,
    BAG_ITEM_ACTION_CANCEL,
    BAG_ITEM_ACTION_MOVE,
    BAG_ITEM_ACTION_SELL,
    BAG_ITEM_ACTION_CHOOSE_FOR_BERRY_POT,
    BAG_ITEM_ACTION_STOP,
    BAG_ITEM_ACTION_COUNT,
    BAG_ITEM_ACTION_NONE = 0xFF,
};

// The pocket-switch screen (touch and hold a pocket tab, then slide)
typedef struct BagAppPocketSwitch {
    u8 pocket;
    u8 direction; // 0 left, 1 right, 2 none
    u8 stage;
    u8 timer;
    u8 unk_4;
    u8 unk_5[2];
    u8 flags;
} BagAppPocketSwitch;

// The 3D bag model: one open/close animation pair per pocket and a common extra animation
typedef struct BagAppModel {
    NNSG3dRenderObj renderObj;
    NNSG3dResMdl *model;
    NNSG3dResFileHeader *modelData;
    void *pocketAnimData[8];
    void *pocketAnimData2[8];
    void *extraAnimData;
    NNSG3dAnmObj *pocketAnims[8];
    NNSG3dAnmObj *pocketAnims2[8];
    NNSG3dAnmObj *extraAnim;
    u32 pocket;
} BagAppModel;

// Camera turn between the pockets' key angles
typedef struct BagAppCameraTween {
    u32 from;
    u32 to;
    u32 queued; // next target once this turn ends, -1 for none
    u16 frame;
    u16 duration;
} BagAppCameraTween;

// Top screen 3D scene
typedef struct BagAppScene {
    NNSFndAllocator allocator;
    Camera *camera;
    BagAppModel model;
    VecFx32 cameraTarget;
    CameraParam cameraParam;
    BagAppCameraTween tween;
    VecFx32 modelPos;
} BagAppScene;

// Flash (palette) or animation of a pressed button sprite, then a switch to `nextState`
typedef struct BagAppButtonFeedback {
    u8 sprite;
    u8 flashPalette : 4;
    u8 restorePalette : 4;
    u8 isAnim;
    u8 stage : 4;
    u8 timer : 4;
    int nextState;
} BagAppButtonFeedback;

typedef struct BagApp {
    BgConfig *bgConfig;
    Window windows[11];    // 0 item description, 1 TM/HM move info, 2 prompt, 3 message box, 7 CANCEL
    Window dynWindows[24]; // added while needed: 0-11 item list, 12 selected entry, 13-16 item menu labels,
                           // 17-19 quantity digits, 20 quantity label, 21 wide message, 22 money, 23 sell price
    BagView *bagView;
    Bag *bag;
    PlayerProfile *profile;
    Options *options;
    NARC *narc;
    SpriteSystem *spriteSystem;
    SpriteManager *spriteManager;
    ManagedSprite *sprites[BAG_SPRITE_COUNT];
    MessagePrinter *msgPrinter;
    MsgData *msgData;
    MessageFormat *msgFormat;
    MsgData *itemNamesMsgData;
    MsgData *moveNamesMsgData;
    String *menuStrings[BAG_ITEM_ACTION_COUNT];
    u8 unk_340[8];
    u32 unk_348;
    u8 unk_34C[4];
    String *itemNames[165];
    String *msgBuf;
    String *unk_5E8;
    String *quantityTemplate;
    u8 unk_5F0[4];
    String *unusedStrings[8];
    u8 numPockets;
    u8 trainerGender;
    u8 printerId;
    u8 unk_617;
    u8 unk_618;
    BagAppPocketSwitch pocketSwitch;
    u8 unk_621[0x23];
    int cursorButton; // enum BagAppButton
    u8 unk_648;
    u8 unk_649[2];
    u8 unk_64B;
    u8 unk_64C[0x20];
    u32 menuCursor; // item menu row, or the target slot while moving an item
    u8 switchPocket;
    u8 isMovingItem;
    u8 moveSource; // pocket index of the item being moved
    int unk_674;
    u8 unk_678[3];
    u8 taskState;
    int (*task)(struct BagApp *app);
    s16 quantity;
    u16 maxQuantity;
    int sellPrice;
    u8 unk_688[2];
    u8 listBank; // which of the two item list window banks is shown
    u8 unk_68B;
    void *bgPaletteBuffers[2];
    NNSG2dPaletteData *bgPalettes[2];
    void *objPaletteBuffer;
    NNSG2dPaletteData *objPalette;
    u16 itemIds[165];
    int (*itemActions[5])(struct BagApp *app);
    YesNoPrompt *yesNoPrompt;
    BagAppScene scene;
    BagAppButtonFeedback buttonFeedback;
    int chosenAction;
#if defined(SDK_PORT)
    // NitroSystem's native palette unpacker returns rotating scratch storage.
    // Pocket colors must remain valid across later item-icon palette loads.
    u16 pocketBgColors[2][256];
    u16 pocketObjColors[256];
#endif
} BagApp;

void BagApp_LoadInfoPanelTilemap(BagApp *app, int mode);
BOOL BagApp_IsBerryOrMulch(u32 pocketId, u32 itemId);
void BagApp_AddItemListWindows(BagApp *app);
void BagApp_RemoveItemListWindows(BagApp *app);
void BagApp_AddItemActionWindows(BagApp *app);
void BagApp_RemoveItemActionWindows(BagApp *app);
void BagApp_BufferSlotItemName(BagApp *app, u32 slot, u32 fieldIdx);
void BagApp_BufferSlotItemNamePlural(BagApp *app, u32 slot, u32 fieldIdx);
void BagApp_PrintItemDescription(BagApp *app, u16 itemId);
void BagApp_PrintTMHMMoveInfo(BagApp *app, u16 itemId);
void BagApp_PrintTMHMNumber(BagApp *app, Window *window, ItemSlot *slot, int y);
void *BagApp_LoadListIconChars(BagApp *app, NNSG2dCharacterData **charData);

void BagApp_LoadItemMenuStrings(BagApp *app);
void BagApp_FreeItemMenuStrings(BagApp *app);
void BagApp_PrintItemSelectedMessage(BagApp *app, const u8 *actions, int count);
void BagApp_ShowItemDescription(BagApp *app, Window *window, u32 itemId);
void BagApp_ClearDescriptionWindow(BagApp *app, Window *window);
void BagApp_ShowPocketDescription(BagApp *app, Window *window, u32 pocket);
void BagApp_CloseItemPromptWindow(BagApp *app);
void BagApp_CloseItemMenuWindows(BagApp *app);
void BagApp_CloseItemActionWindows(BagApp *app);
void BagApp_PrintMoveItemPrompt(BagApp *app);
void BagApp_PrintQuantityDigits(BagApp *app, u32 numDigits);
void BagApp_PrintTrashConfirmMessage(BagApp *app);
u8 BagApp_PrintMessage(BagApp *app, BOOL wide);
void BagApp_CreateYesNoPrompt(BagApp *app);
void BagApp_DestroyYesNoPrompt(BagApp *app);
void BagApp_PrintSellPrice(BagApp *app);
void BagApp_PrintMoney(BagApp *app, BOOL skipLabel);
void BagApp_PrintMsg115And116(BagApp *app);
void BagApp_PrintCancelButton(BagApp *app, BOOL left);
void BagApp_DrawItemList(BagApp *app, int scroll, int unused, BOOL dim);
void BagApp_DrawSelectedItemEntry(BagApp *app, s32 scroll, s32 slot);
void BagApp_ClearSelectedItemEntry(BagApp *app);
void BagApp_PrintPageNumber(BagApp *app, s32 itemCount, s32 scroll, s32 unused);
void BagApp_PrintItemMenuLabel(Window *window, String **strings, u8 action);
void BagApp_ClearItemMenuLabels(Window *windows);
void BagApp_PrintTrashLabel(BagApp *app);
void BagApp_PrintSellLabel(BagApp *app);
void BagApp_ClearQuantityLabel(BagApp *app);
void BagApp_ClearCancelButtonText(BagApp *app);
void BagApp_InitSprites(BagApp *app);
void BagApp_FreeSprites(BagApp *app);
void BagApp_TickSprites(BagApp *app);
void BagApp_SetField648(BagApp *app);
void BagApp_UpdateField648(BagApp *app);
void BagApp_SetCursorPosition(BagApp *app, s32 button);
void BagApp_HideCursor(BagApp *app);
void BagApp_SetMoveCursorPosition(BagApp *app, int slot);
void BagApp_SetQuantityCursorPosition(BagApp *app, s32 index);
void BagApp_LoadPocketObjPalette(BagApp *app, int pocket);
void BagApp_ShowMoveSlotSprites(BagApp *app, int count, int movingSlot, BOOL hide);
void BagApp_DrawItemIcons(BagApp *app, BagViewPocket *pocket, int count, BOOL update);
void BagApp_ShowMovingItemIcon(BagApp *app, BagViewPocket *pocket, int slot);
void BagApp_ShowItemMenuSprites(BagApp *app, const u8 *actions);
void BagApp_HideItemMenuSprites(BagApp *app);
void BagApp_ShowSelectedItemIcon(BagApp *app, int slot);
s32 BagApp_GetUnusedDigitCount(s32 max);
void BagApp_ShowQuantityButtons(BagApp *app, int numDigits, int max);
void BagApp_HideQuantityButtons(BagApp *app);
void BagApp_ShowPocketTabSprites(BagApp *app, BOOL show);
void BagApp_SetCancelSpriteVisible(BagApp *app, BOOL show);

// Sub-screen tilemap edit: 1 copies a rect of the loaded tilemap, 2 clears one
typedef struct BagAppTilemapCommand {
    u8 type;
    u8 srcX;
    u8 srcY;
    u8 destX;
    u8 destY;
    u8 width;
    u8 height;
    u8 padding;
} BagAppTilemapCommand;

typedef struct BagAppTileRect {
    u8 x;
    u8 y;
    u8 width;
    u8 height;
} BagAppTileRect;

// Camera key per pocket (0: closed bag, 1-8: pockets), one set per trainer gender
typedef struct BagAppCameraKey {
    u16 angleX;
    u16 angleY;
    u8 unk_4[4];
    fx32 distance;
    fx32 modelY;
} BagAppCameraKey;

typedef struct BagAppWindowTemplate {
    u32 x;
    u32 y;
    u32 baseTile;
} BagAppWindowTemplate;

typedef struct BagAppWindowPos {
    u32 x;
    u32 y;
} BagAppWindowPos;

typedef struct BagAppItemMenuChoices {
    u8 actions[5]; // enum BagAppItemAction
} BagAppItemMenuChoices;

// Cursor sprite position, animation and palette of each button, move-item slot and quantity button.
typedef struct BagAppCursorPos {
    u8 x;
    u8 y;
    u8 anim;
    u8 palette;
} BagAppCursorPos;

extern const OverlayManagerTemplate gOverlayTemplate_Bag;

extern const BagAppItemMenuChoices gDefaultItemMenuChoices;
extern const GraphicsModes gBagGraphicsModes;
extern const GraphicsBanks gBagGraphicsBanks;
extern const BgTemplate gBgTemplate_Main1;
extern const BgTemplate gBgTemplate_Main2;
extern const BgTemplate gBgTemplate_Main3;
extern const BgTemplate gBgTemplate_Sub0;
extern const BgTemplate gBgTemplate_Sub1;
extern const BgTemplate gBgTemplate_Sub2;
extern const BgTemplate gBgTemplate_Sub3;
extern const u8 gPocketCapacities[];
// The navigation tables are {up, down, left, right} per button.
extern const u8 gBagAppButtonNavTable[][4];
extern const u8 gBagAppMoveNavTable[][4];
extern const u8 gBagAppItemMenuNavTable[][4];
extern u8 gItemMenuCursorButtons[]; // not const: BagApp_HandleItemMenuInput stores the cursor before reading it
extern const TouchscreenHitbox *const gTouchHitboxTables[];
extern int (*const gItemActionFuncs[])(BagApp *app);
int BagApp_ItemAction_Use(BagApp *app);
int BagApp_ItemAction_Action9(BagApp *app);
int BagApp_ItemAction_Confirm(BagApp *app);
int BagApp_ItemAction_Trash(BagApp *app);
int BagApp_ItemAction_Register(BagApp *app);
int BagApp_ItemAction_Deselect(BagApp *app);
int BagApp_ItemAction_Move(BagApp *app);
int BagApp_ItemAction_Give(BagApp *app);
int BagApp_ItemAction_ChooseForBerryPatch(BagApp *app);
extern const BagAppTilemapCommand gSubScreenTilemapCommands[][4];
extern const BagAppTileRect gUnusedSlotRects[][2];
extern const BagAppTileRect gItemSlotRects[];
extern const VecFx32 gCameraTarget;
extern const CameraParam gCameraSetup;
extern const VecFx32 gBagModelPosition;
extern const VecFx32 gBagModelScale;
extern const MtxFx33 gBagModelRotation;
extern const GXRgb gBagEdgeColorTable[8];
extern const BagAppCameraKey gCameraTweenKeys[2][9];
extern const BagAppWindowTemplate gItemListWindowTemplates[];
extern const BagAppWindowPos gItemMenuWindowPositions[];
extern const BagAppWindowPos gQuantityDigitWindowPositions[];
extern s32 gDebugSubScreenLayoutCount;
extern s32 gDebugSubScreenLayout;

extern const u8 gListPocketCapacities[];
extern const BagAppCursorPos gBagAppCursorPositions[];
extern const BagAppCursorPos gBagAppMoveCursorPositions[];
extern const BagAppCursorPos gBagAppQuantityCursorPositions[];
extern const SpriteResourceCountsListUnion gBagSpriteResourceCounts;
extern const OamManagerParam gBagOamManagerParam;
extern const OamCharTransferParam gBagOamCharTransferParam;
extern const ManagedSpriteTemplate gBagSpriteTemplates[];
extern const int gQuantityButtonCounts[];
extern const int gHiddenQuantityButtonCounts[];
extern const int gHiddenQuantityButtons[][4];
extern const int gQuantityButtonSprites[][6];
extern const int gQuantityButtonAnims[][6];

#endif // POKEHEARTGOLD_APPLICATION_BAG_BAG_APP_H
