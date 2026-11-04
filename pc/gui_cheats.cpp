#include <algorithm>
#include <simulator/imgui/imgui.hpp>
#include <string>
#include <vector>

#include "include/port/cheats.h"
#include "include/port/sim_config_prj.h"

#include "../include/constants/items.h"
#include "../include/constants/maps.h"
#include "../include/constants/species.h"

namespace SIM::GUI {
static void ConfigCheckbox(const char *label, BOOL &value, bool &changed) {
    bool enabled = value != 0;
    if (ImGui::Checkbox(label, &enabled)) {
        value = enabled;
        changed = true;
    }
}

static void NameSelector(const char *label, int &id, bool item) {
    static std::vector<std::string> speciesNames, itemNames;
    auto &names = item ? itemNames : speciesNames;
    if (names.empty() && PC_Cheats_FieldReady()) {
        names.emplace_back("None");
        for (int i = 1; i < (item ? ITEMS_COUNT : SPECIES_ARCEUS + 1); i++) {
            char name[128];
            PC_Cheats_GetName(item, i, name, sizeof(name));
            names.push_back(std::to_string(i) + " - " + name);
        }
    }
    if (!names.empty()) {
        ImGui::Combo(label, &id, [](void *data, int index) {
            auto &entries = *static_cast<std::vector<std::string> *>(data);
            return index >= 0 && index < static_cast<int>(entries.size()) ? entries[index].c_str() : ""; }, &names, static_cast<int>(names.size()));
    }
}

void CheatsMain(bool *open) {
    ImGui::Begin("Cheats", open);
    auto *config = SIM_Config_prj_GetConfig();
    bool changed = false;
    ConfigCheckbox("Walk Through Walls", config->walkThroughWalls, changed);
    ConfigCheckbox("Disable Random Encounters", config->disableRandomEncounters, changed);
    ConfigCheckbox("Run from Trainer Battles", config->runFromTrainerBattles, changed);
    if (changed) {
        SIM_Config_prj_Save();
    }

    bool ready = PC_Cheats_FieldReady();
    if (!ready) {
        ImGui::TextWrapped("Load a game and close menus, dialogue, or battles to use the generators and map jump.");
    }
    ImGui::BeginDisabled(!ready);
    static std::string status;
    ImGui::SeparatorText("Give Pokemon");
    static int species = 1, level = 5;
    NameSelector("Species", species, false);
    ImGui::InputInt("Level", &level);
    level = std::clamp(level, 1, 100);
    if (ImGui::Button("Give Pokemon")) {
        status = PC_Cheats_GiveMon(species, level) ? "Pokemon added to your party." : "Could not add Pokemon. Check for an empty party slot.";
    }
    ImGui::SeparatorText("Give Items");
    static int item = ITEM_POTION, quantity = 1;
    NameSelector("Item", item, true);
    ImGui::InputInt("Quantity", &quantity);
    quantity = std::clamp(quantity, 1, 999);
    if (ImGui::Button("Give Items")) {
        status = PC_Cheats_GiveItem(item, quantity) ? "Items added to your bag." : "Could not add items. Check bag space and quantity limits.";
    }
    ImGui::TextWrapped("Save in-game to keep generated Pokemon and items.");
    ImGui::SeparatorText("Jump to Map");
    static int destination = 0;
    static const char *destinations[] = {
        "New Bark Town", "Cherrygrove City", "Violet City", "Azalea Town", "Goldenrod City", "Ecruteak City", "Olivine City", "Cianwood City", "Mahogany Town", "Lake of Rage", "Blackthorn City", "Mt. Silver", "Pallet Town", "Viridian City", "Pewter City", "Cerulean City", "Lavender Town", "Vermilion City", "Celadon City", "Fuchsia City", "Cinnabar Island", "Indigo Plateau", "Saffron City", "Safari Zone Gate", "Battle Frontier", "Pokeathlon Dome", "Victory Road", "Union Cave", "Mt. Moon", "Rock Tunnel"
    };
    ImGui::Combo("Destination", &destination, destinations, IM_ARRAYSIZE(destinations));
    if (ImGui::Button("Jump to Destination")) {
        status = PC_Cheats_Fly(destination + 1) ? "Map jump started." : "Map jump unavailable.";
    }
    // Starting a warp installs a field task immediately. Recheck before another action.
    ImGui::BeginDisabled(!PC_Cheats_FieldReady());
    static int map = MAP_NEW_BARK, x = 0, z = 0;
    ImGui::InputInt("Map ID", &map);
    ImGui::InputInt("X", &x);
    ImGui::InputInt("Z", &z);
    ImGui::TextWrapped("Custom jumps require valid tile coordinates for the destination map.");
    if (ImGui::Button("Jump to Coordinates")) {
        status = PC_Cheats_Warp(map, x, z) ? "Map jump started." : "Map jump unavailable or invalid destination.";
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    if (!status.empty()) {
        ImGui::TextWrapped("%s", status.c_str());
    }
    ImGui::End();
}
}
