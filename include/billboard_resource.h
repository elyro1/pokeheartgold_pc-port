#ifndef POKEHEARTGOLD_BILLBOARD_RESOURCE_H
#define POKEHEARTGOLD_BILLBOARD_RESOURCE_H

#include "global.h"

#include "billboard_internal.h"
#include "billboard_gfx_sequence.h"

typedef const void *BillboardRangesHandle;
typedef struct BillboardModelEntry {
    u16 id;
    u16 member;
} BillboardModelEntry;
typedef struct SpriteBillboardInfo {
    u8 unk0[2];
    u8 modelId;
    u8 sequenceId;
    const void *ranges;
} SpriteBillboardInfo;
extern const SpriteBillboardInfo gSpriteBillboardInfos[24];


#endif // POKEHEARTGOLD_BILLBOARD_RESOURCE_H
