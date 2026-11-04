#!/usr/bin/env python3
"""List the native game sources selected for the PC build."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    replaced = {ROOT / name for name in json.loads((ROOT / "pc/source_replacements.json").read_text())}
    sources = sorted(p for p in [*ROOT.glob("src/**/*.c"), *ROOT.glob("src/**/*.cpp"),
                                 *ROOT.glob("lib/src/*.c"), *ROOT.glob("pc/support/*.c")] if p not in replaced)
    print("\n".join("../" + p.relative_to(ROOT).as_posix() for p in sources))


if __name__ == "__main__":
    main()
