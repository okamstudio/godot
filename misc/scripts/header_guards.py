#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
from pathlib import Path

if len(sys.argv) < 2:
    print("Invalid usage of header_guards.py, it should be called with a path to one or multiple files.")
    sys.exit(1)

changed = []
invalid = []

for file in sys.argv[1:]:
    header_start = -1
    HEADER_CHECK_OFFSET = -1

    with open(file.strip(), "rt", encoding="utf-8", newline="\n") as f:
        lines = f.readlines()

    for idx, line in enumerate(lines):
        sline = line.strip()

        if header_start < 0:
            if sline == "":  # Skip empty lines at the top.
                continue

            if sline.startswith("/**********"):  # Godot header starts this way.
                header_start = idx
            else:
                HEADER_CHECK_OFFSET = 0  # There is no Godot header.
                break
        else:
            if not sline.startswith("*") and not sline.startswith("/*"):  # Not in the Godot header anymore.
                HEADER_CHECK_OFFSET = idx + 1  # The include should be two lines below the Godot header.
                break

    if HEADER_CHECK_OFFSET < 0:
        invalid.append(file)
        continue

    HEADER_BEGIN_OFFSET = HEADER_CHECK_OFFSET + 1
    HEADER_END_OFFSET = len(lines) - 1

    if HEADER_BEGIN_OFFSET >= HEADER_END_OFFSET:
        invalid.append(file)
        continue

    split = file.split("/")  # Already in posix-format.

    prefix = ""
    if split[0] == "modules" and split[-1] == "register_types.h":
        prefix = f"{split[1]}_"  # Name of module.
    elif split[0] == "platform" and (file.endswith("api/api.h") or "/export/" in file):
        prefix = f"{split[1]}_"  # Name of platform.
    elif file.startswith("modules/mono/utils") and "mono" not in split[-1]:
        prefix = "MONO_"
    elif file == "servers/rendering/storage/utilities.h":
        prefix = "RENDERER_"

    suffix = ""
    if "dummy" in file and "dummy" not in split[-1]:
        suffix = "_DUMMY"
    elif "gles3" in file and "gles3" not in split[-1]:
        suffix = "_GLES3"
    elif "renderer_rd" in file and "rd" not in split[-1]:
        suffix = "_RD"
    elif split[-1] == "ustring.h":
        suffix = "_GODOT"

    name = (f"{prefix}{Path(file).stem}{suffix}{Path(file).suffix}".upper()
            .replace(".", "_").replace("-", "_").replace(" ", "_"))  # fmt: skip

    HEADER_CHECK = f"#ifndef {name}\n"
    HEADER_BEGIN = f"#define {name}\n"
    HEADER_END = f"#endif // {name}\n"

    if (
        lines[HEADER_CHECK_OFFSET] == HEADER_CHECK
        and lines[HEADER_BEGIN_OFFSET] == HEADER_BEGIN
        and lines[HEADER_END_OFFSET] == HEADER_END
    ):
        continue

    # Guards might exist but with the wrong names.
    if (
        lines[HEADER_CHECK_OFFSET].startswith("#ifndef")
        and lines[HEADER_BEGIN_OFFSET].startswith("#define")
        and lines[HEADER_END_OFFSET].startswith("#endif")
    ):
        lines[HEADER_CHECK_OFFSET] = HEADER_CHECK
        lines[HEADER_BEGIN_OFFSET] = HEADER_BEGIN
        lines[HEADER_END_OFFSET] = HEADER_END
        with open(file, "wt", encoding="utf-8", newline="\n") as f:
            f.writelines(lines)
        changed.append(file)
        continue

    header_check = -1
    header_begin = -1
    header_end = -1
    pragma_once = -1
    objc = False

    for idx, line in enumerate(lines):
        if line.startswith("// #import"):  # Some dummy obj-c files only have commented out import lines.
            objc = True
            break
        if not line.startswith("#"):
            continue
        elif line.startswith("#ifndef") and header_check == -1:
            header_check = idx
        elif line.startswith("#define") and header_begin == -1:
            header_begin = idx
        elif line.startswith("#endif") and header_end == -1:
            header_end = idx
        elif line.startswith("#pragma once"):
            pragma_once = idx
            break
        elif line.startswith("#import"):
            objc = True
            break

    if objc:
        continue

    if pragma_once != -1:
        lines.pop(pragma_once)
        lines.insert(HEADER_CHECK_OFFSET, HEADER_CHECK)
        lines.insert(HEADER_BEGIN_OFFSET, HEADER_BEGIN)
        lines.append("\n")
        lines.append(HEADER_END)
        with open(file, "wt", encoding="utf-8", newline="\n") as f:
            f.writelines(lines)
        changed.append(file)
        continue

    if header_check == -1 and header_begin == -1 and header_end == -1:
        # Guards simply didn't exist
        lines.insert(HEADER_CHECK_OFFSET, HEADER_CHECK)
        lines.insert(HEADER_BEGIN_OFFSET, HEADER_BEGIN)
        lines.append("\n")
        lines.append(HEADER_END)
        with open(file, "wt", encoding="utf-8", newline="\n") as f:
            f.writelines(lines)
        changed.append(file)
        continue

    if header_check != -1 and header_begin != -1 and header_end != -1:
        # All prepends "found", see if we can salvage this.
        if header_check == header_begin - 1 and header_begin < header_end:
            lines.pop(header_check)
            lines.pop(header_begin - 1)
            lines.pop(header_end - 2)
            if lines[header_end - 3] == "\n":
                lines.pop(header_end - 3)
            lines.insert(HEADER_CHECK_OFFSET, HEADER_CHECK)
            lines.insert(HEADER_BEGIN_OFFSET, HEADER_BEGIN)
            lines.append("\n")
            lines.append(HEADER_END)
            with open(file, "wt", encoding="utf-8", newline="\n") as f:
                f.writelines(lines)
            changed.append(file)
            continue

    invalid.append(file)

if changed:
    for file in changed:
        print(f"FIXED: {file}")
if invalid:
    for file in invalid:
        print(f"REQUIRES MANUAL CHANGES: {file}")
    sys.exit(1)
