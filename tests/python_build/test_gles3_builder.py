import json

import pytest

from gles3_builders import GLES3HeaderStruct, build_gles3_header


@pytest.mark.parametrize(
    ["shader_files", "builder", "header_struct"],
    [
        ("gles3/vertex_fragment", build_gles3_header, GLES3HeaderStruct),
    ],
    indirect=["shader_files"],
)
def test_gles3_builder(shader_files, builder, header_struct):
    header = header_struct()
    env = {
        "target": "release",
    }

    builder(shader_files["path_input"], "drivers/gles3/shader_gles3.h", "GLES3", header_data=header, env=env)

    with open(shader_files["path_expected_parts"], "r", encoding="utf-8") as f:
        expected_parts = json.load(f)
        assert expected_parts == header.__dict__

    with open(shader_files["path_output"], "r", encoding="utf-8") as f:
        actual_output = f.read()
        assert actual_output

    with open(shader_files["path_expected_full"], "r", encoding="utf-8") as f:
        expected_output = f.read()

    assert actual_output == expected_output
