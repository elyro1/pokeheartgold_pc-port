#include "global.h"

#include "constants/sprites.h"

#include "pokeathlon/pokeathlon_course_ceremony.h"
#include "pokeathlon/pokeathlon_course_ui.h"

#include "assert.h"
#include "char_tiles.h"
#include "font.h"
#include "gf_gfx_loader.h"
#include "heap.h"
#include "options.h"
#include "palette.h"
#include "player_data.h"
#include "pokemon.h"
#include "pokemon_icon_idx.h"
#include "screen_fade.h"
#include "system.h"
#include "touchscreen.h"

static void PokeathlonCourse_LoadMonPicEx(ManagedSprite *sprite, PokeathlonCourseMonInfo *info, BOOL mainScreen, u32 grayScale, enum HeapID heapId, BOOL loadPalette);
static void PokeathlonCourse_LoadSpriteCharsAt(Sprite *sprite, void *data, u32 size, u32 offset);
static void PokeathlonCourse_LoadSpriteChars(Sprite *sprite, void *data, u32 size);

static inline void *GetTexDictData(NNSG3dResTex *tex, u32 index) {
    if (tex) {
        return NNS_G3dGetResDataByIdx(&tex->dict, index);
    }
    return NULL;
}

// The trainer pic shown for an NPC team's overworld sprite.
typedef struct PokeathlonNpcTrainerPic {
    int spriteId;
    int fileId;
} PokeathlonNpcTrainerPic;

static const PokeathlonNpcTrainerPic sPokeathlonNpcTrainerPics[253] = {
    { SPRITE_BABYBOY1,       0     },
    { SPRITE_BABYGIRL1,      1     },
    { SPRITE_BOY1,           2     },
    { SPRITE_BOY2,           3     },
    { SPRITE_BOY3,           4     },
    { SPRITE_GIRL1,          5     },
    { SPRITE_GIRL2,          6     },
    { SPRITE_GIRL3,          7     },
    { SPRITE_MAN1,           8     },
    { SPRITE_MAN2,           9     },
    { SPRITE_MAN3,           10    },
    { SPRITE_WOMAN1,         12    },
    { SPRITE_WOMAN2,         13    },
    { SPRITE_WOMAN3,         14    },
    { SPRITE_MIDDLEMAN1,     16    },
    { SPRITE_MIDDLEWOMAN1,   17    },
    { SPRITE_OLDMAN1,        18    },
    { SPRITE_OLDWOMAN1,      19    },
    { SPRITE_BIGMAN,         34    },
    { SPRITE_MOUNT,          35    },
    { SPRITE_CYCLEHERO,      71    },
    { SPRITE_REPORTER,       31    },
    { SPRITE_CAMERAMAN,      32    },
    { SPRITE_SHOPM1,         23    },
    { SPRITE_SHOPW1,         24    },
    { SPRITE_ASSISTANTM,     25    },
    { SPRITE_ASSISTANTW,     26    },
    { SPRITE_BADMAN,         36    },
    { SPRITE_SKIERW,         22    },
    { SPRITE_POLICEMAN,      37    },
    { SPRITE_IDOL,           38    },
    { SPRITE_GENTLEMAN,      39    },
    { SPRITE_LADY,           40    },
    { SPRITE_CYCLEM,         20    },
    { SPRITE_CYCLEW,         21    },
    { SPRITE_WORKMAN,        41    },
    { SPRITE_FARMER,         42    },
    { SPRITE_COWGIRL,        43    },
    { SPRITE_CLOWN,          44    },
    { SPRITE_ARTIST,         45    },
    { SPRITE_SPORTSMAN,      46    },
    { SPRITE_EXPLORE,        47    },
    { SPRITE_FIGHTER,        48    },
    { SPRITE_CAMPBOY,        49    },
    { SPRITE_PICNICGIRL,     50    },
    { SPRITE_FISHING,        51    },
    { SPRITE_AMBRELLA,       52    },
    { SPRITE_SEAMAN,         53    },
    { SPRITE_WAITER,         29    },
    { SPRITE_WAITRESS,       30    },
    { SPRITE_GORGGEOUSM,     27    },
    { SPRITE_GORGGEOUSW,     28    },
    { SPRITE_MAN5,           11    },
    { SPRITE_WOMAN5,         15    },
    { SPRITE_MYSTERY,        33    },
    { SPRITE_PIKACHU,        68    },
    { SPRITE_ROCK,           91    },
    { SPRITE_BREAKROCK,      92    },
    { SPRITE_TREE,           93    },
    { SPRITE_MONSTARBALL,    94    },
    { SPRITE_HEROINE,        70    },
    { SPRITE_CYCLEHEROINE,   72    },
    { SPRITE_DOCTOR,         54    },
    { SPRITE_SEVEN1,         62    },
    { SPRITE_SEVEN2,         63    },
    { SPRITE_SEVEN3,         64    },
    { SPRITE_SEVEN4,         65    },
    { SPRITE_SEVEN5,         66    },
    { SPRITE_GSBOY1,         67    },
    { SPRITE_GSRIVEL,        58    },
    { SPRITE_SUNGLASSES,     57    },
    { SPRITE_TOWERBOSS,      61    },
    { SPRITE_MAID,           55    },
    { SPRITE_SPHERO,         75    },
    { SPRITE_SPHEROINE,      76    },
    { SPRITE_WATERHERO,      77    },
    { SPRITE_WATERHEROINE,   78    },
    { SPRITE_SWIMHERO,       73    },
    { SPRITE_SWIMHEROINE,    74    },
    { SPRITE_FISHINGHERO,    79    },
    { SPRITE_FISH_HEROINE,   80    },
    { SPRITE_LADDERHERO,     81    },
    { SPRITE_LADDERHEROINE,  82    },
    { SPRITE_DELIVERY,       56    },
    { SPRITE_POKEHERO,       85    },
    { SPRITE_POKEHEROINE,    86    },
    { SPRITE_SAVEHERO,       87    },
    { SPRITE_SAVEHEROINE,    88    },
    { SPRITE_BANZAIHERO,     89    },
    { SPRITE_BANZAIHEROINE,  90    },
    { SPRITE_RED,            170   },
    { SPRITE_THIEF,          171   },
    { SPRITE_ITAKO,          83    },
    { SPRITE_BOZU,           172   },
    { SPRITE_ROCKETW,        173   },
    { SPRITE_ROCKETM,        174   },
    { SPRITE_RKANBUW,        175   },
    { SPRITE_RKANBUM,        176   },
    { SPRITE_BOARDER,        177   },
    { SPRITE_FIRE,           178   },
    { SPRITE_GANG,           179   },
    { SPRITE_DANCER,         180   },
    { SPRITE_MASAKI,         181   },
    { SPRITE_CAPTAIN,        182   },
    { SPRITE_RGYARADOSU,     183   },
    { SPRITE_HERO_2,         213   },
    { SPRITE_HEROINE_2,      214   },
    { SPRITE_ITAKO_,         84    },
    { SPRITE_GSBABYBOY1,     113   },
    { SPRITE_GSBABYGIRL1,    114   },
    { SPRITE_GSBOY2,         115   },
    { SPRITE_GSBOY3,         116   },
    { SPRITE_GSGIRL1,        117   },
    { SPRITE_GSGIRL2,        118   },
    { SPRITE_GSGIRL3,        119   },
    { SPRITE_GSMAN1,         120   },
    { SPRITE_GSMAN2,         121   },
    { SPRITE_GSMAN3,         122   },
    { SPRITE_GSWOMAN1,       123   },
    { SPRITE_GSWOMAN2,       124   },
    { SPRITE_GSWOMAN3,       125   },
    { SPRITE_GSMIDDLEMAN1,   126   },
    { SPRITE_MIDDLEWOMAN1_2, 127   },
    { SPRITE_GSOLDMAN1,      128   },
    { SPRITE_GSOLDWOMAN1,    129   },
    { SPRITE_GSBIGMAN,       130   },
    { SPRITE_MOUNT_2,        131   },
    { SPRITE_SHOPM1_2,       132   },
    { SPRITE_PCWOMAN1,       133   },
    { SPRITE_PCWOMAN2,       134   },
    { SPRITE_PCWOMAN3,       135   },
    { SPRITE_GSASSISTANTM,   136   },
    { SPRITE_GSGENTLEMAN,    137   },
    { SPRITE_GSSWIMMERM,     138   },
    { SPRITE_GSSWIMMERW,     139   },
    { SPRITE_GSFIGHTER,      140   },
    { SPRITE_CAMPBOY_2,      141   },
    { SPRITE_PICNICGIRL_2,   142   },
    { SPRITE_FISHING_2,      143   },
    { SPRITE_SEAMAN_2,       144   },
    { SPRITE_MYSTERY_2,      145   },
    { SPRITE_GSLEADER1,      146   },
    { SPRITE_GSLEADER2,      147   },
    { SPRITE_GSLEADER3,      148   },
    { SPRITE_GSLEADER4,      149   },
    { SPRITE_GSLEADER5,      150   },
    { SPRITE_GSLEADER6,      151   },
    { SPRITE_GSLEADER7,      152   },
    { SPRITE_GSLEADER8,      153   },
    { SPRITE_GSBIGFOUR1,     154   },
    { SPRITE_GSBIGFOUR2,     155   },
    { SPRITE_GSBIGFOUR3,     156   },
    { SPRITE_GSBIGFOUR4,     157   },
    { SPRITE_WATARU,         158   },
    { SPRITE_GSMAMA,         159   },
    { SPRITE_OOKIDO,         160   },
    { SPRITE_GSDSBOY,        161   },
    { SPRITE_GSLEADER9,      162   },
    { SPRITE_GSLEADER10,     163   },
    { SPRITE_GSLEADER11,     164   },
    { SPRITE_GSLEADER12,     165   },
    { SPRITE_GSLEADER13,     166   },
    { SPRITE_GSLEADER14,     167   },
    { SPRITE_GSLEADER15,     168   },
    { SPRITE_GSLEADER16,     169   },
    { SPRITE_MANIA,          184   },
    { SPRITE_JUGGRER,        185   },
    { SPRITE_USOKKY,         186   },
    { SPRITE_MINAKI,         187   },
    { SPRITE_THLONOWNER,     188   },
    { SPRITE_INSTRUCTOR,     189   },
    { SPRITE_KURUMI,         190   },
    { SPRITE_NANAMI,         191   },
    { SPRITE_REDMAMA,        192   },
    { SPRITE_COUNTERM,       193   },
    { SPRITE_KABIGON,        194   },
    { SPRITE_ICE,            195   },
    { SPRITE_ROTOMF,         200   },
    { SPRITE_ROTOMS,         198   },
    { SPRITE_ROTOMI,         197   },
    { SPRITE_ROTOMW,         199   },
    { SPRITE_ROTOMG,         196   },
    { SPRITE_STOP,           243   },
    { SPRITE_MONO_PIP,       242   },
    { SPRITE_GTOROPHY,       229   },
    { SPRITE_STOROPHY,       230   },
    { SPRITE_BTOROPHY,       231   },
    { SPRITE_POKEWALL,       239   },
    { SPRITE_MEDAL,          241   },
    { SPRITE_SCROLL,         240   },
    { SPRITE_SIGNSHOES,      237   },
    { SPRITE_SIGNCLOTHES,    234   },
    { SPRITE_SIGNFLAG,       235   },
    { SPRITE_SIGNPOKEGEAR,   236   },
    { SPRITE_SIGNBALL,       238   },
    { SPRITE_STATUEHERO,     232   },
    { SPRITE_STATUEHEROINE,  233   },
    { SPRITE_HERO,           69    },
    { SPRITE_RHERO,          95    },
    { SPRITE_RHEROINE,       96    },
    { SPRITE_RBANZAIHERO,    97    },
    { SPRITE_RBANZAIINE,     98    },
    { SPRITE_PKTHHERO,       99    },
    { SPRITE_PKTHHEROINE,    100   },
    { SPRITE_BONGURI,        251   },
    { SPRITE_BONGURI_R,      255   },
    { 0x108,                 0x102 },
    { SPRITE_BONGURI_B,      252   },
    { SPRITE_BONGURI_G,      254   },
    { 0x10B,                 0x101 },
    { 0x10C,                 0x100 },
    { SPRITE_BONGURI_BK,     253   },
    { 0x10E,                 0x106 },
    { 0x10F,                 0x109 },
    { 0x110,                 0x103 },
    { 0x111,                 0x105 },
    { 0x112,                 0x108 },
    { 0x113,                 0x107 },
    { 0x114,                 0x104 },
    { SPRITE_BRAINS1,        103   },
    { SPRITE_BRAINS2,        104   },
    { SPRITE_BRAINS3,        105   },
    { SPRITE_BRAINS4,        106   },
    { SPRITE_PRINCESS,       107   },
    { SPRITE_BFSM,           108   },
    { SPRITE_BFSW1,          109   },
    { SPRITE_BFSW2,          110   },
    { SPRITE_WIFISM,         101   },
    { SPRITE_WIFISF,         102   },
    { SPRITE_AJI_PERU,       111   },
    { SPRITE_YADON,          209   },
    { SPRITE_DAIGO,          59    },
    { SPRITE_CHAMPION,       60    },
    { SPRITE_SHAKEHERO,      207   },
    { SPRITE_SHAKEHEROINE,   208   },
    { SPRITE_HOU_OBJ01,      210   },
    { SPRITE_LUG_OBJ01,      211   },
    { SPRITE_GSWOMAN6,       212   },
    { SPRITE_LEADER1,        215   },
    { SPRITE_LEADER2,        216   },
    { SPRITE_LEADER3,        217   },
    { SPRITE_LEADER4,        218   },
    { SPRITE_LEADER5,        219   },
    { SPRITE_LEADER6,        220   },
    { SPRITE_LEADER7,        221   },
    { SPRITE_LEADER8,        222   },
    { SPRITE_GANTETSU,       223   },
    { SPRITE_JUPETTA,        224   },
    { SPRITE_SAKAKI,         225   },
    { SPRITE_SUIT,           226   },
    { SPRITE_CHOUROU,        227   },
    { SPRITE_RAPURASU,       228   },
    { 0xFFFF,                0     },
};

static const TileRect sPokeathlonCourseMonPicRect = {
    .x = 0,
    .y = 0,
    .w = 10,
    .h = 10,
};

static const u32 sPokeathlonCourseTrainerPicFileIds[6] = { 99, 100, 189, 9, 6, 3 };

void PokeathlonCourse_LoadTrainerPic(ManagedSprite *sprite, int fileId, int index, enum HeapID heapId) {
    PokeathlonCourse_LoadTrainerPicIntoSprite(sprite->sprite, fileId, index, heapId);
}

// Converts texture index of the overworld model fileId (NARC data/mmodel/mmodel) into the sprite's characters
// and palette.

void PokeathlonCourse_LoadTrainerPicIntoSprite(Sprite *sprite, int fileId, int index, enum HeapID heapId) {
    NARC *narc;
    void *file;
    NNSG3dResTex *tex;
    void *buffer;
    u8 *texData;
    int offset;
    int location;
    int paletteOffset;
    NNS_G2D_VRAM_TYPE vramType;
    thunk_Sprite_SetDrawFlag(sprite, FALSE);
    narc = NARC_New(NARC_data_mmodel_mmodel, heapId);
    file = NARC_AllocAndReadWholeMember(narc, fileId, heapId);
    tex = NNS_G3dGetTex(file);
    offset = (*(u32 *)GetTexDictData(tex, index) & 0xFFFF) << 3;
    texData = (u8 *)tex + tex->texInfo.ofsTex;
    buffer = Heap_AllocAtEnd(heapId, 0x200);
    CharTiles_CopyRectFromBuffer(texData + offset, 4, 0, 0, 4, 4, buffer);
    PokeathlonCourse_LoadSpriteChars(sprite, buffer, 0x200);
    Heap_Free(buffer);
    vramType = Sprite_GetVramType(sprite);
    paletteOffset = tex->plttInfo.ofsPlttData;
    DC_FlushRange((u8 *)tex + paletteOffset, 0x20);
    location = NNS_G2dGetImagePaletteLocation(Sprite_GetPaletteProxy(sprite), vramType);
    if (vramType == NNS_G2D_VRAM_TYPE_2DMAIN) {
        GX_LoadOBJPltt((u8 *)tex + paletteOffset, location, 0x20);
    } else {
        GXS_LoadOBJPltt((u8 *)tex + paletteOffset, location, 0x20);
    }
    Heap_Free(file);
    NARC_Delete(narc);
    thunk_Sprite_SetDrawFlag(sprite, TRUE);
}

void PokeathlonCourse_LoadTeamTrainerPic(ManagedSprite **sprites, PokeathlonCourseData *course, u8 team, int index) {
    PokeathlonCourse_LoadPlayerTrainerPic(sprites[team]->sprite, course, team, index);
}

void PokeathlonCourse_LoadPlayerTrainerPic(Sprite *sprite, PokeathlonCourseData *course, u32 team, u32 index) {
    PlayerProfile *profile = PokeathlonCourse_GetPlayerProfileFromData(course, team);
    enum HeapID heapId = PokeathlonCourse_GetHeapID(course);
    u16 fileId;
    if (PlayerProfile_GetTrainerGender(profile) == 0) {
        fileId = PokeathlonCourse_GetTrainerPicFileId(0);
    } else {
        fileId = PokeathlonCourse_GetTrainerPicFileId(1);
    }
    PokeathlonCourse_LoadTrainerPicIntoSprite(sprite, fileId, (u8)index, heapId);
}

u16 PokeathlonCourse_GetTrainerPicFileId(u32 kind) {
    GF_ASSERT(kind < NELEMS(sPokeathlonCourseTrainerPicFileIds));
    return sPokeathlonCourseTrainerPicFileIds[kind];
}

void PokeathlonCourse_LoadMonPic(ManagedSprite *sprite, PokeathlonCourseMonInfo *info, BOOL mainScreen, u32 palette, enum HeapID heapId) {
    PokeathlonCourse_LoadMonPicEx(sprite, info, mainScreen, palette, heapId, TRUE);
}

void PokeathlonCourse_LoadMonPicWithPalette(ManagedSprite *sprite, PokeathlonCourseMonInfo *info, BOOL mainScreen, u32 palette, enum HeapID heapId, BOOL loadPalette) {
    PokeathlonCourse_LoadMonPicEx(sprite, info, mainScreen, palette, heapId, loadPalette);
}

// Front picture of the Pokemon (no sprite if species is 0).

static void PokeathlonCourse_LoadMonPicEx(ManagedSprite *sprite, PokeathlonCourseMonInfo *info, BOOL mainScreen, u32 grayScale, enum HeapID heapId, BOOL loadPalette) {
    PokepicTemplate template;
    TileRect rect;
    void *buffer;
    if (info->species == SPECIES_NONE) {
        ManagedSprite_SetDrawFlag(sprite, FALSE);
        return;
    }
    GetMonSpriteCharAndPlttNarcIdsEx(&template, info->species, info->gender, 2, info->isShiny, info->form, info->personality);
    rect = sPokeathlonCourseMonPicRect;
    buffer = Heap_AllocAtEnd(heapId, 0xC80);
    CharTiles_LoadMonRectAsObjFromNarc((NarcId) template.narcID, template.charDataID, heapId, &rect, buffer, info->personality, FALSE, 2, info->species);
    PokeathlonCourse_LoadSpriteChars(sprite->sprite, buffer, 0xC80);
    if (loadPalette) {
        PokeathlonCourse_LoadSpritePalette(sprite, (NarcId) template.narcID, template.palDataID, mainScreen, grayScale, heapId);
    }
    ManagedSprite_SetDrawFlag(sprite, TRUE);
    Heap_Free(buffer);
}

void PokeathlonCourse_LoadMonIcon(ManagedSprite *sprite, const PokeathlonCourseMonInfo *info, int palette, NARC *narc, u32 offset, enum HeapID heapId) {
    NNSG2dCharacterData *charData;
    void *file = GfGfxLoader_GetCharDataFromOpenNarc(narc, GetMonIconNaixEx(info->species, FALSE, info->form), FALSE, &charData, heapId);
    GF_ASSERT(file);
    PokeathlonCourse_LoadSpriteCharsAt(sprite->sprite, charData->pRawData, 0x200, offset);
    Heap_Free(file);
    ManagedSprite_SetPaletteOverride(sprite, palette + GetMonIconPaletteEx(info->species, info->form, FALSE));
}

static void PokeathlonCourse_LoadSpriteCharsAt(Sprite *sprite, void *data, u32 size, u32 offset) {
    NNS_G2D_VRAM_TYPE vramType = Sprite_GetVramType(sprite);
    u32 location = NNS_G2dGetImageLocation(Sprite_GetImageProxy(sprite), vramType);
    DC_FlushRange(data, size);
    switch (vramType) {
    case NNS_G2D_VRAM_TYPE_2DMAIN:
        GX_LoadOBJ(data, location + offset, size);
        break;
    case NNS_G2D_VRAM_TYPE_2DSUB:
        GXS_LoadOBJ(data, location + offset, size);
        break;
    default:
        GF_ASSERT(FALSE);
        break;
    }
}

static void PokeathlonCourse_LoadSpriteChars(Sprite *sprite, void *data, u32 size) {
    PokeathlonCourse_LoadSpriteCharsAt(sprite, data, size, 0);
}

void PokeathlonCourse_LoadSpritePalette(ManagedSprite *sprite, NarcId narcId, int fileId, BOOL mainScreen, BOOL grayScale, enum HeapID heapId) {
    NNS_G2D_VRAM_TYPE vramType;
    enum GFPalLoadLocation location;
    u32 offset;

    if (mainScreen) {
        vramType = NNS_G2D_VRAM_TYPE_2DMAIN;
        location = GF_PAL_LOCATION_MAIN_OBJ;
    } else {
        vramType = NNS_G2D_VRAM_TYPE_2DSUB;
        location = GF_PAL_LOCATION_SUB_OBJ;
    }
    offset = NNS_G2dGetImagePaletteLocation(Sprite_GetPaletteProxy(sprite->sprite), vramType);
    GfGfxLoader_GXLoadPal(narcId, fileId, location, (enum GFPalSlotOffset)offset, 0x20, heapId);
    if (grayScale) {
        void *palette;
        if (mainScreen) {
            palette = (void *)HW_OBJ_PLTT;
        } else {
            palette = (void *)HW_DB_OBJ_PLTT;
        }
#if defined(SDK_PORT)
        TintPalette_GrayScale((u16 *)((u8 *)palette + offset), 0x20);
#else
        TintPalette_GrayScale((u16 *)(offset + (u32)palette), 0x20);
#endif
    }
}

int PokeathlonCourse_GetNpcTrainerPicFileId(int spriteId) {
    int i;

    for (i = 0; i < 9999; i++) {
        // BUG: the index is compared with the end marker instead of the entry, so a sprite missing from the
        // table reads on past its end.
        if (i == 0xFFFF) {
            return 0;
        }
        if (spriteId == sPokeathlonNpcTrainerPics[i].spriteId) {
            return sPokeathlonNpcTrainerPics[i].fileId;
        }
    }
    GF_ASSERT(FALSE);
    return 0;
}
