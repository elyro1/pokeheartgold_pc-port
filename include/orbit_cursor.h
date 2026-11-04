#ifndef POKEHEARTGOLD_ORBIT_CURSOR_H
#define POKEHEARTGOLD_ORBIT_CURSOR_H

#include "global.h"

#include "filesystem.h"
#include "heap.h"
#include "palette.h"
#include "sprite_system.h"
#include "sys_task.h"

typedef struct OrbitCursorArgs {
    int count;
    enum HeapID heapId;
    NNS_G2D_VRAM_TYPE vram;
    int fixedPalette;
    fx32 yOffset;
} OrbitCursorArgs;

#define ORBIT_CURSOR_NO_LEAD_SPRITE 0xFF

typedef struct OrbitCursorSprite {
    ManagedSprite *sprite;
    s16 x;
    s16 y;
    int orbitPhase;
    int angle;
} OrbitCursorSprite;

typedef struct OrbitCursor {
    NARC *narc;
    SpriteSystem *spriteSys;
    SpriteManager *spriteMan;
    PaletteData *plttData;
    int count;
    enum HeapID heapId;
    NNS_G2D_VRAM_TYPE vram;
    int fixedPalette;
    fx32 yOffset;
    OrbitCursorSprite sprites[12];
    int state;
    int leadSprite; // ORBIT_CURSOR_NO_LEAD_SPRITE until a sprite starts orbiting
    int orbitingCount;
    BOOL active;
    SysTask *task;
    int unk_F8;
    int radiusX;
    int radiusY;
    int unk_104;
} OrbitCursor;

OrbitCursor *OrbitCursor_New(OrbitCursorArgs args, SpriteSystem *spriteSys, SpriteManager *spriteMan, PaletteData *plttData);
void OrbitCursor_ReloadPalette(OrbitCursor *cursor, int palette);
void OrbitCursor_LoadResources(OrbitCursor *cursor, int palette);
void OrbitCursor_CreateSprites(OrbitCursor *cursor);
void OrbitCursor_DeleteSprites(OrbitCursor *cursor);
BOOL OrbitCursor_Start(OrbitCursor *cursor, int palette);
BOOL OrbitCursor_SetActive(OrbitCursor *cursor, BOOL active);
BOOL OrbitCursor_SetTarget(OrbitCursor *cursor, int x, int y);
BOOL OrbitCursor_MoveTarget(OrbitCursor *cursor, int x, int y);
BOOL OrbitCursor_IsNewTarget(OrbitCursor *cursor, int x, int y);
BOOL OrbitCursor_SetPosition(OrbitCursor *cursor, s16 x, s16 y);
BOOL OrbitCursor_Delete(OrbitCursor *cursor);
BOOL OrbitCursor_SetVisible(OrbitCursor *cursor, BOOL visible);
void OrbitCursor_SetRadius(OrbitCursor *cursor, int radiusX, int radiusY);
void OrbitCursor_SetAltAnim(OrbitCursor *cursor);
void OrbitCursor_SetDefaultAnim(OrbitCursor *cursor);
void OrbitCursor_Task(SysTask *task, void *data);

extern const int gOrbitCursorPaletteMembers[];

#if defined(SDK_PORT)
#else
typedef char AssertTouchSpriteSize[(sizeof(OrbitCursorSprite) == 0x10) ? 1 : -1];
typedef char AssertTouchMotionSize[(sizeof(OrbitCursor) == 0x108) ? 1 : -1];
typedef char AssertTouchSprites[(offsetof(OrbitCursor, sprites) == 0x24) ? 1 : -1];
typedef char AssertTouchState[(offsetof(OrbitCursor, state) == 0xE4) ? 1 : -1];
typedef char AssertTouchTask[(offsetof(OrbitCursor, task) == 0xF4) ? 1 : -1];
#endif

#endif
