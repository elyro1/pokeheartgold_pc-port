#ifndef HG_PC_FS_OVERLAY_H
#define HG_PC_FS_OVERLAY_H
#include_next <nitro/fs/overlay.h>
#include "hg_overlay_ids.h"

/* The SDK's generic PC default maps all overlays to zero. Game data and
 * dispatch decisions still require the original stable ordinal identities. */
#undef FS_EXTERN_OVERLAY
#undef FS_OVERLAY_ID
#define FS_EXTERN_OVERLAY(name)
#define FS_OVERLAY_ID(name) HG_PC_OVERLAY_ID_ ## name
#endif
