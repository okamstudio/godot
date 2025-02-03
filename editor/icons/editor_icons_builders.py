"""Functions used to generate source files during build time"""

import os

import methods


# See also `scene/theme/icons/default_theme_icons_builders.py`.
def make_editor_icons_action(target, source, env):
    icons_names = []
    icons_raw = []
    icons_med = []
    icons_big = []

    for idx, svg in enumerate(source):
        path = str(svg)
        with open(path, encoding="utf-8", newline="\n") as file:
            icons_raw.append(methods.to_raw_cstring(file.read()))

        name = os.path.splitext(os.path.basename(path))[0]
        icons_names.append(f'"{name}"')

        if name.endswith("MediumThumb"):
            icons_med.append(str(idx))
        elif name.endswith(("BigThumb", "GodotFile")):
            icons_big.append(str(idx))

    icons_names_str = ",\n\t".join(icons_names)
    icons_raw_str = ",\n\t".join(icons_raw)

    with methods.generated_wrapper(target[0]) as file:
        file.write(f"""\
inline constexpr int editor_icons_count = {len(icons_names)};
inline constexpr const char *editor_icons_sources[] = {{
	{icons_raw_str}
}};

inline constexpr const char *editor_icons_names[] = {{
	{icons_names_str}
}};

inline constexpr int editor_md_thumbs_count = {len(icons_med)};
inline constexpr int editor_md_thumbs_indices[] = {{ {", ".join(icons_med)} }};

inline constexpr int editor_bg_thumbs_count = {len(icons_big)};
inline constexpr int editor_bg_thumbs_indices[] = {{ {", ".join(icons_big)} }};
""")
