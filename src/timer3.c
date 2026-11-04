#include "global.h"

#include "timer_3.h"
#if defined(SDK_PORT)
#include <SDL2/SDL_timer.h>
static u64 sTimer3Start;
#endif

vu64 gTimer3Overflows;
int gTimer3NeedsReset;

#if defined(SDK_PORT)
#else
static void Timer3_IrqHandler(void);
#endif

void Init_Timer3(void) {
    gTimer3Overflows = 0;
    gTimer3NeedsReset = 0;
#if defined(SDK_PORT)
    sTimer3Start = SDL_GetPerformanceCounter();
#else
    OS_SetTimerControl(OS_TIMER_3, 0);
    OS_SetTimerCount(OS_TIMER_3, 0);
    OS_SetTimerControl(OS_TIMER_3, (OS_TIMER_PRESCALER_64 << REG_OS_TM3CNT_H_PS_SHIFT) | (1 << REG_OS_TM3CNT_H_I_SHIFT) | (1 << REG_OS_TM3CNT_H_E_SHIFT));
    OS_SetIrqFunction(OS_IE_TIMER3, Timer3_IrqHandler);
    OS_EnableIrqMask(OS_IE_TIMER3);
#endif
}

#if defined(SDK_PORT)

u64 Timer3_GetTicks(void) {
    u64 now = SDL_GetPerformanceCounter();
#else
static void Timer3_IrqHandler(void) {
    gTimer3Overflows++;
#endif
    if (gTimer3NeedsReset) {
#if defined(SDK_PORT)
        sTimer3Start = now;
#else
        OS_SetTimerControl(OS_TIMER_3, 0);
        OS_SetTimerCount(OS_TIMER_3, 0);
        OS_SetTimerControl(OS_TIMER_3, (OS_TIMER_PRESCALER_64 << REG_OS_TM3CNT_H_PS_SHIFT) | (1 << REG_OS_TM3CNT_H_I_SHIFT) | (1 << REG_OS_TM3CNT_H_E_SHIFT));
#endif
        gTimer3NeedsReset = 0;
    }
#if defined(SDK_PORT)
    u64 frequency = SDL_GetPerformanceFrequency();
    u64 elapsed = now - sTimer3Start;
    return ((elapsed / frequency) * OS_SYSTEM_CLOCK +
            (elapsed % frequency) * OS_SYSTEM_CLOCK / frequency) / 64;
#else
    OS_SetIrqCheckFlag(OS_IE_TIMER3);
    OS_SetIrqFunction(OS_IE_TIMER3, Timer3_IrqHandler);
}

u64 Timer3_GetTicks(void) {
    vu16 count_lo;
    vu64 count_hi;
    OSIntrMode bak_psr;

    bak_psr = OS_DisableInterrupts();
    count_lo = *(REGType16 *)((u32)(REG_TM0CNT_L_ADDR + 4 * OS_TIMER_3));
    count_hi = gTimer3Overflows & 0x0000FFFFFFFFFFFFull;
    if ((OS_GetRequestIrqMask() & OS_IE_TIMER3) && !(count_lo & 0x8000)) {
        count_hi++;
    }
    OS_RestoreInterrupts(bak_psr);

    return (count_hi << 16) | count_lo;
#endif
}

u64 GetTimer3Ticks(void) {
    return Timer3_GetTicks();
}

u64 Timer3TicksToSeconds(u64 ticks) {
    return (ticks * 64) / OS_SYSTEM_CLOCK;
}
