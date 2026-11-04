#include "save_fashion.h"

#include "global.h"

#include "constants/save_arrays.h"

#include "application/dress_up/dress_up_internal.h"

#include "fashion_case.h"
#include "graphics_object_manager.h"
#include "heap.h"
#include "pm_string.h"
#include "pm_version.h"
#include "pokemon.h"
#include "save.h"

// Fashion save data (save array 0xC): 11 dressed-up Pokemon entries, 5 larger entries and the fashion case
// (accessory counts, backdrop order).

#define FASHION_ENTRY_MAGIC_EMPTY 0x1234
#define FASHION_ENTRY_MAGIC_SAVED 0x2345

struct SaveFashionMon {
    u32 personality;
    u32 otId;
    u16 species;
    u16 nickname[11];
    u16 secondText[8];
    s8 z;
    u8 x;
    u8 y;
    u8 form;
    u8 secondTextGender; // gender of the trainer named by secondText
};

// One dressed-up Pokemon entry.
typedef struct FashionEntry {
    u32 magic;
    SaveFashionMon mon;
    u32 mask; // bit n: items[n] is placed
    MailMessage mailMessage;
    u8 items[FASHION_ENTRY_ITEM_MAX][4]; // accessory id, x, y, z
    u8 backdrop;
    u8 language;
    u8 padding_72[2];
} FashionEntry;

// One of the larger entries; only the magic is accessed here.
typedef struct FashionEntryLarge {
    u32 magic;
    u8 data[0x94];
} FashionEntryLarge;

typedef struct FashionItemCoords {
    u16 x;
    u16 y;
    u16 xCopy;
    u16 yCopy;
} FashionItemCoords;

static BOOL IsValidFashionMagic(u32 value);
static void GetFashionObjectPlacement(DressUpMonPic *monPic, u8 *outX, u8 *outY, u8 *outZ);
static void FashionMon_Init(SaveFashionMon *dst, Pokemon *mon, u8 x, u8 y, s8 z);
static void FashionMon_InitAtObject(SaveFashionMon *dst, Pokemon *mon, DressUpMonPic *monPic);
static void FashionMon_SetSecondText(SaveFashionMon *dst, String *str, u32 gender);
static void FashionMon_CreatePokemon(SaveFashionMon *src, Pokemon *mon);
static void SetFashionItemPlacement(u8 *dst, u8 accessoryId, u8 x, u8 y, u8 z);
static void SetStackableAccessoryCount(u32 *data, int value, u8 index);
static u8 GetStackableAccessoryCount(u32 *data, u8 index);
static void SetUniqueAccessoryFlag(u32 *data, u8 value, u8 index);
static u8 GetUniqueAccessoryFlag(u32 *data, u8 index);
static void SetWallpaperOrder(u32 *data, u32 value, u8 index);
static u8 GetWallpaperOrder(u32 *data, u8 index);
static u8 CountOwnedWallpapers(u32 *data);
static BOOL IsStackableAccessory(u32 value);
static void FashionCase_Init(FashionCase *fashionCase);
static void FashionEntry_Copy(SaveFashionDataSub *dst_, SaveFashionDataSub *src_);
static BOOL FashionEntryLarge_IsSaved(SaveFashionDataSub3FC *entry_);
static void FashionMon_GetSecondText(SaveFashionMon *mon, String *string);
static BOOL Save_FashionData_IsNewEntry(SaveFashionData *fashionData, SaveFashionDataSub *candidate);

#if defined(SDK_PORT)

#else
void MATHi_CRC32InitTableRev(u32 *table, u32 polynomial);
u32 MATH_CalcCRC32(const u32 *table, const void *data, u32 size);
#endif

static BOOL IsValidFashionMagic(u32 value) {
    if (value == FASHION_ENTRY_MAGIC_EMPTY || value == FASHION_ENTRY_MAGIC_SAVED) {
        return TRUE;
    }
    return FALSE;
}

static void GetFashionObjectPlacement(DressUpMonPic *monPic, u8 *outX, u8 *outY, u8 *outZ) {
    int x;
    int y;
    int z;
    DressUpMon_GetCenterPos(monPic, &x, &y);
    z = DressUpMon_GetPriority(monPic);
    GF_ASSERT(x < 256);
    GF_ASSERT(y < 256);
    GF_ASSERT(z > -128);
    *outX = x;
    *outY = y;
    *outZ = z;
}

static void FashionMon_Init(SaveFashionMon *dst, Pokemon *mon, u8 x, u8 y, s8 z) {
    dst->species = GetMonData(mon, MON_DATA_SPECIES, NULL);
    GetMonData(mon, MON_DATA_NICKNAME, dst->nickname);
    dst->personality = GetMonData(mon, MON_DATA_PERSONALITY, NULL);
    dst->otId = GetMonData(mon, MON_DATA_OT_ID, NULL);
    dst->form = GetMonData(mon, MON_DATA_FORM, NULL);
    dst->x = x;
    dst->y = y;
    dst->z = z;
}

static void FashionMon_InitAtObject(SaveFashionMon *dst, Pokemon *mon, DressUpMonPic *monPic) {
    u8 x, y, z;
    GetFashionObjectPlacement(monPic, &x, &y, &z);
    FashionMon_Init(dst, mon, x, y, z);
}

static void FashionMon_SetSecondText(SaveFashionMon *dst, String *str, u32 gender) {
    CopyStringToU16Array(str, dst->secondText, 8);
    dst->secondTextGender = gender;
}

static void FashionMon_CreatePokemon(SaveFashionMon *src, Pokemon *mon) {
    CreateMon(mon, src->species, 0, 0, TRUE, src->personality, TRUE, src->otId);
    SetMonData(mon, MON_DATA_NICKNAME, src->nickname);
    SetMonData(mon, MON_DATA_FORM, &src->form);
}

static void SetFashionItemPlacement(u8 *dst, u8 accessoryId, u8 x, u8 y, u8 z) {
    dst[0] = accessoryId;
    dst[1] = x;
    dst[2] = y;
    dst[3] = z;
}

static void SetStackableAccessoryCount(u32 *data, int value, u8 index) {
    u8 shift;
    u8 word;
    GF_ASSERT(index < FASHION_STACKABLE_ACCESSORY_COUNT);
    shift = index % 8;
    shift *= 4;
    word = index / 8;
    data[word] &= ~(15 << shift);
    data[word] |= value << shift;
}

static u8 GetStackableAccessoryCount(u32 *data, u8 index) {
    u8 word;
    u8 shift;
    u8 value;
    GF_ASSERT(index < FASHION_STACKABLE_ACCESSORY_COUNT);
    word = index / 8;
    shift = index % 8;
    shift *= 4;
    value = (data[word] >> shift) & 15;
    if (value > FASHION_STACKABLE_ACCESSORY_MAX) {
        value = FASHION_STACKABLE_ACCESSORY_MAX;
    }
    return value;
}

static void SetUniqueAccessoryFlag(u32 *data, u8 value, u8 index) {
    u8 shift;
    u8 word;
    GF_ASSERT(value < 2);
    shift = index % 32;
    word = index / 32;
    data[word] &= ~(1 << shift);
    data[word] |= value << shift;
}

static u8 GetUniqueAccessoryFlag(u32 *data, u8 index) {
    u8 word = index / 32;
    u8 shift = index % 32;
    return (data[word] >> shift) & 1;
}

static void SetWallpaperOrder(u32 *data, u32 value, u8 index) {
    u8 shift;
    u8 word;
    GF_ASSERT(value <= FASHION_WALLPAPER_COUNT);
    shift = index % 4;
    shift *= 8;
    word = index / 4;
    data[word] &= ~(0xFF << shift);
    data[word] |= value << shift;
}

static u8 GetWallpaperOrder(u32 *data, u8 index) {
    u8 word = index / 4;
    u8 shift = index % 4;
    shift *= 8;
    return data[word] >> shift;
}

static u8 CountOwnedWallpapers(u32 *data) {
    int i = 0;
    int count = i;
    for (i = 0; i < FASHION_WALLPAPER_COUNT; i++) {
        if (GetWallpaperOrder(data, i) != FASHION_WALLPAPER_NOT_OWNED) {
            count++;
        }
    }
    return count;
}

static BOOL IsStackableAccessory(u32 value) {
    if (value < FASHION_STACKABLE_ACCESSORY_COUNT) {
        return TRUE;
    }
    return FALSE;
}

static void FashionCase_Init(FashionCase *fashionCase) {
    int i;
    memset(fashionCase, 0, sizeof(FashionCase));
    for (i = 0; i < FASHION_WALLPAPER_COUNT; i++) {
        SetWallpaperOrder(&fashionCase->wallpaperOrder[0], FASHION_WALLPAPER_NOT_OWNED, i);
    }
}

void Save_FashionData_Init(SaveFashionData *fashionData) {
    int i;
    for (i = 0; i < FASHION_ENTRY_COUNT; i++) {
        memset(&fashionData->entries[i], 0, sizeof(SaveFashionDataSub));
        ((FashionEntry *)&fashionData->entries[i])->magic = FASHION_ENTRY_MAGIC_EMPTY;
    }
    {
        int j = 0;
        u8 *clear = (u8 *)fashionData->largeEntries;
        u8 *set = (u8 *)fashionData;
        u32 magic = FASHION_ENTRY_MAGIC_EMPTY;
        for (; j < FASHION_LARGE_ENTRY_COUNT; j++) {
            memset(clear, 0, sizeof(SaveFashionDataSub3FC));
            ((FashionEntryLarge *)(set + offsetof(SaveFashionData, largeEntries)))->magic = magic;
            clear += sizeof(SaveFashionDataSub3FC);
            set += sizeof(SaveFashionDataSub3FC);
        }
    }
    FashionCase_Init(&fashionData->fashionCase);
}

u32 Save_FashionData_sizeof(void) {
    return sizeof(SaveFashionData);
}

u32 FashionEntry_sizeof(void) {
    return sizeof(SaveFashionDataSub);
}

SaveFashionDataSub *FashionEntry_New(enum HeapID heapId) {
    SaveFashionDataSub *entry = Heap_Alloc(heapId, sizeof(SaveFashionDataSub));
    memset(entry, 0, sizeof(SaveFashionDataSub));
    ((FashionEntry *)entry)->magic = FASHION_ENTRY_MAGIC_EMPTY;
    return entry;
}

SaveFashionDataSub *Save_FashionData_GetEntry(SaveFashionData *fashionData, int index) {
    GF_ASSERT(index < FASHION_ENTRY_COUNT);
    GF_ASSERT(IsValidFashionMagic(((FashionEntry *)&fashionData->entries[index])->magic));
    return &fashionData->entries[index];
}

FashionCase *Save_FashionData_GetFashionCase(SaveFashionData *fashionData) {
    return &fashionData->fashionCase;
}

BOOL Save_FashionData_IsEntrySaved(SaveFashionData *fashionData, int index) {
    GF_ASSERT(index < FASHION_ENTRY_COUNT);
    return FashionEntry_IsSaved(&fashionData->entries[index]);
}

BOOL Save_FashionData_IsLargeEntrySaved(SaveFashionData *fashionData, int index) {
    GF_ASSERT(index < FASHION_LARGE_ENTRY_COUNT);
    return FashionEntryLarge_IsSaved(&fashionData->largeEntries[index]);
}

int FashionCase_CanGiveAccessory(FashionCase *fashionCase, int id, int quantity) {
    BOOL result = TRUE;
    u32 current = FashionCase_GetAccessoryCount(fashionCase, id);
    if (IsStackableAccessory(id)) {
        if (current + quantity > FASHION_STACKABLE_ACCESSORY_MAX) {
            result = FALSE;
        }
    } else if (current + quantity > 1) {
        result = FALSE;
    }
    return result;
}

BOOL FashionCase_HasWallpaper(FashionCase *fashionCase, int index) {
    if (FashionCase_GetWallpaperOrder(fashionCase, index) != FASHION_WALLPAPER_NOT_OWNED) {
        return TRUE;
    }
    return FALSE;
}

u32 FashionCase_GetAccessoryCount(FashionCase *fashionCase, int id) {
    GF_ASSERT((u32)id < FASHION_ACCESSORY_COUNT);
    if (IsStackableAccessory(id)) {
        return GetStackableAccessoryCount(fashionCase->accessories, id);
    }
    GF_ASSERT((u32)id >= FASHION_STACKABLE_ACCESSORY_COUNT);
    id = (u8)(id - FASHION_STACKABLE_ACCESSORY_COUNT);
    return GetUniqueAccessoryFlag(&fashionCase->accessories[8], id);
}

int FashionCase_GetWallpaperOrder(FashionCase *fashionCase, int index) {
    GF_ASSERT((u32)index < FASHION_WALLPAPER_COUNT);
    return GetWallpaperOrder(&fashionCase->wallpaperOrder[0], index);
}

u32 FashionCase_CountAccessories(FashionCase *fashionCase) {
    int i = 0;
    u32 count = i;
    for (i = 0; i < FASHION_ACCESSORY_COUNT; i++) {
        count += FashionCase_GetAccessoryCount(fashionCase, i);
    }
    return count;
}

u32 FashionCase_CountWallpapers(FashionCase *fashionCase) {
    int i = 0;
    u32 count = i;
    for (i = 0; i < FASHION_WALLPAPER_COUNT; i++) {
        if (FashionCase_GetWallpaperOrder(fashionCase, i) != FASHION_WALLPAPER_NOT_OWNED) {
            count++;
        }
    }
    return count;
}

void FashionCase_GiveFashionItem(FashionCase *fashionCase, int id, int quantity) {
    GF_ASSERT((u32)id < FASHION_ACCESSORY_COUNT);
    if (IsStackableAccessory(id)) {
        u8 count = GetStackableAccessoryCount(fashionCase->accessories, id);
        count += quantity;
        if (count > FASHION_STACKABLE_ACCESSORY_MAX) {
            count = FASHION_STACKABLE_ACCESSORY_MAX;
        }
        SetStackableAccessoryCount(fashionCase->accessories, count, id);
    } else {
        u8 count = GetUniqueAccessoryFlag(&fashionCase->accessories[8], id);
        count += quantity;
        if (count > 1) {
            count = 1;
        }
        GF_ASSERT((u32)id >= FASHION_STACKABLE_ACCESSORY_COUNT);
        id = (u8)(id - FASHION_STACKABLE_ACCESSORY_COUNT);
        SetUniqueAccessoryFlag(&fashionCase->accessories[8], count, id);
    }
}

void FashionCase_TakeAccessory(FashionCase *fashionCase, int id, int quantity) {
    GF_ASSERT((u32)id < FASHION_ACCESSORY_COUNT);
    if (IsStackableAccessory(id)) {
        u8 count = GetStackableAccessoryCount(fashionCase->accessories, id);
        if (count > (u32)quantity) {
            count -= quantity;
        } else {
            count = 0;
        }
        SetStackableAccessoryCount(fashionCase->accessories, count, id);
    } else {
        GF_ASSERT((u32)id >= FASHION_STACKABLE_ACCESSORY_COUNT);
        id = (u8)(id - FASHION_STACKABLE_ACCESSORY_COUNT);
        SetUniqueAccessoryFlag(&fashionCase->accessories[8], 0, id);
    }
}

void FashionCase_GiveContestBackground(FashionCase *fashionCase, int id) {
    GF_ASSERT((u32)id < FASHION_WALLPAPER_COUNT);
    if (GetWallpaperOrder(&fashionCase->wallpaperOrder[0], id) == FASHION_WALLPAPER_NOT_OWNED) {
        u8 count = CountOwnedWallpapers(&fashionCase->wallpaperOrder[0]);
        SetWallpaperOrder(&fashionCase->wallpaperOrder[0], count, id);
    }
}

BOOL FashionEntry_IsSaved(SaveFashionDataSub *entry_) {
    FashionEntry *entry = (FashionEntry *)entry_;
    GF_ASSERT(IsValidFashionMagic(entry->magic));
    return entry->magic == FASHION_ENTRY_MAGIC_SAVED;
}

void FashionEntry_MarkSaved(SaveFashionDataSub *entry_) {
    FashionEntry *entry = (FashionEntry *)entry_;
    GF_ASSERT(IsValidFashionMagic(entry->magic));
    entry->magic = FASHION_ENTRY_MAGIC_SAVED;
    entry->language = gGameLanguage;
}

void FashionEntry_Clear(SaveFashionDataSub *entry_) {
    FashionEntry *entry = (FashionEntry *)entry_;
    GF_ASSERT(IsValidFashionMagic(entry->magic));
    memset(entry, 0, sizeof(FashionEntry));
    entry->magic = FASHION_ENTRY_MAGIC_EMPTY;
}

void FashionEntry_SetMon(SaveFashionDataSub *entry_, Pokemon *mon, DressUpMonPic *monPic) {
    FashionEntry *entry = (FashionEntry *)entry_;
    GF_ASSERT(IsValidFashionMagic(entry->magic));
    FashionMon_InitAtObject(&entry->mon, mon, monPic);
}

void FashionEntry_PlaceItem(SaveFashionDataSub *entry_, DressUpSlot *source, int index) {
    FashionEntry *entry = (FashionEntry *)entry_;
    volatile FashionItemCoords coords;
    u32 packed;
    int z;
    int y;
    u32 mask;

    packed = GraphicsObject_GetPosition(source->object);
    coords.x = packed;
    coords.y = packed >> 16;
    coords.xCopy = coords.x;
    coords.yCopy = coords.y;
    z = GraphicsObject_GetDepth(source->object);
    GF_ASSERT(index < FASHION_ENTRY_ITEM_MAX);
    GF_ASSERT((s16)coords.xCopy < 256);
    y = (s16)coords.yCopy;
    // y < 256
    GF_ASSERT(((volatile int *)&coords)[-1] < 256);
    GF_ASSERT(z > -128);
    mask = 1 << index;
    GF_ASSERT(!(entry->mask & mask));
    GF_ASSERT(IsValidFashionMagic(entry->magic));
    SetFashionItemPlacement(entry->items[index], source->id, (s16)coords.xCopy, y, z);
    entry->mask |= mask;
}

void FashionEntry_SetBackdrop(SaveFashionDataSub *entry_, int value) {
    FashionEntry *entry = (FashionEntry *)entry_;
    GF_ASSERT(IsValidFashionMagic(entry->magic));
    entry->backdrop = value;
}

void FashionEntry_SetMailMessageWord(SaveFashionDataSub *entry_, int value) {
    FashionEntry *entry = (FashionEntry *)entry_;
    GF_ASSERT(IsValidFashionMagic(entry->magic));
    MailMsg_Init(&entry->mailMessage);
    ((void (*)(MailMessage *, int, int))MailMsg_SetFieldI)(&entry->mailMessage, 0, value);
}

static void FashionEntry_Copy(SaveFashionDataSub *dst_, SaveFashionDataSub *src_) {
    FashionEntry *dst = (FashionEntry *)dst_;
    GF_ASSERT(IsValidFashionMagic(dst->magic));
    memcpy(dst, src_, sizeof(FashionEntry));
}

void FashionEntry_SetMonSecondText(SaveFashionDataSub *entry_, String *string, u32 gender) {
    FashionEntry *entry = (FashionEntry *)entry_;
    GF_ASSERT(IsValidFashionMagic(entry->magic));
    FashionMon_SetSecondText(&entry->mon, string, gender);
}

BOOL FashionEntry_IsItemPlaced(SaveFashionDataSub *entry_, int index) {
    FashionEntry *entry = (FashionEntry *)entry_;
    GF_ASSERT(index < FASHION_ENTRY_ITEM_MAX);
    GF_ASSERT(IsValidFashionMagic(entry->magic));
    return entry->mask & (1 << index);
}

SaveFashionMon *FashionEntry_GetMon(SaveFashionDataSub *entry_) {
    FashionEntry *entry = (FashionEntry *)entry_;
    GF_ASSERT(IsValidFashionMagic(entry->magic));
    return &entry->mon;
}

u8 *FashionEntry_GetItem(SaveFashionDataSub *entry_, int index) {
    FashionEntry *entry = (FashionEntry *)entry_;
    GF_ASSERT(index < FASHION_ENTRY_ITEM_MAX);
    GF_ASSERT(entry->mask & (1 << index));
    GF_ASSERT(IsValidFashionMagic(entry->magic));
    return entry->items[index];
}

void FashionEntry_GetMonSecondText(SaveFashionDataSub *entry_, String *string) {
    FashionEntry *entry = (FashionEntry *)entry_;
    GF_ASSERT(IsValidFashionMagic(entry->magic));
    FashionMon_GetSecondText(&entry->mon, string);
}

u8 FashionEntry_GetBackdrop(SaveFashionDataSub *entry_) {
    FashionEntry *entry = (FashionEntry *)entry_;
    GF_ASSERT(IsValidFashionMagic(entry->magic));
    return entry->backdrop;
}

u16 FashionEntry_GetMailMessageWord(SaveFashionDataSub *entry_) {
    FashionEntry *entry = (FashionEntry *)entry_;
    return MailMsg_GetFieldI(&entry->mailMessage, 0);
}

static BOOL FashionEntryLarge_IsSaved(SaveFashionDataSub3FC *entry_) {
    FashionEntryLarge *entry = (FashionEntryLarge *)entry_;
    GF_ASSERT(IsValidFashionMagic(entry->magic));
    return entry->magic == FASHION_ENTRY_MAGIC_SAVED;
}

static void FashionMon_GetSecondText(SaveFashionMon *mon, String *string) {
    CopyU16ArrayToString(string, mon->secondText);
}

s8 FashionMon_GetZ(SaveFashionMon *mon) {
    return mon->z;
}

u8 FashionMon_GetX(SaveFashionMon *mon) {
    return mon->x;
}

u8 FashionMon_GetY(SaveFashionMon *mon) {
    return mon->y;
}

void FashionMon_ToPokemon(SaveFashionMon *mon, Pokemon *pokemon) {
    FashionMon_CreatePokemon(mon, pokemon);
}

u8 FashionItem_GetId(u8 *data) {
    return data[0];
}

u8 FashionItem_GetX(u8 *data) {
    return data[1];
}

u8 FashionItem_GetY(u8 *data) {
    return data[2];
}

s8 FashionItem_GetZ(s8 *data) {
    return data[3];
}

static BOOL Save_FashionData_IsNewEntry(SaveFashionData *fashionData, SaveFashionDataSub *candidate) {
#if defined(SDK_PORT)
    MATHCRC32Table table;
#else
    u32 table[256];
#endif
    BOOL result = TRUE;
    u32 crc;
    int i;
    if (FashionEntry_IsSaved(candidate) == TRUE) {
#if defined(SDK_PORT)
        MATHi_CRC32InitTableRev(&table, MATH_CRC32_STANDARD_POLY);
        crc = MATH_CalcCRC32(&table, candidate, sizeof(SaveFashionDataSub));
#else
        MATHi_CRC32InitTableRev(table, MATH_CRC32_STANDARD_POLY);
        crc = MATH_CalcCRC32(table, candidate, sizeof(SaveFashionDataSub));
#endif
        for (i = 0; i < FASHION_ENTRY_COUNT; i++) {
            SaveFashionDataSub *entry = Save_FashionData_GetEntry(fashionData, i);
            u32 other;
#if defined(SDK_PORT)
            MATHi_CRC32InitTableRev(&table, MATH_CRC32_STANDARD_POLY);
            other = MATH_CalcCRC32(&table, entry, sizeof(SaveFashionDataSub));
#else
            MATHi_CRC32InitTableRev(table, MATH_CRC32_STANDARD_POLY);
            other = MATH_CalcCRC32(table, entry, sizeof(SaveFashionDataSub));
#endif
            if (other == crc) {
                result = FALSE;
                break;
            }
        }
    } else {
        result = FALSE;
    }
    return result;
}

void Save_FashionData_InsertEntries(u8 count, int skip, SaveFashionData *fashionData, SaveFashionDataSub **entries) {
    int accepted = 0;
    int i;
    for (i = 0; i < count; i++) {
        if (i != skip && entries[i] != NULL && Save_FashionData_IsNewEntry(fashionData, entries[i]) == TRUE) {
            accepted++;
        }
    }
    for (i = FASHION_ENTRY_COUNT - 1; i >= 1; i--) {
        if (i + accepted < FASHION_ENTRY_COUNT) {
            FashionEntry_Copy(Save_FashionData_GetEntry(fashionData, i + accepted), Save_FashionData_GetEntry(fashionData, i));
        }
    }
    {
        int dst = 1;
        for (i = 0; i < count; i++) {
            SaveFashionDataSub *entry;
            if (i == skip) {
                continue;
            }
            entry = entries[i];
            if (entry != NULL && Save_FashionData_IsNewEntry(fashionData, entry) == TRUE) {
                FashionEntry_Copy(Save_FashionData_GetEntry(fashionData, dst++), entry);
            }
        }
    }
}

SaveFashionData *Save_FashionData_Get(SaveData *saveData) {
    return SaveArray_Get(saveData, SAVE_DRESSUP_DATA);
}
