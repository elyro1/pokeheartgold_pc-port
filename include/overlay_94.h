#ifndef POKEHEARTGOLD_OVY_94_H
#define POKEHEARTGOLD_OVY_94_H

#include "party_menu.h"
#if defined(SDK_PORT)
#include "particle_work_size.h"
#endif

#if defined(SDK_PORT)
#define PARTICLE_HEAP_SIZE PARTICLE_WORK_SIZE(0x4800)
#else
#define PARTICLE_HEAP_SIZE (0x4800)
#endif

void PartyMenu_InitIconFormChangeData(PartyMenu *partyMenu);
BOOL PartyMenu_AnimateIconFormChange(PartyMenu *partyMenu);

#endif // POKEHEARTGOLD_OVY_94_H
