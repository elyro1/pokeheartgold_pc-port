#!/usr/bin/env python3
"""Generate message IDs only; runtime message bytes come from the verified ROM.

Matches GMM::FromFile/WriteGmmHeader (row order, ignoring index attributes),
msg.mk's sorted file list, and the trainer-name template for bank 729.
"""
import argparse
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
TRAINERS = ROOT / "files/poketool/trainer/trainers.json"


def messages():
    paths = {p.stem: p for p in (ROOT / "files/msgdata/msg").glob("*.gmm")}
    paths["msg_0729"] = None
    return dict(sorted(paths.items()))


def definitions(path):
    if path is None:
        return [f"msg_0729_{i:05d}" for i, _ in enumerate(json.loads(TRAINERS.read_text(encoding="utf-8"))["trainers"])]
    return [row.get("id") or f"{path.stem}_{i:05d}" for i, row in enumerate(ET.parse(path).getroot().findall("row"))]


def write_if_changed(path, text):
    if not path.exists() or path.read_text(encoding="utf-8") != text:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list-inputs", action="store_true")
    parser.add_argument("--list-names", action="store_true")
    parser.add_argument("--headers", type=Path)
    parser.add_argument("--index", type=Path)
    args = parser.parse_args()
    banks = messages()
    if args.list_inputs:
        # Relative to pc/generated, where Meson consumes this list.
        print("\n".join("../../" + p.relative_to(ROOT).as_posix() for p in [*[p for p in banks.values() if p], TRAINERS]))
    elif args.list_names:
        print("\n".join(name + ".h" for name in banks))
    elif args.headers:
        for name, path in banks.items():
            names = definitions(path)
            if len(set(names)) != len(names) or any(not re.fullmatch(r"[A-Za-z_]\w*", x) for x in names):
                raise ValueError(f"Invalid or duplicate message identifiers in {name}")
            guard = "HG_PC_GENERATED_" + name.upper() + "_H"
            body = "\n".join(f"#define {symbol} {i}" for i, symbol in enumerate(names))
            write_if_changed(args.headers / (name + ".h"), f"/* Generated message IDs. */\n#ifndef {guard}\n#define {guard}\n{body}\n#endif\n")
    elif args.index:
        body = "\n".join(f"#define NARC_msg_{name}_bin {i}" for i, name in enumerate(banks))
        write_if_changed(args.index, f"/* Generated archive IDs, in msg.mk order. */\n#ifndef HG_PC_MSG_NAIX\n#define HG_PC_MSG_NAIX\n{body}\n#define FILES_MSGDATA_MSG_COUNT {len(banks)}\n#endif\n")
    else:
        parser.error("select an output mode")


if __name__ == "__main__":
    main()
