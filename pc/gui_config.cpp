#include <simulator/imgui/imgui.hpp>
#include <simulator/sim_gui.hpp>

#include "include/port/sim_config_prj.h"
namespace SIM::GUI {
void CheatsMain(bool *open);
static bool sCheatsOpen;
void PrjMain(bool *open) {
    ImGui::Begin("HeartGold / SoulSilver", open);
    AppButton("Cheats", &sCheatsOpen, nullptr, CheatsMain);
    auto *c = SIM_Config_prj_GetConfig();
    bool changed = false;
    int fps = c->enable60fps ? 1 : 0;
    if (ImGui::Combo("Frame rate", &fps, "30 FPS\0"
                                         "60 FPS\0")) {
        c->enable60fps = fps == 1;
        changed = true;
    }
    ImGui::BeginDisabled(!c->enable60fps);
    int pacing = c->battlePacing;
    if (ImGui::Combo("Battle pacing", &pacing, "Original\0Smooth\0Quick\0")) {
        c->battlePacing = static_cast<BattlePacing>(pacing);
        changed = true;
    }
    bool ui = c->battleUiSpeedFix;
    if (ImGui::Checkbox("Normal battle UI and timing", &ui)) {
        c->battleUiSpeedFix = ui;
        changed = true;
    }
    ImGui::TextWrapped("Original keeps animation timing; Smooth adds sprite interpolation; Quick accelerates battle animations. Field speed correction is automatic.");
    ImGui::EndDisabled();
    bool autosave = c->autosave;
    if (ImGui::Checkbox("Autosave", &autosave)) {
        c->autosave = autosave;
        changed = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Archive progress every two minutes when overworld control is safe. Keeps the newest ten autosaves per slot.");
    }
    if (changed) {
        SIM_Config_prj_Save();
    }
    ImGui::End();
}
}
