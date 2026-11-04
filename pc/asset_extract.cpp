/* Command-line entry to the exact extractor used by libntr's file dialog.
 * assets.py validates the game/revision and runs this in a staging directory. */
#include <cstdio>
#include <filesystem>
#include <string>

#include "runtime_directory.h"

int SIM_NDSExtract(std::string ndsFilePath);

int main(int argc, char **argv) {
    if (argc != 2 && !(argc == 4 && std::string(argv[1]) == "--output")) {
        std::fprintf(stderr, "Usage: hg-extract [--output DIRECTORY] ROM.nds\n");
        return 2;
    }
    try {
        // Resolve input paths before changing the working directory.
        auto rom = std::filesystem::absolute(argv[argc - 1]);
        if (argc == 4) {
            std::filesystem::current_path(std::filesystem::absolute(argv[2]));
        } else {
            HG_PC_UseRuntimeDirectory();
        }
        return SIM_NDSExtract(rom.string());
    } catch (const std::exception &error) {
        std::fprintf(stderr, "Asset extraction failed: %s\n", error.what());
        return 1;
    }
}
