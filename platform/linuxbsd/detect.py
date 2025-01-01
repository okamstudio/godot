import os
import platform
import sys
from typing import TYPE_CHECKING

from methods import get_compiler_version, print_error, print_info, print_warning, using_gcc
from platform_methods import detect_arch, validate_arch

if TYPE_CHECKING:
    from SCons.Script.SConscript import SConsEnvironment


def get_name():
    return "LinuxBSD"


def can_build():
    if os.name != "posix" or sys.platform == "darwin":
        return False

    pkgconf_error = os.system("pkg-config --version > /dev/null")
    if pkgconf_error:
        print_error("pkg-config not found. Aborting.")
        return False

    return True


def get_opts():
    from SCons.Variables import BoolVariable, EnumVariable

    return [
        EnumVariable("linker", "Linker program", "default", ("default", "bfd", "gold", "lld", "mold")),
        BoolVariable("use_llvm", "Use the LLVM compiler", False),
        BoolVariable("use_static_cpp", "Link libgcc and libstdc++ statically for better portability", True),
        BoolVariable("use_coverage", "Test Godot coverage", False),
        BoolVariable("use_ubsan", "Use LLVM/GCC compiler undefined behavior sanitizer (UBSAN)", False),
        BoolVariable("use_asan", "Use LLVM/GCC compiler address sanitizer (ASAN)", False),
        BoolVariable("use_lsan", "Use LLVM/GCC compiler leak sanitizer (LSAN)", False),
        BoolVariable("use_tsan", "Use LLVM/GCC compiler thread sanitizer (TSAN)", False),
        BoolVariable("use_msan", "Use LLVM compiler memory sanitizer (MSAN)", False),
        BoolVariable("use_sowrap", "Dynamically load system libraries", True),
        BoolVariable("alsa", "Use ALSA", True),
        BoolVariable("pulseaudio", "Use PulseAudio", True),
        BoolVariable("dbus", "Use D-Bus to handle screensaver and portal desktop settings", True),
        BoolVariable("speechd", "Use Speech Dispatcher for Text-to-Speech support", True),
        BoolVariable("fontconfig", "Use fontconfig for system fonts support", True),
        BoolVariable("udev", "Use udev for gamepad connection callbacks", True),
        BoolVariable("x11", "Enable X11 display", True),
        BoolVariable("wayland", "Enable Wayland display", True),
        BoolVariable("libdecor", "Enable libdecor support", True),
        BoolVariable("touch", "Enable touch events", True),
        BoolVariable("execinfo", "Use libexecinfo on systems where glibc is not available", False),
    ]


def get_doc_classes():
    return [
        "EditorExportPlatformLinuxBSD",
    ]


def get_doc_path():
    return "doc_classes"


def get_flags():
    return {
        "arch": detect_arch(),
        "supported": ["mono"],
    }


def configure(env: "SConsEnvironment"):
    # Validate arch.
    supported_arches = ["x86_32", "x86_64", "arm32", "arm64", "rv64", "ppc32", "ppc64", "loongarch64"]
    validate_arch(env["arch"], get_name(), supported_arches)

    ## Build type

    if env.dev_build:
        # This is needed for our crash handler to work properly.
        # gdb works fine without it though, so maybe our crash handler could too.
        env.Append(LINKFLAGS=["-rdynamic"])

    # Cross-compilation
    # TODO: Support cross-compilation on architectures other than x86.
    host_is_64_bit = sys.maxsize > 2**32
    if host_is_64_bit and env["arch"] == "x86_32":
        env.Append(CCFLAGS=["-m32"])
        env.Append(LINKFLAGS=["-m32"])
    elif not host_is_64_bit and env["arch"] == "x86_64":
        env.Append(CCFLAGS=["-m64"])
        env.Append(LINKFLAGS=["-m64"])

    # CPU architecture flags.
    if env["arch"] == "rv64":
        # G = General-purpose extensions, C = Compression extension (very common).
        env.Append(CCFLAGS=["-march=rv64gc"])

    ## Compiler configuration

    if "CXX" in env and "clang" in os.path.basename(env["CXX"]):
        # Convenience check to enforce the use_llvm overrides when CXX is clang(++)
        env["use_llvm"] = True

    if env["use_llvm"]:
        if "clang++" not in os.path.basename(env["CXX"]):
            env["CC"] = "clang"
            env["CXX"] = "clang++"
        env.extra_suffix = ".llvm" + env.extra_suffix

    if env["linker"] != "default":
        print("Using linker program: " + env["linker"])
        if env["linker"] == "mold" and using_gcc(env):  # GCC < 12.1 doesn't support -fuse-ld=mold.
            cc_version = get_compiler_version(env)
            cc_semver = (cc_version["major"], cc_version["minor"])
            if cc_semver < (12, 1):
                found_wrapper = False
                for path in ["/usr/libexec", "/usr/local/libexec", "/usr/lib", "/usr/local/lib"]:
                    if os.path.isfile(path + "/mold/ld"):
                        env.Append(LINKFLAGS=["-B" + path + "/mold"])
                        found_wrapper = True
                        break
                if not found_wrapper:
                    for path in os.environ["PATH"].split(os.pathsep):
                        if os.path.isfile(path + "/ld.mold"):
                            env.Append(LINKFLAGS=["-B" + path])
                            found_wrapper = True
                            break
                    if not found_wrapper:
                        print_error(
                            "Couldn't locate mold installation path. Make sure it's installed in /usr, /usr/local or in PATH environment variable."
                        )
                        sys.exit(255)
            else:
                env.Append(LINKFLAGS=["-fuse-ld=mold"])
        else:
            env.Append(LINKFLAGS=["-fuse-ld=%s" % env["linker"]])

    if env["use_coverage"]:
        env.Append(CCFLAGS=["-ftest-coverage", "-fprofile-arcs"])
        env.Append(LINKFLAGS=["-ftest-coverage", "-fprofile-arcs"])

    if env["use_ubsan"] or env["use_asan"] or env["use_lsan"] or env["use_tsan"] or env["use_msan"]:
        env.extra_suffix += ".san"
        env.Append(CCFLAGS=["-DSANITIZERS_ENABLED"])

        if env["use_ubsan"]:
            env.Append(
                CCFLAGS=[
                    "-fsanitize=undefined,shift,shift-exponent,integer-divide-by-zero,unreachable,vla-bound,null,return,signed-integer-overflow,bounds,float-divide-by-zero,float-cast-overflow,nonnull-attribute,returns-nonnull-attribute,bool,enum,vptr,pointer-overflow,builtin"
                ]
            )
            env.Append(LINKFLAGS=["-fsanitize=undefined"])
            if env["use_llvm"]:
                env.Append(
                    CCFLAGS=[
                        "-fsanitize=nullability-return,nullability-arg,function,nullability-assign,implicit-integer-sign-change"
                    ]
                )
            else:
                env.Append(CCFLAGS=["-fsanitize=bounds-strict"])

        if env["use_asan"]:
            env.Append(CCFLAGS=["-fsanitize=address,pointer-subtract,pointer-compare"])
            env.Append(LINKFLAGS=["-fsanitize=address"])

        if env["use_lsan"]:
            env.Append(CCFLAGS=["-fsanitize=leak"])
            env.Append(LINKFLAGS=["-fsanitize=leak"])

        if env["use_tsan"]:
            env.Append(CCFLAGS=["-fsanitize=thread"])
            env.Append(LINKFLAGS=["-fsanitize=thread"])

        if env["use_msan"] and env["use_llvm"]:
            env.Append(CCFLAGS=["-fsanitize=memory"])
            env.Append(CCFLAGS=["-fsanitize-memory-track-origins"])
            env.Append(CCFLAGS=["-fsanitize-recover=memory"])
            env.Append(LINKFLAGS=["-fsanitize=memory"])

    env.Append(CCFLAGS=["-ffp-contract=off"])

    # LTO

    if env["lto"] == "auto":  # Full LTO for production.
        env["lto"] = "full"

    if env["lto"] != "none":
        if env["lto"] == "thin":
            if not env["use_llvm"]:
                print_error("ThinLTO is only compatible with LLVM, use `use_llvm=yes` or `lto=full`.")
                sys.exit(255)
            env.Append(CCFLAGS=["-flto=thin"])
            env.Append(LINKFLAGS=["-flto=thin"])
        elif not env["use_llvm"] and env.GetOption("num_jobs") > 1:
            env.Append(CCFLAGS=["-flto"])
            env.Append(LINKFLAGS=["-flto=" + str(env.GetOption("num_jobs"))])
        else:
            env.Append(CCFLAGS=["-flto"])
            env.Append(LINKFLAGS=["-flto"])

        if not env["use_llvm"]:
            env["RANLIB"] = "gcc-ranlib"
            env["AR"] = "gcc-ar"

    env.Append(CCFLAGS=["-pipe"])

    ## Dependencies

    if env["use_sowrap"]:
        env.Append(CPPDEFINES=["SOWRAP_ENABLED"])

    if env["wayland"]:
        if os.system("wayland-scanner -v 2>/dev/null") != 0:
            print_warning("wayland-scanner not found. Disabling Wayland support.")
            env["wayland"] = False

    if env["touch"]:
        env.Append(CPPDEFINES=["TOUCH_ENABLED"])

    # FIXME: Check for existence of the libs before parsing their flags with pkg-config

    # freetype depends on libpng and zlib, so bundling one of them while keeping others
    # as shared libraries leads to weird issues. And graphite and harfbuzz need freetype.
    ft_linked_deps = [
        env["builtin_freetype"],
        env["builtin_libpng"],
        env["builtin_zlib"],
        env["builtin_graphite"],
        env["builtin_harfbuzz"],
    ]
    if (not all(ft_linked_deps)) and any(ft_linked_deps):  # All or nothing.
        print_error(
            "These libraries should be either all builtin, or all system provided:\n"
            "freetype, libpng, zlib, graphite, harfbuzz.\n"
            "Please specify `builtin_<name>=no` for all of them, or none."
        )
        sys.exit(255)

    if not env["builtin_freetype"]:
        env.ParseConfig("pkg-config freetype2 --cflags --libs")

    if not env["builtin_graphite"]:
        env.ParseConfig("pkg-config graphite2 --cflags --libs")

    if not env["builtin_icu4c"]:
        env.ParseConfig("pkg-config icu-i18n icu-uc --cflags --libs")

    if not env["builtin_harfbuzz"]:
        env.ParseConfig("pkg-config harfbuzz harfbuzz-icu --cflags --libs")

    if not env["builtin_libpng"]:
        env.ParseConfig("pkg-config libpng16 --cflags --libs")

    if not env["builtin_enet"]:
        env.ParseConfig("pkg-config libenet --cflags --libs")

    if not env["builtin_zstd"]:
        env.ParseConfig("pkg-config libzstd --cflags --libs")

    if env["brotli"] and not env["builtin_brotli"]:
        env.ParseConfig("pkg-config libbrotlicommon libbrotlidec --cflags --libs")

    # Sound and video libraries
    # Keep the order as it triggers chained dependencies (ogg needed by others, etc.)

    if not env["builtin_libtheora"]:
        env["builtin_libogg"] = False  # Needed to link against system libtheora
        env["builtin_libvorbis"] = False  # Needed to link against system libtheora
        env.ParseConfig("pkg-config theora theoradec --cflags --libs")
    else:
        if env["arch"] in ["x86_64", "x86_32"]:
            env["x86_libtheora_opt_gcc"] = True

    if not env["builtin_libvorbis"]:
        env["builtin_libogg"] = False  # Needed to link against system libvorbis
        env.ParseConfig("pkg-config vorbis vorbisfile --cflags --libs")

    if not env["builtin_libogg"]:
        env.ParseConfig("pkg-config ogg --cflags --libs")

    if not env["builtin_libwebp"]:
        env.ParseConfig("pkg-config libwebp --cflags --libs")

    if not env["builtin_mbedtls"]:
        # mbedTLS only provides a pkgconfig file since 3.6.0, but we still support 2.28.x,
        # so fallback to manually specifying LIBS if it fails.
        if os.system("pkg-config --exists mbedtls") == 0:  # 0 means found
            env.ParseConfig("pkg-config mbedtls mbedcrypto mbedx509 --cflags --libs")
        else:
            env.Append(LIBS=["mbedtls", "mbedcrypto", "mbedx509"])

    if not env["builtin_wslay"]:
        env.ParseConfig("pkg-config libwslay --cflags --libs")

    if not env["builtin_miniupnpc"]:
        env.ParseConfig("pkg-config miniupnpc --cflags --libs")

    # On Linux wchar_t should be 32-bits
    # 16-bit library shouldn't be required due to compiler optimizations
    if not env["builtin_pcre2"]:
        env.ParseConfig("pkg-config libpcre2-32 --cflags --libs")

    if not env["builtin_recastnavigation"]:
        # No pkgconfig file so far, hardcode default paths.
        env.Prepend(CPPPATH=["/usr/include/recastnavigation"])
        env.Append(LIBS=["Recast"])

    if not env["builtin_embree"] and env["arch"] in ["x86_64", "arm64"]:
        # No pkgconfig file so far, hardcode expected lib name.
        env.Append(LIBS=["embree4"])

    if not env["builtin_openxr"]:
        env.ParseConfig("pkg-config openxr --cflags --libs")

    if env["fontconfig"]:
        if not env["use_sowrap"]:
            if os.system("pkg-config --exists fontconfig") == 0:  # 0 means found
                env.ParseConfig("pkg-config fontconfig --cflags --libs")
                env.Append(CPPDEFINES=["FONTCONFIG_ENABLED"])
            else:
                print_warning("fontconfig development libraries not found. Disabling the system fonts support.")
                env["fontconfig"] = False
        else:
            env.Append(CPPDEFINES=["FONTCONFIG_ENABLED"])

    if env["alsa"]:
        if not env["use_sowrap"]:
            if os.system("pkg-config --exists alsa") == 0:  # 0 means found
                env.ParseConfig("pkg-config alsa --cflags --libs")
                env.Append(CPPDEFINES=["ALSA_ENABLED", "ALSAMIDI_ENABLED"])
            else:
                print_warning("ALSA development libraries not found. Disabling the ALSA audio driver.")
                env["alsa"] = False
        else:
            env.Append(CPPDEFINES=["ALSA_ENABLED", "ALSAMIDI_ENABLED"])

    if env["pulseaudio"]:
        if not env["use_sowrap"]:
            if os.system("pkg-config --exists libpulse") == 0:  # 0 means found
                env.ParseConfig("pkg-config libpulse --cflags --libs")
                env.Append(CPPDEFINES=["PULSEAUDIO_ENABLED"])
            else:
                print_warning("PulseAudio development libraries not found. Disabling the PulseAudio audio driver.")
                env["pulseaudio"] = False
        else:
            env.Append(CPPDEFINES=["PULSEAUDIO_ENABLED", "_REENTRANT"])

    if env["dbus"]:
        if not env["use_sowrap"]:
            if os.system("pkg-config --exists dbus-1") == 0:  # 0 means found
                env.ParseConfig("pkg-config dbus-1 --cflags --libs")
                env.Append(CPPDEFINES=["DBUS_ENABLED"])
            else:
                print_warning("D-Bus development libraries not found. Disabling screensaver prevention.")
                env["dbus"] = False
        else:
            env.Append(CPPDEFINES=["DBUS_ENABLED"])

    if env["speechd"]:
        if not env["use_sowrap"]:
            if os.system("pkg-config --exists speech-dispatcher") == 0:  # 0 means found
                env.ParseConfig("pkg-config speech-dispatcher --cflags --libs")
                env.Append(CPPDEFINES=["SPEECHD_ENABLED"])
            else:
                print_warning("speech-dispatcher development libraries not found. Disabling text to speech support.")
                env["speechd"] = False
        else:
            env.Append(CPPDEFINES=["SPEECHD_ENABLED"])

    if not env["use_sowrap"]:
        if os.system("pkg-config --exists xkbcommon") == 0:  # 0 means found
            env.ParseConfig("pkg-config xkbcommon --cflags --libs")
            env.Append(CPPDEFINES=["XKB_ENABLED"])
        else:
            if env["wayland"]:
                print_error("libxkbcommon development libraries required by Wayland not found. Aborting.")
                sys.exit(255)
            else:
                print_warning(
                    "libxkbcommon development libraries not found. Disabling dead key composition and key label support."
                )
    else:
        env.Append(CPPDEFINES=["XKB_ENABLED"])

    if platform.system() == "Linux":
        env.Append(CPPDEFINES=["JOYDEV_ENABLED"])
        if env["udev"]:
            if not env["use_sowrap"]:
                if os.system("pkg-config --exists libudev") == 0:  # 0 means found
                    env.ParseConfig("pkg-config libudev --cflags --libs")
                    env.Append(CPPDEFINES=["UDEV_ENABLED"])
                else:
                    print_warning("libudev development libraries not found. Disabling controller hotplugging support.")
                    env["udev"] = False
            else:
                env.Append(CPPDEFINES=["UDEV_ENABLED"])
    else:
        env["udev"] = False  # Linux specific

    # Linkflags below this line should typically stay the last ones
    if not env["builtin_zlib"]:
        env.ParseConfig("pkg-config zlib --cflags --libs")

    env.Prepend(CPPPATH=["#platform/linuxbsd"])
    if env["use_sowrap"]:
        env.Prepend(CPPPATH=["#thirdparty/linuxbsd_headers"])

    env.Append(
        CPPDEFINES=[
            "LINUXBSD_ENABLED",
            "UNIX_ENABLED",
            ("_FILE_OFFSET_BITS", 64),
        ]
    )

    if env["x11"]:
        if not env["use_sowrap"]:
            if os.system("pkg-config --exists x11"):
                print_error("X11 libraries not found. Aborting.")
                sys.exit(255)
            env.ParseConfig("pkg-config x11 --cflags --libs")
            if os.system("pkg-config --exists xcursor"):
                print_error("Xcursor library not found. Aborting.")
                sys.exit(255)
            env.ParseConfig("pkg-config xcursor --cflags --libs")
            if os.system("pkg-config --exists xinerama"):
                print_error("Xinerama library not found. Aborting.")
                sys.exit(255)
            env.ParseConfig("pkg-config xinerama --cflags --libs")
            if os.system("pkg-config --exists xext"):
                print_error("Xext library not found. Aborting.")
                sys.exit(255)
            env.ParseConfig("pkg-config xext --cflags --libs")
            if os.system("pkg-config --exists xrandr"):
                print_error("XrandR library not found. Aborting.")
                sys.exit(255)
            env.ParseConfig("pkg-config xrandr --cflags --libs")
            if os.system("pkg-config --exists xrender"):
                print_error("XRender library not found. Aborting.")
                sys.exit(255)
            env.ParseConfig("pkg-config xrender --cflags --libs")
            if os.system("pkg-config --exists xi"):
                print_error("Xi library not found. Aborting.")
                sys.exit(255)
            env.ParseConfig("pkg-config xi --cflags --libs")
        env.Append(CPPDEFINES=["X11_ENABLED"])

    if env["wayland"]:
        if not env["use_sowrap"]:
            if os.system("pkg-config --exists libdecor-0"):
                print_warning("libdecor development libraries not found. Disabling client-side decorations.")
                env["libdecor"] = False
            else:
                env.ParseConfig("pkg-config libdecor-0 --cflags --libs")
            if os.system("pkg-config --exists wayland-client"):
                print_error("Wayland client library not found. Aborting.")
                sys.exit(255)
            env.ParseConfig("pkg-config wayland-client --cflags --libs")
            if os.system("pkg-config --exists wayland-cursor"):
                print_error("Wayland cursor library not found. Aborting.")
                sys.exit(255)
            env.ParseConfig("pkg-config wayland-cursor --cflags --libs")
            if os.system("pkg-config --exists wayland-egl"):
                print_error("Wayland EGL library not found. Aborting.")
                sys.exit(255)
            env.ParseConfig("pkg-config wayland-egl --cflags --libs")

        if env["libdecor"]:
            env.Append(CPPDEFINES=["LIBDECOR_ENABLED"])

        env.Prepend(CPPPATH=["#platform/linuxbsd", "#thirdparty/linuxbsd_headers/wayland/"])
        env.Append(CPPDEFINES=["WAYLAND_ENABLED"])
        env.Append(LIBS=["rt"])  # Needed by glibc, used by _allocate_shm_file

    if env["vulkan"]:
        env.Append(CPPDEFINES=["VULKAN_ENABLED", "RD_ENABLED"])
        if not env["use_volk"]:
            env.ParseConfig("pkg-config vulkan --cflags --libs")
        if not env["builtin_glslang"]:
            # No pkgconfig file so far, hardcode expected lib name.
            env.Append(LIBS=["glslang", "SPIRV"])

    if env["opengl3"]:
        env.Append(CPPDEFINES=["GLES3_ENABLED"])

    env.Append(LIBS=["pthread"])

    if platform.system() == "Linux":
        env.Append(LIBS=["dl"])

    if platform.libc_ver()[0] != "glibc":
        if env["execinfo"]:
            env.Append(LIBS=["execinfo"])
            env.Append(CPPDEFINES=["CRASH_HANDLER_ENABLED"])
        else:
            # The default crash handler depends on glibc, so if the host uses
            # a different libc (BSD libc, musl), libexecinfo is required.
            print_info("Using `execinfo=no` disables the crash handler on platforms where glibc is missing.")
    else:
        env.Append(CPPDEFINES=["CRASH_HANDLER_ENABLED"])

    if platform.system() == "FreeBSD":
        env.Append(LINKFLAGS=["-lkvm"])

    # Link those statically for portability
    if env["use_static_cpp"]:
        env.Append(LINKFLAGS=["-static-libgcc", "-static-libstdc++"])
        if env["use_llvm"] and platform.system() != "FreeBSD":
            env["LINKCOM"] = env["LINKCOM"] + " -l:libatomic.a"
    else:
        if env["use_llvm"] and platform.system() != "FreeBSD":
            env.Append(LIBS=["atomic"])
