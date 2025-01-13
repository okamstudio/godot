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


def prepare_timer():
    import atexit
    import time

    def print_elapsed_time(time_at_start: float):
        time_elapsed = time.time() - time_at_start
        time_formatted = time.strftime("%H:%M:%S", time.gmtime(time_elapsed))
        time_centiseconds = round((time_elapsed % 1) * 100)
        print(f"[Time elapsed: {time_formatted}.{time_centiseconds}]")

    atexit.register(print_elapsed_time, time.time())


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
    import os

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
