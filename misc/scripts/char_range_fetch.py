#!/usr/bin/env python3

# Script used to dump char ranges for specific properties from
# the Unicode Character Database to the `char_range.inc` file.
# NOTE: This script is deliberately not integrated into the build system;
# you should run it manually whenever you want to update the data.

import os
import sys
from typing import Final, List, Tuple
from urllib.request import urlopen

if __name__ == "__main__":
    sys.path.insert(1, os.path.join(os.path.dirname(__file__), "../../"))

from methods import generate_copyright_header

URL: Final[str] = "https://www.unicode.org/Public/16.0.0/ucd/DerivedCoreProperties.txt"


xid_start: List[Tuple[int, int]] = []
xid_continue: List[Tuple[int, int]] = []
uppercase_letter: List[Tuple[int, int]] = []
lowercase_letter: List[Tuple[int, int]] = []
unicode_letter: List[Tuple[int, int]] = []


def merge_ranges(ranges: List[Tuple[int, int]]) -> None:
    if len(ranges) < 2:
        return

    last_start: int = ranges[0][0]
    last_end: int = ranges[0][1]
    original_ranges: List[Tuple[int, int]] = ranges[1:]

    ranges.clear()

    for curr_range in original_ranges:
        curr_start: int = curr_range[0]
        curr_end: int = curr_range[1]
        if last_end + 1 != curr_start:
            ranges.append((last_start, last_end))
            last_start = curr_start
        last_end = curr_end

    ranges.append((last_start, last_end))


def parse_unicode_data() -> None:
    lines: List[str] = [line.decode("utf-8") for line in urlopen(URL)]

    for line in lines:
        if line.startswith("#") or not line.strip():
            continue

        split_line: List[str] = line.split(";")

        char_range: str = split_line[0].strip()
        char_property: str = split_line[1].strip().split("#")[0].strip()

        range_start: str = char_range
        range_end: str = char_range
        if ".." in char_range:
            range_start, range_end = char_range.split("..")

        range_tuple: Tuple[int, int] = (int(range_start, 16), int(range_end, 16))

        if char_property == "XID_Start":
            xid_start.append(range_tuple)
        elif char_property == "XID_Continue":
            xid_continue.append(range_tuple)
        elif char_property == "Uppercase":
            uppercase_letter.append(range_tuple)
        elif char_property == "Lowercase":
            lowercase_letter.append(range_tuple)
        elif char_property == "Alphabetic":
            unicode_letter.append(range_tuple)

    # Underscore technically isn't in XID_Start, but for our purposes it's included.
    xid_start.append((0x005F, 0x005F))
    xid_start.sort(key=lambda x: x[0])

    merge_ranges(xid_start)
    merge_ranges(xid_continue)
    merge_ranges(uppercase_letter)
    merge_ranges(lowercase_letter)
    merge_ranges(unicode_letter)


def make_array(array_name: str, range_list: List[Tuple[int, int]]) -> str:
    result: str = f"constexpr inline CharRange {array_name}[] = {{\n"

    for start, end in range_list:
        result += f"\t{{ 0x{start:x}, 0x{end:x} }},\n"

    result += "};\n\n"

    return result


def generate_char_range_inc() -> None:
    parse_unicode_data()

    source: str = generate_copyright_header("char_range.inc")

    source += f"""
// This file was generated using the `misc/scripts/char_range_fetch.py` script.

#ifndef CHAR_RANGE_INC
#define CHAR_RANGE_INC

#include "core/typedefs.h"

// Unicode Derived Core Properties
// Source: {URL}

struct CharRange {{
\tchar32_t start;
\tchar32_t end;
}};\n\n"""

    source += make_array("xid_start", xid_start)
    source += make_array("xid_continue", xid_continue)
    source += make_array("uppercase_letter", uppercase_letter)
    source += make_array("lowercase_letter", lowercase_letter)
    source += make_array("unicode_letter", unicode_letter)

    source += "#endif // CHAR_RANGE_INC\n"

    char_range_path: str = os.path.join(os.path.dirname(__file__), "../../core/string/char_range.inc")
    with open(char_range_path, "w", newline="\n") as f:
        f.write(source)

    print("`char_range.inc` generated successfully.")


if __name__ == "__main__":
    generate_char_range_inc()
