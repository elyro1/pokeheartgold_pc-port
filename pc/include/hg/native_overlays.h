#ifndef HG_PC_NATIVE_OVERLAYS_H
#define HG_PC_NATIVE_OVERLAYS_H
#include <nitro.h>
BOOL PC_LoadOverlay(MIProcessor target, FSOverlayID id);
BOOL PC_UnloadOverlay(MIProcessor target, FSOverlayID id);
BOOL PC_IsOverlayLoaded(FSOverlayID id);
#endif
