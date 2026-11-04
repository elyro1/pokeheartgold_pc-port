#ifndef HG_PC_DWC_COMPAT_H
#define HG_PC_DWC_COMPAT_H
#include <dwc.h>
#include <string.h>
void HG_DWC_PrepareFirmware(void);
static inline DWCError HG_DWC_GetLastErrorEx(s32 *code, void *type) {
    DWCErrorType nativeType;
    DWCError result = DWC_GetLastErrorEx(code, &nativeType);
    s32 gameType = (s32)nativeType;
    if (type) memcpy(type, &gameType, sizeof(gameType));
    return result;
}
#endif
