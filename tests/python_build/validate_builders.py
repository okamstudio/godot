#!/usr/bin/env python3
if __name__ != "__main__":
    raise ImportError(f"{__name__} should not be used as a module.")

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../"))

from gles3_builders import build_gles3_header
from glsl_builders import build_raw_header, build_rd_header

FUNC_PATH_PAIR = [
    (build_gles3_header, "tests/python_build/fixtures/gles3/vertex_fragment.glsl"),
    (build_raw_header, "tests/python_build/fixtures/glsl/compute.glsl"),
    (build_raw_header, "tests/python_build/fixtures/glsl/vertex_fragment.glsl"),
    (build_rd_header, "tests/python_build/fixtures/rd_glsl/compute.glsl"),
    (build_rd_header, "tests/python_build/fixtures/rd_glsl/vertex_fragment.glsl"),
]


def main() -> int:
    ret = 0

    for func, path in FUNC_PATH_PAIR:
        if os.path.exists(out_path := os.path.abspath(path + ".gen.h")):
            with open(out_path, "rb") as file:
                raw = file.read()
            func(path)
            with open(out_path, "rb") as file:
                if raw != file.read():
                    ret += 1
        else:
            func(path)
            ret += 1

    return ret


sys.exit(main())
