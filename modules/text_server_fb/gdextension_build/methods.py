import os
import sys
from enum import Enum

# Colors are disabled in non-TTY environments such as pipes. This means
# that if output is redirected to a file, it won't contain color codes.
# Colors are always enabled on continuous integration.
_colorize = bool(sys.stdout.isatty() or os.environ.get("CI"))


class ANSI(Enum):
    """
    Enum class for adding ansi colorcodes directly into strings.
    Automatically converts values to strings representing their
    internal value, or an empty string in a non-colorized scope.
    """

    RESET = "\x1b[0m"

    BOLD = "\x1b[1m"
    ITALIC = "\x1b[3m"
    UNDERLINE = "\x1b[4m"
    STRIKETHROUGH = "\x1b[9m"
    REGULAR = "\x1b[22;23;24;29m"

    BLACK = "\x1b[30m"
    RED = "\x1b[31m"
    GREEN = "\x1b[32m"
    YELLOW = "\x1b[33m"
    BLUE = "\x1b[34m"
    MAGENTA = "\x1b[35m"
    CYAN = "\x1b[36m"
    WHITE = "\x1b[37m"

    PURPLE = "\x1b[38;5;93m"
    PINK = "\x1b[38;5;206m"
    ORANGE = "\x1b[38;5;214m"
    GRAY = "\x1b[38;5;244m"

    def __str__(self) -> str:
        global _colorize
        return str(self.value) if _colorize else ""


def no_verbose(env):
    colors = [ANSI.BLUE, ANSI.BOLD, ANSI.REGULAR, ANSI.RESET]

    # There is a space before "..." to ensure that source file names can be
    # Ctrl + clicked in the VS Code terminal.
    compile_source_message = "{}Compiling {}$SOURCE{} ...{}".format(*colors)
    java_compile_source_message = "{}Compiling {}$SOURCE{} ...{}".format(*colors)
    compile_shared_source_message = "{}Compiling shared {}$SOURCE{} ...{}".format(*colors)
    link_program_message = "{}Linking Program {}$TARGET{} ...{}".format(*colors)
    link_library_message = "{}Linking Static Library {}$TARGET{} ...{}".format(*colors)
    ranlib_library_message = "{}Ranlib Library {}$TARGET{} ...{}".format(*colors)
    link_shared_library_message = "{}Linking Shared Library {}$TARGET{} ...{}".format(*colors)
    java_library_message = "{}Creating Java Archive {}$TARGET{} ...{}".format(*colors)
    compiled_resource_message = "{}Creating Compiled Resource {}$TARGET{} ...{}".format(*colors)
    generated_file_message = "{}Generating {}$TARGET{} ...{}".format(*colors)

    env["CXXCOMSTR"] = compile_source_message
    env["CCCOMSTR"] = compile_source_message
    env["SHCCCOMSTR"] = compile_shared_source_message
    env["SHCXXCOMSTR"] = compile_shared_source_message
    env["ARCOMSTR"] = link_library_message
    env["RANLIBCOMSTR"] = ranlib_library_message
    env["SHLINKCOMSTR"] = link_shared_library_message
    env["LINKCOMSTR"] = link_program_message
    env["JARCOMSTR"] = java_library_message
    env["JAVACCOMSTR"] = java_compile_source_message
    env["RCCOMSTR"] = compiled_resource_message
    env["GENCOMSTR"] = generated_file_message


def disable_warnings(self):
    # 'self' is the environment
    if self["platform"] == "windows" and not self["use_mingw"]:
        # We have to remove existing warning level defines before appending /w,
        # otherwise we get: "warning D9025 : overriding '/W3' with '/w'"
        WARN_FLAGS = ["/Wall", "/W4", "/W3", "/W2", "/W1", "/W0"]
        self["CCFLAGS"] = [x for x in self["CCFLAGS"] if x not in WARN_FLAGS]
        self["CFLAGS"] = [x for x in self["CFLAGS"] if x not in WARN_FLAGS]
        self["CXXFLAGS"] = [x for x in self["CXXFLAGS"] if x not in WARN_FLAGS]
        self.AppendUnique(CCFLAGS=["/w"])
    else:
        self.AppendUnique(CCFLAGS=["-w"])


def make_icu_data(target, source, env):
    dst = target[0].srcnode().abspath
    with open(dst, "w", encoding="utf-8", newline="\n") as g:
        g.write("/* THIS FILE IS GENERATED DO NOT EDIT */\n")
        g.write("/* (C) 2016 and later: Unicode, Inc. and others. */\n")
        g.write("/* License & terms of use: https://www.unicode.org/copyright.html */\n")
        g.write("#ifndef _ICU_DATA_H\n")
        g.write("#define _ICU_DATA_H\n")
        g.write('#include "unicode/utypes.h"\n')
        g.write('#include "unicode/udata.h"\n')
        g.write('#include "unicode/uversion.h"\n')

        with open(source[0].srcnode().abspath, "rb") as f:
            buf = f.read()

        g.write('extern "C" U_EXPORT const size_t U_ICUDATA_SIZE = ' + str(len(buf)) + ";\n")
        g.write('extern "C" U_EXPORT const unsigned char U_ICUDATA_ENTRY_POINT[] = {\n')
        for i in range(len(buf)):
            g.write("\t" + str(buf[i]) + ",\n")

        g.write("};\n")
        g.write("#endif")


def write_macos_plist(target, binary_name, identifier, name):
    os.makedirs(f"{target}/Resource/", exist_ok=True)
    with open(f"{target}/Resource/Info.plist", "w", encoding="utf-8", newline="\n") as f:
        f.write(f"""\
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleExecutable</key>
	<string>{binary_name}</string>
	<key>CFBundleIdentifier</key>
	<string>{identifier}</string>
	<key>CFBundleInfoDictionaryVersion</key>
	<string>6.0</string>
	<key>CFBundleName</key>
	<string>{name}</string>
	<key>CFBundlePackageType</key>
	<string>FMWK</string>
	<key>CFBundleShortVersionString</key>
	<string>1.0.0</string>
	<key>CFBundleSupportedPlatforms</key>
	<array>
		<string>MacOSX</string>
	</array>
	<key>CFBundleVersion</key>
	<string>1.0.0</string>
	<key>LSMinimumSystemVersion</key>
	<string>10.14</string>
</dict>
</plist>
""")
