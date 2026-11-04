#include "autosave.h"
#include "port/sim_config_prj.h"

#include "constants/field/map_load.h"
#include "constants/player_avatar.h"
#include "field_system.h"
#include "frontier_field_support.h"
#include "overlay_01.h"
#include "player_avatar.h"
#include "screen_fade.h"
#include "sys_flags.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

extern const char *CARD_PortSavePath;

#ifdef _WIN32
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#define WIN32_LEAN_AND_MEAN
// Only kernel file APIs are needed; WinUser's ScrollWindow conflicts with the game.
#define NOUSER
#include <windows.h>
#endif

#define AUTOSAVE_LIMIT 10

static struct timespec lastAttempt;

void PC_AutosaveReset(void) {
    clock_gettime(CLOCK_MONOTONIC, &lastAttempt);
}

static BOOL Autosave_IsSafe(FieldSystem *fieldSystem) {
    if (!fieldSystem->processManager->parent || fieldSystem->processManager->child
        || fieldSystem->processManager->isDone
        || !FieldSystem_IsPlayerMovementAllowed(fieldSystem)
        || fieldSystem->mapLoadType != MAP_LOAD_TYPE_OVERWORLD
        || fieldSystem->textbox_open || fieldSystem->startMenuState
        || fieldSystem->bugContest || !fieldSystem->mapSystems
        || !fieldSystem->mapObjectManager || !fieldSystem->playerAvatar
        || !IsPaletteFadeFinished()) {
        return FALSE;
    }
    SaveVarsFlags *flags = Save_VarsFlags_Get(fieldSystem->saveData);
    if (Save_VarsFlags_CheckSafariSysFlag(flags)
        || Save_VarsFlags_CheckBugContestFlag(flags)
        || Save_VarsFlags_CheckPalParkSysFlag(flags)
        || FieldSystem_MapIsBattleTowerMultiPartnerSelectRoom(fieldSystem)) {
        return FALSE;
    }
    // Wait for a tile boundary, including when walking continuously.
    u32 state = PlayerAvatar_GetPlayerMoveState(fieldSystem->playerAvatar);
    return (state == PLAYER_MOVE_STATE_NONE || state == PLAYER_MOVE_STATE_END)
        && !PlayerAvatar_CheckForcedMovement(fieldSystem->playerAvatar);
}

static BOOL Autosave_Sync(FILE *file)
{
#ifdef _WIN32
    return _commit(_fileno(file)) == 0;
#else
    return fsync(fileno(file)) == 0;
#endif
}

static BOOL Autosave_Replace(const char *from, const char *to)
{
#ifdef _WIN32
    return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(from, to) == 0;
#endif
}

static BOOL Autosave_Copy(FILE *source, FILE *target)
{
    unsigned char buffer[4096];
    size_t size;
    while ((size = fread(buffer, 1, sizeof(buffer), source)) != 0) {
        if (fwrite(buffer, 1, size, target) != size) return FALSE;
    }
    return !ferror(source);
}

// Only our complete archive filenames participate in retention. Other slots,
// temporary files and unrelated files in autosave/ are left alone.
static BOOL Autosave_ParseName(const char *name, const char *base, char *stamp, unsigned *serial)
{
    size_t prefix = strlen(base);
    if (strncmp(name, base, prefix) != 0 || name[prefix] != '.') return FALSE;
    unsigned year, month, day, hour, minute, second;
    int end = 0;
    const char *suffix = name + prefix + 1;
    if (sscanf(suffix, "%4u-%2u-%2u_%2u-%2u-%2u-%u.bak%n",
            &year, &month, &day, &hour, &minute, &second, serial, &end) != 7
        || end == 0 || suffix[end] != '\0' || end < 25) {
        return FALSE;
    }
    memcpy(stamp, suffix, 19);
    stamp[19] = '\0';
    return TRUE;
}

static BOOL Autosave_Archive(const char *temporary, const char *directory, const char *base, char *backup, size_t capacity)
{
    BOOL ok = FALSE;
    BOOL reserved = FALSE;
    time_t now = time(NULL);
    struct tm local;
    char stamp[32];
#ifdef _WIN32
    if (localtime_s(&local, &now) != 0
#else
    if (!localtime_r(&now, &local)
#endif
        || !strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", &local)) goto cleanup;

    DIR *dir = opendir(directory);
    if (!dir) goto cleanup;
    // Continue after the largest existing suffix, even after retention removed
    // an earlier suffix or the process restarted within the same second.
    unsigned next = 0;
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        char existingStamp[20];
        unsigned serial;
        if (Autosave_ParseName(entry->d_name, base, existingStamp, &serial)
            && strcmp(stamp, existingStamp) == 0 && serial >= next) {
            if (serial >= 9999) {
                closedir(dir);
                goto cleanup;
            }
            next = serial + 1;
        }
    }
    closedir(dir);

    for (unsigned i = next; i < 10000; i++) {
        int length = snprintf(backup, capacity, "%s/%s.%s-%04u.bak", directory, base, stamp, i);
        if (length < 0 || (size_t)length >= capacity) break;
        FILE *target = fopen(backup, "wbx");
        if (!target) {
            if (errno == EEXIST) continue;
            break;
        }
        reserved = TRUE;
        // Publish the completed snapshot atomically, replacing only our own
        // reserved archive name, never the normal slot file.
        if (fclose(target) == 0) ok = Autosave_Replace(temporary, backup);
        break;
    }
cleanup:
    if (!ok && backup[0]) {
        if (reserved) remove(backup);
        backup[0] = '\0';
    }
    return ok;
}

static BOOL Autosave_Prune(const char *directory, const char *base, size_t capacity)
{
    char *oldest = malloc(capacity);
    char *candidate = malloc(capacity);
    BOOL ok = FALSE;
    if (!oldest || !candidate) goto cleanup;
    for (;;) {
        DIR *dir = opendir(directory);
        if (!dir) goto cleanup;
        unsigned count = 0, oldestSerial = 0;
        time_t oldestTime = 0;
        char oldestStamp[20] = "";
        struct dirent *entry;
        while ((entry = readdir(dir))) {
            char stamp[20];
            unsigned serial;
            if (!Autosave_ParseName(entry->d_name, base, stamp, &serial)) continue;
            int length = snprintf(candidate, capacity, "%s/%s", directory, entry->d_name);
            struct stat info;
            if (length < 0 || (size_t)length >= capacity || stat(candidate, &info) != 0
                || !S_ISREG(info.st_mode)) continue;
            // Order by archive write time, then filename and numeric suffix
            // for deterministic ordering within the same second.
            int order = strcmp(stamp, oldestStamp);
            if (count == 0 || info.st_mtime < oldestTime
                || (info.st_mtime == oldestTime && (order < 0 || (order == 0 && serial < oldestSerial)))) {
                strcpy(oldest, candidate);
                oldestTime = info.st_mtime;
                strcpy(oldestStamp, stamp);
                oldestSerial = serial;
            }
            count++;
        }
        closedir(dir);
        if (count <= AUTOSAVE_LIMIT) {
            ok = TRUE;
            break;
        }
        if (remove(oldest) != 0) break;
    }
cleanup:
    free(candidate);
    free(oldest);
    return ok;
}

static BOOL Autosave_Write(FieldSystem *fieldSystem)
{
    const char *path = CARD_PortSavePath;
    const char *base = strrchr(path, '/');
#ifdef _WIN32
    const char *backslash = strrchr(path, '\\');
    if (backslash && (!base || backslash > base)) base = backslash;
#endif
    base = base ? base + 1 : path;
    size_t capacity = strlen(path) + 100;
    char *directory = malloc(capacity);
    char *temporary = malloc(capacity);
    char *backup = calloc(capacity, 1);
    SaveData *snapshot = malloc(sizeof(*snapshot));
    BOOL ok = FALSE;
    BOOL saved = FALSE;
    if (!directory || !temporary || !backup || !snapshot) goto cleanup;
    memcpy(snapshot, fieldSystem->saveData, sizeof(*snapshot));
    snprintf(directory, capacity, "%.*sautosave", (int)(base - path), path);
#ifdef _WIN32
    int result = _mkdir(directory);
#else
    int result = mkdir(directory, 0700);
#endif
    if (result != 0 && errno != EEXIST) goto cleanup;

    snprintf(temporary, capacity, "%s/%s.autosave-XXXXXX", directory, base);
    int fd = mkstemp(temporary);
    if (fd < 0) goto cleanup;
#ifdef _WIN32
    _setmode(fd, _O_BINARY);
#endif
    FILE *target = fdopen(fd, "wb");
    if (!target) {
        close(fd);
        goto remove_temporary;
    }
    FILE *source = fopen(path, "rb");
    ok = source != NULL || errno == ENOENT;
    if (source) {
        ok = Autosave_Copy(source, target);
        if (fclose(source) != 0) ok = FALSE;
    }
    if (fclose(target) != 0) ok = FALSE;
    if (!ok) goto remove_temporary;

    // The normal save updates objects, location and save counters.
    // Restore live data afterwards so its counters, block offsets and dirty
    // state continue to describe the normal slot, even after a successful autosave.
    CARD_PortSavePath = temporary;
    saved = TRUE;
    ok = Field_SaveGameNormal(fieldSystem);
    CARD_PortSavePath = path;
    if (ok) {
        target = fopen(temporary, "rb+");
        ok = target != NULL;
        if (target) {
            if (!Autosave_Sync(target)) ok = FALSE;
            if (fclose(target) != 0) ok = FALSE;
        }
    }
    if (ok) ok = Autosave_Archive(temporary, directory, base, backup, capacity);
    if (ok) {
        fprintf(stderr, "[autosave] Saved %s\n", backup);
        if (!Autosave_Prune(directory, base, capacity)) {
            fprintf(stderr, "[autosave] Could not remove oldest archives for %s\n", path);
        }
    }

remove_temporary:
    if (saved) memcpy(fieldSystem->saveData, snapshot, sizeof(*snapshot));
    remove(temporary);
cleanup:
    free(snapshot);
    free(backup);
    free(temporary);
    free(directory);
    return ok;
}

void PC_AutosaveUpdate(FieldSystem *fieldSystem)
{
    if (!SIM_Config_prj_GetConfig()->autosave) return;
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return;
    double elapsed = (double)(now.tv_sec - lastAttempt.tv_sec)
        + (double)(now.tv_nsec - lastAttempt.tv_nsec) / 1000000000.0;
    if (elapsed < 120.0 || !Autosave_IsSafe(fieldSystem)) return;
    lastAttempt = now;
    if (!Autosave_Write(fieldSystem)) {
        fprintf(stderr, "[autosave] Failed; previous save preserved\n");
    }
}
