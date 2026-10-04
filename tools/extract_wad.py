#!/usr/bin/env python3

###
# Extracts the files needed for the build from a News Channel WAD.
#
# Usage:
#   python3 tools/extract_wad.py "path/to/News Channel (USA) (v7) (Channel).wad"
#   python3 tools/extract_wad.py --contents "path/to/News Channel (USA) (v7) (Channel).wad"
#
# Without options only the main DOL is written (orig/HAGE/sys/main.dol), which
# is all the matching (Wii) build needs.
#
# With --contents the channel's other contents are written too, as
# orig/HAGE/contents/NN.app (NN = content index). The native PC build loads
# them at run time (see docs/pc_port.md). They are stored exactly as they are
# in the WAD (contents 2 to 10 are U8 archives).
#
# Everything under orig/ is ignored by git. Never commit extracted files.
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

# Contents the program reads at run time: index -> (SHA-1, what it holds).
# The game's "archive number" n is content index n + 2 (SystemInit opens
# contents 6..11 as archives 4..9); see docs/pc_port_readiness.md, section 6.2.
RUNTIME_CONTENTS = {
    0: ("f982c7d40f154e13cb38bc2a72d568f2c5638fef", "channel banner (not read by the game)"),
    2: ("f5dd17b3200dd4d6be2b25b577c99e5f941b7324", "Operations Guide viewer module (wwwlib-rvl.lz7, PowerPC RSO)"),
    3: ("32b339cbbb507d502779259a7866995d030b1d88", "Operations Guide viewer font (WiiNTLG-Regular.ttc)"),
    4: ("a097c92839270470aaa51396b758ff65b2d1aedc", "small archive (not opened by the game code)"),
    5: ("310a965fb2ea8068f3cc0806b51689d721cae6e6", "small archive (not opened by the game code)"),
    6: ("3644384b0cfc920ef620120e8721bf53c85bda27", "HOME Menu layouts and sounds (HomeButton3/)"),
    7: ("4fad97fd4a288c47e0587f3bbd292379f8709eb9", "archive fonts (wbf1.brfna, wbf2.brfna)"),
    8: ("9f43cba39269655819f49e9e47dae734ffc03ec7", "globe model (earth.brres.LZ)"),
    9: ("331c506e42fd626ee18b8b81bc481bb453753e1a", "main assets: layouts, textures, fonts, effects, sound"),
    10: ("bc8f38c22eee0288593bcb8619908743c9028aa9", "Operations Guide pages (html-*.arc)"),
    11: ("33ac4d5a49137e8730ea463a4c83626cf2809f4a", "opened by the game, no file named in the source"),
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
    parser.add_argument(
        "--contents",
        action="store_true",
        help="also write the run-time contents to <out>/contents/NN.app (for the PC build)",
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

    if args.contents:
        out_dir = args.out / "contents"
        out_dir.mkdir(parents=True, exist_ok=True)
        present = {record.index for record in title.tmd.content_records}
        for index, (sha1, description) in RUNTIME_CONTENTS.items():
            if index not in present:
                sys.exit(f"Content {index} is missing from the WAD")
            data = title.get_content_by_index(index)
            digest = hashlib.sha1(data).hexdigest()
            if digest != sha1:
                sys.exit(f"Content {index} hash mismatch: {digest} != {sha1}")
            out_path = out_dir / f"{index:02d}.app"
            out_path.write_bytes(data)
            print(f"Wrote {out_path} ({len(data)} bytes): {description}")


if __name__ == "__main__":
    main()
