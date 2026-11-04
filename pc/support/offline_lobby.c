#include <hg/offline_lobby.h>

/* PPW is a private DS online service. Keep the game's public result/state
 * protocol, without constructing its ARM/C++ containers or starting a network.
 * Result 7 is a normal request failure accepted by the game's CheckResult;
 * process state 10 is its terminal lobby error, never a connected state. */
enum { LOBBY_NOT_INITIALIZED = 4, LOBBY_UNAVAILABLE = 7 };
static int attempted;

static int RequestFailure(void) {
    return attempted ? LOBBY_UNAVAILABLE : LOBBY_NOT_INITIALIZED;
}

int PPW_LobbyInitializeAsync(const void *game, const void *secret, int mode,
    void (**callbacks)(void), void *userData, void *value, u32 playerDataSize) {
    (void)game; (void)secret; (void)mode; (void)callbacks;
    (void)userData; (void)value; (void)playerDataSize;
    attempted = 1;
    return LOBBY_UNAVAILABLE;
}

int PPW_LobbyShutdownAsync(void) { attempted = 0; return 0; }
int PPW_LobbyProcess(void) { return attempted ? 10 : 0; }
int PPW_LobbyGetSubChannelState(void) { return attempted ? 3 : 0; }
int PPW_LobbyGetMyUserId(void) { return -1; }
int PPW_LobbyGetLastError(void) { return RequestFailure(); }
int PPW_LobbyToErrorCode(int error) { return error >= 4 ? error + 0x297c : error + 0xa410; }
int PPW_LobbyInitResultToLastError(int result) {
    switch (result) {
    case 0: return 0;
    case 1: return 2;
    case 2: return 3;
    default: return 1;
    }
}
int PPW_LobbyGetSchedule(struct WiFiPlazaLobbySchedule *data, u32 *size) {
    (void)data; (void)size;
    return RequestFailure();
}
int PPW_LobbySetChannelData(int channel, int id, const char *key, const void *data, u32 size) {
    (void)channel; (void)id; (void)key; (void)data; (void)size;
    return RequestFailure();
}
void PPW_LobbyGetChannelDataAsync(int channel, int id, const char *key, int value) {
    (void)channel; (void)id; (void)key; (void)value;
}
int PPW_LobbyJoinSubChannelAsync(int channel) { (void)channel; return RequestFailure(); }
int PPW_LobbyLeaveSubChannel(void) { return RequestFailure(); }
int PPW_LobbySendChannelBinaryMessage(int channel, int type, const void *data, int size) {
    (void)channel; (void)type; (void)data; (void)size;
    return RequestFailure();
}
int PPW_LobbySendPlayerBinaryMessage(int id, int type, const void *data, int size) {
    (void)id; (void)type; (void)data; (void)size;
    return RequestFailure();
}
void PPW_LobbyUpdateMyProfile(struct WiFiPlazaPlayer *player, u32 size, int value) {
    (void)player; (void)size; (void)value;
}
int PPW_LobbyStartRecruit(struct WiFiPlazaLobbyMatch *match) { (void)match; return RequestFailure(); }
int PPW_LobbyUpdateRecruitInfo(struct WiFiPlazaLobbyMatch *match) { (void)match; return RequestFailure(); }
int PPW_LobbyStopRecruit(void) { return RequestFailure(); }
int PPW_LobbySubmitQuestionnaire(int index) { (void)index; return RequestFailure(); }
int PPW_LobbyGetTimeInfo(void *time) { (void)time; return RequestFailure(); }
