#!/usr/bin/env python3
"""Copy the MinGW DLL dependency closure beside the Windows executables."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess

# OS imports are supplied by Windows, never bundled from a developer's machine.
SYSTEM_DLLS = set('''advapi32 avrt bcrypt cfgmgr32 comctl32 comdlg32 crypt32
    d3d9 d3d11 dbghelp dinput8 dwmapi dxgi gdi32 hid imm32 iphlpapi kernel32
    msvcrt ntdll ole32 oleaut32 opengl32 powrprof psapi rpcrt4 secur32 setupapi
    shell32 shlwapi ucrtbase user32 userenv usp10 version winmm winspool ws2_32
    wtsapi32'''.split())


def imports(path, objdump):
    output = subprocess.check_output([objdump, '-p', str(path)], text=True)
    return re.findall(r'DLL Name:\s*(\S+)', output)


def stage(build, directories, objdump):
    available = {}
    for directory in directories:
        if directory.is_dir():
            for path in directory.iterdir():
                if path.is_file() and path.suffix.lower() == '.dll':
                    available.setdefault(path.name.lower(), path)
    pending = [build / 'hg-game.exe', build / 'hg-extract.exe']
    seen = set()
    copied = []
    while pending:
        executable = pending.pop()
        for name in imports(executable, objdump):
            key = name.lower()
            if key in seen:
                continue
            seen.add(key)
            if key.removesuffix('.dll') in SYSTEM_DLLS or key.startswith(('api-ms-', 'ext-ms-')):
                continue
            system = Path(os.environ.get('SystemRoot', 'C:/Windows')) / 'System32' / name
            if os.name == 'nt' and system.is_file():
                continue
            source = available.get(key)
            if source is None:
                raise ValueError(f'Missing runtime DLL {name}; add its directory with --dll-dir')
            target = build / source.name
            if source.resolve() != target.resolve():
                shutil.copy2(source, target)
                copied.append(target.name)
            pending.append(target)
    return copied


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--dll-dir', type=Path, action='append', default=[])
    parser.add_argument('--objdump', default='objdump')
    args = parser.parse_args()
    try:
        # Prefer the current toolchain over DLLs staged by an earlier build.
        directories = [*args.dll_dir, *map(Path, os.get_exec_path()), args.build]
        copied = stage(args.build.resolve(), directories, args.objdump)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'Windows runtime staging failed: {error}\n')
    print('Windows runtime DLLs staged: ' + (', '.join(sorted(copied)) or 'already present'))


if __name__ == '__main__':
    main()
