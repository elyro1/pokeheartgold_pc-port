#ifndef POKEHEARTGOLD_WIFI_SETUP_ACCESS_POINTS_H
#define POKEHEARTGOLD_WIFI_SETUP_ACCESS_POINTS_H

#if defined(SDK_PORT)
#include <nitro/wm.h>
#else
#include <nitro/wm/wm_internal.h>
#endif

#include "global.h"

#include "wifi_setup/wifi_setup_connection.h"

struct WifiSetupAossResult;
struct WifiSetupResult;

// Access-point data of the access-point setup: the scan for nearby access points, the saved connection settings
// (four backup pages, the first three editable) with an edit copy of one of them, and the sound archive.

#define WIFI_SETUP_NUM_SCAN_ENTRIES 20
#define WIFI_SETUP_NUM_CONNECTIONS  4

// WifiSetupScanEntry.security
#define WIFI_SETUP_SECURITY_OPEN 0
#define WIFI_SETUP_SECURITY_WEP  1
#define WIFI_SETUP_SECURITY_WPA  2

// One access point found by the scan.
typedef struct WifiSetupScanEntry {
    u8 ssid[0x20];
    u8 bssid[6];
    u16 linkLevel;
    u8 security; // WIFI_SETUP_SECURITY_*
    u8 pad29;
} WifiSetupScanEntry;

// WM work of the scan, followed by the scan state.
typedef struct WifiSetupApScan {
    u8 wmWork[0xF00];
    u8 scanBuf[0x400];
    WifiSetupScanEntry entries[WIFI_SETUP_NUM_SCAN_ENTRIES];
    WmScanExParam param;
    WmStatus status;
    u8 ending;
} WifiSetupApScan;

typedef struct WifiSetupConnections {
    WifiSetupConnectionPage pages[WIFI_SETUP_NUM_CONNECTIONS];
    WifiSetupConnection edit;
    MATHCRC16Table crcTable;
} WifiSetupConnections;

typedef struct WifiSetupSound {
    NNSSndArc arc;
    NNSSndHandle handle;
    void *data; // sound archive file
    struct WifiSetupTask *task;
} WifiSetupSound;

extern WifiSetupApScan *gWifiSetupApScan;
extern WifiSetupConnections *gWifiSetupConnections;
extern WifiSetupSound *gWifiSetupSound;

extern u8 gWifiSetupWpaVendorPrefix[]; // vendor element prefix that marks WPA
extern u8 gWifiSetupApScanEmptyBssid[];
extern WmScanExParam gWifiSetupApScanParam;
extern u8 gWifiSetupZeroAddress[]; // address 0.0.0.0
extern char gWifiSetupConnectionsAddressFormat[];
extern char gWifiSetupSoundArchivePath[];

BOOL WifiSetupApScan_Start(void);
BOOL WifiSetupApScan_End(void);
int WifiSetupApScan_GetEntries(WifiSetupScanEntry **out);
void WifiSetupConnections_Load(void);
void WifiSetupConnections_Free(void);
WifiSetupConnection *WifiSetupConnections_GetEdit(void);
void WifiSetupConnections_SetAutoAddress(BOOL on);
void WifiSetupConnections_SetAutoDns(BOOL on);
void WifiSetupConnections_SetSsid(const u8 *ssid);
void WifiSetupConnections_SetWepKey(const char *key);
void WifiSetupConnections_SetIpAddress(const char *text);
void WifiSetupConnections_SetSubnetMask(const char *text);
void WifiSetupConnections_SetGateway(const char *text);
void WifiSetupConnections_SetPrimaryDns(const char *text);
void WifiSetupConnections_SetSecondaryDns(const char *text);
void WifiSetupConnections_GetSsid(u8 *dst);
void WifiSetupConnections_FormatIpAddress(char *buf);
void WifiSetupConnections_FormatSubnetMask(char *buf);
void WifiSetupConnections_FormatGateway(char *buf);
void WifiSetupConnections_FormatPrimaryDns(char *buf);
void WifiSetupConnections_FormatSecondaryDns(char *buf);
u8 WifiSetupConnections_GetSetupMethod(int index);
void WifiSetupConnections_Edit(int index);
void WifiSetupConnections_SaveEdit(void);
void WifiSetupConnections_SetFromClientResult(struct WifiSetupResult *result);
void WifiSetupConnections_SetFromInfo(const struct WifiSetupAossResult *result);
WifiSetupConnections *WifiSetupConnections_Get(void);
void WifiSetupConnections_Erase(int index);
void WifiSetupConnections_Reset(void);
void WifiSetup_ParseAddress(const char *text, u8 *address);
void WifiSetupSound_Init(void);
void WifiSetupSound_Free(void);

#endif // POKEHEARTGOLD_WIFI_SETUP_ACCESS_POINTS_H
