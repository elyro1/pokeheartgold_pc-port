#ifndef HG_PC_OFFLINE_FEATURES_H
#define HG_PC_OFFLINE_FEATURES_H

typedef enum PCUnavailableFeature {
    PC_FEATURE_WIFI_SETUP,
    PC_FEATURE_POKEWALKER,
    PC_FEATURE_GBA_MIGRATION,
    PC_FEATURE_RANGER,
    PC_FEATURE_WII_LINK,
    PC_FEATURE_WII_MESSAGES,
    PC_FEATURE_WIFI_PLAZA,
    PC_FEATURE_UNAVAILABLE_COUNT
} PCUnavailableFeature;

void PC_ShowUnavailableFeature(PCUnavailableFeature feature);
#endif
