/* The DS stores UTF-16 units, even on Linux where wchar_t is four bytes. */
#include <simulator/sim.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void LoadText(SIM_config_type *config, const char *nickname, const char *comment) {
    FILE *file = fopen("sim_config.ini", "w");
    assert(file != NULL);
    fprintf(file, "[OSNVRAM]\nOwner_Nickname=%s\nOwner_Comment=%s\n", nickname, comment);
    fclose(file);
    assert(SIM_Config_LoadConfigFile(config));
}

int main(void) {
    char temporary[] = "/tmp/hg-config-text-XXXXXX";
    assert(mkdtemp(temporary) != NULL);
    assert(chdir(temporary) == 0);
    SIM_config_type config = {0};
    SIM_Config_LoadDefaults(&config);
    assert(config.osNvramConfig.ncd.owner.nickname.length == 6);
    const u16 player[] = {'P', 'l', 'a', 'y', 'e', 'r'};
    assert(memcmp(config.osNvramConfig.ncd.owner.nickname.str, player, sizeof(player)) == 0);

    LoadText(&config, "\xe3\x81\x82", "\xc3\xa9");
    assert(config.osNvramConfig.ncd.owner.nickname.length == 1);
    assert(config.osNvramConfig.ncd.owner.nickname.str[0] == 0x3042);
    assert(config.osNvramConfig.ncd.owner.comment.length == 1);
    assert(config.osNvramConfig.ncd.owner.comment.str[0] == 0xe9);
    SIM_Config_SaveConfigFile(&config);
    SIM_config_type restored = {0};
    assert(SIM_Config_LoadConfigFile(&restored));
    assert(memcmp(&config.osNvramConfig.ncd.owner.nickname,
        &restored.osNvramConfig.ncd.owner.nickname, sizeof(config.osNvramConfig.ncd.owner.nickname)) == 0);
    assert(memcmp(&config.osNvramConfig.ncd.owner.comment,
        &restored.osNvramConfig.ncd.owner.comment, sizeof(config.osNvramConfig.ncd.owner.comment)) == 0);

    LoadText(&config, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
    assert(config.osNvramConfig.ncd.owner.nickname.length == NVRAM_CONFIG_NICKNAME_LENGTH);
    assert(config.osNvramConfig.ncd.owner.nickname.str[9] == 'J');
    assert(config.osNvramConfig.ncd.owner.comment.length == NVRAM_CONFIG_COMMENT_LENGTH);
    assert(config.osNvramConfig.ncd.owner.comment.str[25] == 'Z');
    assert(config.osNvramConfig.ncd.option.input_nickname);
    LoadText(&config, "123456789\xf0\x9f\x98\x80", "");
    assert(config.osNvramConfig.ncd.owner.nickname.length == 9);
    assert(config.osNvramConfig.ncd.owner.nickname.str[9] == 0);
    assert(config.osNvramConfig.ncd.owner.comment.length == 0);
    assert(remove("sim_config.ini") == 0);
    assert(chdir("/") == 0);
    assert(rmdir(temporary) == 0);
    return 0;
}
