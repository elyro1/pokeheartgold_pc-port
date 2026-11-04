#ifndef POKEHEARTGOLD_MAP_OBJECT_FOLLOW_H
#define POKEHEARTGOLD_MAP_OBJECT_FOLLOW_H

#include "map_object.h"
#if defined(SDK_PORT)
#include "field_effects.h"
#endif

struct FieldEffect;

void FollowPlayerMovement_Reset(LocalMapObject *object);
LocalMapObject *MapObject_FindTrainerPartner(LocalMapObject *object);
#if defined(SDK_PORT)
struct FieldEffect *HiddenObjectMovement_SetCoverEffect(LocalMapObject *object, struct FieldEffect *effect);
#else
int HiddenObjectMovement_SetCoverEffect(LocalMapObject *object, struct FieldEffect *effect);
#endif

#endif // POKEHEARTGOLD_MAP_OBJECT_FOLLOW_H
