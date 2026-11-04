#include "global.h"
#include "main.h"
#include "overlay_manager.h"
#include "application/main_menu/main_menu.h"
#include <hg/offline_features.h>
#include <simulator/sim.h>
#include <SDL2/SDL.h>
#include <stdio.h>

void PC_ShowUnavailableFeature(PCUnavailableFeature feature) {
    static const char *const names[] = {
        "Nintendo WFC setup", "Pokewalker", "GBA migration", "Pokemon Ranger link",
        "Wii connection", "Wii message settings", "Wi-Fi Plaza"
    };
    GF_ASSERT((unsigned int)feature < NELEMS(names));
    char message[160];
    snprintf(message, sizeof(message), "%s is unavailable in this PC build.", names[feature]);
    if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, names[feature], message, SIM_GetSDLWindow()) != 0) {
        fprintf(stderr, "%s (%s)\n", message, SDL_GetError());
    }
}

/* Public boundary of the DS-only utility. Callers retain their own state. */
void RunWifiSetupApp(enum HeapID heapID) {
    (void)heapID;
    PC_ShowUnavailableFeature(PC_FEATURE_WIFI_SETUP);
}

static BOOL WifiSetupUnavailable_Init(OverlayManager *manager, int *state) {
    (void)manager;
    (void)state;
    PC_ShowUnavailableFeature(PC_FEATURE_WIFI_SETUP);
    RegisterMainOverlay(FS_OVERLAY_ID(OVY_74), &gApp_MainMenu);
    return TRUE;
}

static BOOL WifiSetupUnavailable_Done(OverlayManager *manager, int *state) {
    (void)manager;
    (void)state;
    return TRUE;
}

/* The DS utility template has null update/exit entries because it resets the
 * console inside init. A native return path needs a complete lifecycle. */
const OverlayManagerTemplate gApp_MainMenu_SelectOption_NintendoWFCSetup = {
    WifiSetupUnavailable_Init, WifiSetupUnavailable_Done, WifiSetupUnavailable_Done, FS_OVERLAY_ID_NONE
};
