"""Functions used to generate source files during build time"""

import os.path
from typing import Optional

from methods import print_error, to_raw_cstring


class RDHeaderStruct:
    def __init__(self):
        self.vertex_lines = []
        self.fragment_lines = []
        self.compute_lines = []
        self.raygen_lines = []
        self.any_hit_lines = []
        self.closest_hit_lines = []
        self.miss_lines = []
        self.intersection_lines = []

        self.vertex_included_files = []
        self.fragment_included_files = []
        self.compute_included_files = []
        self.raygen_included_files = []
        self.any_hit_included_files = []
        self.closest_hit_included_files = []
        self.miss_included_files = []
        self.intersection_included_files = []

        self.reading = ""
        self.line_offset = 0
        self.vertex_offset = 0
        self.fragment_offset = 0
        self.compute_offset = 0
        self.raygen_offset = 0
        self.any_hit_offset = 0
        self.closest_hit_offset = 0
        self.miss_offset = 0
        self.intersection_offset = 0


def include_file_in_rd_header(filename: str, header_data: RDHeaderStruct, depth: int) -> RDHeaderStruct:
    with open(filename, "r", encoding="utf-8") as fs:
        line = fs.readline()

        while line:
            index = line.find("//")
            if index != -1:
                line = line[:index]

            if line.find("#[vertex]") != -1:
                header_data.reading = "vertex"
                line = fs.readline()
                header_data.line_offset += 1
                header_data.vertex_offset = header_data.line_offset
                continue

            if line.find("#[fragment]") != -1:
                header_data.reading = "fragment"
                line = fs.readline()
                header_data.line_offset += 1
                header_data.fragment_offset = header_data.line_offset
                continue

            if line.find("#[compute]") != -1:
                header_data.reading = "compute"
                line = fs.readline()
                header_data.line_offset += 1
                header_data.compute_offset = header_data.line_offset
                continue

            if line.find("#[raygen]") != -1:
                header_data.reading = "raygen"
                line = fs.readline()
                header_data.line_offset += 1
                header_data.raygen_offset = header_data.line_offset
                continue

            if line.find("#[any_hit]") != -1:
                header_data.reading = "any_hit"
                line = fs.readline()
                header_data.line_offset += 1
                header_data.any_hit_offset = header_data.line_offset
                continue

            if line.find("#[closest_hit]") != -1:
                header_data.reading = "closest_hit"
                line = fs.readline()
                header_data.line_offset += 1
                header_data.closest_hit_offset = header_data.line_offset
                continue

            if line.find("#[miss]") != -1:
                header_data.reading = "miss"
                line = fs.readline()
                header_data.line_offset += 1
                header_data.miss_offset = header_data.line_offset
                continue

            if line.find("#[intersection]") != -1:
                header_data.reading = "intersection"
                line = fs.readline()
                header_data.line_offset += 1
                header_data.intersection_offset = header_data.line_offset
                continue

            while line.find("#include ") != -1:
                includeline = line.replace("#include ", "").strip()[1:-1]

                if includeline.startswith("thirdparty/"):
                    included_file = os.path.relpath(includeline)

                else:
                    included_file = os.path.relpath(os.path.dirname(filename) + "/" + includeline)

                if included_file not in header_data.vertex_included_files and header_data.reading == "vertex":
                    header_data.vertex_included_files += [included_file]
                    if include_file_in_rd_header(included_file, header_data, depth + 1) is None:
                        print_error(f'In file "{filename}": #include "{includeline}" could not be found!"')
                elif included_file not in header_data.fragment_included_files and header_data.reading == "fragment":
                    header_data.fragment_included_files += [included_file]
                    if include_file_in_rd_header(included_file, header_data, depth + 1) is None:
                        print_error(f'In file "{filename}": #include "{includeline}" could not be found!"')
                elif included_file not in header_data.compute_included_files and header_data.reading == "compute":
                    header_data.compute_included_files += [included_file]
                    if include_file_in_rd_header(included_file, header_data, depth + 1) is None:
                        print_error(f'In file "{filename}": #include "{includeline}" could not be found!"')
                elif included_file not in header_data.raygen_included_files and header_data.reading == "raygen":
                    header_data.raygen_included_files += [included_file]
                    if include_file_in_rd_header(included_file, header_data, depth + 1) is None:
                        print_error(f'In file "{filename}": #include "{includeline}" could not be found!"')
                elif included_file not in header_data.any_hit_included_files and header_data.reading == "any_hit":
                    header_data.any_hit_included_files += [included_file]
                    if include_file_in_rd_header(included_file, header_data, depth + 1) is None:
                        print_error(f'In file "{filename}": #include "{includeline}" could not be found!"')
                elif (
                    included_file not in header_data.closest_hit_included_files and header_data.reading == "closest_hit"
                ):
                    header_data.closest_hit_included_files += [included_file]
                    if include_file_in_rd_header(included_file, header_data, depth + 1) is None:
                        print_error(f'In file "{filename}": #include "{includeline}" could not be found!"')
                elif included_file not in header_data.miss_included_files and header_data.reading == "miss":
                    header_data.miss_included_files += [included_file]
                    if include_file_in_rd_header(included_file, header_data, depth + 1) is None:
                        print_error(f'In file "{filename}": #include "{includeline}" could not be found!"')
                elif (
                    included_file not in header_data.intersection_included_files
                    and header_data.reading == "intersection"
                ):
                    header_data.intersection_included_files += [included_file]
                    if include_file_in_rd_header(included_file, header_data, depth + 1) is None:
                        print_error(f'In file "{filename}": #include "{includeline}" could not be found!"')

                line = fs.readline()

            line = line.replace("\r", "").replace("\n", "")

            if header_data.reading == "vertex":
                header_data.vertex_lines += [line]
            if header_data.reading == "fragment":
                header_data.fragment_lines += [line]
            if header_data.reading == "compute":
                header_data.compute_lines += [line]
            if header_data.reading == "raygen":
                header_data.raygen_lines += [line]
            if header_data.reading == "any_hit":
                header_data.any_hit_lines += [line]
            if header_data.reading == "closest_hit":
                header_data.closest_hit_lines += [line]
            if header_data.reading == "miss":
                header_data.miss_lines += [line]
            if header_data.reading == "intersection":
                header_data.intersection_lines += [line]

            line = fs.readline()
            header_data.line_offset += 1

    return header_data


def build_rd_header(
    filename: str, optional_output_filename: Optional[str] = None, header_data: Optional[RDHeaderStruct] = None
) -> None:
    header_data = header_data or RDHeaderStruct()
    include_file_in_rd_header(filename, header_data, 0)

    if optional_output_filename is None:
        out_file = filename + ".gen.h"
    else:
        out_file = optional_output_filename

    out_file_base = out_file
    out_file_base = out_file_base[out_file_base.rfind("/") + 1 :]
    out_file_base = out_file_base[out_file_base.rfind("\\") + 1 :]
    out_file_ifdef = out_file_base.replace(".", "_").upper()
    out_file_class = out_file_base.replace(".glsl.gen.h", "").title().replace("_", "").replace(".", "") + "ShaderRD"

    if header_data.raygen_lines:
        body_parts = [
            "static const char _raygen_code[] = {\n%s\n\t\t};" % to_raw_cstring(header_data.raygen_lines),
            "static const char _any_hit_code[] = {\n%s\n\t\t};" % to_raw_cstring(header_data.any_hit_lines),
            "static const char _closest_hit_code[] = {\n%s\n\t\t};" % to_raw_cstring(header_data.closest_hit_lines),
            "static const char _miss_code[] = {\n%s\n\t\t};" % to_raw_cstring(header_data.miss_lines),
            "static const char _intersection_code[] = {\n%s\n\t\t};" % to_raw_cstring(header_data.intersection_lines),
            f'setup_raytracing(_raygen_code, _any_hit_code, _closest_hit_code, _miss_code, _intersection_code, "{out_file_class}");',
        ]
    elif header_data.compute_lines:
        body_parts = [
            "static const char _compute_code[] = {\n%s\n\t\t};" % to_raw_cstring(header_data.compute_lines),
            f'setup(nullptr, nullptr, _compute_code, "{out_file_class}");',
        ]
    else:
        body_parts = [
            "static const char _vertex_code[] = {\n%s\n\t\t};" % to_raw_cstring(header_data.vertex_lines),
            "static const char _fragment_code[] = {\n%s\n\t\t};" % to_raw_cstring(header_data.fragment_lines),
            f'setup(_vertex_code, _fragment_code, nullptr, "{out_file_class}");',
        ]

    body_content = "\n\t\t".join(body_parts)

    # Intended curly brackets are doubled so f-string doesn't eat them up.
    shader_template = f"""/* WARNING, THIS FILE WAS GENERATED, DO NOT EDIT */
#ifndef {out_file_ifdef}_RD
#define {out_file_ifdef}_RD

#include "servers/rendering/renderer_rd/shader_rd.h"

class {out_file_class} : public ShaderRD {{

public:

	{out_file_class}() {{

		{body_content}
	}}
}};

#endif
"""

    with open(out_file, "w", encoding="utf-8", newline="\n") as fd:
        fd.write(shader_template)


def build_rd_headers(target, source, env):
    env.NoCache(target)
    for x in source:
        build_rd_header(filename=str(x))


class RAWHeaderStruct:
    def __init__(self):
        self.code = ""


def include_file_in_raw_header(filename: str, header_data: RAWHeaderStruct, depth: int) -> None:
    with open(filename, "r", encoding="utf-8") as fs:
        line = fs.readline()

        while line:
            while line.find("#include ") != -1:
                includeline = line.replace("#include ", "").strip()[1:-1]

                included_file = os.path.relpath(os.path.dirname(filename) + "/" + includeline)
                include_file_in_raw_header(included_file, header_data, depth + 1)

                line = fs.readline()

            header_data.code += line
            line = fs.readline()


def build_raw_header(
    filename: str, optional_output_filename: Optional[str] = None, header_data: Optional[RAWHeaderStruct] = None
):
    header_data = header_data or RAWHeaderStruct()
    include_file_in_raw_header(filename, header_data, 0)

    if optional_output_filename is None:
        out_file = filename + ".gen.h"
    else:
        out_file = optional_output_filename

    out_file_base = out_file.replace(".glsl.gen.h", "_shader_glsl")
    out_file_base = out_file_base[out_file_base.rfind("/") + 1 :]
    out_file_base = out_file_base[out_file_base.rfind("\\") + 1 :]
    out_file_ifdef = out_file_base.replace(".", "_").upper()

    shader_template = f"""/* WARNING, THIS FILE WAS GENERATED, DO NOT EDIT */
#ifndef {out_file_ifdef}_RAW_H
#define {out_file_ifdef}_RAW_H

static const char {out_file_base}[] = {{
{to_raw_cstring(header_data.code)}
}};
#endif
"""

    with open(out_file, "w", encoding="utf-8", newline="\n") as f:
        f.write(shader_template)


def build_raw_headers(target, source, env):
    env.NoCache(target)
    for x in source:
        build_raw_header(filename=str(x))
