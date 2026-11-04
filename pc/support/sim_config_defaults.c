#include <simulator/sim.h>

void __real_SIM_Config_LoadDefaults(SIM_config_type *config);

void __wrap_SIM_Config_LoadDefaults(SIM_config_type *config) {
    __real_SIM_Config_LoadDefaults(config);
    config->screenLayout = SIM_CONFIG_SCREEN_LAYOUT_VERTICAL;
}
