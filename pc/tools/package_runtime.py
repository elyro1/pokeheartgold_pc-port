#!/usr/bin/env python3
"""Stage a standalone PC runtime, without ROM assets or developer settings."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess

from message_headers import ROOT
from windows_runtime import stage as stage_windows


# Keep the host's loader and glibc together. Graphics drivers remain host-owned.
HOST_LIBRARIES = re.compile(
    r"^(?:ld-linux[^/]*|lib(?:c|m|mvec|pthread|dl|rt|resolv|util|anl|nss_[\w-]+)\.so[.\d]*|"
    r"lib(?:GL|GLX|GLdispatch|EGL|drm)[^/]*\.so[.\d]*)$")


def linux_libraries(executables):
    libraries = {}
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith("LD_")}
    environment["LC_ALL"] = "C"
    for executable in executables:
        result = subprocess.run(["ldd", str(executable)], capture_output=True,
                                text=True, encoding="utf-8", check=True,
                                env=environment)
        for line in result.stdout.splitlines():
            if "=> not found" in line:
                raise ValueError(f"Unresolved runtime dependency: {line.strip()}")
            match = re.match(r"\s*([^\s/]+) => (/.*?)\s+\(0x[0-9a-f]+\)", line)
            if match and not HOST_LIBRARIES.fullmatch(match[1]):
                name, source = match.groups()
                if name in libraries and libraries[name] != Path(source):
                    raise ValueError(f"Conflicting runtime libraries for {name}")
                libraries[name] = Path(source)
    return libraries


def relocate_linux_libraries(directory, patchelf):
    # Distribution libraries can carry their own absolute RUNPATH, which blocks
    # inheritance of the executable's RPATH (e.g. Ubuntu's libpulse.so.0).
    # Every bundled library must find its own dependencies in this same folder.
    for library in sorted(directory.iterdir()):
        subprocess.run([patchelf, "--set-rpath", "$ORIGIN", str(library)], check=True)


def verify_linux_runtime(output, names):
    directory = (output / "lib").resolve()
    dependencies = linux_libraries([*[output / name for name in names],
                                    *sorted(directory.iterdir())])
    for name, source in dependencies.items():
        if source.resolve() != directory / name:
            raise ValueError(f"Runtime dependency resolved outside package: {name} => {source}")


def package(build, output, game, platform, dll_dirs=(), objdump="objdump", patchelf="patchelf"):
    # Only fresh directories are accepted, so releases cannot accidentally include
    # saves, extracted ROMs, stale libraries, or local configuration from a run.
    if output.exists() and any(output.iterdir()):
        raise ValueError(f"Package directory must be empty: {output}")
    output.mkdir(parents=True, exist_ok=True)
    suffix = ".exe" if platform == "win64" else ""
    names = ["hg-game" + suffix, "hg-extract" + suffix]
    for name in names:
        shutil.copy2(build / name, output / name)
        if platform == "linux":
            (output / name).chmod(0o755)
    if platform == "win64":
        stage_windows(output, [*dll_dirs, *map(Path, os.get_exec_path()), build], objdump)
    else:
        libraries = linux_libraries([build / name for name in names])
        (output / "lib").mkdir()
        for name, source in libraries.items():
            shutil.copy2(source, output / "lib" / name)
        relocate_linux_libraries(output / "lib", patchelf)
        verify_linux_runtime(output, names)
    header = (ROOT / (game + ".us/rom_header_template.sbin")).read_bytes()[:0x160]
    if len(header) != 0x160:
        raise ValueError("Incomplete ROM header template")
    (output / "header.bin").write_bytes(header)
    # This is a blank writable Wi-Fi settings store, not a dumped DS firmware.
    (output / "firmware.bin").write_bytes(b"\xff" * 0x40000)
    readme = (ROOT / "pc/runtime/README.txt").read_text(encoding="utf-8")
    (output / "README.txt").write_text(readme.replace("@GAME@", game), encoding="utf-8")
    print(f"Staged {game} {platform} runtime in {output}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--game", choices=["heartgold", "soulsilver"], required=True)
    parser.add_argument("--platform", choices=["linux", "win64"], required=True)
    parser.add_argument("--dll-dir", type=Path, action="append", default=[])
    parser.add_argument("--objdump", default="objdump")
    parser.add_argument("--patchelf", default="patchelf")
    args = parser.parse_args()
    try:
        package(args.build.resolve(), args.output.resolve(), args.game, args.platform,
                args.dll_dir, args.objdump, args.patchelf)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Runtime packaging failed: {error}\n")


if __name__ == "__main__":
    main()
