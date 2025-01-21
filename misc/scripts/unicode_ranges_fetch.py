#!/usr/bin/env python3

# Script used to dump char ranges from
# the Unicode Character Database to the `char_range.inc` file.
# NOTE: This script is deliberately not integrated into the build system;
# you should run it manually whenever you want to update the data.

import os
import sys
from typing import Final, List, Set, Tuple
from urllib.request import urlopen

if __name__ == "__main__":
    sys.path.insert(1, os.path.join(os.path.dirname(__file__), "../../"))

from methods import generate_copyright_header

URL: Final[str] = "https://www.unicode.org/Public/16.0.0/ucd/Blocks.txt"


ranges: List[Tuple[str, str, str]] = []

exclude_blocks: Set[str] = {
    "High Surrogates",
    "High Private Use Surrogates",
    "Low Surrogates",
    "Variation Selectors",
    "Specials",
    "Egyptian Hieroglyph Format Controls",
    "Tags",
    "Variation Selectors Supplement",
}


def parse_unicode_data() -> None:
    lines: List[str] = [line.decode("utf-8") for line in urlopen(URL)]

    for line in lines:
        if line.startswith("#") or not line.strip():
            continue

        split_line: List[str] = line.split(";")

        char_range: str = split_line[0].strip()
        block: str = split_line[1].strip()

        if block in exclude_blocks:
            continue

        range_start, range_end = char_range.split("..")

        ranges.append((f"0x{range_start}", f"0x{range_end}", block))


def make_array(array_name: str, ranges: List[Tuple[str, str, str]]) -> str:
    result: str = f"static UniRange {array_name}[] = {{\n"

    for start, end, block in ranges:
        result += f'\t{{ {start}, {end}, U"{block}" }},\n'

    result += """\t{ 0x10FFFF, 0x10FFFF, String() }
};\n\n"""

    return result


def generate_unicode_ranges_inc() -> None:
    parse_unicode_data()

    source: str = generate_copyright_header("unicode_ranges.inc")

    source += f"""
// This file was generated using the `misc/scripts/unicode_ranges_fetch.py` script.

#ifndef UNICODE_RANGES_INC
#define UNICODE_RANGES_INC

// Unicode Character Blocks
// Source: {URL}

struct UniRange {{
\tint32_t start;
\tint32_t end;
\tString name;
}};\n\n"""

    source += make_array("unicode_ranges", ranges)

    source += "#endif // UNICODE_RANGES_INC\n"

    unicode_ranges_path: str = os.path.join(os.path.dirname(__file__), "../../editor/import/unicode_ranges.inc")
    with open(unicode_ranges_path, "w", newline="\n") as f:
        f.write(source)

    print("`unicode_ranges.inc` generated successfully.")


if __name__ == "__main__":
    generate_unicode_ranges_inc()
