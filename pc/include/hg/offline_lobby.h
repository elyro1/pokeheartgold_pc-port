#ifndef HG_PC_OFFLINE_LOBBY_H
#define HG_PC_OFFLINE_LOBBY_H

#include <nitro/types.h>

struct WiFiPlazaLobbySchedule;
struct WiFiPlazaPlayer;
struct WiFiPlazaLobbyMatch;

int PPW_LobbyGetSchedule(struct WiFiPlazaLobbySchedule *data, u32 *size);
int PPW_LobbySetChannelData(int channel, int id, const char *key, const void *data, u32 size);
void PPW_LobbyGetChannelDataAsync(int channel, int id, const char *key, int value);
int PPW_LobbyInitializeAsync(const void *game, const void *secret, int mode,
    void (**callbacks)(void), void *userData, void *value, u32 playerDataSize);
int PPW_LobbyShutdownAsync(void);
int PPW_LobbyProcess(void);
int PPW_LobbyJoinSubChannelAsync(int channel);
int PPW_LobbyLeaveSubChannel(void);
int PPW_LobbyGetSubChannelState(void);
int PPW_LobbySendChannelBinaryMessage(int channel, int type, const void *data, int size);
int PPW_LobbySendPlayerBinaryMessage(int id, int type, const void *data, int size);
int PPW_LobbyGetMyUserId(void);
void PPW_LobbyUpdateMyProfile(struct WiFiPlazaPlayer *player, u32 size, int value);
int PPW_LobbyStartRecruit(struct WiFiPlazaLobbyMatch *match);
int PPW_LobbyUpdateRecruitInfo(struct WiFiPlazaLobbyMatch *match);
int PPW_LobbyStopRecruit(void);
int PPW_LobbySubmitQuestionnaire(int index);
int PPW_LobbyGetLastError(void);
int PPW_LobbyToErrorCode(int error);
int PPW_LobbyInitResultToLastError(int result);
int PPW_LobbyGetTimeInfo(void *time);

#endif
