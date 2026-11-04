#ifndef POKEHEARTGOLD_APPLICATION_PC_BOX_PC_BOX_INTERNAL_H
#define POKEHEARTGOLD_APPLICATION_PC_BOX_PC_BOX_INTERNAL_H
#if defined(SDK_PORT)

/* Many states are declared void in the decompilation but end in a tail call that returns the
 * next state; natively they return it. On the DS the macros leave the original code unchanged. */
#define PCBOX_STATE_RET u32
#define PCBOX_RETURN return
#define PCBOX_BARE_RETURN (void)0
#else
#define PCBOX_RETURN

#endif

#include "bag_types_def.h"
#include "bg_cell_manager.h"
#include "bg_window.h"
#include "char_tiles.h"
#include "filesystem.h"
#include "font_sprite.h"
#include "grid_input_handler.h"
#include "message_format.h"
#include "message_printer.h"
#include "msgdata.h"
#include "obj_char_transfer.h"
#include "options.h"
#include "overlay_14.h"
#include "overlay_manager.h"
#include "palette.h"
#include "pokemon_storage_system.h"
#include "pokemon_types_def.h"
#include "sprite_system.h"
#include "sys_task.h"
#include "yes_no_prompt.h"

void DrawFrame2AroundWindow(Window *window, u32 baseTile, u32 paletteNum);

// Launch modes (PCBoxArgs.mode).
#define PC_BOX_MODE_DEPOSIT      0
#define PC_BOX_MODE_WITHDRAW     1
#define PC_BOX_MODE_MOVE_POKEMON 2
#define PC_BOX_MODE_MOVE_ITEMS   3

// Slots: 0-29 are the active box, 30-35 the party.
#define PC_BOX_SLOT_PARTY_START 30
#define PC_BOX_SLOT_COUNT       36
#define PC_BOX_SLOT_NONE        0xFF

// Move and drop targets beyond the slots: box i of the tray is PC_BOX_TARGET_TRAY_BOX + i.
#define PC_BOX_TARGET_TRAY_BOX 0x80
#define PC_BOX_TRAY_BOX_COUNT  6

// PCBoxView.sprites
enum PCBoxSprite {
    PC_BOX_SPRITE_PREV_BOX_ARROW,
    PC_BOX_SPRITE_NEXT_BOX_ARROW,
    PC_BOX_SPRITE_MON_PIC, // two buffers
    PC_BOX_SPRITE_TRAY_PREV_ARROW = PC_BOX_SPRITE_MON_PIC + 2,
    PC_BOX_SPRITE_TRAY_NEXT_ARROW,
    PC_BOX_SPRITE_TRAY_MARKER,
    PC_BOX_SPRITE_TRAY_CURSOR,
    PC_BOX_SPRITE_TRAY_LABEL,
    PC_BOX_SPRITE_CURSOR,
    PC_BOX_SPRITE_UNUSED,
    PC_BOX_SPRITE_HELD_ITEM,
    PC_BOX_SPRITE_INFO_ITEM,
    PC_BOX_SPRITE_TYPE_ICONS,
    PC_BOX_SPRITE_TRAY_BOXES = PC_BOX_SPRITE_TYPE_ICONS + 2,
    PC_BOX_SPRITE_WALLPAPERS = PC_BOX_SPRITE_TRAY_BOXES + PC_BOX_TRAY_BOX_COUNT,
    PC_BOX_SPRITE_MON_ICONS = PC_BOX_SPRITE_WALLPAPERS + 4,              // slotIcons maps a slot to its icon
    PC_BOX_SPRITE_OUTLINE = PC_BOX_SPRITE_MON_ICONS + PC_BOX_SLOT_COUNT, // 8 icon outline pieces
    PC_BOX_SPRITE_COUNT = PC_BOX_SPRITE_OUTLINE + 8,
};

// gPCBoxStateTable: state N runs PCBox_State_<Name>
enum PCBoxState {
    PC_BOX_STATE_INIT,
    PC_BOX_STATE_TEARDOWN,
    PC_BOX_STATE_WAIT_FADE,
    PC_BOX_STATE_WAIT_PALETTE_BUFFERS,
    PC_BOX_STATE_DELAY,
    PC_BOX_STATE_WAIT_UPDATE_FUNC,
    PC_BOX_STATE_WAIT_BUTTON,
    PC_BOX_STATE_HANDLE_YES_NO,
    PC_BOX_STATE_WAIT_PALETTE_FLASH,
    PC_BOX_STATE_START_SUB_APP,
    PC_BOX_STATE_RUN_SUB_APP,
    PC_BOX_STATE_START,
    PC_BOX_STATE_MOVE_MODE_INPUT,
    PC_BOX_STATE_APPLY_MON_MOVE,
    PC_BOX_STATE_RETURN_TO_ACTION_MENU,
    PC_BOX_STATE_RETURN_FROM_SUMMARY,
    PC_BOX_STATE_RETURN_FROM_NAMING_SCREEN,
    PC_BOX_STATE_GIVE_ITEM_FROM_BAG,
    PC_BOX_STATE_PRINT_ITEM_HELD,
    PC_BOX_STATE_MARKING_CHOSEN,
    PC_BOX_STATE_OPEN_MARKING_MENU,
    PC_BOX_STATE_SETUP_MARKING_MENU_INPUT,
    PC_BOX_STATE_MARKING_MENU_INPUT,
    PC_BOX_STATE_FINISH_CLOSE_MARKING_MENU,
    PC_BOX_STATE_END_CURSOR_SLIDE_TO_MARKING_MENU,
    PC_BOX_STATE_TRY_RELEASE,
    PC_BOX_STATE_RELEASE_MOVE_CHECK,
    PC_BOX_STATE_PRINT_RELEASED,
    PC_BOX_STATE_PRINT_BYE_BYE,
    PC_BOX_STATE_REMOVE_RELEASED_MON,
    PC_BOX_STATE_RELEASE_CAME_BACK,
    PC_BOX_STATE_PRINT_WORRIED,
    PC_BOX_STATE_CANCEL_RELEASE,
    PC_BOX_STATE_END_PARTY_RELEASE_SHIFT,
    PC_BOX_STATE_OPEN_PARTY_PANEL,
    PC_BOX_STATE_SETUP_PARTY_PANEL_INPUT,
    PC_BOX_STATE_MOVE_MODE_PARTY_INPUT,
    PC_BOX_STATE_SLIDE_OUT_PARTY_PANEL,
    PC_BOX_STATE_PARTY_PANEL_CLOSED,
    PC_BOX_STATE_END_MOVE_MODE_PARTY_DRAG,
    PC_BOX_STATE_SETUP_BOX_TRAY_INPUT,
    PC_BOX_STATE_BOX_TRAY_INPUT,
    PC_BOX_STATE_END_BOX_TRAY_DRAG,
    PC_BOX_STATE_END_TRAY_HOLDING_DROP,
    PC_BOX_STATE_RESUME_BOX_TRAY_INPUT,
    PC_BOX_STATE_OPEN_TRAY_BESIDE_PARTY,
    PC_BOX_STATE_SETUP_PARTY_INPUT_AFTER_TRAY,
    PC_BOX_STATE_OPEN_TRAY_FROM_BOX,
    PC_BOX_STATE_SLIDE_IN_TRAY_PARTY_PANEL,
    PC_BOX_STATE_SLIDE_OUT_TRAY,
    PC_BOX_STATE_TRAY_CLOSED,
    PC_BOX_STATE_RETURN_TO_BOX_FROM_TRAY,
    PC_BOX_STATE_HIDE_BUTTONS_FOR_BOX_MENU,
    PC_BOX_STATE_OPEN_BOX_MENU_TRAY,
    PC_BOX_STATE_SHOW_BOX_MENU_EXIT_BUTTON,
    PC_BOX_STATE_OPEN_BOX_MENU,
    PC_BOX_STATE_SETUP_BOX_MENU_INPUT,
    PC_BOX_STATE_CLOSE_BOX_MENU,
    PC_BOX_STATE_SLIDE_OUT_BOX_MENU,
    PC_BOX_STATE_CLOSE_BOX_MENU_TRAY,
    PC_BOX_STATE_BOX_MENU_CLOSED,
    PC_BOX_STATE_BOX_MENU_INPUT,
    PC_BOX_STATE_END_CURSOR_SLIDE_TO_BOX_MENU,
    PC_BOX_STATE_OPEN_WALLPAPER_MENU,
    PC_BOX_STATE_SHOW_WALLPAPER_MENU_TITLE,
    PC_BOX_STATE_SETUP_WALLPAPER_MENU_INPUT,
    PC_BOX_STATE_WALLPAPER_MENU_INPUT,
    PC_BOX_STATE_CLOSE_WALLPAPER_MENU,
    PC_BOX_STATE_SLIDE_OUT_WALLPAPER_LIST,
    PC_BOX_STATE_RETURN_TO_BOX_MENU,
    PC_BOX_STATE_APPLY_WALLPAPER,
    PC_BOX_STATE_WALLPAPER_FADE_OUT,
    PC_BOX_STATE_WALLPAPER_FADE_IN,
    PC_BOX_STATE_END_CURSOR_SLIDE_TO_WALLPAPER_MENU,
    PC_BOX_STATE_END_CURSOR_SLIDE_TO_MOVE_MODE,
    PC_BOX_STATE_END_CURSOR_SLIDE_TO_BOX_TRAY,
    PC_BOX_STATE_END_CURSOR_SLIDE_TO_MOVE_MODE_PARTY,
    PC_BOX_STATE_RESUME_BOX_INPUT,
    PC_BOX_STATE_RESUME_MOVE_MODE_PARTY_INPUT,
    PC_BOX_STATE_PRINT_EGG_CANT_HOLD_ITEM,
    PC_BOX_STATE_PRINT_MAIL_CANT_BE_TAKEN,
    PC_BOX_STATE_WITHDRAW_INPUT,
    PC_BOX_STATE_RETURN_TO_WITHDRAW_INPUT,
    PC_BOX_STATE_SLIDE_IN_WITHDRAW_PARTY,
    PC_BOX_STATE_START_WITHDRAW_MOVE,
    PC_BOX_STATE_END_WITHDRAW_MOVE,
    PC_BOX_STATE_WITHDRAW_PARTY_CLOSED,
    PC_BOX_STATE_END_WITHDRAW_DRAG,
    PC_BOX_STATE_END_CURSOR_SLIDE_TO_WITHDRAW,
    PC_BOX_STATE_END_CURSOR_SLIDE_TO_WITHDRAW_PROMPT,
    PC_BOX_STATE_RESUME_WITHDRAW_INPUT,
    PC_BOX_STATE_DEPOSIT_INPUT,
    PC_BOX_STATE_TRY_DEPOSIT,
    PC_BOX_STATE_RETURN_FROM_DEPOSIT_ERROR,
    PC_BOX_STATE_SHOW_DEPOSIT_BOX_SELECT,
    PC_BOX_STATE_SHOW_DEPOSIT_TITLE,
    PC_BOX_STATE_SETUP_DEPOSIT_BOX_INPUT,
    PC_BOX_STATE_DEPOSIT_BOX_INPUT,
    PC_BOX_STATE_CANCEL_DEPOSIT_BOX_SELECT,
    PC_BOX_STATE_HIDE_DEPOSIT_EXIT_BUTTON,
    PC_BOX_STATE_HIDE_DEPOSIT_BOX_SELECT,
    PC_BOX_STATE_DEPOSIT_BOX_SELECT_CLOSED,
    PC_BOX_STATE_CONFIRM_DEPOSIT_BOX,
    PC_BOX_STATE_CHECK_DEPOSIT_BOX,
    PC_BOX_STATE_SHIFT_PARTY_FOR_DEPOSIT,
    PC_BOX_STATE_SWITCH_BOX,
    PC_BOX_STATE_START_DEPOSIT_MOVE,
    PC_BOX_STATE_END_DEPOSIT_MOVE,
    PC_BOX_STATE_SHOW_DEPOSIT_RETURN_BUTTON,
    PC_BOX_STATE_RESET_DEPOSIT_INPUT,
    PC_BOX_STATE_END_DEPOSIT_DRAG,
    PC_BOX_STATE_END_CURSOR_SLIDE_TO_DEPOSIT,
    PC_BOX_STATE_END_CURSOR_SLIDE_TO_DEPOSIT_PROMPT,
    PC_BOX_STATE_END_CURSOR_SLIDE_TO_DEPOSIT_BOX,
    PC_BOX_STATE_RESUME_DEPOSIT_INPUT,
    PC_BOX_STATE_BOX_TRAY_HOLDING_INPUT,
    PC_BOX_STATE_END_CURSOR_SLIDE_TO_BOX_TRAY_HOLDING,
    PC_BOX_STATE_ITEM_MODE_INPUT,
    PC_BOX_STATE_END_CURSOR_SLIDE_TO_ITEM_MODE,
    PC_BOX_STATE_CLOSE_ITEM_MESSAGE,
    PC_BOX_STATE_ASK_PUT_AWAY_ITEM,
    PC_BOX_STATE_WAIT_PUT_AWAY_ITEM,
    PC_BOX_STATE_CANCEL_PUT_AWAY_ITEM,
    PC_BOX_STATE_END_ITEM_COMMAND,
    PC_BOX_STATE_ITEM_MODE_PRINT_EGG_CANT_HOLD_ITEM,
    PC_BOX_STATE_ITEM_MODE_PRINT_ITEM_HELD,
    PC_BOX_STATE_END_ITEM_MODE_BOX_DRAG,
    PC_BOX_STATE_SWAP_HELD_ITEMS,
    PC_BOX_STATE_DROP_SWAPPED_ITEM,
    PC_BOX_STATE_FINISH_ITEM_SWAP,
    PC_BOX_STATE_ITEM_MODE_BOX_TRAY_INPUT,
    PC_BOX_STATE_END_CURSOR_SLIDE_TO_ITEM_MODE_TRAY,
    PC_BOX_STATE_END_ITEM_TRAY_DRAG,
    PC_BOX_STATE_APPLY_ITEM_TRAY_DROP,
    PC_BOX_STATE_FINISH_ITEM_TRAY_DROP,
    PC_BOX_STATE_RESUME_ITEM_MODE_TRAY_INPUT,
    PC_BOX_STATE_ITEM_TRAY_HOLDING_INPUT,
    PC_BOX_STATE_END_CURSOR_SLIDE_TO_ITEM_TRAY_HOLDING,
    PC_BOX_STATE_APPLY_ITEM_TRAY_HOLDING_DROP,
    PC_BOX_STATE_ITEM_MODE_PARTY_INPUT,
    PC_BOX_STATE_END_CURSOR_SLIDE_TO_ITEM_MODE_PARTY,
    PC_BOX_STATE_END_ITEM_MODE_PARTY_DRAG,
    PC_BOX_STATE_SWAP_PARTY_HELD_ITEMS,
    PC_BOX_STATE_RESUME_ITEM_MODE_PARTY_INPUT,
    PC_BOX_STATE_DROP_SWAPPED_PARTY_ITEM,
    PC_BOX_STATE_FINISH_PARTY_ITEM_SWAP,
    PC_BOX_STATE_RESUME_ITEM_MODE_PARTY_AFTER_MAIL,
    PC_BOX_STATE_ASK_EXIT,
    PC_BOX_STATE_ASK_CONTINUE,
    PC_BOX_STATE_CHOOSE_PARTY,
    PC_BOX_STATE_CHOOSE_TRAY_TAB,
    PC_BOX_STATE_CHOOSE_SUMMARY,
    PC_BOX_STATE_CHOOSE_ITEM,
    PC_BOX_STATE_CHOOSE_MARKING,
    PC_BOX_STATE_CHOOSE_MARKING_DONE,
    PC_BOX_STATE_CHOOSE_RELEASE,
    PC_BOX_STATE_CLOSE_MOVE_MODE_ACTION_MENU,
    PC_BOX_STATE_PARTY_CHOOSE_TRAY,
    PC_BOX_STATE_CLOSE_MOVE_MODE_PARTY_ACTION_MENU,
    PC_BOX_STATE_CHOOSE_PARTY_PANEL_EXIT,
    PC_BOX_STATE_CHOOSE_TRAY_BOX,
    PC_BOX_STATE_CHOOSE_TRAY_EXIT,
    PC_BOX_STATE_BOX_MENU_SWITCH_BOX,
    PC_BOX_STATE_CHOOSE_WALLPAPER,
    PC_BOX_STATE_CHOOSE_RENAME,
    PC_BOX_STATE_SWITCH_BOX_FOR_WALLPAPER,
    PC_BOX_STATE_CHOOSE_WALLPAPER_MENU_EXIT,
    PC_BOX_STATE_CHOOSE_WITHDRAW,
    PC_BOX_STATE_CLOSE_WITHDRAW_ACTION_MENU,
    PC_BOX_STATE_CHOOSE_DEPOSIT,
    PC_BOX_STATE_CLOSE_DEPOSIT_ACTION_MENU,
    PC_BOX_STATE_ITEM_MODE_CHOOSE_PARTY,
    PC_BOX_STATE_ITEM_MODE_CHOOSE_TRAY_TAB,
    PC_BOX_STATE_CHOOSE_BAG_GIVE,
    PC_BOX_STATE_CLOSE_ITEM_MODE_ACTION_MENU,
    PC_BOX_STATE_ITEM_MODE_CHOOSE_TRAY_BOX,
    PC_BOX_STATE_ITEM_MODE_PARTY_CHOOSE_TRAY,
    PC_BOX_STATE_CLOSE_ITEM_MODE_PARTY_ACTION_MENU,
    PC_BOX_STATE_CHOOSE_MOVE_FROM_BOX,
    PC_BOX_STATE_EXIT,
};

enum PCBoxMonMessage {
    PC_BOX_MON_MSG_WHICH_ONE,
    PC_BOX_MON_MSG_SELECTED,
    PC_BOX_MON_MSG_PARTY_FULL,
    PC_BOX_MON_MSG_DEPOSIT_WHERE,
    PC_BOX_MON_MSG_BOX_FULL,
    PC_BOX_MON_MSG_DETACH_CAPSULE,
    PC_BOX_MON_MSG_REMOVE_MAIL,
};

enum PCBoxReleaseMessage {
    PC_BOX_RELEASE_MSG_CONFIRM,
    PC_BOX_RELEASE_MSG_RELEASED,
    PC_BOX_RELEASE_MSG_BYE_BYE,
    PC_BOX_RELEASE_MSG_EGG,
    PC_BOX_RELEASE_MSG_CAME_BACK,
    PC_BOX_RELEASE_MSG_WORRIED,
    PC_BOX_RELEASE_MSG_LAST_MON,
};

enum PCBoxItemMessage {
    PC_BOX_ITEM_MSG_PUT_AWAY = 2,
    PC_BOX_ITEM_MSG_PLACED_IN_BAG,
    PC_BOX_ITEM_MSG_MAIL_CANT_BE_TAKEN,
    PC_BOX_ITEM_MSG_EGG_CANT_HOLD,
    PC_BOX_ITEM_MSG_BAG_FULL,
    PC_BOX_ITEM_MSG_MAIL_CANT_BE_MOVED,
};

typedef struct PCBoxWork PCBoxWork;

// Run each vblank by PCBox_VBlankCallback until it returns FALSE.
// Cells of PCBoxView.panels (BgCellManager)
enum PCBoxPanel {
    PC_BOX_PANEL_BOX_TRAY,
    PC_BOX_PANEL_PARTY,
    PC_BOX_PANEL_MARKING_MENU,
    PC_BOX_PANEL_MENU_ROW_0, // the five rows of the action menu and the box menu
    PC_BOX_PANEL_MENU_ROW_1,
    PC_BOX_PANEL_MENU_ROW_2,
    PC_BOX_PANEL_MENU_ROW_3,
    PC_BOX_PANEL_MENU_ROW_4,
    PC_BOX_PANEL_PARTY_BUTTON,
    PC_BOX_PANEL_TRAY_TAB,
    PC_BOX_PANEL_RETURN_BUTTON,
    PC_BOX_PANEL_MENU_EXIT_BUTTON,
    PC_BOX_PANEL_TITLE,
    PC_BOX_PANEL_WALLPAPER_LIST,
    PC_BOX_PANEL_CHANGE_BOX_BUTTON,
    PC_BOX_PANEL_SUMMARY_BUTTON,
    PC_BOX_PANEL_INFO, // moves or held item
    PC_BOX_PANEL_COUNT,
};

typedef BOOL (*PCBoxUpdateFunc)(PCBoxWork *work);

// Info about the mon in a slot, shown on the top screen (PCBox_CreateMonInfo).
typedef struct PCBoxMonInfo {
    BoxPokemon *boxMon;
    u16 species;
    u16 heldItem;
    u32 personality;
    u8 type1;
    u8 type2;
    u8 ability;
    u8 nature;
    u16 markings;
    u8 level : 7;
    u8 isEgg : 1;
    u8 gender : 7;
    u8 showGender : 1;
    u16 moves[4];
} PCBoxMonInfo;

// Releasing a mon: the scan of all boxes and the party for another mon that knows the mon's HM moves
// (PCBox_StartReleaseMoveCheck / PCBox_RunReleaseMoveCheck), then the shrink animation of its icon.
typedef struct PCBoxReleaseWork {
    ManagedSprite *sprite;
    u16 scanIndex; // box * 30 + slot, 540 + party slot
    u8 moveFlags;  // HM moves not found on another mon yet
    u8 scaleCounter;
    f32 scale;
} PCBoxReleaseWork;

// One mon icon moving between two slots (PCBox_AnimateMonMove).
typedef struct PCBoxMoveEntry {
    Pokemon *mon;
    u32 from;
    u32 to;
    u32 active;
    u32 dx;
    u32 dy;
    s16 sx;
    s16 sy;
    s16 px;
    s16 py;
} PCBoxMoveEntry;

typedef struct PCBoxMoveWork {
    PCBoxMoveEntry entries[7];
    u32 timer;
    u32 from;
    u32 to;
    u32 sel;
} PCBoxMoveWork;

// State of PCBox_AnimatePaletteFlash (filled by PCBox_FlashPanel).
typedef struct PCBoxPaletteFlash {
    u8 isBg : 1;
    u8 index : 7;
    u8 palA : 4;
    u8 palB : 4;
    u8 state;
    u8 timer;
    u8 x;
    u8 y;
    u8 w;
    u8 h;
} PCBoxPaletteFlash;

// Cursor slide between two grid input positions (PCBox_StartCursorSlide), in view->temp.
typedef struct PCBoxCursorSlide {
    u8 x;
    u8 y;
    u8 dx;
    u8 dy;
    u32 xNegative : 1;
    u32 yNegative : 1;
    u32 frames : 30;
} PCBoxCursorSlide;

// Held item icon moving to a slot (PCBox_StartHeldItemMove), in view->temp. Positions in 24.8 fixed point.
typedef struct PCBoxHeldItemMove {
    u16 toSlot;
    u16 fromSlot;
    int partyLayout;
    int x;
    int y;
    int dx;
    int dy;
    u32 xNegative : 1;
    u32 yNegative : 1;
    u32 frames : 30;
} PCBoxHeldItemMove;

// {msgId, kind} entries of the action menu (PCBox_DrawActionMenu).
typedef struct PCBoxMenuEntry {
    u16 msgId;
    u16 kind;
} PCBoxMenuEntry;

// Text drawn as sprites (the tray / wallpaper menu titles).
typedef struct PCBoxTextObj {
    TextOBJ *textObj;
    ObjCharSpaceReservation charTransfer;
} PCBoxTextObj;

// Graphics, text and input state of the app (heap 10, PCBoxWork.view).
typedef struct PCBoxView {
    SysTask *vblankTask;
    PCBoxUpdateFunc updateFunc;
    PCBoxUpdateFunc pendingUpdateFunc;
    void *temp;
    u16 subState;
    u16 counter;
    BgConfig *bgConfig;
    PaletteData *paletteData;
    MessagePrinter *msgPrinter;
    MsgData *msgData;
    MessageFormat *msgFormat;
    String *string;
    GridInputHandler *gridInput;
    Window windows[0x2C];
    BgCellManager *panels;
    SpriteSystem *spriteSystem;
    SpriteManager *spriteManager;
    ManagedSprite *sprites[PC_BOX_SPRITE_COUNT];
    FontSystem *fontSystem;
    PCBoxTextObj textObjs[2];
    YesNoPrompt *yesNoPrompt;
    u16 yesNoHandler; // row of gPCBoxYesNoHandlers
    u16 unk_043A;
    u32 rowCursorPos;
    enum PCBoxState fadeNextState;
    u32 delay;
    s8 wallpaperScroll;
    u8 wallpaperBuffer;
    u8 partyPanelLowered;
    u8 cursorHolding;
    u8 dropTarget;
    u8 wallpaperSelection;
    u8 infoWindowToggle : 4; // which of the two info window sets is drawn next
    u8 movesDrawn : 3;
    u8 restoreIconsOnMenu : 1;
    u8 unk_044F;
    NARC *personalNarc;
    NARC *iconNarc;
    u8 iconChars[30][0x200];
    u8 iconDraw[30];
    u8 iconPalettes[30];
    u8 slotIcons[PC_BOX_SLOT_COUNT];
    u32 touchX;
    u32 touchY;
    u32 markings;
    u32 showOutlineAfterMove;
    u8 boxThumbnails[18][0x400];
    u16 heldItem;
    u16 itemFromSlot;
    u32 dropCancelled;
    u16 monPicBuffer;
    u16 spriteDrawLock;
    PCBoxPaletteFlash paletteFlash;
    PCBoxReleaseWork *releaseWork;
} PCBoxView;

struct PCBoxWork {
    PCBoxArgs *args;
    PCStorage *storage;
    Party *party;
    Bag *bag;
    Options *options;
    OverlayManager *subApp;
    void *subAppArgs;
    u16 itemId;
    u8 subAppIndex;
    u8 activeBox;
    u8 unk_20;
    u8 selectedSlot;
    u8 partyLayout;
    u8 partyPanelOpen;
    u8 trayOpen;
    u8 trayBox;
    u8 summaryFromTray;
    u8 summaryFromHolding;
    u8 summaryCursorPos;
    u8 trayFromPartyPanel;
    u8 moveWithButtons;
    u8 moveFromSlot;
    u32 returnCursorPos;
    enum PCBoxState nextState;
    PCBoxView *view;
};

void PCBox_VBlankCallback(SysTask *task, void *data);
void PCBox_SetUpdateFunc(PCBoxView *view, PCBoxUpdateFunc func);
void PCBox_SetPendingUpdateFunc(PCBoxView *view, PCBoxUpdateFunc func);
void PCBox_StartPendingUpdateFunc(PCBoxView *view);
void PCBox_SetGfxBanks(void);
void PCBox_InitBgLayers(PCBoxWork *work);
void PCBox_FreeBgLayers(PCBoxWork *work);
void PCBox_LoadBgGraphics(PCBoxWork *work);
void PCBox_CreateMsgResources(PCBoxWork *work);
void PCBox_DestroyMsgResources(PCBoxWork *work);
void PCBox_LoadWindowGraphics(PCBoxWork *p);
void PCBox_CreatePaletteData(PCBoxWork *work);
void PCBox_FreePaletteData(PCBoxWork *work);
void PCBox_SetBlendEnabled(int enabled);
void PCBox_CreateYesNoPrompt(PCBoxWork *work);
void PCBox_DestroyYesNoPrompt(PCBoxWork *work);
void PCBox_ShowYesNoPrompt(PCBoxWork *work, u32 cursorPos);
BOOL PCBox_AnimatePaletteFlash(PCBoxWork *work);
void PCBox_SaveMenuInputState(PCBoxWork *work);
u32 PCBox_GetSlotMonData(PCBoxWork *work, u32 idx, u32 field, void *dest);
void PCBox_SetSlotMonData(PCBoxWork *work, u32 idx, u32 field, void *src);
BoxPokemon *PCBox_GetSlotBoxMon(PCBoxWork *work, u32 boxId, u32 idx);
void PCBox_DeleteSlotMon(PCBoxWork *work, u32 boxId, u32 idx);
void PCBox_ApplyMonMove(PCBoxWork *work);
BOOL PCBox_PartyHasOtherUsableMon(PCBoxWork *work, u32 skip);
BOOL PCBox_UpdateArceusGiratinaForm(BoxPokemon *boxMon);
void PCBox_RevertShayminFormOnMove(PCBoxWork *work, u32 from, u32 to);
BOOL PCBox_AnimateMonMove(PCBoxWork *work);
BOOL PCBox_AnimateSelectedSlotMoveEntry(PCBoxWork *work);
BOOL PCBox_AnimateMoveEntries(PCBoxWork *work);
BOOL PCBox_CanSwapHeldItems(PCBoxWork *work, u32 from, u32 to);
void PCBox_SetupMonMove(PCBoxWork *work, u32 from, u32 to);
void PCBox_SetupDepositMoveEntries(PCBoxWork *work, u32 from, u32 to);
void PCBox_SetupMonMoveToPartyEnd(PCBoxWork *work, u32 from, u32 to);
u32 PCBox_GetMoveEntryTarget(PCBoxWork *work, u32 from);
void PCBox_SetupPartyShiftAfterRelease(PCBoxWork *work, u32 releasedSlot);
void PCBox_ApplyMoveEntryIcons(PCBoxWork *work, PCBoxMoveEntry *entry);
void PCBox_StartReleaseMoveCheck(PCBoxWork *work);
void PCBox_FreeReleaseMoveCheck(PCBoxWork *work);
BOOL PCBox_RunReleaseMoveCheck(PCBoxWork *work);
BOOL PCBox_ShowSlotMonInfo(PCBoxWork *work, u32 index);
void PCBox_RedrawSlotMonInfo(PCBoxWork *work, u32 index);
void PCBox_ClearMonInfo(PCBoxWork *work);
void PCBox_ClearMonInfoAndSelection(PCBoxWork *work);
void PCBox_LoadScrolledWallpaper(PCBoxWork *work, s32 member, u32 direction);
void PCBox_SwapWallpaper(PCBoxWork *work, s32 member);
u32 PCBox_GetWallpaperGfxIndex(PCBoxWork *work, u32 box);
u32 PCBox_GetTouchedBoxSlot(s16 x, s16 y);
u32 PCBox_GetTouchedPartySlot(s16 x, s16 y, const u8 *rect);
u32 PCBox_GetTouchedTrayBox(s16 x, s16 y);

void PCBox_InitPanels(PCBoxWork *work);
void PCBox_FreePanels(PCBoxView *view);
void PCBox_DrawMarkingMenu(PCBoxWork *work);
void PCBox_ToggleMarking(PCBoxWork *work, int bit);
void PCBox_SlideInMarkingMenu(BgCellManager *panels);
void PCBox_SlideOutMarkingMenu(BgCellManager *panels);
void PCBox_PlacePartyPanelLeft(BgCellManager *panels);
void PCBox_PlacePartyPanelRight(BgCellManager *panels);
void PCBox_ClearPartyPanelTiles(PCBoxWork *work);
void PCBox_PlacePartyPanelLeftHidden(BgCellManager *panels);
void PCBox_PlacePartyPanelRightHidden(BgCellManager *panels);
void PCBox_SlideInPartyPanel(BgCellManager *panels);
void PCBox_SlideOutPartyPanel(BgCellManager *panels);
void PCBox_ShiftPartyPanelAndReturnButtonRight(BgCellManager *panels);
void PCBox_ShiftPartyPanelRight(BgCellManager *panels);
void PCBox_ShiftPartyPanelAndReturnButtonLeft(BgCellManager *panels);
void PCBox_ShiftPartyPanelLeft(BgCellManager *panels);
void PCBox_LoadPartyPanelTilemap(BgCellManager *panels, u32 index);
void PCBox_SlideInWallpaperList(BgCellManager *panels);
void PCBox_SlideOutWallpaperList(BgCellManager *panels);
void PCBox_ShowPartyButton(PCBoxWork *work);
void PCBox_SlideOutPartyButton(BgCellManager *panels);
void PCBox_SlideInPartyButton(BgCellManager *panels);
void PCBox_ShowTrayTab(PCBoxWork *work);
void PCBox_SlideOutTrayTab(BgCellManager *panels);
void PCBox_SlideInTrayTab(BgCellManager *panels);
void PCBox_ShowReturnButton(PCBoxWork *work);
void PCBox_ShowExitButton(PCBoxWork *work);
void PCBox_HideReturnButton(BgCellManager *panels);
void PCBox_SlideOutReturnButton(BgCellManager *panels);
void PCBox_SlideInReturnButton(BgCellManager *panels);
BOOL PCBox_IsMenuExitButtonShown(BgCellManager *panels);
void PCBox_HidePartyButtonAndTrayTab(BgCellManager *panels);
void PCBox_ShowActionMenu(BgCellManager *panels);
void PCBox_ResetActionMenu(BgCellManager *panels);
void PCBox_SlideInActionMenu(BgCellManager *panels);
void PCBox_SlideOutActionMenu(BgCellManager *panels);
BOOL PCBox_IsActionMenuOpen(BgCellManager *panels);
void PCBox_SlideInTitlePanel(BgCellManager *panels);
void PCBox_SlideOutTitlePanel(BgCellManager *panels);
void PCBox_SlideOutChangeBoxButton(BgCellManager *panels);
BOOL PCBox_IsChangeBoxButtonShown(BgCellManager *panels);
void PCBox_ShowSummaryButton(BgCellManager *panels);
void PCBox_SlideInSummaryButton(BgCellManager *panels);
void PCBox_SlideOutSummaryButton(BgCellManager *panels);
BOOL PCBox_IsSummaryButtonShown(BgCellManager *panels);
void PCBox_ShowBoxTray(PCBoxWork *work);
void PCBox_SlideInBoxTray(BgCellManager *panels);
void PCBox_SlideOutBoxTray(BgCellManager *panels);
void PCBox_ShowBoxTrayAndMenu(PCBoxWork *work);
void PCBox_SlideInBoxMenu(BgCellManager *panels);
void PCBox_SlideOutBoxMenu(BgCellManager *panels);
void PCBox_HideBottomButtons(PCBoxWork *work);
void PCBox_SlideInMovesPanel(PCBoxView *view);
void PCBox_SlideOutMovesPanel(PCBoxView *view);
void PCBox_ShowMovesPanel(PCBoxView *view);
void PCBox_HideMovesPanel(BgCellManager *panels);
void PCBox_SlideInItemInfoPanel(BgCellManager *panels);
void PCBox_SlideOutItemInfoPanel(BgCellManager *panels);
void PCBox_ShowItemInfoPanel(BgCellManager *panels);
void PCBox_HideItemInfoPanel(BgCellManager *panels);
void PCBox_DrawMarkings(PCBoxWork *work, u32 mask);
u32 PCBox_StartSummaryApp(PCBoxWork *work);
u32 PCBox_EndSummaryApp(PCBoxWork *work);
u32 PCBox_StartBagApp(PCBoxWork *work);
u32 PCBox_EndBagApp(PCBoxWork *work);
u32 PCBox_StartNamingScreenApp(PCBoxWork *work);
u32 PCBox_EndNamingScreenApp(PCBoxWork *work);
BOOL PCBox_UpdateMoveModeBoxDrag(PCBoxWork *work);
BOOL PCBox_UpdateMoveModePartyDrag(PCBoxWork *work);
BOOL PCBox_UpdateBoxTrayDrag(PCBoxWork *work);
BOOL PCBox_AnimatePartyShiftAfterRelease(PCBoxWork *work);
BOOL PCBox_AnimateMoveToPartyEnd(PCBoxWork *work);
BOOL PCBox_AnimateMoveToFirstEmptySlot(PCBoxWork *work);
BOOL PCBox_UpdateScrollToPrevBox(PCBoxWork *work);
BOOL PCBox_UpdateScrollToNextBox(PCBoxWork *work);
BOOL PCBox_WaitActionMenuSlide(PCBoxWork *work);
BOOL PCBox_WaitActionMenuAndButtonsSlide(PCBoxWork *work);
BOOL PCBox_UpdateMarkingMenuSlide(PCBoxWork *work);
BOOL PCBox_WaitBottomButtonsSlide(PCBoxWork *work);
BOOL PCBox_WaitPartyPanelSlide(PCBoxWork *work);
BOOL PCBox_WaitPartyPanelAndActionMenuSlide(PCBoxWork *work);
BOOL PCBox_WaitPartyPanelSlideFunc(PCBoxWork *work);
BOOL PCBox_UpdateBoxTraySlideIn(PCBoxWork *work);
BOOL PCBox_UpdateBoxTraySlideOut(PCBoxWork *work);
BOOL PCBox_WaitReturnButtonSlide(PCBoxWork *work);
BOOL PCBox_WaitActionMenuThenTitleSlide(PCBoxWork *work);
BOOL PCBox_WaitTitlePanelSlide(PCBoxWork *work);
BOOL PCBox_UpdateBoxMenuSlide(PCBoxWork *work);
BOOL PCBox_WaitBoxMenuThenWallpaperList(PCBoxWork *work);
BOOL PCBox_WaitWallpaperListSlideOut(PCBoxWork *work);
BOOL PCBox_AnimateDepositBoxSelectOpen(PCBoxWork *work);
BOOL PCBox_AnimateDepositBoxSelectClose(PCBoxWork *work);
BOOL PCBox_WaitSummaryOutChangeBoxIn(PCBoxWork *work);
BOOL PCBox_WaitTrayButtonsSlide(PCBoxWork *work);
BOOL PCBox_WaitChangeBoxOutSummaryIn(PCBoxWork *work);
BOOL PCBox_WaitTitleAndReturnButtonSlide(PCBoxWork *work);
BOOL PCBox_UpdateDepositDrag(PCBoxWork *p);
BOOL PCBox_UpdateWithdrawDrag(PCBoxWork *p);
BOOL PCBox_UpdateCursorSlide(PCBoxWork *work);
BOOL PCBox_WaitSelectWithActionMenu(PCBoxWork *work);
BOOL PCBox_WaitSelectWithActionMenuAndButtons(PCBoxWork *work);
BOOL PCBox_WaitDeselectThenReturnButton(PCBoxWork *work);
BOOL PCBox_WaitDeselectThenButtons(PCBoxWork *work);
void PCBox_AttachIconToCursor(PCBoxWork *work);
BOOL PCBox_AnimateTrayPickUp(PCBoxWork *work);
BOOL PCBox_AnimateTrayDrop(PCBoxWork *work);
BOOL PCBox_WaitItemModeSelect(PCBoxWork *work);
BOOL PCBox_UpdateItemModeBoxDrag(PCBoxWork *work);
BOOL PCBox_WaitItemModeDeselect(PCBoxWork *work);
BOOL PCBox_AnimateSwappedItemMove(PCBoxWork *work);
BOOL PCBox_UpdateItemTrayDrag(PCBoxWork *work);
BOOL PCBox_AnimateTraySwappedItemMove(PCBoxWork *work);
BOOL PCBox_AnimateTrayItemPickUp(PCBoxWork *work);
BOOL PCBox_AnimateTrayItemDrop(PCBoxWork *work);
BOOL PCBox_AnimateTrayItemReturn(PCBoxWork *work);
BOOL PCBox_UpdateItemModePartyDrag(PCBoxWork *work);
BOOL PCBox_AnimatePartySwappedItemMove(PCBoxWork *work);
BOOL PCBox_RunStateMachine(PCBoxWork *work, int *state);

enum PCBoxState PCBox_State_Init(PCBoxWork *work);
enum PCBoxState PCBox_State_Exit(PCBoxWork *work);
enum PCBoxState PCBox_State_WaitFade(PCBoxWork *work);
enum PCBoxState PCBox_State_WaitPaletteBuffers(PCBoxWork *work);
enum PCBoxState PCBox_State_Delay(PCBoxWork *work);
enum PCBoxState PCBox_State_WaitUpdateFunc(PCBoxWork *work);
enum PCBoxState PCBox_State_WaitButton(PCBoxWork *work);
enum PCBoxState PCBox_State_HandleYesNo(PCBoxWork *work);
enum PCBoxState PCBox_State_WaitPaletteFlash(PCBoxWork *work);
enum PCBoxState PCBox_State_StartSubApp(PCBoxWork *work);
enum PCBoxState PCBox_State_RunSubApp(PCBoxWork *work);
#if defined(SDK_PORT)
enum PCBoxState PCBox_State_Start(PCBoxWork *work);
#else
void PCBox_State_Start(PCBoxWork *work);
#endif
enum PCBoxState PCBox_State_MoveModeInput(PCBoxWork *work);
enum PCBoxState PCBox_State_ApplyMonMove(PCBoxWork *work);
#if defined(SDK_PORT)
enum PCBoxState PCBox_State_ReturnToActionMenu(PCBoxWork *work);
enum PCBoxState PCBox_State_ReturnFromSummary(PCBoxWork *work);
enum PCBoxState PCBox_State_ReturnFromNamingScreen(PCBoxWork *work);
enum PCBoxState PCBox_State_GiveItemFromBag(PCBoxWork *work);
#else
void PCBox_State_ReturnToActionMenu(PCBoxWork *work);
void PCBox_State_ReturnFromSummary(PCBoxWork *work);
void PCBox_State_ReturnFromNamingScreen(PCBoxWork *work);
void PCBox_State_GiveItemFromBag(PCBoxWork *work);
#endif
enum PCBoxState PCBox_State_PrintItemHeld(PCBoxWork *work);
#if defined(SDK_PORT)
enum PCBoxState PCBox_State_MarkingChosen(PCBoxWork *work);
enum PCBoxState PCBox_State_OpenMarkingMenu(PCBoxWork *work);
#else
void PCBox_State_MarkingChosen(PCBoxWork *work);
void PCBox_State_OpenMarkingMenu(PCBoxWork *work);
#endif
enum PCBoxState PCBox_State_SetupMarkingMenuInput(PCBoxWork *work);
enum PCBoxState PCBox_State_MarkingMenuInput(PCBoxWork *work);
enum PCBoxState PCBox_State_FinishCloseMarkingMenu(PCBoxWork *work);
enum PCBoxState PCBox_State_TryRelease(PCBoxWork *work);
enum PCBoxState PCBox_State_ReleaseMoveCheck(PCBoxWork *work);
enum PCBoxState PCBox_State_PrintReleased(PCBoxWork *work);
enum PCBoxState PCBox_State_PrintByeBye(PCBoxWork *work);
enum PCBoxState PCBox_State_RemoveReleasedMon(PCBoxWork *work);
enum PCBoxState PCBox_State_EndPartyReleaseShift(PCBoxWork *work);
enum PCBoxState PCBox_State_ReleaseCameBack(PCBoxWork *work);
enum PCBoxState PCBox_State_PrintWorried(PCBoxWork *work);
enum PCBoxState PCBox_State_CancelRelease(PCBoxWork *work);
enum PCBoxState PCBox_State_ResumeDepositInput(PCBoxWork *work);
enum PCBoxState PCBox_State_OpenPartyPanel(PCBoxWork *work);
enum PCBoxState PCBox_State_SetupPartyPanelInput(PCBoxWork *work);
enum PCBoxState PCBox_State_MoveModePartyInput(PCBoxWork *work);
enum PCBoxState PCBox_State_SlideOutPartyPanel(PCBoxWork *work);
enum PCBoxState PCBox_State_PartyPanelClosed(PCBoxWork *work);
enum PCBoxState PCBox_State_EndMoveModePartyDrag(PCBoxWork *work);
enum PCBoxState PCBox_State_SetupBoxTrayInput(PCBoxWork *work);
enum PCBoxState PCBox_State_BoxTrayInput(PCBoxWork *work);
enum PCBoxState PCBox_State_EndBoxTrayDrag(PCBoxWork *p);
enum PCBoxState PCBox_State_EndTrayHoldingDrop(PCBoxWork *p);
enum PCBoxState PCBox_State_ResumeBoxTrayInput(PCBoxWork *work);
enum PCBoxState PCBox_State_OpenTrayBesideParty(PCBoxWork *work);
enum PCBoxState PCBox_State_SetupPartyInputAfterTray(PCBoxWork *work);
enum PCBoxState PCBox_State_OpenTrayFromBox(PCBoxWork *work);
#if defined(SDK_PORT)
enum PCBoxState PCBox_State_SlideInTrayPartyPanel(PCBoxWork *work);
#else
void PCBox_State_SlideInTrayPartyPanel(PCBoxWork *work);
#endif
enum PCBoxState PCBox_State_SlideOutTray(PCBoxWork *work);
#if defined(SDK_PORT)
enum PCBoxState PCBox_State_TrayClosed(PCBoxWork *work);
enum PCBoxState PCBox_State_ReturnToBoxFromTray(PCBoxWork *work);
#else
void PCBox_State_TrayClosed(PCBoxWork *work);
void PCBox_State_ReturnToBoxFromTray(PCBoxWork *work);
#endif
enum PCBoxState PCBox_State_BoxMenuInput(PCBoxWork *work);
enum PCBoxState PCBox_State_HideButtonsForBoxMenu(PCBoxWork *work);
enum PCBoxState PCBox_State_OpenBoxMenuTray(PCBoxWork *work);
enum PCBoxState PCBox_State_ShowBoxMenuExitButton(PCBoxWork *work);
#if defined(SDK_PORT)
enum PCBoxState PCBox_State_OpenBoxMenu(PCBoxWork *work);
#else
void PCBox_State_OpenBoxMenu(PCBoxWork *work);
#endif
enum PCBoxState PCBox_State_SetupBoxMenuInput(PCBoxWork *work);
#if defined(SDK_PORT)
enum PCBoxState PCBox_State_CloseBoxMenu(PCBoxWork *work);
enum PCBoxState PCBox_State_SlideOutBoxMenu(PCBoxWork *work);
#else
void PCBox_State_CloseBoxMenu(PCBoxWork *work);
void PCBox_State_SlideOutBoxMenu(PCBoxWork *work);
#endif
enum PCBoxState PCBox_State_CloseBoxMenuTray(PCBoxWork *work);
enum PCBoxState PCBox_State_BoxMenuClosed(PCBoxWork *work);
enum PCBoxState PCBox_State_OpenWallpaperMenu(PCBoxWork *work);
enum PCBoxState PCBox_State_ShowWallpaperMenuTitle(PCBoxWork *work);
enum PCBoxState PCBox_State_SetupWallpaperMenuInput(PCBoxWork *work);
enum PCBoxState PCBox_State_WallpaperMenuInput(PCBoxWork *work);
enum PCBoxState PCBox_State_CloseWallpaperMenu(PCBoxWork *work);
enum PCBoxState PCBox_State_SlideOutWallpaperList(PCBoxWork *work);
enum PCBoxState PCBox_State_ReturnToBoxMenu(PCBoxWork *work);
enum PCBoxState PCBox_State_ApplyWallpaper(PCBoxWork *work);
enum PCBoxState PCBox_State_WallpaperFadeOut(PCBoxWork *work);
enum PCBoxState PCBox_State_WallpaperFadeIn(PCBoxWork *work);
enum PCBoxState PCBox_State_ResumeBoxInput(PCBoxWork *work);
enum PCBoxState PCBox_State_ResumeMoveModePartyInput(PCBoxWork *work);
enum PCBoxState PCBox_State_WithdrawInput(PCBoxWork *work);
#if defined(SDK_PORT)
enum PCBoxState PCBox_State_ReturnToWithdrawInput(PCBoxWork *work);
enum PCBoxState PCBox_State_SlideInWithdrawParty(PCBoxWork *work);
enum PCBoxState PCBox_State_StartWithdrawMove(PCBoxWork *work);
enum PCBoxState PCBox_State_EndWithdrawMove(PCBoxWork *work);
enum PCBoxState PCBox_State_WithdrawPartyClosed(PCBoxWork *work);
#else
void PCBox_State_ReturnToWithdrawInput(PCBoxWork *work);
void PCBox_State_SlideInWithdrawParty(PCBoxWork *work);
void PCBox_State_StartWithdrawMove(PCBoxWork *work);
void PCBox_State_EndWithdrawMove(PCBoxWork *work);
void PCBox_State_WithdrawPartyClosed(PCBoxWork *work);
#endif
enum PCBoxState PCBox_State_EndWithdrawDrag(PCBoxWork *work);
enum PCBoxState PCBox_State_ResumeWithdrawInput(PCBoxWork *work);
enum PCBoxState PCBox_State_DepositInput(PCBoxWork *work);
enum PCBoxState PCBox_State_TryDeposit(PCBoxWork *work);
enum PCBoxState PCBox_State_ReturnFromDepositError(PCBoxWork *work);
enum PCBoxState PCBox_State_ShowDepositBoxSelect(PCBoxWork *work);
enum PCBoxState PCBox_State_ShowDepositTitle(PCBoxWork *work);
enum PCBoxState PCBox_State_SetupDepositBoxInput(PCBoxWork *work);
enum PCBoxState PCBox_State_DepositBoxInput(PCBoxWork *work);
enum PCBoxState PCBox_State_CancelDepositBoxSelect(PCBoxWork *work);
enum PCBoxState PCBox_State_HideDepositExitButton(PCBoxWork *work);
enum PCBoxState PCBox_State_HideDepositBoxSelect(PCBoxWork *work);
enum PCBoxState PCBox_State_DepositBoxSelectClosed(PCBoxWork *work);
enum PCBoxState PCBox_State_ConfirmDepositBox(PCBoxWork *work);
enum PCBoxState PCBox_State_CheckDepositBox(PCBoxWork *work);
enum PCBoxState PCBox_State_ShiftPartyForDeposit(PCBoxWork *work);
enum PCBoxState PCBox_State_SwitchBox(PCBoxWork *work);
enum PCBoxState PCBox_State_StartDepositMove(PCBoxWork *work);
enum PCBoxState PCBox_State_EndDepositMove(PCBoxWork *work);
enum PCBoxState PCBox_State_ShowDepositReturnButton(PCBoxWork *work);
enum PCBoxState PCBox_State_ResetDepositInput(PCBoxWork *work);
enum PCBoxState PCBox_State_EndDepositDrag(PCBoxWork *work);
enum PCBoxState PCBox_State_EndCursorSlideToMoveMode(PCBoxWork *work);
enum PCBoxState PCBox_State_EndCursorSlideToBoxTray(PCBoxWork *work);
enum PCBoxState PCBox_State_EndCursorSlideToMoveModeParty(PCBoxWork *work);
enum PCBoxState PCBox_State_EndCursorSlideToDeposit(PCBoxWork *work);
enum PCBoxState PCBox_State_EndCursorSlideToDepositPrompt(PCBoxWork *work);
enum PCBoxState PCBox_State_EndCursorSlideToDepositBox(PCBoxWork *work);
enum PCBoxState PCBox_State_EndCursorSlideToMarkingMenu(PCBoxWork *work);
enum PCBoxState PCBox_State_EndCursorSlideToBoxMenu(PCBoxWork *work);
enum PCBoxState PCBox_State_EndCursorSlideToWallpaperMenu(PCBoxWork *work);
enum PCBoxState PCBox_State_EndCursorSlideToWithdraw(PCBoxWork *work);
enum PCBoxState PCBox_State_EndCursorSlideToWithdrawPrompt(PCBoxWork *work);
enum PCBoxState PCBox_State_ItemModeInput(PCBoxWork *work);
enum PCBoxState PCBox_State_CloseItemMessage(PCBoxWork *work);
enum PCBoxState PCBox_State_EndCursorSlideToItemMode(PCBoxWork *work);
enum PCBoxState PCBox_State_AskPutAwayItem(PCBoxWork *work);
enum PCBoxState PCBox_State_WaitPutAwayItem(PCBoxWork *work);
enum PCBoxState PCBox_State_CancelPutAwayItem(PCBoxWork *work);
enum PCBoxState PCBox_State_EndItemCommand(PCBoxWork *work);
enum PCBoxState PCBox_State_ItemModePrintEggCantHoldItem(PCBoxWork *work);
enum PCBoxState PCBox_State_ItemModePrintItemHeld(PCBoxWork *work);
enum PCBoxState PCBox_State_EndItemModeBoxDrag(PCBoxWork *work);
enum PCBoxState PCBox_State_SwapHeldItems(PCBoxWork *work);
enum PCBoxState PCBox_State_DropSwappedItem(PCBoxWork *work);
enum PCBoxState PCBox_State_FinishItemSwap(PCBoxWork *work);
enum PCBoxState PCBox_State_ItemModeBoxTrayInput(PCBoxWork *work);
enum PCBoxState PCBox_State_EndCursorSlideToItemModeTray(PCBoxWork *work);
enum PCBoxState PCBox_State_EndItemTrayDrag(PCBoxWork *work);
enum PCBoxState PCBox_State_ApplyItemTrayDrop(PCBoxWork *work);
enum PCBoxState PCBox_State_FinishItemTrayDrop(PCBoxWork *work);
enum PCBoxState PCBox_State_ResumeItemModeTrayInput(PCBoxWork *work);
enum PCBoxState PCBox_State_ItemTrayHoldingInput(PCBoxWork *work);
enum PCBoxState PCBox_State_EndCursorSlideToItemTrayHolding(PCBoxWork *work);
enum PCBoxState PCBox_State_ApplyItemTrayHoldingDrop(PCBoxWork *work);
enum PCBoxState PCBox_State_ItemModePartyInput(PCBoxWork *work);
enum PCBoxState PCBox_State_EndCursorSlideToItemModeParty(PCBoxWork *work);
enum PCBoxState PCBox_State_EndItemModePartyDrag(PCBoxWork *work);
enum PCBoxState PCBox_State_SwapPartyHeldItems(PCBoxWork *work);
enum PCBoxState PCBox_State_ResumeItemModePartyInput(PCBoxWork *work);
enum PCBoxState PCBox_State_DropSwappedPartyItem(PCBoxWork *work);
enum PCBoxState PCBox_State_FinishPartyItemSwap(PCBoxWork *work);
enum PCBoxState PCBox_State_ResumeItemModePartyAfterMail(PCBoxWork *work);
enum PCBoxState PCBox_State_PrintEggCantHoldItem(PCBoxWork *work);
enum PCBoxState PCBox_State_PrintMailCantBeTaken(PCBoxWork *work);
enum PCBoxState PCBox_State_BoxTrayHoldingInput(PCBoxWork *work);
enum PCBoxState PCBox_State_EndCursorSlideToBoxTrayHolding(PCBoxWork *work);
enum PCBoxState PCBox_TakeHeldItemToBag(PCBoxWork *work);
enum PCBoxState PCBox_ConfirmRelease(PCBoxWork *work);
enum PCBoxState PCBox_PutAwayHeldItem(PCBoxWork *work);
enum PCBoxState PCBox_ConfirmExit(PCBoxWork *work);
enum PCBoxState PCBox_CancelExit(PCBoxWork *work);
enum PCBoxState PCBox_State_AskExit(PCBoxWork *work);
enum PCBoxState PCBox_State_AskContinue(PCBoxWork *work);
#if defined(SDK_PORT)
enum PCBoxState PCBox_State_ChooseParty(PCBoxWork *work);
enum PCBoxState PCBox_State_ChooseTrayTab(PCBoxWork *work);
#else
void PCBox_State_ChooseParty(PCBoxWork *work);
void PCBox_State_ChooseTrayTab(PCBoxWork *work);
#endif
enum PCBoxState PCBox_State_ChooseMoveFromBox(PCBoxWork *work);
enum PCBoxState PCBox_State_ChooseSummary(PCBoxWork *work);
#if defined(SDK_PORT)
enum PCBoxState PCBox_State_ChooseItem(PCBoxWork *work);
enum PCBoxState PCBox_State_ChooseMarking(PCBoxWork *work);
enum PCBoxState PCBox_State_ChooseMarkingDone(PCBoxWork *work);
enum PCBoxState PCBox_State_ChooseRelease(PCBoxWork *work);
enum PCBoxState PCBox_State_CloseMoveModeActionMenu(PCBoxWork *work);
enum PCBoxState PCBox_State_PartyChooseTray(PCBoxWork *work);
enum PCBoxState PCBox_State_CloseMoveModePartyActionMenu(PCBoxWork *work);
enum PCBoxState PCBox_State_ChoosePartyPanelExit(PCBoxWork *work);
enum PCBoxState PCBox_State_ChooseTrayBox(PCBoxWork *work);
enum PCBoxState PCBox_State_ChooseTrayExit(PCBoxWork *work);
#else
void PCBox_State_ChooseItem(PCBoxWork *work);
void PCBox_State_ChooseMarking(PCBoxWork *work);
void PCBox_State_ChooseMarkingDone(PCBoxWork *work);
void PCBox_State_ChooseRelease(PCBoxWork *work);
void PCBox_State_CloseMoveModeActionMenu(PCBoxWork *work);
void PCBox_State_PartyChooseTray(PCBoxWork *work);
void PCBox_State_CloseMoveModePartyActionMenu(PCBoxWork *work);
void PCBox_State_ChoosePartyPanelExit(PCBoxWork *work);
void PCBox_State_ChooseTrayBox(PCBoxWork *work);
void PCBox_State_ChooseTrayExit(PCBoxWork *work);
#endif
enum PCBoxState PCBox_State_BoxMenuSwitchBox(PCBoxWork *work);
#if defined(SDK_PORT)
enum PCBoxState PCBox_State_ChooseWallpaper(PCBoxWork *work);
#else
void PCBox_State_ChooseWallpaper(PCBoxWork *work);
#endif
enum PCBoxState PCBox_State_ChooseRename(PCBoxWork *work);
enum PCBoxState PCBox_State_SwitchBoxForWallpaper(PCBoxWork *work);
enum PCBoxState PCBox_State_ChooseWallpaperMenuExit(PCBoxWork *work);
#if defined(SDK_PORT)
enum PCBoxState PCBox_State_ChooseWithdraw(PCBoxWork *work);
enum PCBoxState PCBox_State_CloseWithdrawActionMenu(PCBoxWork *work);
enum PCBoxState PCBox_State_ChooseDeposit(PCBoxWork *work);
enum PCBoxState PCBox_State_CloseDepositActionMenu(PCBoxWork *work);
enum PCBoxState PCBox_State_ItemModeChooseParty(PCBoxWork *work);
enum PCBoxState PCBox_State_ItemModeChooseTrayTab(PCBoxWork *work);
enum PCBoxState PCBox_State_ChooseBagGive(PCBoxWork *work);
enum PCBoxState PCBox_State_CloseItemModeActionMenu(PCBoxWork *work);
enum PCBoxState PCBox_State_ItemModeChooseTrayBox(PCBoxWork *work);
enum PCBoxState PCBox_State_ItemModePartyChooseTray(PCBoxWork *work);
enum PCBoxState PCBox_State_CloseItemModePartyActionMenu(PCBoxWork *work);
#else
void PCBox_State_ChooseWithdraw(PCBoxWork *work);
void PCBox_State_CloseWithdrawActionMenu(PCBoxWork *work);
void PCBox_State_ChooseDeposit(PCBoxWork *work);
void PCBox_State_CloseDepositActionMenu(PCBoxWork *work);
void PCBox_State_ItemModeChooseParty(PCBoxWork *work);
void PCBox_State_ItemModeChooseTrayTab(PCBoxWork *work);
void PCBox_State_ChooseBagGive(PCBoxWork *work);
void PCBox_State_CloseItemModeActionMenu(PCBoxWork *work);
void PCBox_State_ItemModeChooseTrayBox(PCBoxWork *work);
void PCBox_State_ItemModePartyChooseTray(PCBoxWork *work);
void PCBox_State_CloseItemModePartyActionMenu(PCBoxWork *work);
#endif

void PCBox_FreeSprites(PCBoxView *view);
void PCBox_TickSprites(PCBoxView *view);
void PCBox_SetSpriteAnim(PCBoxView *view, u32 index, u32 anim);
BOOL PCBox_IsSpriteAnimated(PCBoxView *view, u32 i);
void PCBox_SetSpriteVisible(PCBoxView *view, int index, BOOL visible);
BOOL PCBox_IsSpriteVisible(PCBoxView *view, int index);
void PCBox_SetSpritePriority(PCBoxView *view, u32 i, u32 pri);
void PCBox_InitSpriteSystem(PCBoxView *view);
void PCBox_LoadSpriteResources(PCBoxView *view);
void PCBox_CreateSprites(PCBoxWork *work);
void PCBox_LoadSpriteCharsMain(PCBoxView *view, int i, void *src, u32 size);
void PCBox_LoadBoxIconImages(PCBoxWork *work, int box);
void PCBox_RefreshSlotIcon(PCBoxWork *work, int a, int b, int i);
void PCBox_LoadBoxIcons(PCBoxWork *work);
void PCBox_LoadPartyIcons(PCBoxWork *work);
void PCBox_GetSlotPosition(u32 i, s16 *x, s16 *y, int mode);
void PCBox_PlacePartyIconsLeftHidden(PCBoxWork *work);
void PCBox_PlacePartyIconsRightHidden(PCBoxWork *work);
void PCBox_PlacePartyIconsLeft(PCBoxWork *work);
void PCBox_PlacePartyIconsRight(PCBoxWork *work);
void PCBox_SetIconLayer(PCBoxView *view, u32 i, BOOL active);
void PCBox_SetMovingIconPriority(PCBoxView *view, int i, u32 which);
void PCBox_ScrollBoxIcons(PCBoxWork *work, s32 amount);
void PCBox_UpdatePartyIconPositions(PCBoxWork *work);
void PCBox_StartReleaseScale(PCBoxReleaseWork *releaseWork);
BOOL PCBox_UpdateReleaseShrink(PCBoxReleaseWork *releaseWork);
BOOL PCBox_UpdateReleaseGrow(PCBoxReleaseWork *releaseWork);
void PCBox_EndReleaseShrink(PCBoxReleaseWork *releaseWork);
void PCBox_ResetSpriteAffine(PCBoxReleaseWork *releaseWork);
void PCBox_SetIconsTranslucent(PCBoxWork *work, u32 flags, u32 enabled);
void PCBox_SetSlotIconTranslucent(PCBoxView *view, u32 off, u32 v);
void PCBox_ShowMonPic(PCBoxWork *work, PCBoxMonInfo *monInfo, u32 slot);
void PCBox_LoadHeldItemIcon(PCBoxView *view, u16 item);
void PCBox_LoadInfoItemIcon(PCBoxView *view, u16 item);
void PCBox_SetHeldItemIconAffine(PCBoxView *view, u32 on);
void PCBox_SetHeldItemIconPosition(PCBoxView *view, s16 x, s16 y);
void PCBox_MoveHeldItemIconToSlot(PCBoxView *view, u32 index, u32 mode);
void PCBox_PlaceHeldItemIconAtSlot(PCBoxView *view, u32 index, u32 mode);
void PCBox_CreateHeldItemIconOutline(PCBoxView *view);
void PCBox_ShowHeldItemIconOutline(PCBoxView *view);
void PCBox_UpdateHeldItemIconOutline(PCBoxView *view);
void PCBox_UpdateInfoItemIconPosition(PCBoxView *view);
void PCBox_DrawTypeIcons(PCBoxView *view, PCBoxMonInfo *monInfo);
void PCBox_CreateSelectedIconOutline(PCBoxWork *work);
void PCBox_HideSelectedIconOutline(PCBoxWork *work);
void PCBox_SetSelectedIconOutlineVisible(PCBoxWork *work, u32 visible);
void PCBox_UpdateSelectedIconOutline(PCBoxWork *work);
void PCBox_SetBoxArrowsVisible(PCBoxView *view, u32 visible);
void PCBox_SetupWallpaperMenuSprites(PCBoxWork *work);
void PCBox_MoveWallpaperMenuSprites(PCBoxView *view, s32 dy);
void PCBox_UpdateCurrentWallpaperMarker(PCBoxWork *work);
void PCBox_UpdateWallpaperCursor(PCBoxWork *work);
void PCBox_LoadWallpaperThumbnails(PCBoxWork *work);
void PCBox_PlaceTrayTextObjs(PCBoxView *view);
void PCBox_SetupBoxTraySprites(PCBoxWork *work);
void PCBox_MoveBoxTraySprites(PCBoxWork *work, s32 dy);
void PCBox_UpdateCurrentBoxMarker(PCBoxWork *work);
void PCBox_UpdateTrayCursor(PCBoxWork *work);
void PCBox_GetTrayBoxPosition(PCBoxView *view, u32 i, s16 *x, s16 *y);
void PCBox_DrawBoxThumbnail(PCBoxWork *work, int n);
void PCBox_DrawAllBoxThumbnails(PCBoxWork *work);

void PCBox_LoadTrayPageThumbnails(PCBoxWork *work);
void PCBox_RefreshTrayBoxThumbnail(PCBoxWork *work, u32 box);
void PCBox_DrawBoxThumbnailMons(PCBoxWork *work, u32 box, void *dst);
void PCBox_SetupDepositBoxSprites(PCBoxWork *work);
void PCBox_HideBoxTraySprites(PCBoxWork *work);
void PCBox_CreateTrayTextObjs(PCBoxView *view);
void PCBox_DestroyTrayTextObjs(PCBoxView *view);
void PCBox_AddWindows(PCBoxWork *work);
void PCBox_RemoveWindows(PCBoxWork *work);
void PCBox_PrintMonInfo(PCBoxWork *work, PCBoxMonInfo *monInfo);
void PCBox_ClearMonInfoWindows(PCBoxView *view);
int PCBox_DrawMoveWindows(PCBoxWork *work, PCBoxMonInfo *monInfo);
u32 PCBox_PrintItemNameAndDesc(PCBoxWork *work, u16 itemId);
void PCBox_PrintModeTitle(PCBoxWork *work);
void PCBox_PrintInfoLabels(PCBoxWork *work);
void PCBox_DrawBoxNameOnWallpaper(PCBoxWork *work, void *src, u32 baseTile, u32 width, u32 height);
void PCBox_DrawTrayBoxNameAndCount(PCBoxWork *work);
void PCBox_DrawWallpaperName(PCBoxWork *work);
void PCBox_DrawPartyButton(PCBoxWork *work, u32 style);
void PCBox_DrawTrayTab(PCBoxWork *work, u32 unused);
void PCBox_DrawReturnButton(PCBoxWork *work, u32 style);
void PCBox_DrawExitButton(PCBoxWork *work, u32 style);
void PCBox_DrawActionMenu(PCBoxWork *work, const PCBoxMenuEntry *list, u32 count);
void PCBox_DrawBagGiveButton(PCBoxWork *work, u32 which);
void PCBox_DrawChangeBoxButton(PCBoxWork *work);
void PCBox_DrawBoxMenu(PCBoxWork *work);
void PCBox_DrawChangeWallpaperTitle(PCBoxWork *work);
void PCBox_DrawDepositTitle(PCBoxWork *work);
void PCBox_AttachPanelWindows(PCBoxView *view);
void PCBox_AttachPartyButtonWindow(PCBoxView *view);
void PCBox_AttachTrayTabWindow(PCBoxView *view);
void PCBox_AttachReturnButtonWindow(PCBoxView *view);
void PCBox_AttachMenuExitButtonWindow(PCBoxWork *work);
void PCBox_DrawConfirmCancel(PCBoxView *view);
void PCBox_PrintPartyPanelSwitchLabel(PCBoxView *view);
void PCBox_PrintPartyPanelExitLabel(PCBoxView *view);
void PCBox_AttachMarkingMenuWindows(PCBoxView *view);
void PCBox_AttachPartyPanelLabelWindow(PCBoxView *view);
void PCBox_DrawSummaryButton(PCBoxWork *work, u32 unused);
void PCBox_CloseMessageWindow(PCBoxView *view, int windowId);
void PCBox_ClearWindowFrameNoTransfer(PCBoxView *view, u32 idx);
void PCBox_ClearWindowFrame(PCBoxView *view, u32 idx);
void PCBox_PrintExitPrompt(PCBoxWork *work, int windowId, int kind);
void PCBox_PrintTakeItemPrompt(PCBoxWork *work, int item, int windowId);
void PCBox_PrintMailCantBeTaken(PCBoxWork *work, int windowId);
void PCBox_PrintMailCantBeMoved(PCBoxWork *work, int windowId);
void PCBox_PrintTookItem(PCBoxWork *work, int item, int windowId);
void PCBox_PrintBagFull(PCBoxWork *work, int windowId);
void PCBox_PrintItemHeld(PCBoxWork *work, int item, int windowId);
void PCBox_PrintTouchTheMark(PCBoxWork *work, int windowId);
void PCBox_PrintReleaseMessage(PCBoxWork *work, int mode, int windowId);
void PCBox_PrintMenuPrompt(PCBoxWork *work, u32 which, u32 windowId);
void PCBox_PrintMonActionMessage(PCBoxWork *work, u32 unused, u32 kind, u32 windowId);
void PCBox_PrintItemActionMessage(PCBoxWork *work, u32 kind, u32 windowId);
void PCBox_PrintPanelHint(PCBoxWork *work, u32 idx, u32 kind);
void PCBox_DrawWindowFrame(PCBoxWork *work, int windowId);
s32 PCBox_FindTouchedBoxSlot(void);
s32 PCBox_FindTouchedPartySlot(void);
s32 PCBox_FindTouchedTrayPartySlot(void);
void PCBox_CreateModeGridInput(PCBoxWork *work);
void PCBox_CreateGridInput(PCBoxWork *work, int layout, int initialPos);
void PCBox_FreeGridInput(PCBoxWork *work);
void PCBox_PlaceCursorOnGridInput(PCBoxWork *work, int target);
void PCBox_GridOnButton(PCBoxWork *work, int target);
void PCBox_GridOnSwitchToTouchMode(PCBoxWork *work);

void PCBox_GridOnKeyMove(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_GridOnTouch(PCBoxWork *work, u32 newTarget);
u32 PCBox_GetDepositMenuInput(PCBoxWork *work);
void PCBox_DepositGridOnButton(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_DepositBoxGridOnButton(PCBoxWork *work, u32 newTarget);
void PCBox_DepositBoxGridOnKeyMove(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_DepositBoxGridOnTouch(PCBoxWork *work, u32 newTarget);
u32 PCBox_GetWithdrawMenuInput(PCBoxWork *work);
void PCBox_WithdrawGridOnButton(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_WithdrawGridOnTouch(PCBoxWork *work, u32 newTarget);
u32 PCBox_GetMoveModeMenuInput(PCBoxWork *work);
void PCBox_MoveModeGridOnButton(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_MoveModeGridOnKeyMove(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_MoveModeGridOnTouch(PCBoxWork *work, u32 newTarget);
u32 PCBox_GetBoxTrayInput(PCBoxWork *work);
void PCBox_TrayGridOnButton(PCBoxWork *work, u32 newTarget);
void PCBox_TrayGridOnKeyMove(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_TrayGridOnTouch(PCBoxWork *work, u32 newTarget);
u32 PCBox_GetTrayHoldingInput(PCBoxWork *work);
u32 PCBox_GetMoveModePartyMenuInput(PCBoxWork *work);
void PCBox_MoveModePartyGridOnButton(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_MoveModePartyGridOnKeyMove(PCBoxWork *work, u32 newTarget, u32 prevTarget);
u32 PCBox_GetItemModeMenuInput(PCBoxWork *work);
void PCBox_ItemModeGridOnButton(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_ItemModeGridOnKeyMove(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_ItemModeGridOnTouch(PCBoxWork *work, u32 newTarget);
u32 PCBox_GetItemModePartyMenuInput(PCBoxWork *work);
void PCBox_ItemModePartyGridOnButton(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_ItemModePartyGridOnKeyMove(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_BoxMenuGridOnButton(PCBoxWork *work, u32 newTarget);
void PCBox_BoxMenuGridOnKeyMove(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_BoxMenuGridOnTouch(PCBoxWork *work, u32 newTarget);
void PCBox_WallpaperGridOnButton(PCBoxWork *work, u32 newTarget);
void PCBox_WallpaperGridOnKeyMove(PCBoxWork *work, u32 newTarget, u32 prevTarget);
void PCBox_WallpaperGridOnTouch(PCBoxWork *work, u32 newTarget);
u32 PCBox_GetItemTrayHoldingInput(PCBoxWork *work);
void PCBox_StartCursorSlide(PCBoxView *view, u32 newTarget, u32 prevTarget);
BOOL PCBox_IsSummaryShortcutPressed(PCBoxWork *work);

typedef enum PCBoxState (*PCBoxStateFunc)(PCBoxWork *work);

// Sub-apps run from the box (summary, bag, naming screen): start, end, state to return to.
typedef struct PCBoxSubApp {
    u32 (*start)(PCBoxWork *work);
    u32 (*end)(PCBoxWork *work);
    enum PCBoxState nextState;
} PCBoxSubApp;

typedef struct PCBoxScreenPos {
    s16 x;
    s16 y;
} PCBoxScreenPos;

// Callbacks and layouts of the grid inputs (PCBox_CreateGridInput).
typedef struct PCBoxGridLayout {
    const TouchscreenHitbox *hitboxes;
    const DpadMenuBox *dpadBoxes;
    const GridCallbacks *callbacks;
} PCBoxGridLayout;

extern const GraphicsBanks gPCBoxGfxBanks;
extern const GraphicsModes gPCBoxGraphicsModes;
extern const BgTemplate gPCBoxBgTemplate0;
extern const BgTemplate gPCBoxBgTemplate1;
extern const BgTemplate gPCBoxBgTemplate4;
extern const BgTemplate gPCBoxBgTemplate2;
extern const BgTemplate gPCBoxBgTemplate3;
extern const BgTemplate gPCBoxBgTemplate5;
extern const BgTemplate gPCBoxBgTemplate6;
extern const u16 gPCBoxReleaseCheckMoves[];
extern const u8 gPCBoxSlotGridRect[];
extern const u8 gPCBoxTrayRects[];

extern const OamCharTransferParam gPCBoxOamCharTransferParam;
extern const SpriteResourceCountsListUnion gPCBoxSpriteResourceCounts;
extern const OamManagerParam gPCBoxOamManagerParam;
extern const PCBoxScreenPos gPCBoxPartySlotPositions[6];
extern const u8 gPCBoxPartyIconOffsets[6][2];
extern const TileRect gPCBoxMonPicRect;
extern const u8 gPCBoxTrayCursorX[];
extern const s8 gPCBoxOutlineOffsetsX[];
extern const s8 gPCBoxOutlineOffsetsY[];
extern const ManagedSpriteTemplate gPCBoxMonIconSpriteTemplate;
extern const ManagedSpriteTemplate gPCBoxTrayBoxSpriteTemplate;
extern const ManagedSpriteTemplate gPCBoxWallpaperListSpriteTemplate;
extern const ManagedSpriteTemplate gPCBoxUiSprite0Template;
extern const ManagedSpriteTemplate gPCBoxUiSprite1Template;
extern const ManagedSpriteTemplate gPCBoxMonPicSprite2Template;
extern const ManagedSpriteTemplate gPCBoxMonPicSprite3Template;
extern const ManagedSpriteTemplate gPCBoxUiSprite4Template;
extern const ManagedSpriteTemplate gPCBoxUiSprite5Template;
extern const ManagedSpriteTemplate gPCBoxUiSprite6Template;
extern const ManagedSpriteTemplate gPCBoxUiSprite7Template;
extern const ManagedSpriteTemplate gPCBoxUiSprite8Template;
extern const ManagedSpriteTemplate gPCBoxUiSprite9Template;
extern const ManagedSpriteTemplate gPCBoxUiSprite10Template;
extern const ManagedSpriteTemplate gPCBoxItemIconSpriteTemplate;
extern const ManagedSpriteTemplate gPCBoxItemIconSprite12Template;
extern const ManagedSpriteTemplate gPCBoxTypeIconSprite13Template;
extern const ManagedSpriteTemplate gPCBoxTypeIconSprite14Template;

extern const PCBoxStateFunc gPCBoxStateTable[];
// {yes, no} handler pairs of the yes/no prompts
extern const PCBoxStateFunc gPCBoxYesNoHandlers[][2];
extern const PCBoxSubApp gPCBoxSubApps[];
extern const PCBoxMenuEntry gPCBoxDepositMenu[];
extern const PCBoxMenuEntry gPCBoxWithdrawMenu[];
extern const PCBoxMenuEntry gPCBoxMoveMenu[];

extern const u8 gPCBoxThumbnailTiles[];
extern const WindowTemplate gPCBoxWindowTemplates[];
extern const TouchscreenHitbox gPCBoxSlotHitboxes[];
extern const TouchscreenHitbox gPCBoxPartyHitboxes[];
extern const TouchscreenHitbox gPCBoxTrayPartyHitboxes[];
extern const PCBoxGridLayout gPCBoxGridLayouts[];

extern const u8 gPCBoxPartyDropRects[];
extern const u8 gPCBoxTrayPartyDropRects[];
extern const u8 gPCBoxSummaryPages[];
extern const u8 gPCBoxBagPockets[];

extern const TouchscreenHitbox gPCBoxSummaryShortcutHitbox[];

#endif // POKEHEARTGOLD_APPLICATION_PC_BOX_PC_BOX_INTERNAL_H
