#ifndef HG_PC_MSL_H
#define HG_PC_MSL_H

/* Use the host runtime, without putting the DS SDK/MSL tree on the include path. */
#include <stdlib.h>
#include <stdint.h>

/* Spelling used by the original NitroSystem character resource header. */
#define mapingType mappingType

/* The original MSL umbrella also pulled in cw/function_target.h. */
#define ARM_FUNC
#define THUMB_FUNC
#define ENUMS_ALWAYS_INT_ON
#define ENUMS_ALWAYS_INT_OFF
#define ENUMS_ALWAYS_INT_RESET
#define ALIGN(num) __attribute__((aligned(num)))

#define OS_IE_VBLANK OS_IE_V_BLANK
#define OS_IE_HBLANK OS_IE_H_BLANK
#define GX_PLANEMASK_ALL (GX_PLANEMASK_BG0 | GX_PLANEMASK_BG1 | GX_PLANEMASK_BG2 | GX_PLANEMASK_BG3 | GX_PLANEMASK_OBJ)
#define CARD_BACKUP_TYPE_FLASH_4MBITS_EX ((CARDBackupType)(CARD_BACKUP_TYPE_FLASH_4MBITS | (255u << CARD_BACKUP_TYPE_VENDER_SHIFT)))

#endif
