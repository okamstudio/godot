import json

import pytest

from glsl_builders import RAWHeaderStruct, RDHeaderStruct, build_raw_header, build_rd_header


@pytest.mark.parametrize(
    [
        "shader_files",
        "builder",
        "header_struct",
    ],
    [
        ("glsl/vertex_fragment", build_raw_header, RAWHeaderStruct),
        ("glsl/compute", build_raw_header, RAWHeaderStruct),
        ("rd_glsl/vertex_fragment", build_rd_header, RDHeaderStruct),
        ("rd_glsl/compute", build_rd_header, RDHeaderStruct),
    ],
    indirect=["shader_files"],
)
def test_glsl_builder(shader_files, builder, header_struct):
    header = header_struct()
    env = {
        "target": "release",
    }

    builder(shader_files["path_input"], header_data=header, env=env)

    with open(shader_files["path_expected_parts"], "r", encoding="utf-8") as f:
        expected_parts = json.load(f)
        assert expected_parts == header.__dict__

    with open(shader_files["path_output"], "r", encoding="utf-8") as f:
        actual_output = f.read()
        assert actual_output

    with open(shader_files["path_expected_full"], "r", encoding="utf-8") as f:
        expected_output = f.read()

    assert actual_output == expected_output
