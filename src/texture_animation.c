#include "texture_animation.h"

#include <nnsys/g3d_internal.h>

#include "global.h"

#include "ascii_util.h"
#include "billboard_gfx_sequence.h"
#include "heap.h"
#include "vram_transfer_manager.h"

struct TextureAnimation {
    const BillboardGfxSequence *gfxSequence;
    NNSG3dResTex *texture;
    NNSGfdTexKey textureKey;
    NNSGfdPlttKey paletteKey;
    u8 textureIndex;
    u8 paletteIndex;
};

struct TextureAnimationPool {
    TextureAnimation *animations;
    int count;
};

static void TextureAnimationPool_Clear(TextureAnimationPool *pool);
static void TextureAnimation_Reset(TextureAnimation *animation);
static void TextureAnimation_SetFrameIndex(TextureAnimation *animation, u16 frame);
static void TextureAnimation_LoadTexture(TextureAnimation *animation, const BillboardTexPlttIndex *indices);
static void TextureAnimation_LoadPalette(TextureAnimation *animation, const BillboardTexPlttIndex *indices);
static void *TextureAnimation_GetTextureData(TextureAnimation *animation, const BillboardTexPlttIndex *indices);
static void *TextureAnimation_GetPaletteData(TextureAnimation *animation, const BillboardTexPlttIndex *indices);
static u32 Texture_GetImageVramOffset(NNSG3dResTex *texture, u32 *entry);
static u32 Texture_GetImageSize(NNSG3dResTex *texture, u32 *entry);

TextureAnimationPool *TextureAnimationPool_New(int count, enum HeapID heapID) {
    TextureAnimationPool *pool = Heap_Alloc(heapID, sizeof(*pool));
    int i;
    pool->animations = Heap_Alloc(heapID, sizeof(TextureAnimation) * count);
    pool->count = count;
    for (i = 0; i < pool->count; i++) {
        TextureAnimation_Reset(&pool->animations[i]);
    }
    return pool;
}

void TextureAnimationPool_Free(TextureAnimationPool *pool) {
    TextureAnimationPool_Clear(pool);
    Heap_Free(pool->animations);
    Heap_Free(pool);
}

TextureAnimation *TextureAnimationPool_Add(TextureAnimationPool *pool, const BillboardGfxSequence *gfxSequence, NNSG3dResTex *texture, u32 textureKey, u32 paletteKey, fx32 frame) {
    TextureAnimation *animation = NULL;
    int i;
    for (i = 0; i < pool->count; i++) {
        if (pool->animations[i].gfxSequence == NULL) {
            animation = &pool->animations[i];
            break;
        }
    }
    if (animation == NULL) {
        return NULL;
    }
    animation->gfxSequence = gfxSequence;
    animation->texture = texture;
    animation->textureKey = textureKey;
    animation->paletteKey = paletteKey;
    animation->textureIndex = 255;
    animation->paletteIndex = 255;
    TextureAnimation_SetFrameIndex(animation, frame >> FX32_SHIFT);
    return animation;
}

void TextureAnimation_Remove(TextureAnimation *animation) {
    TextureAnimation_Reset(animation);
}

static void TextureAnimationPool_Clear(TextureAnimationPool *pool) {
    int i;
    for (i = 0; i < pool->count; i++) {
        TextureAnimation_Reset(&pool->animations[i]);
    }
}

void TextureAnimation_SetFrame(TextureAnimation *animation, fx32 frame) {
    TextureAnimation_SetFrameIndex(animation, frame >> FX32_SHIFT);
}

static void TextureAnimation_Reset(TextureAnimation *animation) {
    animation->gfxSequence = NULL;
    animation->texture = NULL;
    animation->textureKey = 0;
    animation->paletteKey = 0;
    animation->textureIndex = 0;
    animation->paletteIndex = 0;
}

static void TextureAnimation_SetFrameIndex(TextureAnimation *animation, u16 frame) {
    BillboardTexPlttIndex indices = BillboardGfxSequence_GetTexPlttIndex(animation->gfxSequence, frame);
    if (animation->textureIndex != indices.textureIdx) {
        TextureAnimation_LoadTexture(animation, &indices);
    }
    if (animation->paletteIndex != indices.plttIdx) {
        TextureAnimation_LoadPalette(animation, &indices);
    }
}

static void TextureAnimation_LoadTexture(TextureAnimation *animation, const BillboardTexPlttIndex *indices) {
    void *data = TextureAnimation_GetTextureData(animation, indices);
    NNSGfdTexKey key = animation->textureKey;
    GF_CreateNewVramTransferTask(NNS_GFD_DST_3D_TEX_VRAM, NNS_GfdGetTexKeyAddr(key), data, ((key & 0x7FFF0000) >> 16) << 4);
    animation->textureIndex = indices->textureIdx;
}

static void TextureAnimation_LoadPalette(TextureAnimation *animation, const BillboardTexPlttIndex *indices) {
    void *data = TextureAnimation_GetPaletteData(animation, indices);
    NNSGfdPlttKey key = animation->paletteKey;
    GF_CreateNewVramTransferTask(NNS_GFD_DST_3D_TEX_PLTT, NNS_GfdGetPlttKeyAddr(key), data, ((key & 0xFFFF0000) >> 16) << 3);
    animation->paletteIndex = indices->plttIdx;
}

static void *TextureAnimation_GetTextureData(TextureAnimation *animation, const BillboardTexPlttIndex *indices) {
    return Texture_GetImageData(animation->texture, indices->textureIdx);
}

static void *TextureAnimation_GetPaletteData(TextureAnimation *animation, const BillboardTexPlttIndex *indices) {
    return Texture_GetPaletteData(animation->texture, indices->plttIdx);
}

void *Texture_GetImageData(NNSG3dResTex *texture, u32 index) {
    u32 *entry = texture ? NNS_G3dGetResDataByIdx(&texture->dict, index) : NULL;
    if (entry == NULL) {
        return NULL;
    }
#if defined(SDK_PORT)
    // The dictionary address is relative to the texture bytes in the file.
    // A key can be installed while a billboard is bound for drawing; its
    // packed VRAM address/size must never become part of a CPU source pointer.
    return (u8 *)texture + texture->texInfo.ofsTex + ((*entry & 0xFFFF) << 3);
#else
    return (u8 *)texture + texture->texInfo.ofsTex + (((*entry & 0xFFFF) + texture->texInfo.vramKey) << 3);
#endif
}

void *Texture_GetPaletteData(NNSG3dResTex *texture, u32 index) {
    u16 *entry;
    if (texture && texture->plttInfo.ofsDict) {
        entry = NNS_G3dGetResDataByIdx((NNSG3dResDict *)((u8 *)texture + texture->plttInfo.ofsDict), index);
    } else {
        entry = NULL;
    }
    if (entry == NULL) {
        return NULL;
    }
    return (u8 *)texture + texture->plttInfo.ofsPlttData + (*entry << 3);
}

static u32 Texture_GetImageVramOffset(NNSG3dResTex *texture, u32 *entry) {
    return ((*entry << 16) >> 13) + ((texture->texInfo.vramKey << 16) >> 13);
}

u32 Texture_GetImageVramOffsetByName(NNSG3dResTex *texture, const char *name) {
    NNSG3dResName key;
    u32 *entry;
    Ascii_SetResName(&key, name);
    entry = texture ? NNS_G3dGetResDataByName(&texture->dict, &key) : NULL;
    if (entry == NULL) {
        return 0;
    }
    return Texture_GetImageVramOffset(texture, entry);
}

u32 Texture_GetImageVramOffsetByIndex(NNSG3dResTex *texture, u32 index) {
    u32 *entry = texture ? NNS_G3dGetResDataByIdx(&texture->dict, index) : NULL;
    if (entry == NULL) {
        return 0;
    }
    return Texture_GetImageVramOffset(texture, entry);
}

static u32 Texture_GetImageSize(NNSG3dResTex *texture, u32 *entry) {
    u32 packed = *entry;
    u32 divisor;
    switch ((packed & REG_G3_TEXIMAGE_PARAM_TEXFMT_MASK) >> REG_G3_TEXIMAGE_PARAM_TEXFMT_SHIFT) {
    case GX_TEXFMT_PLTT4:
        divisor = 4;
        break;
    case GX_TEXFMT_PLTT16:
        divisor = 2;
        break;
    case GX_TEXFMT_PLTT256:
        divisor = 1;
        break;
    case GX_TEXFMT_A3I5:
        divisor = 1;
        break;
    case GX_TEXFMT_A5I3:
        divisor = 1;
        break;
    default:
        return 0;
    }
    return ((((packed & REG_G3_TEXIMAGE_PARAM_V_SIZE_MASK) >> REG_G3_TEXIMAGE_PARAM_V_SIZE_SHIFT) << 4) * (((packed & REG_G3_TEXIMAGE_PARAM_T_SIZE_MASK) >> REG_G3_TEXIMAGE_PARAM_T_SIZE_SHIFT) << 4)) / divisor;
}

u32 Texture_GetImageSizeByIndex(NNSG3dResTex *texture, u32 index) {
    u32 *entry = texture ? NNS_G3dGetResDataByIdx(&texture->dict, index) : NULL;
    return Texture_GetImageSize(texture, entry);
}

u32 Texture_GetImageSizeByName(NNSG3dResTex *texture, const char *name) {
    NNSG3dResName key;
    u32 *entry;
    Ascii_SetResName(&key, name);
    entry = texture ? NNS_G3dGetResDataByName(&texture->dict, &key) : NULL;
    return Texture_GetImageSize(texture, entry);
}
