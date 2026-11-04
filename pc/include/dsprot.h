#ifndef HG_PC_DSPROT_H
#define HG_PC_DSPROT_H
#include <nitro/types.h>
/* Native builds do not execute the encrypted ARM hardware probes. ROM input
 * validation belongs to pc/tools/assets.py; these retain the game's normal
 * hardware result and callback contract. */
u32 DSProt_DetectFlashcart(void *callback);
u32 DSProt_DetectNotFlashcart(void *callback);
u32 DSProt_DetectEmulator(void *callback);
u32 DSProt_DetectNotEmulator(void *callback);
u32 DSProt_DetectDummy(void *callback);
u32 DSProt_DetectNotDummy(void *callback);
#endif
