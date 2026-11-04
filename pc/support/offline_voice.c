#include "voice_chat.h"
#include "heap.h"
#include <vct.h>
#include <stdint.h>

/* The native VCT implementation cannot establish sessions. Keep every public
 * entry quiescent and report failure, including calls before initialization
 * and repeated shutdown. No microphone, stream or network session is opened. */
VoiceChat gVoiceChat;
VoiceChatWork *gVoiceChatWork;

void VoiceChat_Init(int heapId, int codec, int count) { (void)heapId; (void)codec; (void)count; }
void VoiceChat_InitAudio(void) {}
void VoiceChat_StartStream(void) {}
void VoiceChat_Update(void) {}
void VoiceChat_Mute(void) {}
void VoiceChat_Unmute(void) {}
void VoiceChat_HangUp(int unused) { (void)unused; }
void VoiceChat_SetEndCallback(void (*callback)(void)) { (void)callback; }
void VoiceChat_EndSession(VCTSession *session) { (void)session; }
void VoiceChat_Shutdown(void) {}
int VoiceChat_GetVoiceActivity(void) { return 0; }
BOOL VoiceChat_RequestSession(u32 aid) { (void)aid; return FALSE; }
BOOL VoiceChat_AcceptSession(int unused) { (void)unused; return FALSE; }
BOOL VoiceChat_ReceiveData(u32 aid, void *data, u32 size) { (void)aid; (void)data; (void)size; return FALSE; }
int VoiceChat_AddConferenceClients(u32 aidMask, int skipAid) { (void)aidMask; (void)skipAid; return 0; }
void VoiceChat_MicCallback(MICResult result, void *arg) { (void)result; (void)arg; }
void VoiceChat_ConferenceEventCallback(u32 unusedId, u32 event, VCTSession *session, void *context) {
    (void)unusedId; (void)event; (void)session; (void)context;
}
void VoiceChat_PhoneEventCallback(u32 unusedId, u32 event, VCTSession *session, void *context) {
    (void)unusedId; (void)event; (void)session; (void)context;
}
void VoiceChat_StreamCallback(int enabled, u32 count, void **buffers, u32 size, int unused, u8 *address) {
    (void)enabled; (void)unused; (void)address;
    for (u32 i = 0; i < count; i++) MI_CpuFill8(buffers[i], 0, size);
}

void VoiceChat_AllocAligned(void **allocation, void **aligned, u32 size, u32 heapId) {
    *allocation = NULL;
    *aligned = NULL;
    if (size > UINT32_MAX - 32) return;
    *allocation = Heap_Alloc((enum HeapID)heapId, size + 32);
    if (*allocation == NULL) return;
    MI_CpuFill8(*allocation, 0, size + 32);
    *aligned = (void *)(((uintptr_t)*allocation + 31) & ~(uintptr_t)31);
}
