#include <hg/native_overlays.h>
#include "hg_overlay_ids.h"

/* Game and overlay functions are linked into the executable on PC. Keep
 * overlay identity/lifetime bookkeeping; never copy ARM images or jump to ROM
 * static-initializer addresses. Per-overlay BSS reset hooks remain port work. */
static BOOL sLoadedOverlays[HG_PC_OVERLAY_COUNT];

BOOL PC_LoadOverlay(MIProcessor target, FSOverlayID id) {
    if (target != MI_PROCESSOR_ARM9 || id >= HG_PC_OVERLAY_COUNT) return FALSE;
    sLoadedOverlays[id] = TRUE;
    return TRUE;
}

BOOL PC_UnloadOverlay(MIProcessor target, FSOverlayID id) {
    if (target != MI_PROCESSOR_ARM9 || id >= HG_PC_OVERLAY_COUNT) return FALSE;
    sLoadedOverlays[id] = FALSE;
    return TRUE;
}

BOOL PC_IsOverlayLoaded(FSOverlayID id) {
    return id < HG_PC_OVERLAY_COUNT && sLoadedOverlays[id];
}

/* Direct SDK calls occur in the game as well as its overlay manager. A native
 * game link must apply these wrappers to both routes. */
BOOL __wrap_FS_LoadOverlay(MIProcessor target, FSOverlayID id) {
    return PC_LoadOverlay(target, id);
}

BOOL __wrap_FS_UnloadOverlay(MIProcessor target, FSOverlayID id) {
    return PC_UnloadOverlay(target, id);
}
