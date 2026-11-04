#include "constants/sndseq.h"

#include "comm_state.h"
#include "comm_wireless.h"
#include "global_terminal/global_terminal_app_internal.h"
#include "sound_system.h"
#include "wireless_status_icon.h"

enum GlobalTerminalAppState {
    GLOBAL_TERMINAL_APP_LOAD_WIFI,
    GLOBAL_TERMINAL_APP_WAIT_WIFI,
    GLOBAL_TERMINAL_APP_START_CONNECT,
    GLOBAL_TERMINAL_APP_RUN_CONNECT,
    GLOBAL_TERMINAL_APP_START_VS_RECORDER,
    GLOBAL_TERMINAL_APP_RUN_VS_RECORDER,
    GLOBAL_TERMINAL_APP_START_DISCONNECT,
    GLOBAL_TERMINAL_APP_RUN_DISCONNECT,
    GLOBAL_TERMINAL_APP_END,
};

static void GlobalTerminalApp_LoadWifi(GlobalTerminalApp *data);
static void GlobalTerminalApp_UnloadWifi(GlobalTerminalApp *data);
#if defined(SDK_PORT)
static void *GlobalTerminalApp_DwcAlloc(DWCAllocType name, u32 size, int align);
static void GlobalTerminalApp_DwcFree(DWCAllocType name, void *ptr, u32 size);
#else
static void *GlobalTerminalApp_DwcAlloc(int name, u32 size, int align);
static void GlobalTerminalApp_DwcFree(int name, void *ptr, u32 size);
#endif

// DWC allocations of the Wi-Fi apps run by GlobalTerminalApp_Main (set once the DWC heap exists).
static NNSFndHeapHandle sDwcHeap;

static const OverlayManagerTemplate sOverlayTemplate_GlobalTerminalConnect = { GlobalTerminalConnect_Init, GlobalTerminalConnect_Main, GlobalTerminalConnect_Exit, FS_OVERLAY_ID_NONE };

BOOL GlobalTerminalApp_Init(OverlayManager *man, int *state) {
    GlobalTerminalApp *data;

    Main_SetVBlankIntrCB(NULL, NULL);
    HBlankInterruptDisable();
    GfGfx_DisableEngineAPlanes();
    GfGfx_DisableEngineBPlanes();
    GX_SetVisiblePlane(GX_PLANEMASK_NONE);
    GXS_SetVisiblePlane(GX_PLANEMASK_NONE);
    GX_SetVisibleWnd(GX_WNDMASK_NONE);
    GXS_SetVisibleWnd(GX_WNDMASK_NONE);
    G2_BlendNone();
    G2S_BlendNone();
    Heap_Create(HEAP_ID_3, HEAP_ID_123, 0x28000);
    data = OverlayManager_CreateAndGetData(man, sizeof(GlobalTerminalApp), HEAP_ID_123);
    MI_CpuFill8(data, 0, sizeof(GlobalTerminalApp));
    data->args = OverlayManager_GetArgs(man);
    Sound_SetSceneAndPlayBGM(11, SEQ_GS_WIFI_ACCESS, 1);
    return TRUE;
}

BOOL GlobalTerminalApp_Main(OverlayManager *man, int *state) {
    GlobalTerminalApp *data = OverlayManager_GetData(man);

    switch (*state) {
    case GLOBAL_TERMINAL_APP_LOAD_WIFI:
        GlobalTerminalApp_LoadWifi(data);
        *state = GLOBAL_TERMINAL_APP_WAIT_WIFI;
        break;
    case GLOBAL_TERMINAL_APP_WAIT_WIFI:
        if (CommWireless_IsStartedUp()) {
            sDwcHeap = data->dwcHeap;
            DWC_SetMemFunc(GlobalTerminalApp_DwcAlloc, GlobalTerminalApp_DwcFree);
            data->wifiLoaded = 1;
            ++*state;
        }
        break;
    case GLOBAL_TERMINAL_APP_START_CONNECT:
        data->subApp = OverlayManager_New(&sOverlayTemplate_GlobalTerminalConnect, data, HEAP_ID_123);
        ++*state;
        break;
    case GLOBAL_TERMINAL_APP_RUN_CONNECT:
        if (OverlayManager_Run(data->subApp) == TRUE) {
            OverlayManager_Delete(data->subApp);
            if (data->isConnected == 1) {
                data->disconnect = 1;
                ++*state;
            } else {
                *state = GLOBAL_TERMINAL_APP_END;
            }
        }
        break;
    case GLOBAL_TERMINAL_APP_START_VS_RECORDER: {
        const OverlayManagerTemplate *tmpl = VsRecorderApp_GetTemplate(data->args->vsRecorderMode);
        data->subApp = OverlayManager_New(tmpl, data->args->fieldSystem, HEAP_ID_123);
        ++*state;
        break;
    }
    case GLOBAL_TERMINAL_APP_RUN_VS_RECORDER:
        if (OverlayManager_Run(data->subApp) == TRUE) {
            OverlayManager_Delete(data->subApp);
            ++*state;
        }
        break;
    case GLOBAL_TERMINAL_APP_START_DISCONNECT:
        data->subApp = OverlayManager_New(&sOverlayTemplate_GlobalTerminalConnect, data, HEAP_ID_123);
        ++*state;
        break;
    case GLOBAL_TERMINAL_APP_RUN_DISCONNECT:
        if (OverlayManager_Run(data->subApp) == TRUE) {
            OverlayManager_Delete(data->subApp);
            data->disconnect = 0;
            ++*state;
        }
        break;
    case GLOBAL_TERMINAL_APP_END:
        return TRUE;
    }

    if (data->wifiLoaded == 1 && data->disconnect == 1 && data->isConnected == 1) {
        DWC_UpdateConnection();
        WirelessStatusIcon_SetLevel(3 - DWC_GetLinkLevel());
    }
    return FALSE;
}

BOOL GlobalTerminalApp_Exit(OverlayManager *man, int *state) {
    GlobalTerminalApp *data = OverlayManager_GetData(man);

    GlobalTerminalApp_UnloadWifi(data);
    Heap_Free(data->args);
    OverlayManager_FreeData(man);
    Heap_Destroy(HEAP_ID_123);
    return TRUE;
}

static void GlobalTerminalApp_LoadWifi(GlobalTerminalApp *data) {
    if (data->wifiLoaded == 0) {
        LoadDwcOverlay();
        LoadOVY38();
        Comm_InitDwc(HEAP_ID_123);
        data->heapBuffer = Heap_Alloc(HEAP_ID_123, 0x20020);
#if defined(SDK_PORT)
        data->dwcHeap = NNS_FndCreateExpHeapEx((void *)(((uintptr_t)data->heapBuffer + 0x1F) & ~(uintptr_t)0x1F), 0x20000, 0);
#else
        data->dwcHeap = NNS_FndCreateExpHeapEx((void *)(((u32)data->heapBuffer + 0x1F) & ~0x1F), 0x20000, 0);
#endif
        CommWireless_StartUp();
        Sys_ClearSleepDisableFlag(4);
    }
}

static void GlobalTerminalApp_UnloadWifi(GlobalTerminalApp *data) {
    if (data->wifiLoaded == 1) {
        NNS_FndDestroyExpHeap(data->dwcHeap);
        Heap_Free(data->heapBuffer);
        UnloadOVY38();
        UnloadDwcOverlay();
        CommWireless_Terminate();
        data->wifiLoaded = 0;
    }
}

#if defined(SDK_PORT)
static void *GlobalTerminalApp_DwcAlloc(DWCAllocType name, u32 size, int align) {
#else
static void *GlobalTerminalApp_DwcAlloc(int name, u32 size, int align) {
#endif
    void *ptr;
    OSIntrMode intrMode = OS_DisableInterrupts();
    ptr = NNS_FndAllocFromExpHeapEx(sDwcHeap, size, align);
    OS_RestoreInterrupts(intrMode);
    return ptr;
}

#if defined(SDK_PORT)
static void GlobalTerminalApp_DwcFree(DWCAllocType name, void *ptr, u32 size) {
#else
static void GlobalTerminalApp_DwcFree(int name, void *ptr, u32 size) {
#endif
    if (ptr != NULL) {
        OSIntrMode intrMode = OS_DisableInterrupts();
        NNS_FndFreeToExpHeap(sDwcHeap, ptr);
        OS_RestoreInterrupts(intrMode);
    }
}
#if defined(SDK_PORT)

/* Native storage for the otherwise unreferenced .bss word of asm/global_terminal_app_data.s. */
u32 _021D1108;
#endif
