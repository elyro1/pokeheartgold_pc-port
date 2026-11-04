#ifndef POKEHEARTGOLD_APPLICATION_PLAZA_GAME_H
#define POKEHEARTGOLD_APPLICATION_PLAZA_GAME_H

#include "bg_window.h"
#include "gf_gfx_planes.h"
#include "message_format.h"
#include "msgdata.h"
#include "obj_char_transfer.h"
#include "player_data.h"
#include "pm_string.h"
#include "render_window.h"
#include "save.h"
#include "sprite.h"
#include "sprite_resource_header.h"
#include "sprite_resource.h"
#include "yes_no_prompt.h"

// Shared code of the Wi-Fi Plaza games (SWALOT Plop, MIME JR. Top and WOBBUFFET Pop): the player list
// shown before a game, the results screen shown after it, and the countdown / finish sign.

#define PLAZA_GAME_MAX_PLAYERS 4

// Launch arguments of a Plaza Game, shared with the Wi-Fi Plaza lobby.
typedef struct PlazaGameArgs {
    u32 tapToy;
    u8 slots[PLAZA_GAME_MAX_PLAYERS];
    u8 specialVisitors[20]; // per lobby slot
    BOOL connectionLost;
    PlayerProfile *profiles[PLAZA_GAME_MAX_PLAYERS];
    struct WiFiPlazaWork *plazaWork;
    SaveData *saveData;
    u8 voiceChat;
    u8 fromLobby;
    u8 padding_3A[2];
    u32 mimeJrTopVoiceChat;
} PlazaGameArgs;

typedef struct PlazaGameRoster {
    SaveData *saveData;
    u8 netIds[PLAZA_GAME_MAX_PLAYERS];
    u8 count;
    u8 localIndex;
    u8 voiceChat;
    u8 launchedFromField;
    PlazaGameArgs *args;
} PlazaGameRoster;

typedef struct PlazaGameResultData {
    u32 scores[PLAZA_GAME_MAX_PLAYERS];
    u8 ranks[PLAZA_GAME_MAX_PLAYERS];
    u32 total;
} PlazaGameResultData;

typedef struct PlazaGamePlayerInfo {
    u8 countries[PLAZA_GAME_MAX_PLAYERS];
    u8 regions[PLAZA_GAME_MAX_PLAYERS];
    PlayerProfile *profiles[PLAZA_GAME_MAX_PLAYERS];
} PlazaGamePlayerInfo;

typedef struct PlazaGameBgTemplate {
    u32 bgId;
    BgTemplate template;
} PlazaGameBgTemplate;

typedef struct PlazaGameBg {
    BgConfig *bgConfig;
    const PlazaGameBgTemplate *templates;
    u32 count;
} PlazaGameBg;

typedef struct PlazaGameText {
    MsgData *msgData;
    MessageFormat *msgFormat;
    String *string;
    String *buffer;
} PlazaGameText;

typedef struct PlazaGameMessageBox {
    Window windows[2];
    u16 printerIds[2];
    String *strings[2];
    WaitingIcon *waitingIcons[2];
    u32 colorVariant;
} PlazaGameMessageBox;

typedef struct PlazaGameSprites {
    SpriteList *spriteList;
    G2dRenderer renderer;
} PlazaGameSprites;

typedef struct PlazaGameGfxRes {
    u32 active;
    SpriteResource *resources[4];
    SpriteResourcesHeader header;
} PlazaGameGfxRes;

typedef struct PlazaGameGfx {
    GF_2DGfxResMan *managers[4];
    PlazaGameGfxRes *records;
    u32 count;
} PlazaGameGfx;

// Uniformly accelerated motion from start, reaching its end position after duration frames.
typedef struct PlazaGameMotion {
    fx32 current;
    fx32 start;
    fx32 velocity;
    fx32 accel;
    int duration;
} PlazaGameMotion;

// The game name banner that slides in at the top of the player list and the results screen.
typedef struct PlazaGameTitle {
    Window window;
    u32 game;
    String *string;
    PlazaGameMotion motion;
    s16 frame;
    s16 state;
} PlazaGameTitle;

// One row of the player list.
typedef struct PlazaGamePlayer {
    Window window;
    Sprite *sprite;
    Sprite *rankSprite;
    u16 count;
    u8 index;
    u8 seat;
    PlazaGameMotion motion;
    s16 frame;
    s16 delay;
    u32 se;
} PlazaGamePlayer;

// Per-scanline horizontal scroll of the player rows.
typedef struct PlazaGameLineScroll {
    PlazaGameBg *bg;
    u16 buffers[2][192];
    struct DoubleBuffer *buffer;
} PlazaGameLineScroll;

typedef struct PlazaGameWindowBits {
    u8 planes : 5;
    u8 effect : 1;
    u8 unused : 2;
} PlazaGameWindowBits;

// Window 0 frame around the countdown / finish sign, opened by two sliding bars.
typedef struct PlazaGameSignWindow {
    Sprite *sprites[2];
    s16 step;
    s16 offset;
    u8 lines[192];
    u8 pendingLines[192];
    SysTask *task;
    PlazaGameWindowBits savedOutside;
    PlazaGameWindowBits signOutside;
    u8 unk192[2];
    u32 savedWindows;
} PlazaGameSignWindow;

typedef struct PlazaGameSign {
    enum HeapID heapId;
    u16 state;
    s16 timer;
    SpriteList *spriteList;
    NARC *narc;
    PlazaGameGfx gfx;
    PlazaGameGfxRes *gfxRes;
    SysTask *task;
    Sprite *sprite;
    PlazaGameSignWindow window;
} PlazaGameSign;

// "Would you like to keep playing?" prompt and the vote of all players.
typedef struct PlazaGameContinue {
    u8 state;
    u8 voiceChat;
    u16 timer;
    YesNoPrompt *prompt;
    YesNoPromptTemplate template;
    u8 votes;
    u8 allYes;
    u8 received;
    u8 result;
} PlazaGameContinue;

// Popped balloon counter of the WOBBUFFET Pop results.
typedef struct PlazaGameBalloons {
    void *screenRaw;
    NNSG2dScreenData *screen;
    Sprite *sprites[15];
    PlazaGameGfxRes *gfxRes;
    u8 state;
    u8 timer;
    u8 count;
    u8 total;
} PlazaGameBalloons;

// Player list shown before a game ("These players have been accepted.").
typedef struct PlazaGameEntry {
    u8 state;
    u8 timer;
    u16 heapId;
    u16 game;
    u8 selection;
    u8 selectionReceived;
    PlazaGameRoster roster;
    PlazaGamePlayerInfo info;
    PlazaGameBg bg;
    PlazaGameText text;
    PlazaGameMessageBox messageBox;
    PlazaGameSprites sprites;
    PlazaGameGfx gfx;
    PlazaGameGfxRes *gfxRes;
    PlazaGameTitle title;
    void *screenRaw;
    NNSG2dScreenData *screen;
    PlazaGamePlayer players[PLAZA_GAME_MAX_PLAYERS];
    PlazaGameLineScroll lineScroll;
    SysTask *task;
    SysTask *vblankTask;
} PlazaGameEntry;

// Results screen shown after a game ("Here are the results!").
typedef struct PlazaGameResults {
    u16 timer;
    u16 heapId;
    u8 state;
    u8 showCountries;
    u8 done;
    u8 game;
    u8 continueResult;
    u8 tapToyPoweredUp;
    u8 unk0A[2];
    PlazaGameRoster roster;
    PlazaGameResultData data;
    PlazaGamePlayerInfo info;
    PlazaGameBg bg;
    PlazaGameText text;
    PlazaGameMessageBox messageBox;
    PlazaGameSprites sprites;
    PlazaGameGfx gfx;
    PlazaGameGfxRes *gfxRes;
    PlazaGameContinue continuePrompt;
    PlazaGameBalloons balloons;
    void *screenRaw;
    NNSG2dScreenData *screen;
    PlazaGamePlayer players[PLAZA_GAME_MAX_PLAYERS];
    PlazaGameLineScroll lineScroll;
    SysTask *task;
    SysTask *vblankTask;
    u32 prevTapToy;
    u16 rankStep;
    u16 delay;
    void *flashPlttRaw;
    NNSG2dPaletteData *flashPltt;
    u16 flashFrame;
    u16 flashEnabled;
    u32 fanfareState;
} PlazaGameResults;

extern const u8 gPlazaGameMimeJrTopSelectionWeights[];
extern const u8 gPlazaGameWobbuffetPopSelectionWeights[];
extern const u8 gPlazaGameSwalotPlopSelectionWeights[];
extern const u8 gPlazaGameResultsFlashSlots[];
extern const s16 gPlazaGameResultsRankOffsets[];
extern const fx32 gPlazaGameTitleSlideOutEndX[];
extern const fx32 gPlazaGameTitleSlideOutVelocity[];
extern const u32 gPlazaGameTitleColors[];
extern const fx32 gPlazaGameTitleSlideInVelocity[];
extern const fx32 gPlazaGameTitleRestX[];
extern const fx32 gPlazaGameTitleBounceX[];
extern const fx32 gPlazaGameTitleBounceOutVelocity[];
extern const fx32 gPlazaGameTitleBounceBackVelocity[];
extern const fx32 gPlazaGameTitleSlideInStartX[];
extern const GraphicsModes gPlazaGameResultsGraphicsModes;
extern const s16 gPlazaGamePlayerSpritePositions[];
extern const s16 gPlazaGamePlayerSpriteY[];
extern const u8 gPlazaGameSeatOrders[][4];
extern const ObjCharTransferTemplate gPlazaGameObjCharTransfer;
extern const GraphicsModes gPlazaGameEntryGraphicsModes;
extern const VecFx32 gPlazaGameSignWindowSpritePositions[2];
extern const GraphicsBanks gPlazaGameEntryBanks;
extern const GraphicsBanks gPlazaGameResultsBanks;
extern const u8 gPlazaGameSeatRowsByCount[][12];
extern const u8 gPlazaGameScreenRowsByCount[][12];
extern const u8 gPlazaGameSeatAnimsByCount[][12];
extern const u8 gPlazaGameSeatRows[][12];
extern const u8 gPlazaGameSeatAnims[][12];
extern const u16 gPlazaGameAltMessages[20][2];
extern const PlazaGameBgTemplate gPlazaGameEntryBgTemplates[];
extern const PlazaGameBgTemplate gPlazaGameResultsBgTemplates[];
#if defined(SDK_PORT)
#include "communication_command.h"
extern const CommCommand gPlazaGameCommands[];
#else
extern const u8 gPlazaGameCommands[];
#endif

void PlazaGameRoster_Init(PlazaGameRoster *roster, u32 fromLobby, SaveData *saveData, u8 voiceChat, PlazaGameArgs *args);
int PlazaGameRoster_GetIndexOfNetId(const PlazaGameRoster *roster, u32 netId);
BOOL PlazaGameRoster_IsSpecialVisitor(const PlazaGameRoster *roster, u32 netId);
PlayerProfile *PlazaGameRoster_GetProfile(const PlazaGameRoster *roster, u32 netId);
PlazaGameEntry *PlazaGameEntry_NewSwalotPlop(const PlazaGameRoster *roster, enum HeapID heapId);
PlazaGameEntry *PlazaGameEntry_NewMimeJrTop(const PlazaGameRoster *roster, enum HeapID heapId);
PlazaGameEntry *PlazaGameEntry_NewWobbuffetPop(const PlazaGameRoster *roster, enum HeapID heapId);
void PlazaGameEntry_Free(PlazaGameEntry *entry);
BOOL PlazaGameEntry_IsDone(const PlazaGameEntry *entry);
u8 PlazaGameEntry_GetSelection(const PlazaGameEntry *entry);
PlazaGameResults *PlazaGameResults_NewSwalotPlop(const PlazaGameRoster *roster, const PlazaGameResultData *data, enum HeapID heapId);
PlazaGameResults *PlazaGameResults_NewMimeJrTop(const PlazaGameRoster *roster, const PlazaGameResultData *data, enum HeapID heapId);
PlazaGameResults *PlazaGameResults_NewWobbuffetPop(const PlazaGameRoster *roster, const PlazaGameResultData *data, enum HeapID heapId);
void PlazaGameResults_Free(PlazaGameResults *results);
u8 PlazaGameResults_IsDone(const PlazaGameResults *results);
u8 PlazaGameResults_GetContinue(const PlazaGameResults *results);
void PlazaGame_CalcRanks(PlazaGameResultData *data, u32 count);
BOOL PlazaGameRoster_CheckConnectionLost(PlazaGameRoster *roster);
BOOL PlazaGameRoster_ProcessConnectionLost(PlazaGameRoster *roster);
PlazaGameSign *PlazaGameSign_New(SpriteList *spriteList, enum HeapID heapId);
void PlazaGameSign_Free(PlazaGameSign *sign);
int PlazaGameSign_GetPlttOffset(PlazaGameSign *sign);
void PlazaGameSign_StartCountdown(PlazaGameSign *sign);
void PlazaGameSign_StartFinish(PlazaGameSign *sign);
BOOL PlazaGameSign_IsDone(const PlazaGameSign *sign);

// Internal.
int PlazaGame_GetCommandSizeNone(void);
int PlazaGame_GetCommandSizeU32(void);
void PlazaGame_HandleSelectionCommand(int netId, int size, int *data, PlazaGameEntry *entry);
void PlazaGame_HandleVoteYesCommand(int netId, int size, void *data, PlazaGameResults *results);
void PlazaGame_HandleVoteNoCommand(int netId, int size, void *data, PlazaGameResults *results);
void PlazaGame_HandleContinueCommand(int netId, int size, void *data, PlazaGameResults *results);
void PlazaGame_HandleQuitCommand(int netId, int size, void *data, PlazaGameResults *results);

#endif // POKEHEARTGOLD_APPLICATION_PLAZA_GAME_H
