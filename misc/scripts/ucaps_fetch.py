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


def fetch_unicode_data() -> Tuple[List[Tuple[str, str]], List[Tuple[str, str]]]:
    lines: List[str] = [line.decode("utf-8") for line in urlopen(URL)]

    lower_to_upper = []
    upper_to_lower = []

    for line in lines:
        split_line: List[str] = line.split(";")

        code_value: str = split_line[0].strip()
        uppercase_mapping: str = split_line[12].strip()
        lowercase_mapping: str = split_line[13].strip()

        if uppercase_mapping:
            lower_to_upper.append((f"0x{code_value}", f"0x{uppercase_mapping}"))
        if lowercase_mapping:
            upper_to_lower.append((f"0x{code_value}", f"0x{lowercase_mapping}"))

    return lower_to_upper, upper_to_lower


def make_cap_table(table_name: str, table: List[Tuple[str, str]], starting_from: int) -> str:
    result: str = f"static const char32_t {table_name}[][2] = {{\n"

    for first, second in table:
        if int(first, 16) < starting_from:
            continue
        result += f"\t{{ {first}, {second} }},\n"

    result += "};\n\n"

    return result


def generate_ucaps_fetch() -> None:
    lower_to_upper, upper_to_lower = fetch_unicode_data()

    source: str = generate_copyright_header("ucaps.h")

    source += """
#ifndef UCAPS_H
#define UCAPS_H

// This file was generated using the `misc/scripts/ucaps_fetch.py` script.

"""

    # We skip the lower bit characters because they are handled with a manual if statement.
    source += make_cap_table("caps_table", lower_to_upper, starting_from=0x00FF)
    source += make_cap_table("reverse_caps_table", upper_to_lower, starting_from=0x0100)

    source += "#endif // UCAPS_H\n"

    ucaps_path: str = os.path.join(os.path.dirname(__file__), "../../core/string/ucaps.h")
    with open(ucaps_path, "w", newline="\n") as f:
        f.write(source)

    print("`ucaps.h` generated successfully.")


if __name__ == "__main__":
    generate_ucaps_fetch()
