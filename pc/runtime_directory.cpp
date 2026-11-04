#include "runtime_directory.h"

#include <SDL2/SDL_error.h>
#include <SDL2/SDL_filesystem.h>
#include <SDL2/SDL_stdinc.h>
#include <filesystem>
#include <memory>
#include <stdexcept>

void HG_PC_UseRuntimeDirectory() {
    std::unique_ptr<char, decltype(&SDL_free)> base(SDL_GetBasePath(), SDL_free);
    if (!base) {
        throw std::runtime_error(SDL_GetError());
    }
    std::filesystem::path directory(reinterpret_cast<const char8_t *>(base.get()));
    if (!std::filesystem::exists(directory / "header.bin") &&
        std::filesystem::exists(directory / "run/header.bin")) {
        directory /= "run";
    }
    std::filesystem::current_path(directory);
}
