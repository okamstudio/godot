#!/usr/bin/env python3

import argparse
import os
import re
import sys
from subprocess import call


def main():
    # Change to the directory where the script is located,
    # so that the script can be run from any location.
    os.chdir(os.path.dirname(os.path.realpath(__file__)))

    parser = argparse.ArgumentParser(description="Creates a new unit test file.")
    parser.add_argument(
        "name",
        type=str,
        help="Specifies the class or component name to be tested, in PascalCase (e.g., MeshInstance3D). The name will be prefixed with 'test_' for the header file and 'Test' for the namespace.",
    )
    parser.add_argument(
        "path",
        type=str,
        nargs="?",
        help="The path to the unit test file relative to the tests folder (e.g. core). This should correspond to the relative path of the class or component being tested. (default: .)",
        default=".",
    )
    parser.add_argument(
        "-i",
        "--invasive",
        action="store_true",
        help="if set, the script will automatically insert the include directive in test_main.cpp. Use with caution!",
    )
    args = parser.parse_args()

    snake_case_regex = re.compile(r"(?<!^)(?=[A-Z, 0-9])")
    # Replace 2D, 3D, and 4D with 2d, 3d, and 4d, respectively. This avoids undesired splits like node_3_d.
    prefiltered_name = re.sub(r"([234])D", lambda match: match.group(1).lower() + "d", args.name)
    name_snake_case = snake_case_regex.sub("_", prefiltered_name).lower()
    file_path = os.path.normpath(os.path.join(args.path, f"test_{name_snake_case}.h"))

    # Ensure the directory exists.
    os.makedirs(os.path.dirname(file_path), exist_ok=True)

    print(file_path)
    if os.path.isfile(file_path):
        print(f'ERROR: The file "{file_path}" already exists.')
        sys.exit(1)
    with open(file_path, "w", encoding="utf-8", newline="\n") as file:
        file.write(
            """/**************************************************************************/
/*  test_{name_snake_case}.h {padding} */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#ifndef TEST_{name_upper_snake_case}_H
#define TEST_{name_upper_snake_case}_H

#include "tests/test_macros.h"

namespace Test{name_pascal_case} {{

TEST_CASE("[{name_pascal_case}] Example test case") {{
    // TODO: Remove this comment and write your test code here.
}}

}} // namespace Test{name_pascal_case}

#endif // TEST_{name_upper_snake_case}_H
""".format(
                name_snake_case=name_snake_case,
                # Capitalize the first letter but keep capitalization for the rest of the string.
                # This is done in case the user passes a camelCase string instead of PascalCase.
                name_pascal_case=args.name[0].upper() + args.name[1:],
                name_upper_snake_case=name_snake_case.upper(),
                # The padding length depends on the test name length.
                padding=" " * (61 - len(name_snake_case)),
            )
        )

    # Print an absolute path so it can be Ctrl + clicked in some IDEs and terminal emulators.
    print("Test header file created:")
    print(os.path.abspath(file_path))

    if args.invasive:
        print("Trying to insert include directive in test_main.cpp...")
        with open("test_main.cpp", "r", encoding="utf-8") as file:
            contents = file.read()
        match = re.search(r'#include "tests.*\n', contents)

        if match:
            new_string = contents[: match.start()] + f'#include "tests/{file_path}"\n' + contents[match.start() :]

            with open("test_main.cpp", "w", encoding="utf-8", newline="\n") as file:
                file.write(new_string)
                print("Done.")
            # Use clang format to sort include directives afster insertion.
            clang_format_args = ["clang-format", "test_main.cpp", "-i"]
            retcode = call(clang_format_args)
            if retcode != 0:
                print(
                    "Include directives in test_main.cpp could not be sorted automatically using clang-format. Please sort them manually."
                )
        else:
            print("Could not find a valid position in test_main.cpp to insert the include directive.")

    else:
        print("\nRemember to #include the new test header in this file (following alphabetical order):")
        print(os.path.abspath("test_main.cpp"))
        print("Insert the following line in the appropriate place:")
        print(f'#include "tests/{file_path}"')


if __name__ == "__main__":
    main()
