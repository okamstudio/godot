"""Functions used to generate source files during build time"""

import os

import methods


# See also `editor/icons/editor_icons_builders.py`.
def make_default_theme_icons_action(target, source, env):
    icons_names = []
    icons_raw = []

    for svg in source:
        path = str(svg)
        with open(path, encoding="utf-8", newline="\n") as file:
            icons_raw.append(methods.to_raw_cstring(file.read()))

        name = os.path.splitext(os.path.basename(path))[0]
        icons_names.append(f'"{name}"')

    icons_names_str = ",\n\t".join(icons_names)
    icons_raw_str = ",\n\t".join(icons_raw)

    with methods.generated_wrapper(target[0]) as file:
        file.write(f"""\
#include "modules/modules_enabled.gen.h"

inline constexpr int default_theme_icons_count = {len(icons_names)};
inline constexpr const char *default_theme_icons_sources[] = {{
	{icons_raw_str}
}};

inline constexpr const char *default_theme_icons_names[] = {{
	{icons_names_str}
}};
""")
