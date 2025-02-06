#!/usr/bin/env python3

# Script used to dump case mappings from
# the Unicode Character Database to the `ucaps.h` file.
# NOTE: This script is deliberately not integrated into the build system;
# you should run it manually whenever you want to update the data.

import os
import sys
from typing import Final, List, Tuple
from urllib.request import urlopen

if __name__ == "__main__":
    sys.path.insert(1, os.path.join(os.path.dirname(__file__), "../../"))

from methods import generate_copyright_header

URL: Final[str] = "https://www.unicode.org/Public/16.0.0/ucd/UnicodeData.txt"


lower_to_upper: List[Tuple[str, str]] = []
upper_to_lower: List[Tuple[str, str]] = []


def parse_unicode_data() -> None:
    lines: List[str] = [line.decode("utf-8") for line in urlopen(URL)]

    for line in lines:
        split_line: List[str] = line.split(";")

        code_value: str = split_line[0].strip()
        uppercase_mapping: str = split_line[12].strip()
        lowercase_mapping: str = split_line[13].strip()

        if uppercase_mapping:
            lower_to_upper.append((f"0x{code_value}", f"0x{uppercase_mapping}"))
        if lowercase_mapping:
            upper_to_lower.append((f"0x{code_value}", f"0x{lowercase_mapping}"))


def make_cap_table(table_name: str, len_name: str, table: List[Tuple[str, str]]) -> str:
    result: str = f"static const int {table_name}[{len_name}][2] = {{\n"

    for first, second in table:
        result += f"\t{{ {first}, {second} }},\n"

    result += "};\n\n"

    return result


def generate_ucaps_fetch() -> None:
    parse_unicode_data()

    source: str = generate_copyright_header("ucaps.h")

    source += f"""
#pragma once

// This file was generated using the `misc/scripts/ucaps_fetch.py` script.

#define LTU_LEN {len(lower_to_upper)}
#define UTL_LEN {len(upper_to_lower)}\n\n"""

    source += make_cap_table("caps_table", "LTU_LEN", lower_to_upper)
    source += make_cap_table("reverse_caps_table", "UTL_LEN", upper_to_lower)

    source += """static int _find_upper(int ch) {
\tint low = 0;
\tint high = LTU_LEN - 1;
\tint middle;

\twhile (low <= high) {
\t\tmiddle = (low + high) / 2;

\t\tif (ch < caps_table[middle][0]) {
\t\t\thigh = middle - 1; // Search low end of array.
\t\t} else if (caps_table[middle][0] < ch) {
\t\t\tlow = middle + 1; // Search high end of array.
\t\t} else {
\t\t\treturn caps_table[middle][1];
\t\t}
\t}

\treturn ch;
}

static int _find_lower(int ch) {
\tint low = 0;
\tint high = UTL_LEN - 1;
\tint middle;

\twhile (low <= high) {
\t\tmiddle = (low + high) / 2;

\t\tif (ch < reverse_caps_table[middle][0]) {
\t\t\thigh = middle - 1; // Search low end of array.
\t\t} else if (reverse_caps_table[middle][0] < ch) {
\t\t\tlow = middle + 1; // Search high end of array.
\t\t} else {
\t\t\treturn reverse_caps_table[middle][1];
\t\t}
\t}

\treturn ch;
}
"""

    ucaps_path: str = os.path.join(os.path.dirname(__file__), "../../core/string/ucaps.h")
    with open(ucaps_path, "w", newline="\n") as f:
        f.write(source)

    print("`ucaps.h` generated successfully.")


if __name__ == "__main__":
    generate_ucaps_fetch()
