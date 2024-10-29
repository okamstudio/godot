"""Functions used to generate source files during build time"""

import os

import methods


def parse_template(inherits, source, delimiter):
    script_template = {
        "inherits": inherits,
        "name": "",
        "description": "",
        "version": "",
        "script": "",
        "space-indent": "4",
    }
    meta_prefix = delimiter + " meta-"
    meta = ["name", "description", "version", "space-indent"]

    with open(source, "r", encoding="utf-8") as f:
        lines = f.readlines()
        for line in lines:
            if line.startswith(meta_prefix):
                line = line[len(meta_prefix) :]
                for m in meta:
                    if line.startswith(m):
                        strip_length = len(m) + 1
                        script_template[m] = line[strip_length:].strip()
            else:
                script_template["script"] += line
        if script_template["space-indent"] != "":
            indent = " " * int(script_template["space-indent"])
            script_template["script"] = script_template["script"].replace(indent, "_TS_")
        if script_template["name"] == "":
            script_template["name"] = os.path.splitext(os.path.basename(source))[0].replace("_", " ").title()
        script_template["script"] = (
            script_template["script"].replace('"', '\\"').lstrip().replace("\n", "\\n").replace("\t", "_TS_")
        )
        return (
            f'{{ String("{script_template["inherits"]}"), '
            + f'String("{script_template["name"]}"), '
            + f'String("{script_template["description"]}"), '
            + f'String("{script_template["script"]}") }},'
        )


def make_templates(target, source, env):
    delimiter = "#"  # GDScript single line comment delimiter by default.
    if source:
        ext = os.path.splitext(str(source[0]))[1]
        if ext == ".cs":
            delimiter = "//"

    parsed_templates = []

    for filepath in source:
        filepath = str(filepath)
        node_name = os.path.basename(os.path.dirname(filepath))
        parsed_templates.append(parse_template(node_name, filepath, delimiter))

    parsed_template_string = "\n\t".join(parsed_templates)

    with methods.generated_wrapper(target[0], prefix="code") as file:
        file.write(f"""\
#include "core/object/object.h"
#include "core/object/script_language.h"

inline constexpr int TEMPLATES_ARRAY_SIZE = {len(parsed_templates)};
static const struct ScriptLanguage::ScriptTemplate TEMPLATES[TEMPLATES_ARRAY_SIZE] = {{
	{parsed_template_string}
}};
""")
