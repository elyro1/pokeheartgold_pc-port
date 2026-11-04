#!/usr/bin/env python3
"""Generate native archive IDs from the original asset build metadata.

Directory archives retain nitroarc's .narcorder + sorted remaining files rule.
Object/CSV archives follow their original o2narc/csv2bin metadata. No assets are
rebuilt and the output never depends on a locally supplied ROM.
"""
import argparse
import csv
import fnmatch
import json
from pathlib import Path
import re
import subprocess
import struct

from message_headers import ROOT, write_if_changed


def requested_headers():
    names = set()
    for directory, pattern in (("src", "*.c"), ("include", "*.h")):
        for path in (ROOT / directory).rglob(pattern):
            names.update(re.findall(r'#include ["<]([^">]+\.naix)[">]', path.read_text(encoding="utf-8")))
    return sorted(names - {"msgdata/msg.naix"})


def make_metadata(game):
    result = subprocess.run([
        "make", "--no-print-directory", "-rnp", "-f", "pc/tools/resource_metadata.mk",
        "pc-resource-metadata", "GAME_VERSION=" + game.upper(),
    ], cwd=ROOT, capture_output=True, text=True, encoding="utf-8", check=True)
    archives = {}
    variables = {}
    for line in result.stdout.splitlines():
        match = re.match(r"((?:\./)?files/[^ :]+\.narc):\s*(.*)", line)
        if match and "=" not in match[2]:
            archives[match[1].removeprefix("./")] = match[2].split("|")[0].split()
        match = re.match(r"(\w+) := (.*)", line)
        if match:
            variables[match[1]] = match[2].split()
    return archives, variables


def directory_members(directory, dependencies):
    order_path = directory / ".narcorder"
    order = order_path.read_text(encoding="utf-8").splitlines() if order_path.exists() else []
    order = [x.strip() for x in order if x.strip()]
    ignored = [".*ignore", ".*keep", ".*order"]
    ignore_path = directory / ".narcignore"
    if ignore_path.exists():
        ignored += ignore_path.read_text(encoding="utf-8").splitlines()
    members = {p.relative_to(directory).as_posix() for p in directory.rglob("*") if p.is_file()}
    for dep in dependencies:
        dep = ROOT / dep
        if dep.is_relative_to(directory):
            members.add(dep.relative_to(directory).as_posix())
    members = {x for x in members if not any(fnmatch.fnmatchcase(x, pattern.strip()) for pattern in ignored if pattern.strip())}
    return order + sorted(members - set(order))


def identifiers(header, archives, variables):
    stem = Path(header).stem
    source = ROOT / "files" / header
    if stem == "zukan_data":
        labels = re.findall(r"^\s*\.type (\w+), @object", source.with_suffix(".json.txt").read_text(encoding="utf-8"), re.M)
        data = json.loads(source.with_suffix(".json").read_text(encoding="utf-8"))
        labels += [f"sort_order_{group['type']}_{option['id']}" for group in data["sorting"] for option in group["options"]]
        return ["NARC_zukan_data_" + label for label in labels]
    if stem == "zukan_enc":
        data = json.loads(source.with_suffix(".json").read_text(encoding="utf-8"))
        count = 2 + sum(len(mons) for mons in data["encounters"].values())
        return [f"NARC_zukan_enc_zukan_enc_{i:08d}" for i in range(count)]
    if stem == "zukan_hw_data":
        return [f"NARC_zukan_hw_data_zukan_hw_data_{i}_bin" for i in range(2)]
    if stem in ("g_enc_data", "s_enc_data"):
        data = json.loads((source.parent / "gs_enc_data.json").read_text(encoding="utf-8"))
        return [f"NARC_{stem}_{stem}_{i:08d}" for i in range(len(data["encounters"]))]
    if header == "arc/ppark.naix":
        return ["NARC_ppark_ppark_00000000"]
    if header == "itemtool/itemdata/item_data.naix":
        with source.with_suffix(".csv").open(encoding="utf-8", newline="") as stream:
            count = len(list(csv.DictReader(stream)))
        return [f"NARC_item_data_{i:04d}_bin" for i in range(count)]
    if header == "poketool/pokegra/otherpoke.naix":
        names = [Path(line.split()[1]).name for line in source.with_suffix(".txt").read_text(encoding="utf-8").splitlines() if line.strip()]
        members = sorted(names)
    else:
        dependencies = archives.get("files/" + Path(header).with_suffix(".narc").as_posix(), [])
        if stem == "plist_gra":
            dependencies = ["files/graphic/plist_gra/" + x for x in variables["PLIST_GRA_FILES"]]
        members = directory_members(source.with_suffix(""), dependencies)
    if not members:
        raise ValueError(f"No archive members found for {header}")
    return [f"NARC_{stem}_" + re.sub(r"\W", "_", name) for name in members]


def archive_paths(game):
    text = (ROOT / "filesystem.mk").read_text(encoding="utf-8")
    text = text.replace("$(shortname)", "gold" if game == "heartgold" else "silver")
    text = text.replace("$(buildname)", game + ".us")
    return dict(re.findall(r"arc_strip_name,files/([^,]+),files/([^\)]+)", text))


def build_indices(game):
    archives, variables = make_metadata(game)
    result = {header: identifiers(header, archives, variables) for header in requested_headers()}
    for header, names in result.items():
        if len(names) != len(set(names)) or any(not re.fullmatch(r"[A-Za-z_]\w*", name) for name in names):
            raise ValueError(f"Invalid or duplicate archive IDs in {header}")
    return result


def verify_counts(indices, assets, game):
    paths = archive_paths(game)
    for header, names in indices.items():
        archive = Path(header).with_suffix(".narc").as_posix()
        if Path(header).stem in ("zukan_enc", "zukan_hw_data"):
            archive = archive[:-5] + ("_gold.narc" if game == "heartgold" else "_silver.narc")
        target = assets / paths.get(archive, archive)
        with target.open("rb") as stream:
            prefix = stream.read(0x20)
        if prefix[:4] != b"NARC" or prefix[0x10:0x14] != b"BTAF":
            raise ValueError(f"Invalid archive header: {target}")
        count = struct.unpack_from("<H", prefix, 0x18)[0]
        if count != len(names):
            raise ValueError(f"{header}: generated {len(names)} members, ROM has {count}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", choices=["heartgold", "soulsilver"], required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--verify-assets", type=Path)
    args = parser.parse_args()
    indices = build_indices(args.game)
    if args.verify_assets:
        verify_counts(indices, args.verify_assets, args.game)
    if args.output:
        lines = ["/* Generated from the original archive metadata. */", "#ifndef HG_PC_RESOURCE_INDICES_H", "#define HG_PC_RESOURCE_INDICES_H"]
        for header, names in indices.items():
            lines += ["\n/* " + header + " */"] + [f"#define {name} {i}" for i, name in enumerate(names)]
            write_if_changed(args.output / header, '#include <resource_indices.h>\n')
        write_if_changed(args.output / "resource_indices.h", "\n".join(lines + ["#endif", ""]))
    print(f"Generated {len(indices)} resource indexes ({sum(map(len, indices.values()))} member IDs).")


if __name__ == "__main__":
    main()
