/* The native SDK supplies main() and calls the game's NitroMain(). */
#include <stdint.h>
#include <nitro/types.h>
_Static_assert(sizeof(void *) == 8 && sizeof(uint32_t) == 4, "native game ABI");
_Static_assert(sizeof(u32) == 4 && sizeof(s32) == 4, "native SDK integers must remain 32-bit");
