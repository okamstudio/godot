#!/usr/bin/env python3

if __name__ != "__main__":
    raise ImportError(f"{__name__} should not be used as a module.")

import argparse
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../"))

from methods import print_error, print_warning, toggle_color


def evaluate_formatting(path: str) -> int:
    try:
        with open(path, "rb") as file:
            raw = file.read()

        if not raw:
            return 0

        # TODO: Replace hardcoded choices by parsing relevant `.gitattributes`/`.editorconfig`.
        EOL = "\r\n" if path.endswith((".csproj", ".sln", ".bat")) or path.startswith("misc/msvs") else "\n"
        WANTS_BOM = path.endswith((".csproj", ".sln"))

        reformat_decode = EOL.join([line.rstrip() for line in raw.decode("utf-8-sig").splitlines()]).rstrip() + EOL
        reformat_encode = reformat_decode.encode("utf-8-sig" if WANTS_BOM else "utf-8")

        if raw == reformat_encode:
            return 0

        with open(path, "wb") as file:
            file.write(reformat_encode)

        print_warning(f'File "{path}" had improper formatting. Fixed!')
        return 1
    except OSError:
        print_error(f'Failed to open file "{path}", skipping format.')
        return 1
    except UnicodeDecodeError:
        print_error(f'File at "{path}" is not UTF-8, requires manual changes.')
        return 1


def main() -> int:
    parser = argparse.ArgumentParser(
        prog="file-format", description="Ensure files have proper formatting (newlines, encoding, etc)."
    )
    parser.add_argument("files", nargs="+", help="Paths to files for formatting.")
    parser.add_argument("-c", "--color", action="store_true", help="If passed, force colored output.")
    args = parser.parse_args()

    if args.color:
        toggle_color(True)

    ret = 0
    for file in args.files:
        ret += evaluate_formatting(file)
    return ret


try:
    sys.exit(main())
except KeyboardInterrupt:
    import signal

    signal.signal(signal.SIGINT, signal.SIG_DFL)
    os.kill(os.getpid(), signal.SIGINT)
