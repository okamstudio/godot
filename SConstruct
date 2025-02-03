#!/usr/bin/env python

EnsureSConsVersion(0, 98, 1)

# System
import atexit
import glob
import os
import pickle
import sys
import time
from collections import OrderedDict

# Local
import methods
import gles_builders
import scu_builders
from platform_methods import run_in_subprocess

# scan possible build platforms

platform_list = []  # list of platforms
platform_opts = {}  # options for each platform
platform_flags = {}  # flags for each platform

active_platforms = []
active_platform_ids = []
platform_exporters = []
platform_apis = []

time_at_start = time.time()

for x in sorted(glob.glob("platform/*")):
    if not os.path.isdir(x) or not os.path.exists(x + "/detect.py"):
        continue
    tmppath = "./" + x

    sys.path.insert(0, tmppath)
    import detect

    if os.path.exists(x + "/export/export.cpp"):
        platform_exporters.append(x[9:])
    if os.path.exists(x + "/api/api.cpp"):
        platform_apis.append(x[9:])
    if detect.is_active():
        active_platforms.append(detect.get_name())
        active_platform_ids.append(x)
    if detect.can_build():
        x = x.replace("platform/", "")  # rest of world
        x = x.replace("platform\\", "")  # win32
        platform_list += [x]
        platform_opts[x] = detect.get_opts()
        platform_flags[x] = detect.get_flags()
    sys.path.remove(tmppath)
    sys.modules.pop("detect")

methods.save_active_platforms(active_platforms, active_platform_ids)

custom_tools = ["default"]

platform_arg = ARGUMENTS.get("platform", ARGUMENTS.get("p", False))

if platform_arg == "android":
    custom_tools = ["clang", "clang++", "as", "ar", "link"]
elif platform_arg == "javascript":
    # Use generic POSIX build toolchain for Emscripten.
    custom_tools = ["cc", "c++", "ar", "link", "textfile", "zip"]
elif os.name == "nt" and methods.get_cmdline_bool("use_mingw", False):
    custom_tools = ["mingw"]

# We let SCons build its default ENV as it includes OS-specific things which we don't
# want to have to pull in manually.
# Then we prepend PATH to make it take precedence, while preserving SCons' own entries.
env_base = Environment(tools=custom_tools)
env_base.PrependENVPath("PATH", os.getenv("PATH"))
env_base.PrependENVPath("PKG_CONFIG_PATH", os.getenv("PKG_CONFIG_PATH"))
if "TERM" in os.environ:  # Used for colored output.
    env_base["ENV"]["TERM"] = os.environ["TERM"]

env_base.disabled_modules = []
env_base.use_ptrcall = False
env_base.module_version_string = ""
env_base.msvc = False

env_base.__class__.disable_module = methods.disable_module

env_base.__class__.add_module_version_string = methods.add_module_version_string

env_base.__class__.add_source_files = methods.add_source_files
env_base.__class__.use_windows_spawn_fix = methods.use_windows_spawn_fix
env_base.__class__.split_lib = methods.split_lib

env_base.__class__.add_shared_library = methods.add_shared_library
env_base.__class__.add_library = methods.add_library
env_base.__class__.add_program = methods.add_program
env_base.__class__.CommandNoCache = methods.CommandNoCache
env_base.__class__.disable_warnings = methods.disable_warnings
env_base.__class__.force_optimization_on_debug = methods.force_optimization_on_debug

env_base["x86_libtheora_opt_gcc"] = False
env_base["x86_libtheora_opt_vc"] = False

# avoid issues when building with different versions of python out of the same directory
env_base.SConsignFile(".sconsign{0}.dblite".format(pickle.HIGHEST_PROTOCOL))

# Build options

customs = ["custom.py"]

profile = ARGUMENTS.get("profile", "")
if profile:
    if os.path.isfile(profile):
        customs.append(profile)
    elif os.path.isfile(profile + ".py"):
        customs.append(profile + ".py")

opts = Variables(customs, ARGUMENTS)

# Target build options
opts.Add("p", "Platform (alias for 'platform')", "")
opts.Add("platform", "Target platform (%s)" % ("|".join(platform_list),), "")
opts.Add(BoolVariable("tools", "Build the tools (a.k.a. the Godot editor)", True))
opts.Add(EnumVariable("target", "Compilation target", "debug", ("debug", "release_debug", "release")))
opts.Add("arch", "Platform-dependent architecture (arm/arm64/x86/x64/mips/...)", "")
opts.Add(EnumVariable("bits", "Target platform bits", "default", ("default", "32", "64")))
opts.Add(EnumVariable("optimize", "Optimization type", "speed", ("speed", "size", "none")))
opts.Add(BoolVariable("production", "Set defaults to build Godot for use in production", False))
opts.Add(EnumVariable("lto", "Link-time optimization (production builds)", "none", ("none", "auto", "thin", "full")))

# Components
opts.Add(BoolVariable("deprecated", "Enable deprecated features", True))
opts.Add(BoolVariable("minizip", "Enable ZIP archive support using minizip", True))
opts.Add(BoolVariable("xaudio2", "Enable the XAudio2 audio driver", False))
opts.Add(BoolVariable("disable_exceptions", "Force disabling exception handling code", True))
opts.Add("custom_modules", "A list of comma-separated directory paths containing custom modules to build.", "")
opts.Add(BoolVariable("custom_modules_recursive", "Detect custom modules recursively for each specified path.", True))

# Advanced options
opts.Add(BoolVariable("dev", "If yes, alias for verbose=yes warnings=extra werror=yes", False))
opts.Add(BoolVariable("fast_unsafe", "Enable unsafe options for faster rebuilds", False))
opts.Add(BoolVariable("compiledb", "Generate compilation DB (`compile_commands.json`) for external tools", False))
opts.Add(BoolVariable("verbose", "Enable verbose output for the compilation", False))
opts.Add(BoolVariable("progress", "Show a progress indicator during compilation", True))
opts.Add(EnumVariable("warnings", "Level of compilation warnings", "all", ("extra", "all", "moderate", "no")))
opts.Add(BoolVariable("werror", "Treat compiler warnings as errors", False))
opts.Add("extra_suffix", "Custom extra suffix added to the base filename of all generated binary files", "")
opts.Add(BoolVariable("vsproj", "Generate a Visual Studio solution", False))
opts.Add(
    BoolVariable(
        "split_libmodules",
        "Split intermediate libmodules.a in smaller chunks to prevent exceeding linker command line size (forced to True when using MinGW)",
        False,
    )
)
opts.Add(BoolVariable("disable_3d", "Disable 3D nodes for a smaller executable", False))
opts.Add(BoolVariable("disable_advanced_gui", "Disable advanced GUI nodes and behaviors", False))
opts.Add(BoolVariable("modules_enabled_by_default", "If no, disable all modules except ones explicitly enabled", True))
opts.Add(BoolVariable("no_editor_splash", "Don't use the custom splash screen for the editor", True))
opts.Add("system_certs_path", "Use this path as SSL certificates default for editor (for package maintainers)", "")
opts.Add(BoolVariable("use_precise_math_checks", "Math checks use very precise epsilon (debug option)", False))
opts.Add(BoolVariable("scu_build", "Use single compilation unit build", False))
opts.Add(
    EnumVariable(
        "rids",
        "Server object management technique (debug option)",
        "pointers",
        ("pointers", "handles", "tracked_handles"),
    )
)

# Thirdparty libraries
opts.Add(BoolVariable("builtin_bullet", "Use the built-in Bullet library", True))
opts.Add(BoolVariable("builtin_certs", "Use the built-in SSL certificates bundles", True))
opts.Add(BoolVariable("builtin_embree", "Use the built-in Embree library", True))
opts.Add(BoolVariable("builtin_enet", "Use the built-in ENet library", True))
opts.Add(BoolVariable("builtin_freetype", "Use the built-in FreeType library", True))
opts.Add(BoolVariable("builtin_libogg", "Use the built-in libogg library", True))
opts.Add(BoolVariable("builtin_libpng", "Use the built-in libpng library", True))
opts.Add(BoolVariable("builtin_libtheora", "Use the built-in libtheora library", True))
opts.Add(BoolVariable("builtin_libvorbis", "Use the built-in libvorbis library", True))
opts.Add(BoolVariable("builtin_libvpx", "Use the built-in libvpx library", True))
opts.Add(BoolVariable("builtin_libwebp", "Use the built-in libwebp library", True))
opts.Add(BoolVariable("builtin_wslay", "Use the built-in wslay library", True))
opts.Add(BoolVariable("builtin_mbedtls", "Use the built-in mbedTLS library", True))
opts.Add(BoolVariable("builtin_miniupnpc", "Use the built-in miniupnpc library", True))
opts.Add(BoolVariable("builtin_opus", "Use the built-in Opus library", True))
opts.Add(BoolVariable("builtin_pcre2", "Use the built-in PCRE2 library", True))
opts.Add(BoolVariable("builtin_pcre2_with_jit", "Use JIT compiler for the built-in PCRE2 library", True))
opts.Add(BoolVariable("builtin_recast", "Use the built-in Recast library", True))
opts.Add(BoolVariable("builtin_rvo2", "Use the built-in RVO2 library", True))
opts.Add(BoolVariable("builtin_squish", "Use the built-in squish library", True))
opts.Add(BoolVariable("builtin_xatlas", "Use the built-in xatlas library", True))
opts.Add(BoolVariable("builtin_zlib", "Use the built-in zlib library", True))
opts.Add(BoolVariable("builtin_zstd", "Use the built-in Zstd library", True))

# Compilation environment setup
opts.Add("CXX", "C++ compiler")
opts.Add("CC", "C compiler")
opts.Add("LINK", "Linker")
opts.Add("CCFLAGS", "Custom flags for both the C and C++ compilers")
opts.Add("CFLAGS", "Custom flags for the C compiler")
opts.Add("CXXFLAGS", "Custom flags for the C++ compiler")
opts.Add("LINKFLAGS", "Custom flags for the linker")

# Update the environment to have all above options defined
# in following code (especially platform and custom_modules).
opts.Update(env_base)

# Platform selection: validate input, and add options.

selected_platform = ""

if env_base["platform"] != "":
    selected_platform = env_base["platform"]
elif env_base["p"] != "":
    selected_platform = env_base["p"]
else:
    # Missing `platform` argument, try to detect platform automatically
    if (
        sys.platform.startswith("linux")
        or sys.platform.startswith("dragonfly")
        or sys.platform.startswith("freebsd")
        or sys.platform.startswith("netbsd")
        or sys.platform.startswith("openbsd")
    ):
        selected_platform = "x11"
    elif sys.platform == "darwin":
        selected_platform = "osx"
    elif sys.platform == "win32":
        selected_platform = "windows"
    else:
        print("Could not detect platform automatically. Supported platforms:")
        for x in platform_list:
            print("\t" + x)
        print("\nPlease run SCons again and select a valid platform: platform=<string>")

    if selected_platform != "":
        print("Automatically detected platform: " + selected_platform)

if selected_platform == "macos":
    # Alias for forward compatibility.
    print('Platform "macos" is still called "osx" in Godot 3.x. Building for platform "osx".')
    selected_platform = "osx"

if selected_platform == "ios":
    # Alias for forward compatibility.
    print('Platform "ios" is still called "iphone" in Godot 3.x. Building for platform "iphone".')
    selected_platform = "iphone"

if selected_platform in ["linux", "bsd", "linuxbsd"]:
    if selected_platform == "linuxbsd":
        # Alias for forward compatibility.
        print('Platform "linuxbsd" is still called "x11" in Godot 3.x. Building for platform "x11".')
    # Alias for convenience.
    selected_platform = "x11"

if selected_platform == "web":
    # Alias for forward compatibility.
    print('Platform "web" is still called "javascript" in Godot 3.x. Building for platform "javascript".')
    selected_platform = "javascript"

# Make sure to update this to the found, valid platform as it's used through the buildsystem as the reference.
# It should always be re-set after calling `opts.Update()` otherwise it uses the original input value.
env_base["platform"] = selected_platform

# Add platform-specific options.
if selected_platform in platform_opts:
    for opt in platform_opts[selected_platform]:
        opts.Add(opt)

# Update the environment to take platform-specific options into account.
opts.Update(env_base)
env_base["platform"] = selected_platform  # Must always be re-set after calling opts.Update().

# Detect modules.
modules_detected = OrderedDict()
module_search_paths = ["modules"]  # Built-in path.

if env_base["custom_modules"]:
    paths = env_base["custom_modules"].split(",")
    for p in paths:
        try:
            module_search_paths.append(methods.convert_custom_modules_path(p))
        except ValueError as e:
            print(e)
            sys.exit(255)

for path in module_search_paths:
    if path == "modules":
        # Built-in modules don't have nested modules,
        # so save the time it takes to parse directories.
        modules = methods.detect_modules(path, recursive=False)
    else:  # Custom.
        modules = methods.detect_modules(path, env_base["custom_modules_recursive"])
        # Provide default include path for both the custom module search `path`
        # and the base directory containing custom modules, as it may be different
        # from the built-in "modules" name (e.g. "custom_modules/summator/summator.h"),
        # so it can be referenced simply as `#include "summator/summator.h"`
        # independently of where a module is located on user's filesystem.
        env_base.Prepend(CPPPATH=[path, os.path.dirname(path)])
    # Note: custom modules can override built-in ones.
    modules_detected.update(modules)

# Add module options
for name, path in modules_detected.items():
    sys.path.insert(0, path)
    import config

    if env_base["modules_enabled_by_default"]:
        enabled = True
        try:
            enabled = config.is_enabled()
        except AttributeError:
            pass
    else:
        enabled = False

    sys.path.remove(path)
    sys.modules.pop("config")

    opts.Add(BoolVariable("module_" + name + "_enabled", "Enable module '%s'" % (name,), enabled))

methods.write_modules(modules_detected)

# Update the environment again after all the module options are added.
opts.Update(env_base)
env_base["platform"] = selected_platform  # Must always be re-set after calling opts.Update().
Help(opts.GenerateHelpText(env_base))

# add default include paths

env_base.Prepend(CPPPATH=["#"])

# configure ENV for platform
env_base.platform_exporters = platform_exporters
env_base.platform_apis = platform_apis

# Build type defines - more platform-specific ones can be in detect.py.
if env_base["target"] == "release_debug" or env_base["target"] == "debug":
    # DEBUG_ENABLED enables debugging *features* and debug-only code, which is intended
    # to give *users* extra debugging information for their game development.
    env_base.Append(CPPDEFINES=["DEBUG_ENABLED"])

if env_base["target"] == "debug":
    # DEV_ENABLED enables *engine developer* code which should only be compiled for those
    # working on the engine itself.
    env_base.Append(CPPDEFINES=["DEV_ENABLED"])
else:
    # Disable assert() for production targets (only used in thirdparty code).
    env_base.Append(CPPDEFINES=["NDEBUG"])

# SCons speed optimization controlled by the `fast_unsafe` option, which provide
# more than 10 s speed up for incremental rebuilds.
# Unsafe as they reduce the certainty of rebuilding all changed files, so it's
# enabled by default for `debug` builds, and can be overridden from command line.
# Ref: https://github.com/SCons/scons/wiki/GoFastButton
if methods.get_cmdline_bool("fast_unsafe", env_base["target"] == "debug"):
    # Renamed to `content-timestamp` in SCons >= 4.2, keeping MD5 for compat.
    env_base.Decider("MD5-timestamp")
    env_base.SetOption("implicit_cache", 1)
    env_base.SetOption("max_drift", 60)

if env_base["use_precise_math_checks"]:
    env_base.Append(CPPDEFINES=["PRECISE_MATH_CHECKS"])

if env_base["tools"]:
    if not env_base.File("#main/splash_editor.png").exists():
        # Force disabling editor splash if missing.
        env_base["no_editor_splash"] = True
    if env_base["no_editor_splash"]:
        env_base.Append(CPPDEFINES=["NO_EDITOR_SPLASH"])

if not env_base["deprecated"]:
    env_base.Append(CPPDEFINES=["DISABLE_DEPRECATED"])

if env_base["rids"] == "handles":
    env_base.Append(CPPDEFINES=["RID_HANDLES_ENABLED"])
    print("WARNING: Building with RIDs as handles.")

if env_base["rids"] == "tracked_handles":
    env_base.Append(CPPDEFINES=["RID_HANDLES_ENABLED"])
    env_base.Append(CPPDEFINES=["RID_HANDLE_ALLOCATION_TRACKING_ENABLED"])
    print("WARNING: Building with RIDs as tracked handles.")

env_base.Append(CPPDEFINES=["BT_USE_DOUBLE_PRECISION"])  # Enable double precision in bullet physics engine

if selected_platform in platform_list:
    tmppath = "./platform/" + selected_platform
    sys.path.insert(0, tmppath)
    import detect

    env = env_base.Clone()

    # Default num_jobs to local cpu count if not user specified.
    # SCons has a peculiarity where user-specified options won't be overridden
    # by SetOption, so we can rely on this to know if we should use our default.
    initial_num_jobs = env.GetOption("num_jobs")
    altered_num_jobs = initial_num_jobs + 1
    env.SetOption("num_jobs", altered_num_jobs)
    # os.cpu_count() requires Python 3.4+.
    if hasattr(os, "cpu_count") and env.GetOption("num_jobs") == altered_num_jobs:
        cpu_count = os.cpu_count()
        if cpu_count is None:
            print("Couldn't auto-detect CPU count to configure build parallelism. Specify it with the -j argument.")
        else:
            safer_cpu_count = cpu_count if cpu_count <= 4 else cpu_count - 1
            print(
                "Auto-detected %d CPU cores available for build parallelism. Using %d cores by default. You can override it with the -j argument."
                % (cpu_count, safer_cpu_count)
            )
            env.SetOption("num_jobs", safer_cpu_count)

    env.extra_suffix = ""

    if env["extra_suffix"] != "":
        env.extra_suffix += "." + env["extra_suffix"]

    # Environment flags
    CCFLAGS = env.get("CCFLAGS", "")
    env["CCFLAGS"] = ""
    env.Append(CCFLAGS=str(CCFLAGS).split())

    CFLAGS = env.get("CFLAGS", "")
    env["CFLAGS"] = ""
    env.Append(CFLAGS=str(CFLAGS).split())

    CXXFLAGS = env.get("CXXFLAGS", "")
    env["CXXFLAGS"] = ""
    env.Append(CXXFLAGS=str(CXXFLAGS).split())

    LINKFLAGS = env.get("LINKFLAGS", "")
    env["LINKFLAGS"] = ""
    env.Append(LINKFLAGS=str(LINKFLAGS).split())

    # Platform specific flags.
    # These can sometimes override default options.
    flag_list = platform_flags[selected_platform]
    for f in flag_list:
        if not (f[0] in ARGUMENTS):  # allow command line to override platform flags
            env[f[0]] = f[1]

    # 'dev' and 'production' are aliases to set default options if they haven't been
    # set manually by the user.
    # These need to be checked *after* platform specific flags so that different
    # default values can be set (e.g. to keep LTO off for `production` on some platforms).
    if env["dev"]:
        env["verbose"] = methods.get_cmdline_bool("verbose", True)
        env["warnings"] = ARGUMENTS.get("warnings", "extra")
        env["werror"] = methods.get_cmdline_bool("werror", True)
    if env["production"]:
        env["use_static_cpp"] = methods.get_cmdline_bool("use_static_cpp", True)
        env["debug_symbols"] = methods.get_cmdline_bool("debug_symbols", False)
        # LTO "auto" means we handle the preferred option in each platform detect.py.
        env["lto"] = ARGUMENTS.get("lto", "auto")
        if not env["tools"] and env["target"] == "debug":
            print(
                "WARNING: Requested `production` build with `tools=no target=debug`, "
                "this will give you a full debug template (use `target=release_debug` "
                "for an optimized template with debug features)."
            )

    # Run SCU file generation script if in a SCU build.
    if env["scu_build"]:
        methods.set_scu_folders(scu_builders.generate_scu_files(env["verbose"], env_base["target"] != "debug"))

    # Must happen after the flags' definition, as configure is when most flags
    # are actually handled to change compile options, etc.
    detect.configure(env)

    # Needs to happen after configure to handle "auto".
    if env["lto"] != "none":
        print("Using LTO: " + env["lto"])

    # Set our C and C++ standard requirements.
    # Prepending to make it possible to override
    # This needs to come after `configure`, otherwise we don't have env.msvc.
    if not env.msvc:
        # Specifying GNU extensions support explicitly, which are supported by
        # both GCC and Clang. This mirrors GCC and Clang's current default
        # compile flags if no -std is specified.
        env.Prepend(CFLAGS=["-std=gnu11"])
        env.Prepend(CXXFLAGS=["-std=gnu++14"])
    else:
        # MSVC doesn't have clear C standard support, /std only covers C++.
        # We apply it to CCFLAGS (both C and C++ code) in case it impacts C features.
        env.Prepend(CCFLAGS=["/std:c++14"])

    # Handle renamed options.
    if "use_lto" in ARGUMENTS or "use_thinlto" in ARGUMENTS:
        print("Error: The `use_lto` and `use_thinlto` boolean options have been unified to `lto=<none|thin|full>`.")
        print("       Please adjust your scripts accordingly.")
        Exit(255)
    if "use_lld" in ARGUMENTS:
        print("Error: The `use_lld` boolean option has been replaced by `linker=<default|bfd|gold|lld|mold>`.")
        print("       Please adjust your scripts accordingly.")
        Exit(255)

    # Disable exception handling. Godot doesn't use exceptions anywhere, and this
    # saves around 20% of binary size and very significant build time (GH-80513).
    if env["disable_exceptions"]:
        if env.msvc:
            env.Append(CPPDEFINES=[("_HAS_EXCEPTIONS", 0)])
        else:
            env.Append(CCFLAGS=["-fno-exceptions"])
    elif env.msvc:
        env.Append(CCFLAGS=["/EHsc"])

    # Configure compiler warnings
    if env.msvc:  # MSVC
        # Truncations, narrowing conversions, signed/unsigned comparisons...
        disable_nonessential_warnings = ["/wd4267", "/wd4244", "/wd4305", "/wd4018", "/wd4800"]
        if env["warnings"] == "extra":
            env.Append(CCFLAGS=["/Wall"])  # Implies /W4
        elif env["warnings"] == "all":
            env.Append(CCFLAGS=["/W3"] + disable_nonessential_warnings)
        elif env["warnings"] == "moderate":
            env.Append(CCFLAGS=["/W2"] + disable_nonessential_warnings)
        else:  # 'no'
            env.Append(CCFLAGS=["/w"])

        if env["werror"]:
            env.Append(CCFLAGS=["/WX"])
            env.Append(LINKFLAGS=["/WX"])
    else:  # GCC, Clang
        version = methods.get_compiler_version(env) or [-1, -1]

        common_warnings = []

        if methods.using_gcc(env):
            common_warnings += ["-Wno-misleading-indentation"]
            if version[0] >= 7:
                common_warnings += ["-Wshadow-local"]
        elif methods.using_clang(env) or methods.using_emcc(env):
            # We often implement `operator<` for structs of pointers as a requirement
            # for putting them in `Set` or `Map`. We don't mind about unreliable ordering.
            common_warnings += ["-Wno-ordered-compare-function-pointers"]

        if env["warnings"] == "extra":
            # Note: enable -Wimplicit-fallthrough for Clang (already part of -Wextra for GCC)
            # once we switch to C++11 or later (necessary for our FALLTHROUGH macro).
            env.Append(CCFLAGS=["-Wall", "-Wextra", "-Wwrite-strings", "-Wno-unused-parameter"] + common_warnings)
            env.Append(CXXFLAGS=["-Wctor-dtor-privacy", "-Wnon-virtual-dtor"])
            if methods.using_gcc(env):
                env.Append(
                    CCFLAGS=[
                        "-Walloc-zero",
                        "-Wduplicated-branches",
                        "-Wduplicated-cond",
                        "-Wstringop-overflow=4",
                        "-Wlogical-op",
                    ]
                )
                env.Append(CXXFLAGS=["-Wnoexcept", "-Wplacement-new=1"])
                if version[0] >= 9:
                    env.Append(CCFLAGS=["-Wattribute-alias=2"])
        elif env["warnings"] == "all":
            env.Append(CCFLAGS=["-Wall"] + common_warnings)
        elif env["warnings"] == "moderate":
            env.Append(CCFLAGS=["-Wall", "-Wno-unused"] + common_warnings)
        else:  # 'no'
            env.Append(CCFLAGS=["-w"])

        if env["werror"]:
            env.Append(CCFLAGS=["-Werror"])
            if methods.using_gcc(env) and version[0] >= 12:  # False positives in our error macros, see GH-58747.
                env.Append(CCFLAGS=["-Wno-error=return-type"])

    if hasattr(detect, "get_program_suffix"):
        suffix = "." + detect.get_program_suffix()
    else:
        suffix = "." + selected_platform

    if env["target"] == "release":
        if env["tools"]:
            print("ERROR: The editor can only be built with `target=debug` or `target=release_debug`.")
            print("       Use `tools=no target=release` to build a release export template.")
            Exit(255)
        suffix += ".opt"
    elif env["target"] == "release_debug":
        if env["tools"]:
            suffix += ".opt.tools"
        else:
            suffix += ".opt.debug"
    else:
        if env["tools"]:
            print(
                "Note: Building a debug binary (which will run slowly). Use `target=release_debug` to build an optimized release binary."
            )
            suffix += ".tools"
        else:
            print(
                "Note: Building a debug binary (which will run slowly). Use `target=release` to build an optimized release binary."
            )
            suffix += ".debug"

    if env["arch"] != "":
        suffix += "." + env["arch"]
    elif env["bits"] == "32":
        suffix += ".32"
    elif env["bits"] == "64":
        suffix += ".64"

    suffix += env.extra_suffix

    sys.path.remove(tmppath)
    sys.modules.pop("detect")

    modules_enabled = OrderedDict()
    env.module_icons_paths = []
    env.doc_class_path = {}

    for name, path in modules_detected.items():
        if not env["module_" + name + "_enabled"]:
            continue
        sys.path.insert(0, path)
        env.current_module = name
        import config

        # can_build changed number of arguments between 3.0 (1) and 3.1 (2),
        # so try both to preserve compatibility for 3.0 modules
        can_build = False
        try:
            can_build = config.can_build(env, selected_platform)
        except TypeError:
            print(
                "Warning: module '%s' uses a deprecated `can_build` "
                "signature in its config.py file, it should be "
                "`can_build(env, platform)`." % x
            )
            can_build = config.can_build(selected_platform)
        if can_build:
            config.configure(env)
            # Get doc classes paths (if present)
            try:
                doc_classes = config.get_doc_classes()
                doc_path = config.get_doc_path()
                for c in doc_classes:
                    env.doc_class_path[c] = path + "/" + doc_path
            except Exception:
                pass
            # Get icon paths (if present)
            try:
                icons_path = config.get_icons_path()
                env.module_icons_paths.append(path + "/" + icons_path)
            except Exception:
                # Default path for module icons
                env.module_icons_paths.append(path + "/" + "icons")
            modules_enabled[name] = path

        sys.path.remove(path)
        sys.modules.pop("config")

    env.module_list = modules_enabled

    methods.generate_version_header(env.module_version_string)

    env["PROGSUFFIX"] = suffix + env.module_version_string + env["PROGSUFFIX"]
    env["OBJSUFFIX"] = suffix + env["OBJSUFFIX"]
    # (SH)LIBSUFFIX will be used for our own built libraries
    # LIBSUFFIXES contains LIBSUFFIX and SHLIBSUFFIX by default,
    # so we need to append the default suffixes to keep the ability
    # to link against thirdparty libraries (.a, .so, .lib, etc.).
    if os.name == "nt":
        # On Windows, only static libraries and import libraries can be
        # statically linked - both using .lib extension
        env["LIBSUFFIXES"] += [env["LIBSUFFIX"]]
    else:
        env["LIBSUFFIXES"] += [env["LIBSUFFIX"], env["SHLIBSUFFIX"]]
    env["LIBSUFFIX"] = suffix + env["LIBSUFFIX"]
    env["SHLIBSUFFIX"] = suffix + env["SHLIBSUFFIX"]

    if env.use_ptrcall:
        env.Append(CPPDEFINES=["PTRCALL_ENABLED"])
    if env["tools"]:
        env.Append(CPPDEFINES=["TOOLS_ENABLED"])
    if env["disable_3d"]:
        if env["tools"]:
            print(
                "Build option 'disable_3d=yes' cannot be used with 'tools=yes' (editor), "
                "only with 'tools=no' (export template)."
            )
            sys.exit(255)
        else:
            env.Append(CPPDEFINES=["_3D_DISABLED"])
    if env["disable_advanced_gui"]:
        if env["tools"]:
            print(
                "Build option 'disable_advanced_gui=yes' cannot be used with 'tools=yes' (editor), "
                "only with 'tools=no' (export template)."
            )
            sys.exit(255)
        else:
            env.Append(CPPDEFINES=["ADVANCED_GUI_DISABLED"])
    if env["minizip"]:
        env.Append(CPPDEFINES=["MINIZIP_ENABLED"])

    editor_module_list = ["freetype"]
    for x in editor_module_list:
        if not env["module_" + x + "_enabled"]:
            if env["tools"]:
                print(
                    "Build option 'module_" + x + "_enabled=no' cannot be used with 'tools=yes' (editor), "
                    "only with 'tools=no' (export template)."
                )
                sys.exit(255)

    if not env["verbose"]:
        methods.no_verbose(sys, env)

    if not env["platform"] == "server":  # FIXME: detect GLES3
        env.Append(
            BUILDERS={
                "GLES3_GLSL": env.Builder(
                    action=run_in_subprocess(gles_builders.build_gles3_headers), suffix="glsl.gen.h", src_suffix=".glsl"
                )
            }
        )
        env.Append(
            BUILDERS={
                "GLES2_GLSL": env.Builder(
                    action=run_in_subprocess(gles_builders.build_gles2_headers), suffix="glsl.gen.h", src_suffix=".glsl"
                )
            }
        )

    scons_cache_path = os.environ.get("SCONS_CACHE")
    if scons_cache_path != None:
        CacheDir(scons_cache_path)
        print("Scons cache enabled... (path: '" + scons_cache_path + "')")

    if env["vsproj"]:
        env.vs_incs = []
        env.vs_srcs = []

    if env["compiledb"]:
        # Generating the compilation DB (`compile_commands.json`) requires SCons 4.0.0 or later.
        from SCons import __version__ as scons_raw_version

        scons_ver = env._get_major_minor_revision(scons_raw_version)

        if scons_ver < (4, 0, 0):
            print("The `compiledb=yes` option requires SCons 4.0 or later, but your version is %s." % scons_raw_version)
            Exit(255)

        env.Tool("compilation_db")
        env.Alias("compiledb", env.CompilationDatabase())

    Export("env")

    # build subdirs, the build order is dependent on link order.

    SConscript("core/SCsub")
    SConscript("servers/SCsub")
    SConscript("scene/SCsub")
    if env["tools"]:
        SConscript("editor/SCsub")
    SConscript("drivers/SCsub")

    SConscript("platform/SCsub")
    SConscript("modules/SCsub")
    SConscript("main/SCsub")

    SConscript("platform/" + selected_platform + "/SCsub")  # build selected platform

    # Microsoft Visual Studio Project Generation
    if env["vsproj"]:
        if os.name != "nt":
            print("Error: The `vsproj` option is only usable on Windows with Visual Studio.")
            Exit(255)
        env["CPPPATH"] = [Dir(path) for path in env["CPPPATH"]]
        methods.generate_vs_project(env, GetOption("num_jobs"))
        methods.generate_cpp_hint_file("cpp.hint")

    # Check for the existence of headers
    conf = Configure(env)
    if "check_c_headers" in env:
        for header in env["check_c_headers"]:
            if conf.CheckCHeader(header[0]):
                env.AppendUnique(CPPDEFINES=[header[1]])

elif selected_platform != "":
    if selected_platform == "list":
        print("The following platforms are available:\n")
    else:
        print('Invalid target platform "' + selected_platform + '".')
        print("The following platforms were detected:\n")

    for x in platform_list:
        print("\t" + x)

    print("\nPlease run SCons again and select a valid platform: platform=<string>")

    if selected_platform == "list":
        # Exit early to suppress the rest of the built-in SCons messages
        sys.exit(0)
    else:
        sys.exit(255)

# The following only makes sense when the 'env' is defined, and assumes it is.
if "env" in locals():
    methods.show_progress(env)
    # TODO: replace this with `env.Dump(format="json")`
    # once we start requiring SCons 4.0 as min version.
    methods.dump(env)

    methods.clean_cache(env)


def print_elapsed_time():
    elapsed_time_sec = round(time.time() - time_at_start, 3)
    time_ms = round((elapsed_time_sec % 1) * 1000)
    print("[Time elapsed: {}.{:03}]".format(time.strftime("%H:%M:%S", time.gmtime(elapsed_time_sec)), time_ms))


atexit.register(print_elapsed_time)
