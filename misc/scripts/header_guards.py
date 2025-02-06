#!/usr/bin/env python3

if __name__ != "__main__":
    raise ImportError(f"{__name__} should not be used as a module.")

import argparse
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../"))

from methods import generate_header_guard, print_error, print_warning, toggle_color


def evaluate_header_guards(path: str) -> int:
    try:
        with open(path, encoding="utf-8", newline="\n") as file:
            lines = file.readlines()
    except OSError:
        print_error(f'Failed to open file "{path}", skipping header guard check.')
        return 1

    if not lines:
        return 0

    header_found = False
    HEADER_CHECK_OFFSET = -1

    for idx, line in enumerate(lines):
        sline = line.lstrip()
        if not header_found:
            if not sline:  # Skip empty lines at the top.
                continue

            if sline.startswith("/**********"):  # Godot header starts this way.
                header_found = True
            else:
                HEADER_CHECK_OFFSET = 0  # There is no Godot header.
                break
        else:
            if not sline.startswith("*") and not sline.startswith("/*"):  # Not in the Godot header anymore.
                HEADER_CHECK_OFFSET = idx + 1  # The include should be two lines below the Godot header.
                break

    if HEADER_CHECK_OFFSET < 0:
        return 0  # Dummy file.

    HEADER_BEGIN_OFFSET = HEADER_CHECK_OFFSET + 1
    HEADER_END_OFFSET = len(lines) - 1

    if HEADER_BEGIN_OFFSET >= HEADER_END_OFFSET:
        return 0  # Dummy file.

    split = path.split("/")  # Already in posix-format.

    prefix = ""
    if split[0] == "modules" and split[-1] == "register_types.h":
        prefix = split[1]  # Name of module.
    elif split[0] == "platform" and (path.endswith("api/api.h") or "/export/" in path):
        prefix = split[1]  # Name of platform.
    elif path.startswith("modules/mono/utils") and "mono" not in split[-1]:
        prefix = "mono"
    elif path == "servers/rendering/storage/utilities.h":
        prefix = "renderer"

    suffix = ""
    if "dummy" in path and not any("dummy" in x for x in (prefix, split[-1])):
        suffix = "dummy"
    elif "gles3" in path and not any("gles3" in x for x in (prefix, split[-1])):
        suffix = "gles3"
    elif "renderer_rd" in path and not any("rd" in x for x in (prefix, split[-1])):
        suffix = "rd"
    elif split[-1] == "ustring.h":
        suffix = "godot"

    header_guard = generate_header_guard(path, prefix, suffix)

    HEADER_CHECK = f"#ifndef {header_guard}\n"
    HEADER_BEGIN = f"#define {header_guard}\n"
    HEADER_END = f"#endif // {header_guard}\n"

    if (
        lines[HEADER_CHECK_OFFSET] == HEADER_CHECK
        and lines[HEADER_BEGIN_OFFSET] == HEADER_BEGIN
        and lines[HEADER_END_OFFSET] == HEADER_END
    ):
        return 0

    # Guards might exist but with the wrong names.
    if (
        lines[HEADER_CHECK_OFFSET].startswith("#ifndef")
        and lines[HEADER_BEGIN_OFFSET].startswith("#define")
        and lines[HEADER_END_OFFSET].startswith("#endif")
    ):
        lines[HEADER_CHECK_OFFSET] = HEADER_CHECK
        lines[HEADER_BEGIN_OFFSET] = HEADER_BEGIN
        lines[HEADER_END_OFFSET] = HEADER_END
        try:
            with open(path, "w", encoding="utf-8", newline="\n") as file:
                file.writelines(lines)
            print_warning(f'File "{path}" had improper header guards. Fixed!')
        except OSError:
            print_error(f'Failed to open file "{path}", aborting header guard fix.')
        return 1

    header_check = -1
    header_begin = -1
    header_end = -1
    pragma_once = -1

    for idx, line in enumerate(lines):
        if not line.startswith("#"):
            continue
        elif line.startswith("#ifndef") and header_check == -1:
            header_check = idx
        elif line.startswith("#define") and header_begin == -1:
            header_begin = idx
        elif line.startswith("#endif"):
            header_end = idx
        elif line.startswith("#pragma once"):
            pragma_once = idx
            break

    if pragma_once != -1:
        lines.pop(pragma_once)
        lines.insert(HEADER_CHECK_OFFSET, HEADER_CHECK + HEADER_BEGIN + "\n")
        lines.append("\n" + HEADER_END)
        try:
            with open(path, "w", encoding="utf-8", newline="\n") as file:
                file.writelines(lines)
            print_warning(f'File "{path}" used `#pragma once` instead of header guards. Fixed!')
        except OSError:
            print_error(f'Failed to open file "{path}", aborting header guard fix.')
        return 1

    if header_check == -1 and header_begin == -1 and header_end == -1:
        # Guards simply didn't exist.
        lines.insert(HEADER_CHECK_OFFSET, HEADER_CHECK + HEADER_BEGIN + "\n")
        lines.append("\n" + HEADER_END)
        try:
            with open(path, "w", encoding="utf-8", newline="\n") as file:
                file.writelines(lines)
            print_warning(f'File "{path}" lacked header guards. Fixed!')
        except OSError:
            print_error(f'Failed to open file "{path}", aborting header guard fix.')
        return 1

    print_error(f'File "{path}" has invalid header guards, requires manual changes.')
    return 1


def main() -> int:
    parser = argparse.ArgumentParser(prog="header-guards", description="Ensure header files have valid header guards.")
    parser.add_argument("files", nargs="+", help="Paths to files for header guard evaluation.")
    parser.add_argument("-c", "--color", action="store_true", help="If passed, force colored output.")
    args = parser.parse_args()

    if args.color:
        toggle_color(True)

    ret = 0
    for file in args.files:
        ret += evaluate_header_guards(file)
    return ret


try:
    sys.exit(main())
except KeyboardInterrupt:
    import signal

    signal.signal(signal.SIGINT, signal.SIG_DFL)
    os.kill(os.getpid(), signal.SIGINT)
