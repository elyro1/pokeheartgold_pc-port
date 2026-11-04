#!/usr/bin/env python3
"""Validate a US ROM, then stage libntr's extraction into the PC run folder."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
ROM_SPECS = {
    "heartgold": (b"IPKE", "4fcded0e2713dc03929845de631d0932ea2b5a37"),
    "soulsilver": (b"IPGE", "f8dc38ea20c17541a43b58c5e6d18c1732c7e582"),
}
MANIFEST = "hg-assets.json"


def sha1(path):
    with path.open("rb") as source:
        digest = hashlib.sha1()
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
        return digest.hexdigest()


def validate_rom(path, game):
    code, expected = ROM_SPECS[game]
    with path.open("rb") as source:
        header = source.read(0x160)
    if len(header) < 0x160 or header[0xC:0x10] != code or header[0x10:0x12] != b"01" or header[0x1E] != 0:
        raise ValueError(f"Expected a US {game} revision 0 ROM ({code.decode()}, maker 01)")
    actual = sha1(path)
    if actual != expected:
        raise ValueError(f"ROM SHA1 mismatch: expected {expected}, got {actual}")
    return actual


def required_paths():
    header = (ROOT / "include/filesystem_files_def.h").read_text()
    table = header.split("char *sNarcFileList[] = {", 1)[1].split("};", 1)[0]
    return [*re.findall(r'"([^"\n]+)"', table), "data/sound/gs_sound_data.sdat"]


def verify_assets(output, game):
    manifest = json.loads((output / MANIFEST).read_text())
    if manifest["game"] != game or manifest["rom_sha1"] != ROM_SPECS[game][1]:
        raise ValueError("Extracted assets belong to a different game or ROM revision")
    for name in required_paths() + ["header.bin"]:
        if name not in manifest["files"]:
            raise ValueError(f"Asset manifest is missing {name}")
    for name, expected in manifest["files"].items():
        path = output / name
        if Path(name).is_absolute() or ".." in Path(name).parts or not path.is_file() or sha1(path) != expected:
            raise ValueError(f"Missing or modified asset: {name}")
    return manifest


def extract_assets(rom, output, game, extractor):
    digest = validate_rom(rom, game)
    if output.exists() and (output / "header.bin").exists():
        with (output / "header.bin").open("rb") as source:
            header = source.read(0x160)
        if header[0xC:0x10] != ROM_SPECS[game][0]:
            raise ValueError("Output directory already contains another game's assets")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".hg-extract-", dir=output.parent) as temporary:
        staging = Path(temporary)
        log = subprocess.run([str(extractor), "--output", str(staging), str(rom)], cwd=staging,
                             capture_output=True, text=True)
        if log.returncode:
            raise ValueError(f"libntr extraction failed ({log.returncode}):\n{log.stdout}\n{log.stderr}")
        for name in required_paths():
            if not (staging / name).is_file():
                raise ValueError(f"ROM extraction is incomplete: missing {name}")
        with rom.open("rb") as source:
            if (staging / "header.bin").read_bytes()[:0x160] != source.read(0x160):
                raise ValueError("Extracted ROM header does not match the input")
        files = {p.relative_to(staging).as_posix(): sha1(p) for p in sorted(staging.rglob("*")) if p.is_file()}
        manifest = {"game": game, "rom_sha1": digest, "files": files}
        (staging / MANIFEST).write_text(json.dumps(manifest, indent=2) + "\n")
        verify_assets(staging, game)
        # Publish only verified extracted files; retain saves/configuration.
        output.mkdir(parents=True, exist_ok=True)
        for name in files:
            target = output / name
            target.parent.mkdir(parents=True, exist_ok=True)
            os.replace(staging / name, target)
        os.replace(staging / MANIFEST, output / MANIFEST)
        # The native DWC backup reads/writes the DS firmware's Wi-Fi settings area from this file; it is
        # user data, so an existing one is kept and it stays out of the asset manifest.
        firmware = output / "firmware.bin"
        if not firmware.exists():
            firmware.write_bytes(b"\xff" * 0x40000)
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", choices=ROM_SPECS, required=True)
    parser.add_argument("--rom", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--extractor", type=Path)
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    try:
        if args.verify:
            manifest = verify_assets(args.output.resolve(), args.game)
        else:
            if args.rom is None or args.extractor is None:
                parser.error("--rom and --extractor are required unless using --verify")
            manifest = extract_assets(args.rom.resolve(), args.output.resolve(), args.game, args.extractor.resolve())
        print(f"Verified {len(manifest['files'])} {args.game} assets in {args.output}")
    except (OSError, ValueError, KeyError) as error:
        print(f"assets: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
