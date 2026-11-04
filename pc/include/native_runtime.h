#ifndef HG_PC_NATIVE_RUNTIME_H
#define HG_PC_NATIVE_RUNTIME_H
/* Native IRQ callbacks run at the simulator's presentation boundary. Field
 * initialization can queue a complete batch of model/texture uploads before
 * that boundary; DS hardware services VBlank while the same code is loading. */
#define HG_PC_VBLANK_TASK_CAPACITY 256
#endif
