#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* DWC stores writable Wi-Fi settings separately from the extracted ROM. The
 * SDK ROM picker does not create this file, while pc/tools/assets.py does. */
void HG_DWC_PrepareFirmware(void) {
    const char *path = "firmware.bin";
    FILE *file = fopen(path, "rb+");
    if (file != NULL) {
        fclose(file);
        return;
    }
    if (errno != ENOENT) {
        perror("Cannot open Wi-Fi settings (firmware.bin)");
        exit(EXIT_FAILURE);
    }

    /* Exclusive creation keeps an existing user's settings intact. */
    file = fopen(path, "wbx");
    if (file == NULL) {
        perror("Cannot create Wi-Fi settings (firmware.bin)");
        exit(EXIT_FAILURE);
    }
    unsigned char blank[4096];
    memset(blank, 0xff, sizeof(blank));
    int failed = 0;
    for (size_t offset = 0; offset < 0x40000; offset += sizeof(blank)) {
        if (fwrite(blank, 1, sizeof(blank), file) != sizeof(blank)) {
            failed = 1;
            break;
        }
    }
    if (fclose(file) != 0) {
        failed = 1;
    }
    if (failed) {
        remove(path);
        fputs("Cannot write Wi-Fi settings (firmware.bin)\n", stderr);
        exit(EXIT_FAILURE);
    }
}
