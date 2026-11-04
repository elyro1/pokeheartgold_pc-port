#include "application/main_menu/ranger_link.h"
#include "application/pokewalker/pokewalker_ir.h"

/* Main-menu entries already report these physical links as unavailable. Keep
 * their application-facing APIs safe if another caller reaches them: no WM
 * work buffers, card SPI, IR polling thread, successful transfers or peers. */
static BOOL rangerFinished = TRUE;

void RangerLink_Init(const RangerLinkConfig *config, void *buffer) {
    (void)config; (void)buffer;
    rangerFinished = FALSE;
}
void RangerLink_Update(void) {}
void RangerLink_StartChild(void) {}
BOOL RangerLink_CanSelectParent(int index) { (void)index; return FALSE; }
void RangerLink_SelectParent(int index) { (void)index; }
int RangerLink_GetLinkLevel(void) { return 0; }
int RangerLink_CountPeers(void) { return 0; }
void RangerLink_StartSendList(void) {}
void RangerLink_StartSendNames(void) {}
void RangerLink_StartCommand(const void *data, int size, int command) {
    (void)data; (void)size; (void)command;
}
BOOL RangerLink_IsSendDone(void) { return FALSE; }
BOOL RangerLink_IsCommandDone(void) { return FALSE; }
BOOL RangerLink_IsCommandAcknowledged(void) { return FALSE; }
BOOL RangerLink_IsConnected(void) { return FALSE; }
void RangerLink_RequestEnd(void) { rangerFinished = TRUE; }
BOOL RangerLink_IsFinished(void) { return rangerFinished; }
void RangerLink_SetChannel(u16 channel) { (void)channel; }

/* These two public helpers are independent of the physical link. */
u32 RangerLink_ReadMacAddressTail(u8 *macAddress) {
    return (u32)macAddress[2] | ((u32)macAddress[3] << 8)
        | ((u32)macAddress[4] << 16) | ((u32)macAddress[5] << 24);
}
void RangerLink_SplitDecimal(u32 value, u32 *low, u32 *high) {
    *low = value % 10000;
    *high = (value / 10000) % 10000;
}

/* Pokewalker error 1 is the original application's disconnected result.
 * Keep it available on every poll, including before start and after stop. */
void PokewalkerIr_Start(void *stack, u32 stackSize, u32 priority) {
    (void)stack; (void)stackSize; (void)priority;
}
void PokewalkerIr_Stop(void) {}
void PokewalkerIr_SetIdentities(PokewalkerIrIdentity *identity, PokewalkerIrIdentity *peerIdentity) {
    (void)identity; (void)peerIdentity;
}
void PokewalkerIr_SetCompressBuffer(void *buffer) { (void)buffer; }
void PokewalkerIr_RequestEraseRecords(int active) { (void)active; }
void PokewalkerIr_RequestUpload(u8 *a, u8 *b, u8 *c) { (void)a; (void)b; (void)c; }
void PokewalkerIr_RequestReceive(u8 *a, u8 *b, u8 *c, u8 *d) { (void)a; (void)b; (void)c; (void)d; }
void PokewalkerIr_RequestSendStroll(u8 *a, u8 *b, u8 *c, u8 *d) { (void)a; (void)b; (void)c; (void)d; }
void PokewalkerIr_RequestReceiveGifts(PokewalkerIrStatus *status, void *records) { (void)status; (void)records; }
BOOL PokewalkerIrStatus_IsConnected(void) { return FALSE; }
u8 PokewalkerIr_GetChunkProgress(void) { return 0; }
u8 PokewalkerIr_GetTotalProgress(void) { return 0; }
u32 PokewalkerIr_GetAndClearError(void) { return 1; }
