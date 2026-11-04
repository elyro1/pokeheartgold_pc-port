#ifndef POKEHEARTGOLD_OVERLAY_00_THUMB_H
#define POKEHEARTGOLD_OVERLAY_00_THUMB_H

#include "wifi_club_app.h"

int WifiConnection_ClassifyErrorCode(int errorCode, u32 context);
int WifiConnection_GetTargetFriendIndex(void);
void WifiConnection_SetDisconnectWhenAlone(s32 value);
BOOL WifiConnection_StopServerMatching(void);
void WifiConnection_ResetStopServerMatching(void);
void WifiConnection_SetDisconnectCallback(void *callback, void *arg);
void WifiConnection_StopVoiceChat(void);
void WifiConnection_StartVoiceChat(int heapId);
void WifiConnection_SetOwnStatusData(void *data, int size);
void WifiConnection_SetVoiceChatEnabled(u8 value);
#if defined(SDK_PORT)
#else
void DWC_SetAuthServer(int server);
#endif
void WifiConnection_SetExtraHeapEnabled(BOOL enable, int heapId);
s32 WifiConnection_GetFriendStatus(s32 index);
s32 WifiConnection_GetNewClientIndex(void);
BOOL WifiConnection_GetVoiceActivity(void);
BOOL WifiConnection_IsVoiceChatEnabled(void);
void WifiConnection_SetConnectFilterCallback(BOOL (*callback)(s32));
void WifiConnection_ClearFriendDataChanged(void);
BOOL WifiConnection_IsFriendDataChanged(void);
void WifiConnection_SetFriendStatusBuffer(WifiClubPlayerStatus *data, s32 stride);

#endif // POKEHEARTGOLD_OVERLAY_00_THUMB_H
