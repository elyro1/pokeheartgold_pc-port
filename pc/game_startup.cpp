#include "runtime_directory.h"

#include <cstdio>
#include <exception>

extern "C" int __real_main(int argc, char **argv);

// Enter the runtime folder before the SDK opens logs, settings, or ROM files.
extern "C" int __wrap_main(int argc, char **argv) {
    try {
        HG_PC_UseRuntimeDirectory();
    } catch (const std::exception &error) {
        std::fprintf(stderr, "Cannot open game folder: %s\n", error.what());
        return 1;
    }
    return __real_main(argc, argv);
}
