"""Functions used to generate source files during build time"""

import os

import methods


def make_fonts_header(target, source, env):
    with methods.generated_wrapper(target[0]) as file:
        for src in source:
            # Saving uncompressed, since FreeType will reference from memory pointer.
            buffer = methods.get_buffer(src)
            name = os.path.splitext(os.path.basename(str(src)))[0]

            file.write(f"""\
inline constexpr int _font_{name}_size = {len(buffer)};
inline constexpr unsigned char _font_{name}[] = {{
	{methods.format_buffer(buffer, 1)}
}};

""")
