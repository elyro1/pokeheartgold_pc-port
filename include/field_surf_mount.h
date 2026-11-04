#ifndef POKEHEARTGOLD_FIELD_SURF_MOUNT_H
#define POKEHEARTGOLD_FIELD_SURF_MOUNT_H

#if defined(SDK_PORT)
#include "global.h"
#include "field_types_def.h"
#else
#include "map_object_manager.h"
#endif

struct FieldEffect;

struct FieldEffect *SurfMountEffect_Create(LocalMapObject *obj, u32 x, u32 y, u32 dir, BOOL bobbing);

#endif // POKEHEARTGOLD_FIELD_SURF_MOUNT_H
