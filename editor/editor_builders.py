"""Functions used to generate source files during build time"""

import os
import os.path
import subprocess
import tempfile
import uuid

import methods


def make_doc_header(target, source, env):
    buffer = b"".join([methods.get_buffer(src) for src in source])
    decomp_size = len(buffer)
    buffer = methods.compress_buffer(buffer)

    with methods.generated_wrapper(target[0]) as file:
        file.write(f"""\
inline constexpr const char *_doc_data_hash = "{hash(buffer)}";
inline constexpr int _doc_data_compressed_size = {len(buffer)};
inline constexpr int _doc_data_uncompressed_size = {decomp_size};
inline constexpr const unsigned char _doc_data_compressed[] = {{
	{methods.format_buffer(buffer, 1)}
}};
""")


def make_translations_header(target, source, env):
    category = os.path.basename(str(target[0])).split("_")[0]
    sorted_paths = sorted([src.abspath for src in source], key=lambda path: os.path.splitext(os.path.basename(path))[0])

    xl_names = []
    msgfmt = env.Detect("msgfmt")
    if not msgfmt:
        methods.print_warning("msgfmt not found, using .po files instead of .mo")

    with methods.generated_wrapper(target[0], prefix=category) as file:
        for path in sorted_paths:
            name = os.path.splitext(os.path.basename(path))[0]
            # msgfmt erases non-translated messages, so avoid using it if exporting the POT.
            if msgfmt and name != category:
                mo_path = os.path.join(tempfile.gettempdir(), uuid.uuid4().hex + ".mo")
                cmd = f"{msgfmt} {path} --no-hash -o {mo_path}"
                try:
                    subprocess.Popen(cmd, shell=True, stderr=subprocess.PIPE).communicate()
                    buffer = methods.get_buffer(mo_path)
                except OSError as e:
                    methods.print_warning(
                        "msgfmt execution failed, using .po file instead of .mo: path=%r; [%s] %s"
                        % (path, e.__class__.__name__, e)
                    )
                    buffer = methods.get_buffer(path)
                finally:
                    try:
                        if os.path.exists(mo_path):
                            os.remove(mo_path)
                    except OSError as e:
                        # Do not fail the entire build if it cannot delete a temporary file.
                        methods.print_warning(
                            "Could not delete temporary .mo file: path=%r; [%s] %s" % (mo_path, e.__class__.__name__, e)
                        )
            else:
                buffer = methods.get_buffer(path)
                if name == category:
                    name = "source"

            decomp_size = len(buffer)
            buffer = methods.compress_buffer(buffer)

            file.write(f"""\
inline constexpr const unsigned char _{category}_translation_{name}_compressed[] = {{
	{methods.format_buffer(buffer, 1)}
}};

""")

            xl_names.append([name, len(buffer), decomp_size])

        file.write(f"""\
struct {category.capitalize()}TranslationList {{
	const char* lang;
	int comp_size;
	int uncomp_size;
	const unsigned char* data;
}};

inline constexpr {category.capitalize()}TranslationList _{category}_translations[] = {{
""")

        for x in xl_names:
            file.write(f'\t{{ "{x[0]}", {x[1]}, {x[2]}, _{category}_translation_{x[0]}_compressed }},\n')

        file.write("""\
	{ nullptr, 0, 0, nullptr },
};
""")
