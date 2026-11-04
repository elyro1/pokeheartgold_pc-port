#ifndef POKEHEARTGOLD_WIFI_SETUP_SHARE_H
#define POKEHEARTGOLD_WIFI_SETUP_SHARE_H

#if defined(SDK_PORT)
#include <nitro/wm.h>
#else
#include <nitro/wm/wm_internal.h>
#endif

#include "global.h"

// Local wireless session of the access-point setup: DS Download Play of a program to one child, then data sharing
// with that child, sending the connection settings in 64-byte blocks.

// gWifiSetupWh.work->state (names from the log table)
#define WIFI_SETUP_WH_STATE_STOP           0
#define WIFI_SETUP_WH_STATE_IDLE           1
#define WIFI_SETUP_WH_STATE_SCANNING       2
#define WIFI_SETUP_WH_STATE_BUSY           3
#define WIFI_SETUP_WH_STATE_CONNECTED      4
#define WIFI_SETUP_WH_STATE_DATASHARING    5
#define WIFI_SETUP_WH_STATE_KEYSHARING     6
#define WIFI_SETUP_WH_STATE_MEASURECHANNEL 7
#define WIFI_SETUP_WH_STATE_CONNECT_FAIL   8
#define WIFI_SETUP_WH_STATE_ERROR          9
#define WIFI_SETUP_WH_STATE_FATAL          10 // WM could not be reset or ended

// errors of the wireless helper after the WM_ERRCODE_* (WifiSetupWh_SetError)
#define WIFI_SETUP_WH_ERROR_NO_CHANNEL    22 // WM allows no channel
#define WIFI_SETUP_WH_ERROR_CHANNELS_DONE 24 // every allowed channel was measured

// connect modes (WifiSetupWh_StartConnection, gWifiSetupWh.work->connectMode)
#define WIFI_SETUP_WH_MODE_MP_PARENT 0
#define WIFI_SETUP_WH_MODE_MP_CHILD  1
#define WIFI_SETUP_WH_MODE_KS_PARENT 2 // key sharing
#define WIFI_SETUP_WH_MODE_KS_CHILD  3
#define WIFI_SETUP_WH_MODE_DS_PARENT 4 // data sharing
#define WIFI_SETUP_WH_MODE_DS_CHILD  5

// packet kinds of the data sharing (WifiSetupSharePacket.kind)
#define WIFI_SETUP_SHARE_PACKET_HELLO 188 // parent: language code, waiting for the child
#define WIFI_SETUP_SHARE_PACKET_READY 189 // child: ready for the settings blocks

// download state of one child (WifiSetupMbParent_GetChildState)
#define WIFI_SETUP_MB_CHILD_NONE      0
#define WIFI_SETUP_MB_CHILD_CONNECTED 1
#define WIFI_SETUP_MB_CHILD_REQUESTED 2 // asked for the download
#define WIFI_SETUP_MB_CHILD_WAITING   3
#define WIFI_SETUP_MB_CHILD_SENDING   4
#define WIFI_SETUP_MB_CHILD_SENT      5
#define WIFI_SETUP_MB_CHILD_BOOTABLE  6

// requests handled in WIFI_SETUP_SHARE_STATE_IDLE (gWifiSetupShare->request)
#define WIFI_SETUP_SHARE_REQUEST_NONE  0
#define WIFI_SETUP_SHARE_REQUEST_START 1
#define WIFI_SETUP_SHARE_REQUEST_END   2

// session states (gWifiSetupShare->state, WifiSetupShare_Update)
#define WIFI_SETUP_SHARE_STATE_NONE                0
#define WIFI_SETUP_SHARE_STATE_IDLE                1
#define WIFI_SETUP_SHARE_STATE_MEASURE_CHANNEL     2
#define WIFI_SETUP_SHARE_STATE_SETUP_DOWNLOAD      3
#define WIFI_SETUP_SHARE_STATE_WAIT_ENTRY          4
#define WIFI_SETUP_SHARE_STATE_ENTRY_REQUESTED     5 // a child asked for the download
#define WIFI_SETUP_SHARE_STATE_DOWNLOADING         6
#define WIFI_SETUP_SHARE_STATE_START_SHARING       7 // the child booted the program
#define WIFI_SETUP_SHARE_STATE_SHARING             8
#define WIFI_SETUP_SHARE_STATE_CHILD_READY         9
#define WIFI_SETUP_SHARE_STATE_SENDING_SETTINGS    10
#define WIFI_SETUP_SHARE_STATE_SETTINGS_RECEIVED   11
#define WIFI_SETUP_SHARE_STATE_DONE                12
#define WIFI_SETUP_SHARE_STATE_ENTRY_CANCELLED     13 // the child left before the download
#define WIFI_SETUP_SHARE_STATE_CANCEL_DOWNLOAD      16
#define WIFI_SETUP_SHARE_STATE_CANCEL_DOWNLOAD_WAIT 17
#define WIFI_SETUP_SHARE_STATE_DOWNLOAD_ABORTED    18
#define WIFI_SETUP_SHARE_STATE_STOP_WAIT           19
#define WIFI_SETUP_SHARE_STATE_STOPPED             20
#define WIFI_SETUP_SHARE_STATE_CHILD_ABORT         21
#define WIFI_SETUP_SHARE_STATE_ABORT_WAIT          22
#define WIFI_SETUP_SHARE_STATE_ABORTED             23
#define WIFI_SETUP_SHARE_STATE_CHILD_FAILED        24
#define WIFI_SETUP_SHARE_STATE_FAIL_WAIT           25
#define WIFI_SETUP_SHARE_STATE_FAILED              26
#define WIFI_SETUP_SHARE_STATE_SHARING_ERROR       27
#define WIFI_SETUP_SHARE_STATE_ERROR_WAIT          28
#define WIFI_SETUP_SHARE_STATE_ERROR               29
#define WIFI_SETUP_SHARE_STATE_FINALIZE            30
#define WIFI_SETUP_SHARE_STATE_FATAL               31
#define WIFI_SETUP_SHARE_STATE_CANCEL_SHARING      32
#define WIFI_SETUP_SHARE_STATE_CANCEL_SHARING_WAIT 33
#define WIFI_SETUP_SHARE_STATE_END                 34 // the session can be freed

typedef void (*WifiSetupWhLogFunc)(int level, char *format, ...);

typedef BOOL (*WifiSetupWhJudgeFunc)(WMStartParentCallback *arg);

typedef struct WifiSetupWhWork {
    WmParentParam parentParam;
    u32 state;       // WIFI_SETUP_WH_STATE_*
    u32 connectMode; // WIFI_SETUP_WH_MODE_*
    u32 unk_48;
    WifiSetupWhJudgeFunc judgeChild; // may refuse a new child
    u16 unk_50;
    u16 connectedMask; // bit per AID, bit 0 the parent
    u32 errorCode;
    u32 randomSeed;
    u16 channel; // decided by the measurement
    u16 minBusyRatio;
    u16 quietChannels; // channels with the lowest busy ratio
    u8 unk_62[0x1E];
    u8 wmWork[0xF00];
    u8 sendBuffer[0xE0];
    u8 recvBuffer[0x240];
    u32 sendBufferSize;
    u32 recvBufferSize;
    u8 unk_12A8[0x100];
    u32 unk_13A8;
    int (*getWepKey)(u8 *key, struct WifiSetupWhWork *work); // returns the WEP mode; NULL: no WEP
    u32 unk_13B0;
    u8 unk_13B4[0xC];
    u8 wepKey[0x20];
    WMDataSharingInfo dataSharingInfo;
    WMDataSet dataSet;
    WMKeySetBuf keySharing;
} WifiSetupWhWork;

typedef struct WifiSetupWh {
    WifiSetupWhLogFunc log; // NULL: no logging
    WifiSetupWhWork *work;
} WifiSetupWh;

typedef struct WifiSetupSharePacket {
    u16 kind; // WIFI_SETUP_SHARE_PACKET_* or a settings block
    u16 index;
    u8 data[0x40];
} WifiSetupSharePacket;

// Parameters of the session (WifiSetupShare_Init), also the Download Play parent's file description
// (WifiSetupMbParent_Start).
typedef struct WifiSetupSessionParams {
    const char *filePath;
    const u16 *title;
    const u16 *description;
    int iconCharPath;
    int iconPalettePath;
    u32 ggid;
    u8 languageCode; // the session's copy holds 2 here
} WifiSetupSessionParams;

// DS Download Play parent states (WifiSetupMbParent_GetState)
#define WIFI_SETUP_MB_PARENT_STATE_NONE        0 // not set up, or ended without booting the children
#define WIFI_SETUP_MB_PARENT_STATE_READY       1 // MB_Init done
#define WIFI_SETUP_MB_PARENT_STATE_ACCEPTING   2 // children connect and ask for the download
#define WIFI_SETUP_MB_PARENT_STATE_DOWNLOADING 3
#define WIFI_SETUP_MB_PARENT_STATE_BOOTING     4
#define WIFI_SETUP_MB_PARENT_STATE_BOOTED      5 // ended after booting the children
#define WIFI_SETUP_MB_PARENT_STATE_ENDING      6
#define WIFI_SETUP_MB_PARENT_STATE_FAILED      7

// State of the DS Download Play parent: bit masks of child AIDs per stage.
typedef struct WifiSetupMbParentState {
    u16 state; // WIFI_SETUP_MB_PARENT_STATE_*
    u16 connectedMask;
    u16 requestedMask;
    u16 waitingMask;
    u16 sendingMask;
    u16 sentMask;
    u16 bootableMask;
} WifiSetupMbParentState;

typedef struct WifiSetupMbParentChild {
    u8 user[0x16];
    u8 macAddr[6];
    u16 aid;
} WifiSetupMbParentChild;

typedef struct WifiSetupMbParentUser {
    u8 favoriteColor : 4;
    u8 playerNo : 4;
    u8 nameLength;
    u16 name[10];
} WifiSetupMbParentUser;

typedef struct WifiSetupMbParentConnectArg {
    u8 unk_00[0xA];
    u8 macAddr[6];
} WifiSetupMbParentConnectArg;

// Download Play parent work (WifiSetupShare.mbParent).
typedef struct WifiSetupMbParent {
    WifiSetupMbParentState head;
    WifiSetupMbParentChild children[1]; // indexed by aid - 1
    u8 segment[0x10040 - 0x2C];
    u8 mbWork[0x1B140 - 0x10040];
    void *mbWorkPtr;
    void *segmentPtr;
    u8 padding_1B148[0x18];
    WifiSetupWhWork whWork;
} WifiSetupMbParent;

// Session work: the start of WifiSetupSession (wifi_setup_app.h).
typedef struct WifiSetupShare {
    union {
        WifiSetupSharePacket packet;
        u8 raw[0x100];
    } send;

    union {
        WifiSetupSharePacket packets[2]; // by AID
        u8 raw[0x100];
    } received;

    u16 blockIndex; // settings block sent next (0-35, mod 16)
    u16 unk_202;
    u32 errorCount;        // step failures without the child's answer
    u32 receivedFlags[16]; // by AID
    u8 unk_248[0x400];
    u16 tgid;
    u16 channel;
    u8 backup[0x404];
    u8 firstBlock[0x40];
    u8 state;
    u8 prevState;
    u8 languageCode; // sent to the child
    u8 sendingData;  // the child answered, settings blocks are sent
    u32 frame;
    u32 counter;
    u32 timer;
    WifiSetupMbParentChild *childUsers[1]; // by AID - 1
    u8 *data;                              // the connection settings (WifiSetupConnections_Get)
    u32 unk_AA8;
    u8 request;
    WifiSetupSharePacket *sendPacket; // = &send.packet
    WifiSetupSessionParams params;
    u8 unk_AD0[0x30];
    WifiSetupMbParent mbParent;
} WifiSetupShare;

extern WifiSetupWh gWifiSetupWh;
extern WifiSetupShare *gWifiSetupShare;
extern WifiSetupMbParent *gWifiSetupMbParent;

extern char *gWifiSetupWhStateNames[];                   // state names, see WIFI_SETUP_WH_STATE_*
extern char gWifiSetupWhFmt_StateFrom[];                 // "%s -> "
extern char gWifiSetupWhFmt_StateTo[];                   // "%s\n"
extern char gWifiSetupWhStr_ChildConnected[];            // "StartParent - new child (aid %x) connected\n"
extern char gWifiSetupWhStr_ChildDisconnected[];         // "StartParent - child (aid %x) disconnected\n"
extern char gWifiSetupWhStr_UnknownIndicate[];           // "unknown indicate, state = %d\n"
extern char gWifiSetupWhStr_StartParentKeyShareFailed[]; // "DWCi_MOV_WH_StateInStartParentKeyShare failed\n"
extern char gWifiSetupWhStr_EndParentFailed[];           // "DWCi_MOV_WH_StateInEndParent failed\n"
extern char gWifiSetupWhStr_RecvBufferSize[];            // "recv buffer size = %d\n"
extern char gWifiSetupWhStr_SendBufferSize[];            // "send buffer size = %d\n"
extern char gWifiSetupWhStr_ChannelBratio[];             // "channel %d bratio = %x\n"
extern char gWifiSetupWhStr_DecidedChannel[];            // "decided channel = %d\n"
extern char gWifiSetupWhStr_UnknownConnectMode[];        // "unknown connect mode %d\n"
extern char gWifiSetupWhStr_StepNoChild[];               // "DWCi_MOV_WH_StepDataSharing - Warning No Child\n"
extern char gWifiSetupWhStr_StepNoDataSet[];             // "DWCi_MOV_WH_StepDataSharing - Warning No DataSet\n"
extern char gWifiSetupWhStr_AlreadyIdle[];               // "already DWCi_MOV_WH_SYSSTATE_IDLE\n"
extern char gWifiSetupWhStr_Finalize[];                  // "DWCi_MOV_WH_Finalize, state = %d\n"
extern char gWifiSetupArchivePath[];                     // "rom:/dwc/utility.bin"
extern char gWifiSetupArchivePrefixFormat[];             // "%s:/"
extern char gWifiSetupArchiveName[];

void WifiSetupMbParent_Init(WifiSetupMbParent *parent);
void WifiSetupMbParent_Setup(u32 ggid, u32 tgid);
void WifiSetupMbParent_Start(const WifiSetupSessionParams *params, u32 channel);
void WifiSetupMbParent_StartDownload(void);
BOOL WifiSetupMbParent_AreAllBootable(void);
void WifiSetupMbParent_BootChildren(void);
void WifiSetupMbParent_End(void);
u16 WifiSetupMbParent_GetState(void);
u16 WifiSetupMbParent_GetMask(int index);
int WifiSetupMbParent_GetChildState(int aid);

void WifiSetupWh_Init(WifiSetupWhWork *work);
int WifiSetupWh_StartConnection(u32 mode, u16 tgid, u16 channel);

void WifiSetupShare_Init(WifiSetupShare *share, const WifiSetupSessionParams *params);
BOOL WifiSetupShare_Cancel(void);
BOOL WifiSetupShare_Start(void);
BOOL WifiSetupShare_StartDownload(void);
void WifiSetupShare_Update(void);
void WifiSetupShare_GetState(u8 *state, u8 *changed);
WifiSetupMbParentUser *WifiSetupShare_GetChildUser(void);

#endif // POKEHEARTGOLD_WIFI_SETUP_SHARE_H
