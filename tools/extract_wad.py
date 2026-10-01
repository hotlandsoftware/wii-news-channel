#!/usr/bin/env python3

###
# Extracts the files needed for the build from a News Channel WAD.
#
# Usage:
#   python3 tools/extract_wad.py "path/to/News Channel (USA) (v7) (Channel).wad"
#
# Requires libWiiPy (pip install libWiiPy) for WAD decryption.
###

import argparse
import hashlib
import sys
from pathlib import Path

# Title ID 00010002-HAGE, version 7
EXPECTED_TITLE_ID = "0001000248414745"
EXPECTED_VERSION = 7

# Content index -> (output path, SHA-1)
CONTENTS = {
    1: ("sys/main.dol", "1bb643fa5f5e11930849b5efd514f08d30ead975"),
}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("wad", type=Path, help="path to the News Channel WAD")
    parser.add_argument(
        "--out",
        type=Path,
        default=Path(__file__).parent.parent / "orig" / "HAGE",
        help="output directory (default: orig/HAGE)",
    )
    args = parser.parse_args()

    try:
        import libWiiPy
    except ImportError:
        sys.exit("libWiiPy is required: pip install libWiiPy")

    title = libWiiPy.title.Title()
    title.load_wad(args.wad.read_bytes())

    title_id = title.tmd.title_id
    if isinstance(title_id, int):
        title_id = f"{title_id:016x}"
    if title_id.lower() != EXPECTED_TITLE_ID or title.tmd.title_version != EXPECTED_VERSION:
        sys.exit(
            f"Unexpected title {title_id} v{title.tmd.title_version}, "
            f"expected {EXPECTED_TITLE_ID} v{EXPECTED_VERSION}"
        )

    for index, (path, sha1) in CONTENTS.items():
        data = title.get_content_by_index(index)
        digest = hashlib.sha1(data).hexdigest()
        if digest != sha1:
            sys.exit(f"Content {index} hash mismatch: {digest} != {sha1}")
        out_path = args.out / path
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_bytes(data)
        print(f"Wrote {out_path}")


if __name__ == "__main__":
    main()
