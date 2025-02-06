#!/usr/bin/env python3

if __name__ != "__main__":
    raise ImportError(f"{__name__} should not be used as a module.")

import argparse
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../"))

from methods import generate_copyright_header, print_error, print_warning, toggle_color


def evaluate_header(path: str) -> int:
    try:
        with open(path, encoding="utf-8", newline="\n") as file:
            header = generate_copyright_header(path)
            synced = True
            for line in header.splitlines(True):
                if line != file.readline():
                    synced = False
                    break
            if synced:
                return 0

            # Header is mangled or missing; remove all empty/commented lines prior to content.
            content = header
            file.seek(0)
            for line in file:
                if line == "\n" or line.startswith("/*"):
                    continue
                content += f"\n{line}"
                break
            content += file.read()

        with open(path, "w", encoding="utf-8", newline="\n") as file:
            file.write(content)

        print_warning(f'File "{path}" had an improper header. Fixed!')
        return 1
    except OSError:
        print_error(f'Failed to open file "{path}", skipping header check.')
        return 1


def main() -> int:
    parser = argparse.ArgumentParser(prog="copyright-headers", description="Ensure files have valid copyright headers.")
    parser.add_argument("files", nargs="+", help="Paths to files for copyright header evaluation.")
    parser.add_argument("-c", "--color", action="store_true", help="If passed, force colored output.")
    args = parser.parse_args()

    if args.color:
        toggle_color(True)

    ret = 0
    for file in args.files:
        ret += evaluate_header(file)
    return ret


try:
    sys.exit(main())
except KeyboardInterrupt:
    import signal

    signal.signal(signal.SIGINT, signal.SIG_DFL)
    os.kill(os.getpid(), signal.SIGINT)
