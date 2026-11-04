#ifndef POKEHEARTGOLD_GTS_INTERNAL_H
#define POKEHEARTGOLD_GTS_INTERNAL_H

#include "dwc/net_common_internal.h"

#include "bg_window.h"
#include "launch_application_internal.h"
#include "message_format.h"
#include "msgdata.h"
#include "overlay_manager.h"
#include "player_data.h"
#include "render_window.h"
#include "sprite.h"
#include "touchscreen_list_menu.h"
#include "trade_anim.h"
#include "sprite_resource_header.h"
#include "sprite_resource.h"
#include "save_wii_message.h"
#include "evolution_scene.h"
#include "pokemon_summary.h"
#include "yes_no_prompt.h"

// A deposited Pokemon as the server stores it (the payload of post.asp, the answers of get.asp/result.asp,
// search.asp returns a list of them).
#define GTS_RECORD_SIZE 0x124

// GtsApp.offeredBox of a Pokemon offered from the party.
#define GTS_BOX_PARTY 18

// What a record says about its Pokemon. gender: 1 male, 2 female, 3 genderless.
typedef struct GtsMonSummary {
    s16 species;
    s8 gender;
    s8 level;
} GtsMonSummary;

// The Pokemon the depositor wants in exchange.
typedef struct GtsWantedMon {
    s16 species;
    s8 gender;
    s8 minLevel;
    s8 maxLevel;
    s8 unk_5;
} GtsWantedMon;

typedef struct GtsRecord {
    Pokemon mon;
    GtsMonSummary summary;
    GtsWantedMon wanted;
    u8 trainerGender;
    u8 unk_0F7[0x11];
    u32 exchangeId; // exchange.asp is sent it after the offered record to pick this one
    u16 trainerName[PLAYER_NAME_LENGTH + 1];
    u16 trainerId;
    u8 country;
    u8 city;
    u8 trainerClass;
    s8 isTraded; // nonzero once the Pokemon was traded (get.asp)
    u8 version;
    u8 language;
} GtsRecord;

typedef enum GtsServerState {
    GTS_SERVER_STATE_NONE,
    GTS_SERVER_STATE_IDLE,
    GTS_SERVER_STATE_POST,
    GTS_SERVER_STATE_POST_FINISH = 4,
    GTS_SERVER_STATE_GET = 6,
    GTS_SERVER_STATE_RESULT = 8,
    GTS_SERVER_STATE_DELETE = 10,
    GTS_SERVER_STATE_RETURN = 12,
    GTS_SERVER_STATE_SEARCH = 14,
    GTS_SERVER_STATE_EXCHANGE = 16,
    GTS_SERVER_STATE_EXCHANGE_FINISH = 18,
    GTS_SERVER_STATE_INFO = 20,
    GTS_SERVER_STATE_SET_PROFILE = 22,
    GTS_SERVER_STATE_DONE = 24,
} GtsServerState;

// What search.asp is sent: the species and further criteria, then the number of records wanted.
typedef struct GtsSearchCriteria {
    u8 bytes[6];
} GtsSearchCriteria;

typedef struct GtsSearchCriteriaEx {
    u8 bytes[6];
    s8 count;
    u8 unk_7;
} GtsSearchCriteriaEx;

typedef struct GtsServer {
    int state; // GtsServerState
    int result;
    int pid;
    u32 friendKey[2];

    union {
        u8 bytes[GTS_RECORD_SIZE + 4];
        GtsSearchCriteriaEx search;
    } request; // the payload of the request in flight
} GtsServer;

typedef struct GtsFriendKey {
    u32 lo;
    u32 hi;
} GtsFriendKey;

typedef struct GtsServerPending {
    u8 padding_00[0x38];
    u32 exchangeId; // sent after the record by exchange.asp
    u8 padding_3C[4];
    u8 *response; // buffer of the request in flight
} GtsServerPending;

typedef struct GtsServerAnswer {
    u8 padding_00[0x1C];
    u8 shortResponse[4]; // [0] is the server's answer code
} GtsServerAnswer;

extern volatile GtsServer gGtsServer; // polled every frame (GtsServer_Update)
#if defined(SDK_PORT)
#else
extern GtsFriendKey gGtsServerFriendKey;
extern u8 gGtsServerRequest[];
#endif
extern GtsServerPending gGtsServerPending;
extern volatile GtsServerAnswer gGtsServerAnswer; // written by the response callback
#if defined(SDK_PORT)
// The DS labels these inside gGtsServer / gGtsServerAnswer; natively they are views of those members.
#define gGtsServerFriendKey (*(GtsFriendKey *)gGtsServer.friendKey)
#define gGtsServerRequest ((u8 *)gGtsServer.request.bytes)
#define gGtsServerShortResponse ((u8 *)gGtsServerAnswer.shortResponse)
#else
extern u8 gGtsServerShortResponse[];
#endif

// {first letter, letter count} of the letter groups ABC .. YZ.
typedef struct GtsLetterGroup {
    int first;
    int count;
} GtsLetterGroup;

// The first letter of each letter group.
extern const int gGtsSelectorGroupStarts[];
extern const GtsLetterGroup gGtsSelectorLetterGroups[];
// The gender symbol message of each GTS gender (1 male, 2 female, 0 and 3 either).
extern u32 gGtsGenderSymbolMsgs[];

// The screens of the app, in sGtsScreens order
enum GtsScreenId {
    GTS_SCREEN_CONNECTION,
    GTS_SCREEN_MAIN_MENU,
    GTS_SCREEN_SUMMARY,
    GTS_SCREEN_SEARCH_RESULT,
    GTS_SCREEN_SEARCH,
    GTS_SCREEN_SELECT_MON,
    GTS_SCREEN_WANTED,
    GTS_SCREEN_TRANSACTION,
    GTS_SCREEN_VIEW_MON,
    GTS_SCREEN_TRADE,
};

// Gts_Main states; the screen functions return the next one
enum GtsAppState {
    GTS_APP_STATE_WAIT_WIRELESS,
    GTS_APP_STATE_INIT_SCREEN,
    GTS_APP_STATE_WAIT_FADE_IN,
    GTS_APP_STATE_RUN_SCREEN,
    GTS_APP_STATE_EXIT_SCREEN,
    GTS_APP_STATE_END,
};

typedef struct GtsApp GtsApp;

// The cursor of SEEK POKEMON (column 0: rows 0..3, column 1: rows 0..2).
typedef struct GtsSearchCursor {
    u8 unk_00[4];
    u16 unk_04;
    u16 unk_06;
    u16 unk_08;
    u16 unk_0A;
    u8 unk_0C[8];
    u8 *johtoDexFlags;
    u16 *alphabeticalSpecies;
    u32 alphabeticalSpeciesCount;
    u32 genderRatio; // of the wanted species
    int column;
    int leftRow;
    int rightRow;
} GtsSearchCursor;

// The icon of a Pokemon of the shown box, loaded into VRAM in the next VBlank.
typedef struct GtsMonIcon GtsMonIcon;

typedef struct GtsCursorValueTable {
    u16 unk_00;
    u16 unk_02;
    u16 unk_04[10];
    u16 unk_18[10];
} GtsCursorValueTable;

// The selector of species, gender, level and country.
typedef struct GtsSelector GtsSelector;

// What the selector is created with.
typedef struct GtsSelectorArgs {
    BgConfig *bgConfig;
    Window *windows;
    Window *messageWindow;
    Sprite *sprites[4]; // the cursor, the right and left arrows, the screen's own cursor (may be NULL)
    MsgData *gtsMsgData;
    MsgData *speciesNames;
    MsgData *countryNames;
    Pokedex *pokedex;
    u8 *johtoDexFlags;
} GtsSelectorArgs;

typedef struct GtsGlobals {
    NNSFndHeapHandle heap; // allocations of the network library
    GtsApp *app;
} GtsGlobals;

extern GtsGlobals gGtsGlobals;

typedef void (*GtsVBlankFunc)(GtsApp *app);

// The background palette cycle of screen 0, run as a VBlank task.
typedef struct GtsPaletteAnim {
    SysTask *task;
    BOOL active;
    u16 basePltt[4 * 16];
    u16 fadePltt[21 * 16]; // palette 0 blended in 7 steps towards each of palettes 1, 2 and 3
    s16 frame;             // in fadePltt
    s8 reverse;
    u8 skipFrame; // the palette changes every other frame
    u32 screens;  // 0: both, 1: main, 2: sub
} GtsPaletteAnim;

struct GtsApp {
    GtsArgs *args;
    BgConfig *bgConfig;
    u8 padding_0008[0xC];
    enum GtsScreenId screen;
    enum GtsScreenId nextScreen;
    int statusCheckCaller;
    enum GtsScreenId prevScreen;
    int nextScreenArg;
    int afterSaveMsgId; // printed once the save of screen 7 is written
    int state;          // of the current screen
    int nextState;
    u16 mainMenuShown;   // the main menu was entered before
    u16 hasDepositedMon; // the player has a Pokemon on the GTS
    u8 padding_0038[2];
    u16 timer;            // frames until the GTS may be checked again
    int serverResult;     // of the failed request (screen 7 picks the error message from it)
    int connectionResult; // of a failed Wi-Fi connection (DWC_GetLastErrorEx)
    int connectionErrorCode;
    int connectionErrorType;
    void *heapMemory;
    NNSFndHeapHandle heap;        // in heapMemory
    NetConnection wifiConnection; // passed to DWC_InitInetEx when connecting (screen 0)
    OverlayManager *tradeSequence;
    PokemonSummaryArgs summaryArgs; // of screen 8
    TRADE_ANIM_WORK tradeAnim;
    EvolutionTaskData *evolutionTask;
    u32 reloadGraphics; // the screen asks Gts_Main to rebuild the graphics
    u16 unk_0118;
    u8 padding_011A[2];
    u16 menuCursor;
    u8 padding_011E[2];
    u16 selectedBox;         // shown by screen 5, GTS_BOX_PARTY for the party
    u16 selectedSlot;        // screen 5's cursor: slot 0..29, 30 BACK, 31 the box name
    BoxPokemon *selectedMon; // the Pokemon to deposit or to trade
    u32 searchResultCount;   // in searchResults
    int selectedResult;      // in searchResults
    PlayerProfile *partnerProfile;
    u32 offeredBox; // GTS_BOX_PARTY for the party
    u32 offeredSlot;
    GtsRecord record;
    GtsRecord searchResults[8];   // [7] also receives the record of an exchange (screen 7)
    GtsMonSummary depositSummary; // of the Pokemon to deposit
    GtsWantedMon depositWanted;   // what the player wants for it
    GtsWantedMon searchCriteria;  // of SEEK POKEMON
    GtsWantedMon lastSearch;
    u8 padding_0B96[2];
    u32 lastSearchCountry;
    MessageFormat *msgFormat;
    MsgData *gtsMsgData;
    MsgData *speciesNames;
    MsgData *wifiMsgData;
    MsgData *wifiErrorMsgData;
    MsgData *countryNames;
    String *boxName;
    String *cancelText;
    String *msgBuffer;
    String *titleString;
    String *infoStrings[10];
    String *errorBuffer; // the Wi-Fi error message of screen 0
    int textPrinterId;
    SpriteList *spriteList;
    G2dRenderer renderer;
    GF_2DGfxResMan *resourceManagers[4]; // char/pltt/cell/anim
    SpriteResource *resources[8];        // [main/sub screen][char/pltt/cell/anim]
    u8 padding_0D50[0x10];
    SpriteResourcesHeader headers[2];
    u8 padding_0DA8[0x24];
    Sprite *cursorSprite;
    Sprite *selectorCursorSprite;
    u8 padding_0DD4[4];
    Sprite *monIconSprites[30]; // of the shown box
    Sprite *heldItemSprites[30];
    Sprite *capsuleSprites[6]; // the party's Ball Capsules
    Sprite *monSprite;
    Sprite *avatarSprites[8];   // [0] the player, [1 + i] the trainer of searchResults[i]
    Sprite *arrowSprites[2];    // screen 5's box arrows; the selector's on screen 4
    Sprite *avatarMarkerSprite; // over the selected trainer
    Sprite *panelSprite;        // of the sub screen panel
    int scrollY;                // of a search result's screens
    Window waitingIconWindow;   // also the message window
    u8 padding_0F28[0x10];
    Window titleWindow;
    Window boxNameWindow;   // of screen 5; screen 0's Wi-Fi error window
    Window windows[16];     // each screen's own windows
    Window infoWindows[16]; // a Pokemon's summary, trainer and wanted Pokemon
    Window promptWindow;    // the message window of a Yes/No prompt
    Window selectorWindow;
    Window wideWindows[2];
    Window panelWindow;    // the caption of the sub screen panel
    GtsSelector *selector; // the species/gender/level/country selector
    ListMenuItem *listItems;
    struct ListMenu2D *disconnectMenu; // only read by screen 0 (GtsConnection_HandleDisconnectMenu)
    u8 padding_11B4[8];
    WaitingIcon *waitingIcon;
    int waitFrames;
    GtsSearchCursor *searchCursor;
    YesNoPrompt *yesNoPrompt;
    TouchscreenListMenuSpawner *menuSpawner;
    TouchscreenListMenu *menu;
    u16 saveMidState;  // screen 7's state once the first part of the save is written
    u16 saveDoneState; // and once the save is complete
    SysTask *playerAnimTask;
    u16 playerAnimDone; // set by the player's walk in/out task (Gts_StartPlayer*Anim)
    u16 hasSearchResults;
    void *avatarCharAlloc;
    NNSG2dCharacterData *avatarCharData;
    void *avatarPlttAlloc;
    NNSG2dPaletteData *avatarPlttData;
    Pokemon *depositedMon;       // copy of the deposited Pokemon (GtsTrade)
    GtsMonSummary *boxSummaries; // of the shown box's Pokemon
    u16 boxMonCount;             // Pokemon and Eggs in the PC, counted one box per frame
    u16 countingBox;             // 1-based box being counted, 0 when done
    int fadedBothScreens;
    GtsMonIcon *monIcons;      // of the shown box, freed by the VBlank task
    GtsVBlankFunc vblankTask;  // run once
    GtsVBlankFunc vblankFunc;  // run every frame
    s16 avatarPositions[8][2]; // {x, y} of avatarSprites
    u8 padding_122C[8];
    GtsCursorValueTable unk_1234;
    WifiServerProfile profile;  // sent by GtsServer_RequestSetProfile (screen 0)
    u32 profileResponse[2];     // its answer
    u32 searchCountry;          // of SEEK POKEMON, 0 for any
    GtsPaletteAnim paletteAnim; // of screen 0
    s16 subState;               // of screen 0's state, reset when the state changes
    s16 subTimer;               // likewise
    u32 requestFrames;          // frames the request in flight took
};

// Helpers shared by the GTS screens.
extern enum GtsAppState (*const gGtsConnectionStates[])(GtsApp *app);
extern const BgTemplate gGtsConnectionBg0Template;
extern const BgTemplate gGtsConnectionBg1Template;
extern const BgTemplate gGtsConnectionSubBg0Template;
extern const BgTemplate gGtsConnectionSubBg1Template;
// GTS, GTS TRADE POKEMON, GTS SUMMARY, GTS DEPOSIT POKEMON, GTS SEEK POKEMON.
extern const u32 gGtsPanelCaptionMsgIds[];

typedef struct GtsSelectorWindowPosition {
    u8 x;
    u8 y;
} GtsSelectorWindowPosition;

typedef struct GtsSelectorCursorPos {
    u8 x;
    u8 y;
} GtsSelectorCursorPos;

typedef struct GtsSelectorCursorNext {
    u8 up;
    u8 down;
    u8 left;
    u8 right;
} GtsSelectorCursorNext;

typedef struct GtsSelectorWindowPosition3 {
    u8 x;
    u8 y;
    u8 unused;
} GtsSelectorWindowPosition3;

extern int (*const gGtsSelectorStates[])(GtsSelector *selector);
extern const GtsSelectorWindowPosition gGtsSelectorGenderWindowPositions[];
extern const GtsSelectorWindowPosition gGtsSelectorLevelWindowPositions[];
extern const GtsSelectorWindowPosition gGtsSelectorCountryWindowPositions[];
extern const GtsSelectorWindowPosition3 gGtsSelectorSpeciesWindowPositions[];
extern const GtsSelectorWindowPosition gGtsSelectorGridWindowPositions[];
// The cursor positions are {x, y} pairs. The list cursors are in pixels, the grid cursors in tiles of the panel.
extern const GtsSelectorCursorPos gGtsSelectorGenderCursorPositions[];
extern const GtsSelectorCursorPos gGtsSelectorSpeciesCursorPositions[];
extern const GtsSelectorCursorPos gGtsSelectorLevelCursorPositions[];
extern const GtsSelectorCursorPos gGtsSelectorCountryCursorPositions[];
extern const GtsSelectorCursorPos gGtsSelectorCountryGroupCursorPositions[];
extern const GtsSelectorCursorPos gGtsSelectorGroupCursorPositions[];
// The value of each row of the gender list: EITHER MON_GENDERLESS, then MON_MALE, MON_FEMALE, BACK -2.
extern const int gGtsSelectorGenders[];
// The cursor's next button for up, down, left, right on each button.
extern const GtsSelectorCursorNext gGtsSelectorLetterNext[];
extern const GtsSelectorCursorNext gGtsSelectorGroupNext[];
extern const GtsSelectorCursorNext gGtsSelectorCountryGroupNext[];
extern const TouchscreenHitbox gGtsSelectorGroupHitboxes[];
extern const TouchscreenHitbox gGtsSelectorCountryGroupHitboxes[];
extern const TouchscreenHitbox gGtsSelectorListHitboxes[];
extern const TouchscreenHitbox gGtsSelectorGenderHitboxes[];
extern const TouchscreenHitbox gGtsSelectorCountryListHitboxes[];
extern const TouchscreenHitbox gGtsSelectorLetterHitboxes[];
// The message of each letter, A .. Z.
extern const int gGtsSelectorLetterMsgs[];

// The OBJ char offset of each trainer avatar.
extern const u16 gGtsAvatarCharOffsets[];

typedef struct GtsAvatarPosition {
    u16 x;
    u16 y;
} GtsAvatarPosition;

extern const GtsAvatarPosition gGtsAvatarPositions[];
extern const TouchscreenHitbox gGtsAvatarHitboxes[];
extern const u8 gGtsViewMonSummaryPages[];

extern int gGtsCountryCount[];

extern const OverlayManagerTemplate gGtsTradeSequenceTemplate;

extern enum GtsAppState (*const gGtsTransactionStates[])(GtsApp *app);
extern const GraphicsModes gGtsTransactionGraphicsModes;
extern const BgTemplate gGtsTransactionBg0Template;
extern const BgTemplate gGtsTransactionBg1Template;
extern const BgTemplate gGtsTransactionBg2Template;
extern const BgTemplate gGtsTransactionSubBg0Template;
extern const BgTemplate gGtsTransactionSubBg1Template;

typedef struct GtsWantedWindowPosition {
    u16 x;
    u16 y;
} GtsWantedWindowPosition;

// A choice of the level list: kind 0 the levels a deposit asks for (12), kind 1 the levels SEEK POKEMON searches (11).
typedef struct GtsLevelRange {
    u32 msgId;
    s16 minLevel; // 0: none
    s16 maxLevel; // 0: none
} GtsLevelRange;

// The countries in list order, in groups by initial letter: {first index in gGtsCountries, group}.
typedef struct GtsCountryGroup {
    u8 start;
    u8 group;
} GtsCountryGroup;

extern enum GtsAppState (*const gGtsWantedStates[])(GtsApp *app);
extern const BgTemplate gGtsWantedBg0Template;
extern const BgTemplate gGtsWantedBg1Template;
extern const BgTemplate gGtsWantedBg2Template;
extern const BgTemplate gGtsWantedBg3Template;
extern const GtsWantedWindowPosition gGtsWantedWindowPositions[];
// The text colors of the male and female symbols.
extern u32 gGtsGenderColors[];
extern const GtsLevelRange gGtsWantedLevelRanges[];
extern const GtsLevelRange gGtsSearchLevelRanges[];
// NARC_zukan_data_sort_order_letters_*.
extern const int gGtsSpeciesLetterFiles[];
extern const u16 gGtsCountries[];
extern const GtsCountryGroup gGtsCountryGroups[];

void Gts_InitSpriteTemplate(SpriteTemplate *template, GtsApp *app, const SpriteResourcesHeader *header, NNS_G2D_VRAM_TYPE screen);
YesNoPrompt *Gts_CreateYesNoPrompt(BgConfig *bgConfig, u8 y, int tileStart, u32 palette, u8 dimScreen);
YesNoPrompt *Gts_CreateYesNoPromptOnBg(BgConfig *bgConfig, u8 y, int tileStart, u32 palette, int bgId, u8 dimScreen);
YesNoResponse Gts_HandleYesNoPrompt(GtsApp *app);
TouchscreenListMenu *Gts_CreateListMenu(GtsApp *app, u8 numWindows, int y);
void Gts_DestroyListMenu(GtsApp *app);
void Gts_SetState(GtsApp *app, int state, int nextState);
void Gts_SetSpritePositionRounded(Sprite *sprite, int x, int y);
int Gts_GetWifiIconLevel(void);
void Gts_SetNextScreen(GtsApp *app, enum GtsScreenId screen, int arg);
void Gts_SwitchScreen(GtsApp *app);
int Gts_GetTextFrameDelay(GtsApp *app);
void Gts_StartCountingBoxMons(GtsApp *app);
void Gts_CreateWaitingIcon(GtsApp *app);
void Gts_DeleteWaitingIcon(GtsApp *app);
void Gts_SetSpritePosition(Sprite *sprite, int x, int y);
void Gts_DimScreen(BOOL mainScreen);

// The GTS main menu.
void Gts_InitSubScreenBgs(BgConfig *bgConfig, int bg1Y, BOOL keepBg2Tilemap);
void Gts_FreeSubScreenBgs(BgConfig *bgConfig);

// The sub screen panel and the Pokemon summary windows.
void Gts_LoadSubScreenBg(GtsApp *app);
void Gts_ShowSubScreenPanel(GtsApp *app);
void Gts_CreatePanelWindow(GtsApp *app, int caption);
void Gts_DrawMonSummary(MsgData *msgData, MsgData *speciesNames, MessageFormat *msgFormat, Window *windows, BoxPokemon *boxMon, GtsMonSummary *summary);
void Gts_DrawTrainerInfo(MsgData *msgData, Window *trainerWindows, const u16 *trainerName, Pokemon *mon, Window *otWindows);
void Gts_LoadMonPicture(Pokemon *mon);

// SEEK POKEMON.
void GtsSearch_PrintMessage(GtsApp *app, int msgId, int textSpeed, int unused3, u32 unused4);
void GtsSearch_DrawFieldLabels(Window *fieldWindows, Window *lineWindow, MsgData *msgData);
void GtsSearch_DrawResultsButton(BgConfig *bgConfig, Window *window, MsgData *msgData, BOOL enabled);
BOOL GtsSearch_IsSameSearch(GtsWantedMon *criteria, GtsWantedMon *lastSearch, u32 country, u32 lastCountry);
void GtsSearch_ScrollBgs(GtsApp *app);
void GtsSearch_SetBlendBrightness(int brightness);

// The avatars on the sub screen.
void Gts_CreateAvatars(GtsApp *app, u32 gender);
void Gts_StartPlayerEnterAnim(GtsApp *app, int gender);
void Gts_StartPlayerExitAnim(GtsApp *app, int gender);
int Gts_GetTouchedAvatar(int count);
void Gts_ShowResultAvatars(GtsApp *app, int count, BOOL animate);
void Gts_StopAvatarAnims(GtsApp *app);
void Gts_FreeAvatarGraphics(GtsApp *app);
void Gts_SetAvatarMarker(GtsApp *app, u32 index, u32 scrollY);
void Gts_ResetAvatarPositions(GtsApp *app);
void Gts_LowerAvatars(GtsApp *app);

// Choosing the Pokemon to deposit or to trade.
BOOL Gts_IsPartyBox(int box);
BoxPokemon *Gts_GetBoxMon(Party *party, PCStorage *pcStorage, int box, int slot);
BOOL Gts_MonHoldsMail(Pokemon *mon);

// Choosing the wanted Pokemon of a deposit, and the helpers that follow it: the wanted Pokemon's
// texts, level ranges, species and country lists, the record of a deposit or trade.
// align: 0 left, 1 centered, 2 right in the window.
BOOL Gts_SetFixedGender(GtsWantedMon *wanted, u32 genderRatio);

// The screens: init, main and exit return the next Gts_Main state.
enum GtsAppState GtsConnection_Init(GtsApp *, int);
enum GtsAppState GtsConnection_Main(GtsApp *, int);
enum GtsAppState GtsConnection_Exit(GtsApp *, int);
enum GtsAppState GtsMainMenu_Init(GtsApp *, int);
enum GtsAppState GtsMainMenu_Main(GtsApp *, int);
enum GtsAppState GtsMainMenu_Exit(GtsApp *, int);
enum GtsAppState GtsSummary_Init(GtsApp *, int);
enum GtsAppState GtsSummary_Main(GtsApp *, int);
enum GtsAppState GtsSummary_Exit(GtsApp *, int);
enum GtsAppState GtsSearchResult_Init(GtsApp *, int);
enum GtsAppState GtsSearchResult_Main(GtsApp *, int);
enum GtsAppState GtsSearchResult_Exit(GtsApp *, int);
enum GtsAppState GtsSearch_Init(GtsApp *, int);
enum GtsAppState GtsSearch_Main(GtsApp *, int);
enum GtsAppState GtsSearch_Exit(GtsApp *, int);
enum GtsAppState GtsSelectMon_Init(GtsApp *, int);
enum GtsAppState GtsSelectMon_Main(GtsApp *, int);
enum GtsAppState GtsSelectMon_Exit(GtsApp *, int);
enum GtsAppState GtsWanted_Init(GtsApp *, int);
enum GtsAppState GtsWanted_Main(GtsApp *, int);
enum GtsAppState GtsWanted_Exit(GtsApp *, int);
enum GtsAppState GtsTransaction_Init(GtsApp *, int);
enum GtsAppState GtsTransaction_Main(GtsApp *, int);
enum GtsAppState GtsTransaction_Exit(GtsApp *, int);

// The search screen states; some of them are next to the Pokemon selection screen
typedef enum GtsSearchState {
    GTS_SEARCH_STATE_START,
    GTS_SEARCH_STATE_HANDLE_INPUT,
    GTS_SEARCH_STATE_FADE_OUT,
    GTS_SEARCH_STATE_ASK_SPECIES,
    GTS_SEARCH_STATE_OPEN_SPECIES_SELECTOR,
    GTS_SEARCH_STATE_HANDLE_SPECIES_SELECTOR,
    GTS_SEARCH_STATE_ASK_GENDER,
    GTS_SEARCH_STATE_OPEN_GENDER_SELECTOR,
    GTS_SEARCH_STATE_HANDLE_GENDER_SELECTOR,
    GTS_SEARCH_STATE_ASK_LEVEL,
    GTS_SEARCH_STATE_OPEN_LEVEL_SELECTOR,
    GTS_SEARCH_STATE_HANDLE_LEVEL_SELECTOR,
    GTS_SEARCH_STATE_ASK_COUNTRY,
    GTS_SEARCH_STATE_OPEN_COUNTRY_SELECTOR,
    GTS_SEARCH_STATE_HANDLE_COUNTRY_SELECTOR,
    GTS_SEARCH_STATE_START_SEARCH,
    GTS_SEARCH_STATE_SEND_SEARCH,
    GTS_SEARCH_STATE_WAIT_SEARCH,
    GTS_SEARCH_STATE_SHOW_RESULT_COUNT,
    GTS_SEARCH_STATE_WAIT_RESULTS_SHOWN,
    GTS_SEARCH_STATE_SEARCH_FAILED,
    GTS_SEARCH_STATE_WAIT_TEXT,
    GTS_SEARCH_STATE_WAIT_TEXT_AND_PAUSE,
    GTS_SEARCH_STATE_SHOW_EXIT_PROMPT,
    GTS_SEARCH_STATE_HANDLE_EXIT_PROMPT,
    GTS_SEARCH_STATE_RETURN_TO_INPUT,
    GTS_SEARCH_STATE_PRINT_SEARCH_FAILED,
    GTS_SEARCH_STATE_SERVER_ERROR,
    GTS_SEARCH_STATE_SERVER_ERROR_EXIT,
    GTS_SEARCH_STATE_SCROLL_OUT,
    GTS_SEARCH_STATE_SCROLL_IN,
    GTS_SEARCH_STATE_SCROLLED_OUT,
} GtsSearchState;

enum GtsAppState GtsSearch_PrintSearchFailed(GtsApp *);
enum GtsAppState GtsSearch_WaitText(GtsApp *);
enum GtsAppState GtsSearch_WaitTextAndPause(GtsApp *);
enum GtsAppState GtsSearch_ScrollOut(GtsApp *);
enum GtsAppState GtsSearch_ScrollIn(GtsApp *);
enum GtsAppState GtsSearch_HandleScrolledOut(GtsApp *);

enum GtsAppState GtsViewMon_Init(GtsApp *, int);
enum GtsAppState GtsViewMon_Main(GtsApp *, int);
enum GtsAppState GtsViewMon_Exit(GtsApp *, int);
enum GtsAppState GtsTrade_Init(GtsApp *, int);
enum GtsAppState GtsTrade_Main(GtsApp *, int);
enum GtsAppState GtsTrade_Exit(GtsApp *, int);

void Gts_PrintSpeciesName(Window *window, MsgData *speciesNames, int species, int align, int y, u32 color);
void Gts_PrintSpeciesNameNoTransfer(Window *window, MsgData *speciesNames, int species, int y, u32 color);
void Gts_PrintCountryName(Window *window, MsgData *countryNames, MsgData *msgData, u32 country, int align, int y, u32 color);
void Gts_PrintGender(Window *window, MsgData *msgData, int gender, BOOL showGenderless, int y, int xOrAlign, u32 color);
void Gts_PrintGenderNoTransfer(Window *window, MsgData *msgData, int gender, BOOL showGenderless, int x, int y, u32 color);
void Gts_PrintLevelRange(Window *window, MsgData *msgData, int levelRange, int align, int y, u32 color, int kind);
void Gts_PrintLevelRangeAt(Window *window, MsgData *msgData, int levelRange, int align, int x, int y, u32 color, int kind);
void Gts_DrawWantedMonLevelLeft(MsgData *msgData, MsgData *speciesNames, MessageFormat *msgFormat, Window *windows, int species, int gender, int levelRange);
u16 *Gts_LoadSpeciesByLetter(enum HeapID heapID, int letter, u32 *count);
u8 *Gts_CreateJohtoDexFlags(enum HeapID heapID);
void Gts_InitRecord(GtsRecord *record, GtsApp *app);
int Gts_CreateLevelRangeListItems(ListMenuItem **items, MsgData *msgData, int kind);
void Gts_SetLevelRange(GtsWantedMon *wanted, int levelRange, int kind);
int Gts_FindLevelRange(int minLevel, int maxLevel, int kind);
void Gts_SetSearchCountry(GtsApp *app, int index);
int Gts_GetCountryGroupRange(int group, int *start);
int Gts_CreateCountryListItems(ListMenuItem **items, MsgData *msgData, int group);
void GtsCursorValueTable_Clear(GtsCursorValueTable *arg);
void GtsCursorValueTable_SetEntry(GtsCursorValueTable *arg, int index, u16 value1, u16 value2);

// The selector (GtsSelector_Start mode).
typedef enum GtsSelectorMode {
    GTS_SELECTOR_MODE_SPECIES,
    GTS_SELECTOR_MODE_GENDER,
    GTS_SELECTOR_MODE_LEVEL,
    GTS_SELECTOR_MODE_COUNTRY,
} GtsSelectorMode;

// GtsSelector_Main results besides the choice (also what its pages and buttons return).
#define GTS_SELECTOR_NONE (-1) // still choosing / nothing touched
#define GTS_SELECTOR_BACK (-2)

GtsSelector *GtsSelector_New(GtsSelectorArgs *args, int bgId, int levelRangeKind);
void GtsSelector_Start(GtsSelector *selector, int mode);
void GtsSelector_Delete(GtsSelector *selector);
int GtsSelector_Main(GtsSelector *selector);
// The count of the countries starting with the letter (index A = 0); unused is not read.
int GtsSelector_CountCountriesOfLetter(GtsSelector *selector, u32 letter);

// Connecting to and disconnecting from Nintendo Wi-Fi Connection (screen 0), and the text helpers
// every screen uses. Gts_PrintMessage prints in waitingIconWindow (the message window); align: 0 left at x,
// 1 centered, 2 right in the window.
BOOL GtsSelector_HasCountriesInGroup(GtsSelector *selector, int group);
void Gts_PrintMessage(GtsApp *app, MsgData *msgData, int msgId, int textSpeed, u32 unused4);
void Gts_PrintAlignedFont0(Window *window, String *string, int x, int y, int align, u32 color);
void Gts_PrintAlignedFont4(Window *window, String *string, int x, int y, int align, u32 color);
void Gts_PrintPanelCaption(Window *window, MsgData *msgData, int caption);
void Gts_CreateWirelessStatusIcon(GtsApp *app);

#endif // POKEHEARTGOLD_GTS_INTERNAL_H
