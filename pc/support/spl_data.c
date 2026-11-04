#include "spl_internal.h"

/* lib/asm/spl_data.s: callback entries must grow with native pointers. */
void (*const gSplDrawPlaneFuncs[2])(fx32, fx32, fx32, fx32) = {drawXYPlane, drawXZPlane};
void (*const gSplRotateFuncs[2])(fx32, fx32, MtxFx43 *) = {rotTypeY, rotTypeXYZ};
const VecFx16 gSplAxisY = {0, FX16_ONE, 0};
u32 spl_rndm_seed;
