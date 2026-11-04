#ifndef POKEHEARTGOLD_PARTICLE_WORK_SIZE_H
#define POKEHEARTGOLD_PARTICLE_WORK_SIZE_H
/* SPL work areas contain native managers, emitter/particle pools and resource
 * pointer tables. Keep the DS budgets unchanged and allow for wider objects
 * and allocator alignment on the host. */
#define PARTICLE_WORK_SIZE(dsBytes) ((dsBytes) * 2u)
#endif
