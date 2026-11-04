#include "port/cheats.h"

#include "constants/charcode.h"
#include "constants/maps.h"
#include "constants/spawns.h"

#include "bag.h"
#include "field_system.h"
#include "field_warp_tasks.h"
#include "map_header.h"
#include "msgdata.h"
#include "party.h"
#include "pm_string.h"
#include "script_pokemon_util.h"
#include "spawn_warp.h"

BOOL PC_Cheats_FieldReady(void) {
    FieldSystem *fs = PC_Cheats_GetFieldSystem();
    return fs && fs->saveData && fs->location && fs->playerAvatar
        && fs->processManager && !fs->processManager->child
        && !fs->processManager->isDone && !fs->textbox_open && !fs->startMenuState
        && !fs->bugContest && FieldSystem_IsPlayerMovementAllowed(fs);
}

BOOL PC_Cheats_GiveMon(int species, int level) {
    if (!PC_Cheats_FieldReady() || species < 1 || species > SPECIES_ARCEUS || level < 1 || level > 100) {
        return FALSE;
    }
    FieldSystem *fs = PC_Cheats_GetFieldSystem();
    if (Party_GetCount(SaveArray_Party_Get(fs->saveData)) >= 6) {
        return FALSE;
    }
    return GiveMon(HEAP_ID_FIELD2, fs->saveData, species, level, 0, 0, ITEM_NONE, MapHeader_GetMapSec(fs->location->mapId), 24);
}

BOOL PC_Cheats_GiveItem(int item, int quantity) {
    if (!PC_Cheats_FieldReady() || item < 1 || item >= ITEMS_COUNT || quantity < 1 || quantity > 999) {
        return FALSE;
    }
    return Bag_AddItem(Save_Bag_Get(PC_Cheats_GetFieldSystem()->saveData), item, quantity, HEAP_ID_FIELD2);
}

BOOL PC_Cheats_Warp(int map, int x, int z) {
    if (!PC_Cheats_FieldReady() || map <= MAP_EVERYWHERE || map >= MAP_ID_MAX || map == MAP_UNUSED || x < 0 || z < 0) {
        return FALSE;
    }
    CallFieldTask_FlyWarp(PC_Cheats_GetFieldSystem(), map, -1, x, z, DIR_SOUTH);
    return TRUE;
}

BOOL PC_Cheats_Fly(int spawn) {
    if (spawn < SPAWN_NEW_BARK || spawn > SPAWN_ROCK_TUNNEL || !PC_Cheats_FieldReady()) {
        return FALSE;
    }
    Location dest;
    GetFlyWarpData(spawn, &dest);
    return PC_Cheats_Warp(dest.mapId, dest.x, dest.y);
}

// The game uses its own character encoding. Keep the English selector labels in UTF-8.
void PC_Cheats_GetName(BOOL item, int id, char *dest, unsigned capacity) {
    if (!capacity) {
        return;
    }
    dest[0] = '\0';
    if (!PC_Cheats_FieldReady() || id < 1 || id >= (item ? ITEMS_COUNT : SPECIES_ARCEUS + 1)) {
        return;
    }
    String *name = item ? String_New(64, HEAP_ID_FIELD2) : GetSpeciesName(id, HEAP_ID_FIELD2);
    if (item) {
        GetItemNameIntoString(name, id, HEAP_ID_FIELD2);
    }
    const u16 *text = String_cstr(name);
    unsigned used = 0;
    for (; *text != EOS; text++) {
        char ascii[2] = { '?', '\0' };
        const char *glyph = ascii;
        if (*text >= CHAR_0 && *text <= CHAR_9) {
            ascii[0] = '0' + *text - CHAR_0;
        } else if (*text >= CHAR_A && *text <= CHAR_Z) {
            ascii[0] = 'A' + *text - CHAR_A;
        } else if (*text >= CHAR_a && *text <= CHAR_z) {
            ascii[0] = 'a' + *text - CHAR_a;
        } else {
            switch (*text) {
            case CHAR_SPACE:
                glyph = " ";
                break;
            case CHAR_PERIOD:
                glyph = ".";
                break;
            case CHAR_HYPHEN:
                glyph = "-";
                break;
            case CHAR_RAPOST:
                glyph = "'";
                break;
            case CHAR_LPAREN:
                glyph = "(";
                break;
            case CHAR_RPAREN:
                glyph = ")";
                break;
            case CHAR_SLASH:
                glyph = "/";
                break;
            case CHAR_EXCL:
                glyph = "!";
                break;
            case CHAR_eacute:
                glyph = "é";
                break;
            case CHAR_MALE:
                glyph = "♂";
                break;
            case CHAR_FEMALE:
                glyph = "♀";
                break;
            default:
                break;
            }
        }
        unsigned length = strlen(glyph);
        if (used + length >= capacity) {
            break;
        }
        memcpy(dest + used, glyph, length);
        used += length;
    }
    dest[used] = '\0';
    String_Delete(name);
}
