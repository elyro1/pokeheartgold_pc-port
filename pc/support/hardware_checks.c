#include <dsprot.h>

static u32 NormalHardwareResult(void *callback, u32 result) {
    if (result && callback) {
        ((void (*)(void))callback)();
    }
    return result;
}

u32 DSProt_DetectFlashcart(void *callback) { return NormalHardwareResult(callback, 0); }
u32 DSProt_DetectNotFlashcart(void *callback) { return NormalHardwareResult(callback, 1); }
u32 DSProt_DetectEmulator(void *callback) { return NormalHardwareResult(callback, 0); }
u32 DSProt_DetectNotEmulator(void *callback) { return NormalHardwareResult(callback, 1); }
u32 DSProt_DetectDummy(void *callback) { return NormalHardwareResult(callback, 0); }
u32 DSProt_DetectNotDummy(void *callback) { return NormalHardwareResult(callback, 1); }
