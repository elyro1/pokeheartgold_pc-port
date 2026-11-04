#ifndef POKEHEARTGOLD_WIFI_SETUP_LOCAL_WIRELESS_H
#define POKEHEARTGOLD_WIFI_SETUP_LOCAL_WIRELESS_H

#if defined(SDK_PORT)
#include <nitro/wm.h>
#else
#include <nitro/wm/wm_internal.h>
#endif

#include "global.h"

#include "wifi_setup/wifi_setup_ui.h"

// Local-wireless part of the access-point setup: the title banner, the dialog window, the scan for a Wi-Fi USB
// connector (access points whose SSID starts with "NWCUSBAP") and the DS Download Play parent.

#define WIFI_SETUP_BANNER_NUM_LETTERS   5
#define WIFI_SETUP_USB_SCAN_NUM_ENTRIES 16

// result passed to the USB connector scan callback
#define WIFI_SETUP_USB_SCAN_NOT_FOUND 0
#define WIFI_SETUP_USB_SCAN_FOUND     1 // a connector with the flag set
#define WIFI_SETUP_USB_SCAN_MIXED     2 // connectors with and without the flag

// WifiSetupUsbScan.endState
#define WIFI_SETUP_USB_SCAN_RUNNING 0
#define WIFI_SETUP_USB_SCAN_ENDING  1
#define WIFI_SETUP_USB_SCAN_ENDED   2

// Title banner, allocated by WifiSetupBanner_Open.
typedef struct WifiSetupBanner {
    WifiSetupOamBlock *letters[WIFI_SETUP_BANNER_NUM_LETTERS];
    WifiSetupTask *task; // slide task, NULL when the banner is still
    u8 closing;          // set by WifiSetupBanner_Close
} WifiSetupBanner;

typedef struct WifiSetupBannerState {
    u16 *screen; // BG screen data being loaded
    WifiSetupBanner *banner;
} WifiSetupBannerState;

// One USB connector seen by the first scan.
typedef struct WifiSetupUsbScanEntry {
    u8 bssid[6];
    u8 flag; // bit 0 of the byte after the SSID tag
} WifiSetupUsbScanEntry;

// Scan work (32-byte aligned; the WM work buffer comes first).
typedef struct WifiSetupUsbScan {
    u8 wmBuf[0xF00];
    u8 scanBuf[0x400];
    WifiSetupUsbScanEntry entries[WIFI_SETUP_USB_SCAN_NUM_ENTRIES];
    void (*callback)(int result);
    WmScanExParam param;
    WmStatus status;
    OSTick startTick;
    WifiSetupTask *task;
    vu8 endState; // WIFI_SETUP_USB_SCAN_RUNNING / ENDING / ENDED
    u8 found;     // WIFI_SETUP_USB_SCAN_FOUND, seen while no callback was set
    u8 rescanning; // set once the first scan period is over
    u8 unk_1B97[9];
} WifiSetupUsbScan;

// Dialog window: a frame, a text line and up to two buttons sliding up from the bottom of the screen.
typedef struct WifiSetupDialog {
    WifiSetupOamBlock *frame;
    WifiSetupOamBlock *textBlock;
    WifiSetupOamBlock *buttons[2];
    WifiSetupCanvas *canvas;
    WifiSetupTask *task;
    u16 timer;     // kind 5: frames shown
    s8 brightness; // fade
    s8 selected;   // button index or WIFI_SETUP_SELECTED_*
    u8 kind;
    u8 closeDelay;
    u8 fade; // fade the screen in and out
} WifiSetupDialog;

extern WifiSetupBannerState gWifiSetupBanner;
extern WifiSetupUsbScan *gWifiSetupUsbScan;
extern WifiSetupDialog *gWifiSetupDialog;

extern const u8 gWifiSetupBannerLetterTemplates[][WIFI_SETUP_BANNER_NUM_LETTERS]; // per kind
extern const u8 gWifiSetupBannerLetterPalettes[][WIFI_SETUP_BANNER_NUM_LETTERS];  // per kind
extern WifiSetupPoint gWifiSetupBannerLetterPositions[WIFI_SETUP_BANNER_NUM_LETTERS];
extern u8 gWifiSetupUsbScanEmptyBssid[6];
extern u8 gWifiSetupUsbApSsid[]; // "NWCUSBAP"
extern WmScanExParam gWifiSetupUsbScanParam;
extern u8 gWifiSetupDialogWindowPlanes[];
extern WifiSetupRect gWifiSetupDialogWindowRect;
extern u8 gWifiSetupDialogButtonPositions[][2];
extern s8 gWifiSetupDialogAButtonChoice[][2];
extern s8 gWifiSetupDialogBButtonChoice[][2];
extern u8 gWifiSetupDialogButtonTemplates[][2];
extern WifiSetupPoint gWifiSetupDialogButtonSizes[];
extern WifiSetupPoint gWifiSetupDialogTextSizes[];
extern WifiSetupPoint gWifiSetupDialogFramePositions[];
extern WifiSetupPoint gWifiSetupDialogFrameSizes[];
extern WifiSetupPoint gWifiSetupDialogButtonPos[][2];
extern u8 gWifiSetupDialogFrameTemplates[];
extern u8 gWifiSetupDialogButtonCounts[];
extern char *gWifiSetupBannerScreenPaths[];

void WifiSetupSound_Play(int seq);
void WifiSetupSound_SetVolume(int volume);
void WifiSetupSound_SetTrackPitch(int trackMask, int pitch);
void WifiSetupSound_Stop(void);
void WifiSetupSound_UpdateTask(WifiSetupTask *task, void *arg);

void WifiSetupBanner_Open(int kind);
void WifiSetupBanner_Close(void);
void WifiSetupBanner_SetKind(int kind);
BOOL WifiSetupBanner_IsMoving(void);
BOOL WifiSetupBanner_IsNotClosing(void);
void WifiSetupBanner_LoadScreen(int kind);

BOOL WifiSetupUsbScan_Start(void (*callback)(int));
BOOL WifiSetupUsbScan_End(void);
void WifiSetupUsbScan_SetCallback(void (*callback)(int));

void WifiSetupDialog_Open(int message, int kind, u8 fade, int digitPos, int digit);
void WifiSetupDialog_Close(void);
int WifiSetupDialog_GetSelected(void);
BOOL WifiSetupDialog_IsOpen(void);

#endif // POKEHEARTGOLD_WIFI_SETUP_LOCAL_WIRELESS_H
