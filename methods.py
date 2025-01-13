import atexit
import contextlib
import glob
import math
import os
import re
import subprocess
import sys
from collections import OrderedDict
from enum import Enum
from io import StringIO, TextIOWrapper
from pathlib import Path
from typing import Generator, List, Optional, Union, cast

# Get the "Godot" folder name ahead of time
base_folder_path = str(os.path.abspath(Path(__file__).parent)) + "/"
base_folder_only = os.path.basename(os.path.normpath(base_folder_path))
# Listing all the folders we have converted
# for SCU in scu_builders.py
_scu_folders = set()
# Colors are disabled in non-TTY environments such as pipes. This means
# that if output is redirected to a file, it won't contain color codes.
# Colors are always enabled on continuous integration.
_colorize = bool(sys.stdout.isatty() or os.environ.get("CI"))


def set_scu_folders(scu_folders):
    global _scu_folders
    _scu_folders = scu_folders


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


def print_warning(*values: object) -> None:
    """Prints a warning message with formatting."""
    print(f"{ANSI.YELLOW}{ANSI.BOLD}WARNING:{ANSI.REGULAR}", *values, ANSI.RESET, file=sys.stderr)


def print_error(*values: object) -> None:
    """Prints an error message with formatting."""
    print(f"{ANSI.RED}{ANSI.BOLD}ERROR:{ANSI.REGULAR}", *values, ANSI.RESET, file=sys.stderr)


def add_source_files_orig(self, sources, files, allow_gen=False):
    # Convert string to list of absolute paths (including expanding wildcard)
    if isinstance(files, str):
        # Exclude .gen.cpp files from globbing, to avoid including obsolete ones.
        # They should instead be added manually.
        skip_gen_cpp = "*" in files
        files = self.Glob(files)
        if skip_gen_cpp and not allow_gen:
            files = [f for f in files if not str(f).endswith(".gen.cpp")]

    # Add each path as compiled Object following environment (self) configuration
    for path in files:
        obj = self.Object(path)
        if obj in sources:
            print_warning('Object "{}" already included in environment sources.'.format(obj))
            continue
        sources.append(obj)


def add_source_files_scu(self, sources, files, allow_gen=False):
    if self["scu_build"] and isinstance(files, str):
        if "*." not in files:
            return False

        # If the files are in a subdirectory, we want to create the scu gen
        # files inside this subdirectory.
        subdir = os.path.dirname(files)
        subdir = subdir if subdir == "" else subdir + "/"
        section_name = self.Dir(subdir).tpath
        section_name = section_name.replace("\\", "/")  # win32
        # if the section name is in the hash table?
        # i.e. is it part of the SCU build?
        global _scu_folders
        if section_name not in (_scu_folders):
            return False

        # Add all the gen.cpp files in the SCU directory
        add_source_files_orig(self, sources, subdir + "scu/scu_*.gen.cpp", True)
        return True
    return False


# Either builds the folder using the SCU system,
# or reverts to regular build.
def add_source_files(self, sources, files, allow_gen=False):
    if not add_source_files_scu(self, sources, files, allow_gen):
        # Wraps the original function when scu build is not active.
        add_source_files_orig(self, sources, files, allow_gen)
        return False
    return True


def disable_warnings(self):
    # 'self' is the environment
    if self.msvc and not using_clang(self):
        # We have to remove existing warning level defines before appending /w,
        # otherwise we get: "warning D9025 : overriding '/W3' with '/w'"
        WARN_FLAGS = ["/Wall", "/W4", "/W3", "/W2", "/W1", "/W0"]
        self["CCFLAGS"] = [x for x in self["CCFLAGS"] if x not in WARN_FLAGS]
        self["CFLAGS"] = [x for x in self["CFLAGS"] if x not in WARN_FLAGS]
        self["CXXFLAGS"] = [x for x in self["CXXFLAGS"] if x not in WARN_FLAGS]
        self.AppendUnique(CCFLAGS=["/w"])
    else:
        self.AppendUnique(CCFLAGS=["-w"])


def force_optimization_on_debug(self):
    # 'self' is the environment
    if self["target"] == "template_release":
        return

    if self.msvc:
        # We have to remove existing optimization level defines before appending /O2,
        # otherwise we get: "warning D9025 : overriding '/0d' with '/02'"
        self["CCFLAGS"] = [x for x in self["CCFLAGS"] if not x.startswith("/O")]
        self["CFLAGS"] = [x for x in self["CFLAGS"] if not x.startswith("/O")]
        self["CXXFLAGS"] = [x for x in self["CXXFLAGS"] if not x.startswith("/O")]
        self.AppendUnique(CCFLAGS=["/O2"])
    else:
        self.AppendUnique(CCFLAGS=["-O3"])


def add_module_version_string(self, s):
    self.module_version_string += "." + s


def get_version_info(module_version_string="", silent=False):
    build_name = "custom_build"
    if os.getenv("BUILD_NAME") is not None:
        build_name = str(os.getenv("BUILD_NAME"))
        if not silent:
            print(f"Using custom build name: '{build_name}'.")

    import version

    version_info = {
        "short_name": str(version.short_name),
        "name": str(version.name),
        "major": int(version.major),
        "minor": int(version.minor),
        "patch": int(version.patch),
        "status": str(version.status),
        "build": str(build_name),
        "module_config": str(version.module_config) + module_version_string,
        "website": str(version.website),
        "docs_branch": str(version.docs),
    }

    # For dev snapshots (alpha, beta, RC, etc.) we do not commit status change to Git,
    # so this define provides a way to override it without having to modify the source.
    if os.getenv("GODOT_VERSION_STATUS") is not None:
        version_info["status"] = str(os.getenv("GODOT_VERSION_STATUS"))
        if not silent:
            print(f"Using version status '{version_info['status']}', overriding the original '{version.status}'.")

    # Parse Git hash if we're in a Git repo.
    githash = ""
    gitfolder = ".git"

    if os.path.isfile(".git"):
        with open(".git", "r", encoding="utf-8") as file:
            module_folder = file.readline().strip()
        if module_folder.startswith("gitdir: "):
            gitfolder = module_folder[8:]

    if os.path.isfile(os.path.join(gitfolder, "HEAD")):
        with open(os.path.join(gitfolder, "HEAD"), "r", encoding="utf8") as file:
            head = file.readline().strip()
        if head.startswith("ref: "):
            ref = head[5:]
            # If this directory is a Git worktree instead of a root clone.
            parts = gitfolder.split("/")
            if len(parts) > 2 and parts[-2] == "worktrees":
                gitfolder = "/".join(parts[0:-2])
            head = os.path.join(gitfolder, ref)
            packedrefs = os.path.join(gitfolder, "packed-refs")
            if os.path.isfile(head):
                with open(head, "r", encoding="utf-8") as file:
                    githash = file.readline().strip()
            elif os.path.isfile(packedrefs):
                # Git may pack refs into a single file. This code searches .git/packed-refs file for the current ref's hash.
                # https://mirrors.edge.kernel.org/pub/software/scm/git/docs/git-pack-refs.html
                for line in open(packedrefs, "r", encoding="utf-8").read().splitlines():
                    if line.startswith("#"):
                        continue
                    (line_hash, line_ref) = line.split(" ")
                    if ref == line_ref:
                        githash = line_hash
                        break
        else:
            githash = head

    version_info["git_hash"] = githash
    # Fallback to 0 as a timestamp (will be treated as "unknown" in the engine).
    version_info["git_timestamp"] = 0

    # Get the UNIX timestamp of the build commit.
    if os.path.exists(".git"):
        try:
            version_info["git_timestamp"] = subprocess.check_output(
                ["git", "log", "-1", "--pretty=format:%ct", "--no-show-signature", githash]
            ).decode("utf-8")
        except (subprocess.CalledProcessError, OSError):
            # `git` not found in PATH.
            pass

    return version_info


def get_cmdline_bool(option, default):
    """We use `ARGUMENTS.get()` to check if options were manually overridden on the command line,
    and SCons' _text2bool helper to convert them to booleans, otherwise they're handled as strings.
    """
    from SCons.Script import ARGUMENTS
    from SCons.Variables.BoolVariable import _text2bool

    cmdline_val = ARGUMENTS.get(option)
    if cmdline_val is not None:
        return _text2bool(cmdline_val)
    else:
        return default


def detect_modules(search_path, recursive=False):
    """Detects and collects a list of C++ modules at specified path

    `search_path` - a directory path containing modules. The path may point to
    a single module, which may have other nested modules. A module must have
    "register_types.h", "SCsub", "config.py" files created to be detected.

    `recursive` - if `True`, then all subdirectories are searched for modules as
    specified by the `search_path`, otherwise collects all modules under the
    `search_path` directory. If the `search_path` is a module, it is collected
    in all cases.

    Returns an `OrderedDict` with module names as keys, and directory paths as
    values. If a path is relative, then it is a built-in module. If a path is
    absolute, then it is a custom module collected outside of the engine source.
    """
    modules = OrderedDict()

    def add_module(path):
        module_name = os.path.basename(path)
        module_path = path.replace("\\", "/")  # win32
        modules[module_name] = module_path

    def is_engine(path):
        # Prevent recursively detecting modules in self and other
        # Godot sources when using `custom_modules` build option.
        version_path = os.path.join(path, "version.py")
        if os.path.exists(version_path):
            with open(version_path, "r", encoding="utf-8") as f:
                if 'short_name = "godot"' in f.read():
                    return True
        return False

    def get_files(path):
        files = glob.glob(os.path.join(path, "*"))
        # Sort so that `register_module_types` does not change that often,
        # and plugins are registered in alphabetic order as well.
        files.sort()
        return files

    if not recursive:
        if is_module(search_path):
            add_module(search_path)
        for path in get_files(search_path):
            if is_engine(path):
                continue
            if is_module(path):
                add_module(path)
    else:
        to_search = [search_path]
        while to_search:
            path = to_search.pop()
            if is_module(path):
                add_module(path)
            for child in get_files(path):
                if not os.path.isdir(child):
                    continue
                if is_engine(child):
                    continue
                to_search.insert(0, child)
    return modules


def is_module(path):
    if not os.path.isdir(path):
        return False
    must_exist = ["register_types.h", "SCsub", "config.py"]
    for f in must_exist:
        if not os.path.exists(os.path.join(path, f)):
            return False
    return True


def convert_custom_modules_path(path):
    if not path:
        return path
    path = os.path.realpath(os.path.expanduser(os.path.expandvars(path)))
    err_msg = "Build option 'custom_modules' must %s"
    if not os.path.isdir(path):
        raise ValueError(err_msg % "point to an existing directory.")
    if path == os.path.realpath("modules"):
        raise ValueError(err_msg % "be a directory other than built-in `modules` directory.")
    return path


def module_add_dependencies(self, module, dependencies, optional=False):
    """
    Adds dependencies for a given module.
    Meant to be used in module `can_build` methods.
    """
    if module not in self.module_dependencies:
        self.module_dependencies[module] = [[], []]
    if optional:
        self.module_dependencies[module][1].extend(dependencies)
    else:
        self.module_dependencies[module][0].extend(dependencies)


def module_check_dependencies(self, module):
    """
    Checks if module dependencies are enabled for a given module,
    and prints a warning if they aren't.
    Meant to be used in module `can_build` methods.
    Returns a boolean (True if dependencies are satisfied).
    """
    missing_deps = set()
    required_deps = self.module_dependencies[module][0] if module in self.module_dependencies else []
    for dep in required_deps:
        opt = "module_{}_enabled".format(dep)
        if opt not in self or not self[opt] or not module_check_dependencies(self, dep):
            missing_deps.add(dep)

    if missing_deps:
        if module not in self.disabled_modules:
            print_warning(
                "Disabling '{}' module as the following dependencies are not satisfied: {}".format(
                    module, ", ".join(missing_deps)
                )
            )
            self.disabled_modules.add(module)
        return False
    else:
        return True


def sort_module_list(env):
    deps = {k: v[0] + list(filter(lambda x: x in env.module_list, v[1])) for k, v in env.module_dependencies.items()}

    frontier = list(env.module_list.keys())
    explored = []
    while len(frontier):
        cur = frontier.pop()
        deps_list = deps[cur] if cur in deps else []
        if len(deps_list) and any([d not in explored for d in deps_list]):
            # Will explore later, after its dependencies
            frontier.insert(0, cur)
            continue
        explored.append(cur)
    for k in explored:
        env.module_list.move_to_end(k)


def use_windows_spawn_fix(self, platform=None):
    if os.name != "nt":
        return  # not needed, only for windows

    def mySubProcess(cmdline, env):
        startupinfo = subprocess.STARTUPINFO()
        startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        popen_args = {
            "stdin": subprocess.PIPE,
            "stdout": subprocess.PIPE,
            "stderr": subprocess.PIPE,
            "startupinfo": startupinfo,
            "shell": False,
            "env": env,
        }
        popen_args["text"] = True
        proc = subprocess.Popen(cmdline, **popen_args)
        _, err = proc.communicate()
        rv = proc.wait()
        if rv:
            print_error(err)
        elif len(err) > 0 and not err.isspace():
            print(err)
        return rv

    def mySpawn(sh, escape, cmd, args, env):
        # Used by TEMPFILE.
        if cmd == "del":
            os.remove(args[1])
            return 0

        newargs = " ".join(args[1:])
        cmdline = cmd + " " + newargs

        rv = 0
        env = {str(key): str(value) for key, value in iter(env.items())}
        rv = mySubProcess(cmdline, env)

        return rv

    self["SPAWN"] = mySpawn


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
    zip_archive_message = "{}Archiving {}$TARGET{} ...{}".format(*colors)
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
    env["ZIPCOMSTR"] = zip_archive_message
    env["GENCOMSTR"] = generated_file_message


def detect_visual_c_compiler_version(tools_env):
    # tools_env is the variable scons uses to call tools that execute tasks, SCons's env['ENV'] that executes tasks...
    # (see the SCons documentation for more information on what it does)...
    # in order for this function to be well encapsulated i choose to force it to receive SCons's TOOLS env (env['ENV']
    # and not scons setup environment (env)... so make sure you call the right environment on it or it will fail to detect
    # the proper vc version that will be called

    # There is no flag to give to visual c compilers to set the architecture, i.e. scons arch argument (x86_32, x86_64, arm64, etc.).
    # There are many different cl.exe files that are run, and each one compiles & links to a different architecture
    # As far as I know, the only way to figure out what compiler will be run when Scons calls cl.exe via Program()
    # is to check the PATH variable and figure out which one will be called first. Code below does that and returns:
    # the following string values:

    # ""              Compiler not detected
    # "amd64"         Native 64 bit compiler
    # "amd64_x86"     64 bit Cross Compiler for 32 bit
    # "x86"           Native 32 bit compiler
    # "x86_amd64"     32 bit Cross Compiler for 64 bit

    # There are other architectures, but Godot does not support them currently, so this function does not detect arm/amd64_arm
    # and similar architectures/compilers

    # Set chosen compiler to "not detected"
    vc_chosen_compiler_index = -1
    vc_chosen_compiler_str = ""

    # VS 2017 and newer should set VCTOOLSINSTALLDIR
    if "VCTOOLSINSTALLDIR" in tools_env:
        # Newer versions have a different path available
        vc_amd64_compiler_detection_index = (
            tools_env["PATH"].upper().find(tools_env["VCTOOLSINSTALLDIR"].upper() + "BIN\\HOSTX64\\X64;")
        )
        if vc_amd64_compiler_detection_index > -1:
            vc_chosen_compiler_index = vc_amd64_compiler_detection_index
            vc_chosen_compiler_str = "amd64"

        vc_amd64_x86_compiler_detection_index = (
            tools_env["PATH"].upper().find(tools_env["VCTOOLSINSTALLDIR"].upper() + "BIN\\HOSTX64\\X86;")
        )
        if vc_amd64_x86_compiler_detection_index > -1 and (
            vc_chosen_compiler_index == -1 or vc_chosen_compiler_index > vc_amd64_x86_compiler_detection_index
        ):
            vc_chosen_compiler_index = vc_amd64_x86_compiler_detection_index
            vc_chosen_compiler_str = "amd64_x86"

        vc_x86_compiler_detection_index = (
            tools_env["PATH"].upper().find(tools_env["VCTOOLSINSTALLDIR"].upper() + "BIN\\HOSTX86\\X86;")
        )
        if vc_x86_compiler_detection_index > -1 and (
            vc_chosen_compiler_index == -1 or vc_chosen_compiler_index > vc_x86_compiler_detection_index
        ):
            vc_chosen_compiler_index = vc_x86_compiler_detection_index
            vc_chosen_compiler_str = "x86"

        vc_x86_amd64_compiler_detection_index = (
            tools_env["PATH"].upper().find(tools_env["VCTOOLSINSTALLDIR"].upper() + "BIN\\HOSTX86\\X64;")
        )
        if vc_x86_amd64_compiler_detection_index > -1 and (
            vc_chosen_compiler_index == -1 or vc_chosen_compiler_index > vc_x86_amd64_compiler_detection_index
        ):
            vc_chosen_compiler_str = "x86_amd64"

    return vc_chosen_compiler_str


def find_visual_c_batch_file(env):
    # TODO: We should investigate if we can avoid relying on SCons internals here.
    from SCons.Tool.MSCommon.vc import find_batch_file, find_vc_pdir, get_default_version, get_host_target

    msvc_version = get_default_version(env)

    # Syntax changed in SCons 4.4.0.
    if env.scons_version >= (4, 4, 0):
        (host_platform, target_platform, _) = get_host_target(env, msvc_version)
    else:
        (host_platform, target_platform, _) = get_host_target(env)

    if env.scons_version < (4, 6, 0):
        return find_batch_file(env, msvc_version, host_platform, target_platform)[0]

    # SCons 4.6.0+ removed passing env, so we need to get the product_dir ourselves first,
    # then pass that as the last param instead of env as the first param as before.
    # Param names need to be explicit, as they were shuffled around in SCons 4.8.0.
    product_dir = find_vc_pdir(msvc_version=msvc_version, env=env)

    return find_batch_file(msvc_version, host_platform, target_platform, product_dir)[0]


def generate_cpp_hint_file(filename):
    if os.path.isfile(filename):
        # Don't overwrite an existing hint file since the user may have customized it.
        pass
    else:
        try:
            with open(filename, "w", encoding="utf-8", newline="\n") as fd:
                fd.write("#define GDCLASS(m_class, m_inherits)\n")
                for name in ["GDVIRTUAL", "EXBIND", "MODBIND"]:
                    for count in range(13):
                        for suffix in ["", "R", "C", "RC"]:
                            fd.write(f"#define {name}{count}{suffix}(")
                            if "R" in suffix:
                                fd.write("m_ret, ")
                            fd.write("m_name")
                            for idx in range(1, count + 1):
                                fd.write(f", type{idx}")
                            fd.write(")\n")

        except OSError:
            print_warning("Could not write cpp.hint file.")


def glob_recursive(pattern, node="."):
    from SCons import Node
    from SCons.Script import Glob

    results = []
    for f in Glob(str(node) + "/*", source=True):
        if type(f) is Node.FS.Dir:
            results += glob_recursive(pattern, f)
    results += Glob(str(node) + "/" + pattern, source=True)
    return results


def precious_program(env, program, sources, **args):
    program = env.ProgramOriginal(program, sources, **args)
    env.Precious(program)
    return program


def add_shared_library(env, name, sources, **args):
    library = env.SharedLibrary(name, sources, **args)
    env.NoCache(library)
    return library


def add_library(env, name, sources, **args):
    library = env.Library(name, sources, **args)
    env.NoCache(library)
    return library


def add_program(env, name, sources, **args):
    program = env.Program(name, sources, **args)
    env.NoCache(program)
    return program


def CommandNoCache(env, target, sources, command, **args):
    result = env.Command(target, sources, command, **args)
    env.NoCache(result)
    return result


def Run(env, function):
    from SCons.Script import Action

    return Action(function, "$GENCOMSTR")


def detect_darwin_sdk_path(platform, env):
    sdk_name = ""
    if platform == "macos":
        sdk_name = "macosx"
        var_name = "MACOS_SDK_PATH"
    elif platform == "ios":
        sdk_name = "iphoneos"
        var_name = "IOS_SDK_PATH"
    elif platform == "iossimulator":
        sdk_name = "iphonesimulator"
        var_name = "IOS_SDK_PATH"
    else:
        raise Exception("Invalid platform argument passed to detect_darwin_sdk_path")

    if not env[var_name]:
        try:
            sdk_path = subprocess.check_output(["xcrun", "--sdk", sdk_name, "--show-sdk-path"]).strip().decode("utf-8")
            if sdk_path:
                env[var_name] = sdk_path
        except (subprocess.CalledProcessError, OSError):
            print_error("Failed to find SDK path while running xcrun --sdk {} --show-sdk-path.".format(sdk_name))
            raise


def is_apple_clang(env):
    import shlex

    if env["platform"] not in ["macos", "ios"]:
        return False
    if not using_clang(env):
        return False
    try:
        version = subprocess.check_output(shlex.split(env.subst(env["CXX"])) + ["--version"]).strip().decode("utf-8")
    except (subprocess.CalledProcessError, OSError):
        print_warning("Couldn't parse CXX environment variable to infer compiler version.")
        return False
    return version.startswith("Apple")


def get_compiler_version(env):
    """
    Returns a dictionary with various version information:

    - major, minor, patch: Version following semantic versioning system
    - metadata1, metadata2: Extra information
    - date: Date of the build
    """
    import shlex

    ret = {
        "major": -1,
        "minor": -1,
        "patch": -1,
        "metadata1": "",
        "metadata2": "",
        "date": "",
        "apple_major": -1,
        "apple_minor": -1,
        "apple_patch1": -1,
        "apple_patch2": -1,
        "apple_patch3": -1,
    }

    if env.msvc and not using_clang(env):
        try:
            # FIXME: `-latest` works for most cases, but there are edge-cases where this would
            # benefit from a more nuanced search.
            # https://github.com/godotengine/godot/pull/91069#issuecomment-2358956731
            # https://github.com/godotengine/godot/pull/91069#issuecomment-2380836341
            args = [
                env["VSWHERE"],
                "-latest",
                "-prerelease",
                "-products",
                "*",
                "-requires",
                "Microsoft.Component.MSBuild",
                "-utf8",
            ]
            version = subprocess.check_output(args, encoding="utf-8").strip()
            for line in version.splitlines():
                split = line.split(":", 1)
                if split[0] == "catalog_productDisplayVersion":
                    sem_ver = split[1].split(".")
                    ret["major"] = int(sem_ver[0])
                    ret["minor"] = int(sem_ver[1])
                    ret["patch"] = int(sem_ver[2].split()[0])
                # Could potentially add section for determining preview version, but
                # that can wait until metadata is actually used for something.
                if split[0] == "catalog_buildVersion":
                    ret["metadata1"] = split[1]
        except (subprocess.CalledProcessError, OSError):
            print_warning("Couldn't find vswhere to determine compiler version.")
        return ret

    # Not using -dumpversion as some GCC distros only return major, and
    # Clang used to return hardcoded 4.2.1: # https://reviews.llvm.org/D56803
    try:
        version = subprocess.check_output(
            shlex.split(env.subst(env["CXX"])) + ["--version"], shell=(os.name == "nt"), encoding="utf-8"
        ).strip()
    except (subprocess.CalledProcessError, OSError):
        print_warning("Couldn't parse CXX environment variable to infer compiler version.")
        return ret

    match = re.search(
        r"(?:(?<=version )|(?<=\) )|(?<=^))"
        r"(?P<major>\d+)"
        r"(?:\.(?P<minor>\d*))?"
        r"(?:\.(?P<patch>\d*))?"
        r"(?:-(?P<metadata1>[0-9a-zA-Z-]*))?"
        r"(?:\+(?P<metadata2>[0-9a-zA-Z-]*))?"
        r"(?: (?P<date>[0-9]{8}|[0-9]{6})(?![0-9a-zA-Z]))?",
        version,
    )
    if match is not None:
        for key, value in match.groupdict().items():
            if value is not None:
                ret[key] = value

    match_apple = re.search(
        r"(?:(?<=clang-)|(?<=\) )|(?<=^))"
        r"(?P<apple_major>\d+)"
        r"(?:\.(?P<apple_minor>\d*))?"
        r"(?:\.(?P<apple_patch1>\d*))?"
        r"(?:\.(?P<apple_patch2>\d*))?"
        r"(?:\.(?P<apple_patch3>\d*))?",
        version,
    )
    if match_apple is not None:
        for key, value in match_apple.groupdict().items():
            if value is not None:
                ret[key] = value

    # Transform semantic versioning to integers
    for key in [
        "major",
        "minor",
        "patch",
        "apple_major",
        "apple_minor",
        "apple_patch1",
        "apple_patch2",
        "apple_patch3",
    ]:
        ret[key] = int(ret[key] or -1)
    return ret


def using_gcc(env):
    return "gcc" in os.path.basename(env["CC"])


def using_clang(env):
    return "clang" in os.path.basename(env["CC"])


def using_emcc(env):
    return "emcc" in os.path.basename(env["CC"])


def show_progress(env):
    # Progress reporting is not available in non-TTY environments since it messes with the output
    # (for example, when writing to a file). Ninja has its own progress/tracking tool that clashes
    # with ours.
    if not env["progress"] or not sys.stdout.isatty() or env["ninja"]:
        return

    NODE_COUNT_FILENAME = f"{base_folder_path}.scons_node_count"

    class ShowProgress:
        def __init__(self):
            self.count = 0
            self.max = 0
            try:
                with open(NODE_COUNT_FILENAME, "r", encoding="utf-8") as f:
                    self.max = int(f.readline())
            except OSError:
                pass
            if self.max == 0:
                print("NOTE: Performing initial build, progress percentage unavailable!")

        def __call__(self, node, *args, **kw):
            self.count += 1
            if self.max != 0:
                percent = int(min(self.count * 100 / self.max, 100))
                sys.stdout.write(f"\r[{percent:3d}%] ")
                sys.stdout.flush()

    from SCons.Script import Progress

    progressor = ShowProgress()
    Progress(progressor)

    def progress_finish(target, source, env):
        try:
            with open(NODE_COUNT_FILENAME, "w", encoding="utf-8", newline="\n") as f:
                f.write(f"{progressor.count}\n")
        except OSError:
            pass

    env.AlwaysBuild(
        env.CommandNoCache(
            "progress_finish", [], env.Action(progress_finish, "Building node count database .scons_node_count")
        )
    )


def convert_size(size_bytes: int) -> str:
    if size_bytes == 0:
        return "0 bytes"
    SIZE_NAMES = ["bytes", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB"]
    index = math.floor(math.log(size_bytes, 1024))
    power = math.pow(1024, index)
    size = round(size_bytes / power, 2)
    return f"{size} {SIZE_NAMES[index]}"


def get_size(start_path: str = ".") -> int:
    total_size = 0
    for dirpath, _, filenames in os.walk(start_path):
        for file in filenames:
            path = os.path.join(dirpath, file)
            total_size += os.path.getsize(path)
    return total_size


def clean_cache(cache_path: str, cache_limit: int, verbose: bool):
    files = glob.glob(os.path.join(cache_path, "*", "*"))
    if not files:
        return

    # Remove all text files, store binary files in list of (filename, size, atime).
    purge = []
    texts = []
    stats = []
    for file in files:
        try:
            # Save file stats to rewrite after modifying.
            tmp_stat = os.stat(file)
            # Failing a utf-8 decode is the easiest way to determine if a file is binary.
            try:
                with open(file, encoding="utf-8") as out:
                    out.read(1024)
            except UnicodeDecodeError:
                stats.append((file, *tmp_stat[6:8]))
                # Restore file stats after reading.
                os.utime(file, (tmp_stat[7], tmp_stat[8]))
            else:
                texts.append(file)
        except OSError:
            print_error(f'Failed to access cache file "{file}"; skipping.')

    if texts:
        count = len(texts)
        for file in texts:
            try:
                os.remove(file)
            except OSError:
                print_error(f'Failed to remove cache file "{file}"; skipping.')
                count -= 1
        if verbose:
            print("Purging %d text %s from cache..." % (count, "files" if count > 1 else "file"))

    if cache_limit:
        # Sort by most recent access (most sensible to keep) first. Search for the first entry where
        # the cache limit is reached.
        stats.sort(key=lambda x: x[2], reverse=True)
        sum = 0
        for index, stat in enumerate(stats):
            sum += stat[1]
            if sum > cache_limit:
                purge.extend([x[0] for x in stats[index:]])
                break

    if purge:
        count = len(purge)
        for file in purge:
            try:
                os.remove(file)
            except OSError:
                print_error(f'Failed to remove cache file "{file}"; skipping.')
                count -= 1
        if verbose:
            print("Purging %d %s from cache..." % (count, "files" if count > 1 else "file"))


def prepare_cache(env) -> None:
    if env.GetOption("clean"):
        return

    cache_path = ""
    if env["cache_path"]:
        cache_path = cast(str, env["cache_path"])
    elif os.environ.get("SCONS_CACHE"):
        print_warning("Environment variable `SCONS_CACHE` is deprecated; use `cache_path` argument instead.")
        cache_path = cast(str, os.environ.get("SCONS_CACHE"))

    if not cache_path:
        return

    env.CacheDir(cache_path)
    print(f'SCons cache enabled... (path: "{cache_path}")')

    if env["cache_limit"]:
        cache_limit = float(env["cache_limit"])
    elif os.environ.get("SCONS_CACHE_LIMIT"):
        print_warning("Environment variable `SCONS_CACHE_LIMIT` is deprecated; use `cache_limit` argument instead.")
        cache_limit = float(os.getenv("SCONS_CACHE_LIMIT", "0")) / 1024  # Old method used MiB, convert to GiB

    # Convert GiB to bytes; treat negative numbers as 0 (unlimited).
    cache_limit = max(0, int(cache_limit * 1024 * 1024 * 1024))
    if env["verbose"]:
        print(
            "Current cache limit is {} (used: {})".format(
                convert_size(cache_limit) if cache_limit else "∞",
                convert_size(get_size(cache_path)),
            )
        )

    atexit.register(clean_cache, cache_path, cache_limit, env["verbose"])


def dump(env):
    # Dumps latest build information for debugging purposes and external tools.
    from json import dump

    def non_serializable(obj):
        return "<<non-serializable: %s>>" % (type(obj).__qualname__)

    with open(".scons_env.json", "w", encoding="utf-8", newline="\n") as f:
        dump(env.Dictionary(), f, indent=4, default=non_serializable)


# Custom Visual Studio project generation logic that supports any platform that has a msvs.py
# script, so Visual Studio can be used to run scons for any platform, with the right defines per target.
# Invoked with scons vsproj=yes
#
# Only platforms that opt in to vs proj generation by having a msvs.py file in the platform folder are included.
# Platforms with a msvs.py file will be added to the solution, but only the current active platform+target+arch
# will have a build configuration generated, because we only know what the right defines/includes/flags/etc are
# on the active build target.
#
# Platforms that don't support an editor target will have a dummy editor target that won't do anything on build,
# but will have the files and configuration for the windows editor target.
#
# To generate build configuration files for all platforms+targets+arch combinations, users can call
#   scons vsproj=yes
# for each combination of platform+target+arch. This will generate the relevant vs project files but
# skip the build process. This lets project files be quickly generated even if there are build errors.
#
# To generate AND build from the command line:
#   scons vsproj=yes vsproj_gen_only=no
def generate_vs_project(env, original_args, project_name="godot"):
    # Augmented glob_recursive that also fills the dirs argument with traversed directories that have content.
    def glob_recursive_2(pattern, dirs, node="."):
        from SCons import Node
        from SCons.Script import Glob

        results = []
        for f in Glob(str(node) + "/*", source=True):
            if type(f) is Node.FS.Dir:
                results += glob_recursive_2(pattern, dirs, f)
        r = Glob(str(node) + "/" + pattern, source=True)
        if len(r) > 0 and str(node) not in dirs:
            d = ""
            for part in str(node).split("\\"):
                d += part
                if d not in dirs:
                    dirs.append(d)
                d += "\\"
        results += r
        return results

    def get_bool(args, option, default):
        from SCons.Variables.BoolVariable import _text2bool

        val = args.get(option, default)
        if val is not None:
            try:
                return _text2bool(val)
            except (ValueError, AttributeError):
                return default
        else:
            return default

    def format_key_value(v):
        if type(v) in [tuple, list]:
            return v[0] if len(v) == 1 else f"{v[0]}={v[1]}"
        return v

    def get_dependencies(file, env, exts, headers, sources, others):
        for child in file.children():
            if isinstance(child, str):
                child = env.File(x)
            fname = ""
            try:
                fname = child.path
            except AttributeError:
                # It's not a file.
                pass

            if fname:
                parts = os.path.splitext(fname)
                if len(parts) > 1:
                    ext = parts[1].lower()
                    if ext in exts["sources"]:
                        sources += [fname]
                    elif ext in exts["headers"]:
                        headers += [fname]
                    elif ext in exts["others"]:
                        others += [fname]

            get_dependencies(child, env, exts, headers, sources, others)

    filtered_args = original_args.copy()

    # Ignore the "vsproj" option to not regenerate the VS project on every build
    filtered_args.pop("vsproj", None)

    # This flag allows users to regenerate the proj files but skip the building process.
    # This lets projects be regenerated even if there are build errors.
    filtered_args.pop("vsproj_gen_only", None)

    # This flag allows users to regenerate only the props file without touching the sln or vcxproj files.
    # This preserves any customizations users have done to the solution, while still updating the file list
    # and build commands.
    filtered_args.pop("vsproj_props_only", None)

    # The "progress" option is ignored as the current compilation progress indication doesn't work in VS
    filtered_args.pop("progress", None)

    # We add these three manually because they might not be explicitly passed in, and it's important to always set them.
    filtered_args.pop("platform", None)
    filtered_args.pop("target", None)
    filtered_args.pop("arch", None)

    platform = env["platform"]
    target = env["target"]
    arch = env["arch"]

    vs_configuration = {}
    common_build_prefix = []
    confs = []
    for x in sorted(glob.glob("platform/*")):
        # Only platforms that opt in to vs proj generation are included.
        if not os.path.isdir(x) or not os.path.exists(x + "/msvs.py"):
            continue
        tmppath = "./" + x
        sys.path.insert(0, tmppath)
        import msvs

        vs_plats = []
        vs_confs = []
        try:
            platform_name = x[9:]
            vs_plats = msvs.get_platforms()
            vs_confs = msvs.get_configurations()
            val = []
            for plat in vs_plats:
                val += [{"platform": plat[0], "architecture": plat[1]}]

            vsconf = {"platform": platform_name, "targets": vs_confs, "arches": val}
            confs += [vsconf]

            # Save additional information about the configuration for the actively selected platform,
            # so we can generate the platform-specific props file with all the build commands/defines/etc
            if platform == platform_name:
                common_build_prefix = msvs.get_build_prefix(env)
                vs_configuration = vsconf
        except Exception:
            pass

        sys.path.remove(tmppath)
        sys.modules.pop("msvs")

    extensions = {}
    extensions["headers"] = [".h", ".hh", ".hpp", ".hxx", ".inc"]
    extensions["sources"] = [".c", ".cc", ".cpp", ".cxx", ".m", ".mm", ".java"]
    extensions["others"] = [".natvis", ".glsl", ".rc"]

    headers = []
    headers_dirs = []
    for ext in extensions["headers"]:
        for file in glob_recursive_2("*" + ext, headers_dirs):
            headers.append(str(file).replace("/", "\\"))

    sources = []
    sources_dirs = []
    for ext in extensions["sources"]:
        for file in glob_recursive_2("*" + ext, sources_dirs):
            sources.append(str(file).replace("/", "\\"))

    others = []
    others_dirs = []
    for ext in extensions["others"]:
        for file in glob_recursive_2("*" + ext, others_dirs):
            others.append(str(file).replace("/", "\\"))

    skip_filters = False
    import hashlib
    import json

    md5 = hashlib.md5(
        json.dumps(sorted(headers + headers_dirs + sources + sources_dirs + others + others_dirs)).encode("utf-8")
    ).hexdigest()

    if os.path.exists(f"{project_name}.vcxproj.filters"):
        with open(f"{project_name}.vcxproj.filters", "r", encoding="utf-8") as file:
            existing_filters = file.read()
        match = re.search(r"(?ms)^<!-- CHECKSUM$.([0-9a-f]{32})", existing_filters)
        if match is not None and md5 == match.group(1):
            skip_filters = True

    import uuid

    # Don't regenerate the filters file if nothing has changed, so we keep the existing UUIDs.
    if not skip_filters:
        print(f"Regenerating {project_name}.vcxproj.filters")

        with open("misc/msvs/vcxproj.filters.template", "r", encoding="utf-8") as file:
            filters_template = file.read()
        for i in range(1, 10):
            filters_template = filters_template.replace(f"%%UUID{i}%%", str(uuid.uuid4()))

        filters = ""

        for d in headers_dirs:
            filters += f'<Filter Include="Header Files\\{d}"><UniqueIdentifier>{{{str(uuid.uuid4())}}}</UniqueIdentifier></Filter>\n'
        for d in sources_dirs:
            filters += f'<Filter Include="Source Files\\{d}"><UniqueIdentifier>{{{str(uuid.uuid4())}}}</UniqueIdentifier></Filter>\n'
        for d in others_dirs:
            filters += f'<Filter Include="Other Files\\{d}"><UniqueIdentifier>{{{str(uuid.uuid4())}}}</UniqueIdentifier></Filter>\n'

        filters_template = filters_template.replace("%%FILTERS%%", filters)

        filters = ""
        for file in headers:
            filters += (
                f'<ClInclude Include="{file}"><Filter>Header Files\\{os.path.dirname(file)}</Filter></ClInclude>\n'
            )
        filters_template = filters_template.replace("%%INCLUDES%%", filters)

        filters = ""
        for file in sources:
            filters += (
                f'<ClCompile Include="{file}"><Filter>Source Files\\{os.path.dirname(file)}</Filter></ClCompile>\n'
            )

        filters_template = filters_template.replace("%%COMPILES%%", filters)

        filters = ""
        for file in others:
            filters += f'<None Include="{file}"><Filter>Other Files\\{os.path.dirname(file)}</Filter></None>\n'
        filters_template = filters_template.replace("%%OTHERS%%", filters)

        filters_template = filters_template.replace("%%HASH%%", md5)

        with open(f"{project_name}.vcxproj.filters", "w", encoding="utf-8", newline="\r\n") as f:
            f.write(filters_template)

    headers_active = []
    sources_active = []
    others_active = []

    get_dependencies(
        env.File(f"#bin/godot{env['PROGSUFFIX']}"), env, extensions, headers_active, sources_active, others_active
    )

    all_items = []
    properties = []
    activeItems = []
    extraItems = []

    set_headers = set(headers_active)
    set_sources = set(sources_active)
    set_others = set(others_active)
    for file in headers:
        base_path = os.path.dirname(file).replace("\\", "_")
        all_items.append(f'<ClInclude Include="{file}">')
        all_items.append(
            f"  <ExcludedFromBuild Condition=\"!$(ActiveProjectItemList_{base_path}.Contains(';{file};'))\">true</ExcludedFromBuild>"
        )
        all_items.append("</ClInclude>")
        if file in set_headers:
            activeItems.append(file)

    for file in sources:
        base_path = os.path.dirname(file).replace("\\", "_")
        all_items.append(f'<ClCompile Include="{file}">')
        all_items.append(
            f"  <ExcludedFromBuild Condition=\"!$(ActiveProjectItemList_{base_path}.Contains(';{file};'))\">true</ExcludedFromBuild>"
        )
        all_items.append("</ClCompile>")
        if file in set_sources:
            activeItems.append(file)

    for file in others:
        base_path = os.path.dirname(file).replace("\\", "_")
        all_items.append(f'<None Include="{file}">')
        all_items.append(
            f"  <ExcludedFromBuild Condition=\"!$(ActiveProjectItemList_{base_path}.Contains(';{file};'))\">true</ExcludedFromBuild>"
        )
        all_items.append("</None>")
        if file in set_others:
            activeItems.append(file)

    if vs_configuration:
        vsconf = ""
        for a in vs_configuration["arches"]:
            if arch == a["architecture"]:
                vsconf = f'{target}|{a["platform"]}'
                break

        condition = "'$(GodotConfiguration)|$(GodotPlatform)'=='" + vsconf + "'"
        itemlist = {}
        for item in activeItems:
            key = os.path.dirname(item).replace("\\", "_")
            if key not in itemlist:
                itemlist[key] = [item]
            else:
                itemlist[key] += [item]

        for x in itemlist.keys():
            properties.append(
                "<ActiveProjectItemList_%s>;%s;</ActiveProjectItemList_%s>" % (x, ";".join(itemlist[x]), x)
            )
        output = f'bin\\godot{env["PROGSUFFIX"]}'

        with open("misc/msvs/props.template", "r", encoding="utf-8") as file:
            props_template = file.read()

        props_template = props_template.replace("%%VSCONF%%", vsconf)
        props_template = props_template.replace("%%CONDITION%%", condition)
        props_template = props_template.replace("%%PROPERTIES%%", "\n    ".join(properties))
        props_template = props_template.replace("%%EXTRA_ITEMS%%", "\n    ".join(extraItems))

        props_template = props_template.replace("%%OUTPUT%%", output)

        proplist = [format_key_value(v) for v in list(env["CPPDEFINES"])]
        proplist += [format_key_value(j) for j in env.get("VSHINT_DEFINES", [])]
        props_template = props_template.replace("%%DEFINES%%", ";".join(proplist))

        proplist = [str(j) for j in env["CPPPATH"]]
        proplist += [str(j) for j in env.get("VSHINT_INCLUDES", [])]
        props_template = props_template.replace("%%INCLUDES%%", ";".join(proplist))

        proplist = env["CCFLAGS"]
        proplist += [x for x in env["CXXFLAGS"] if not x.startswith("$")]
        proplist += [str(j) for j in env.get("VSHINT_OPTIONS", [])]
        props_template = props_template.replace("%%OPTIONS%%", " ".join(proplist))

        # Windows allows us to have spaces in paths, so we need
        # to double quote off the directory. However, the path ends
        # in a backslash, so we need to remove this, lest it escape the
        # last double quote off, confusing MSBuild
        common_build_postfix = [
            "--directory=&quot;$(ProjectDir.TrimEnd(&apos;\\&apos;))&quot;",
            "progress=no",
            f"platform={platform}",
            f"target={target}",
            f"arch={arch}",
        ]

        for arg, value in filtered_args.items():
            common_build_postfix.append(f"{arg}={value}")

        cmd_rebuild = [
            "vsproj=yes",
            "vsproj_props_only=yes",
            "vsproj_gen_only=no",
            f"vsproj_name={project_name}",
        ] + common_build_postfix

        cmd_clean = [
            "--clean",
        ] + common_build_postfix

        commands = "scons"
        if len(common_build_prefix) == 0:
            commands = "echo Starting SCons &amp;&amp; cmd /V /C " + commands
        else:
            common_build_prefix[0] = "echo Starting SCons &amp;&amp; cmd /V /C " + common_build_prefix[0]

        cmd = " ^&amp; ".join(common_build_prefix + [" ".join([commands] + common_build_postfix)])
        props_template = props_template.replace("%%BUILD%%", cmd)

        cmd = " ^&amp; ".join(common_build_prefix + [" ".join([commands] + cmd_rebuild)])
        props_template = props_template.replace("%%REBUILD%%", cmd)

        cmd = " ^&amp; ".join(common_build_prefix + [" ".join([commands] + cmd_clean)])
        props_template = props_template.replace("%%CLEAN%%", cmd)

        with open(
            f"{project_name}.{platform}.{target}.{arch}.generated.props", "w", encoding="utf-8", newline="\r\n"
        ) as f:
            f.write(props_template)

    proj_uuid = str(uuid.uuid4())
    sln_uuid = str(uuid.uuid4())

    if os.path.exists(f"{project_name}.sln"):
        for line in open(f"{project_name}.sln", "r", encoding="utf-8").read().splitlines():
            if line.startswith('Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}")'):
                proj_uuid = re.search(
                    r"\"{(\b[0-9a-fA-F]{8}\b-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-\b[0-9a-fA-F]{12}\b)}\"$",
                    line,
                ).group(1)
            elif line.strip().startswith("SolutionGuid ="):
                sln_uuid = re.search(
                    r"{(\b[0-9a-fA-F]{8}\b-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-\b[0-9a-fA-F]{12}\b)}", line
                ).group(1)
                break

    configurations = []
    imports = []
    properties = []
    section1 = []
    section2 = []
    for conf in confs:
        godot_platform = conf["platform"]
        for p in conf["arches"]:
            sln_plat = p["platform"]
            proj_plat = sln_plat
            godot_arch = p["architecture"]

            # Redirect editor configurations for non-Windows platforms to the Windows one, so the solution has all the permutations
            # and VS doesn't complain about missing project configurations.
            # These configurations are disabled, so they show up but won't build.
            if godot_platform != "windows":
                section1 += [f"editor|{sln_plat} = editor|{proj_plat}"]
                section2 += [
                    f"{{{proj_uuid}}}.editor|{proj_plat}.ActiveCfg = editor|{proj_plat}",
                ]

            for t in conf["targets"]:
                godot_target = t

                # Windows x86 is a special little flower that requires a project platform == Win32 but a solution platform == x86.
                if godot_platform == "windows" and godot_target == "editor" and godot_arch == "x86_32":
                    sln_plat = "x86"

                configurations += [
                    f'<ProjectConfiguration Include="{godot_target}|{proj_plat}">',
                    f"  <Configuration>{godot_target}</Configuration>",
                    f"  <Platform>{proj_plat}</Platform>",
                    "</ProjectConfiguration>",
                ]

                properties += [
                    f"<PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='{godot_target}|{proj_plat}'\">",
                    f"  <GodotConfiguration>{godot_target}</GodotConfiguration>",
                    f"  <GodotPlatform>{proj_plat}</GodotPlatform>",
                    "</PropertyGroup>",
                ]

                if godot_platform != "windows":
                    configurations += [
                        f'<ProjectConfiguration Include="editor|{proj_plat}">',
                        "  <Configuration>editor</Configuration>",
                        f"  <Platform>{proj_plat}</Platform>",
                        "</ProjectConfiguration>",
                    ]

                    properties += [
                        f"<PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='editor|{proj_plat}'\">",
                        "  <GodotConfiguration>editor</GodotConfiguration>",
                        f"  <GodotPlatform>{proj_plat}</GodotPlatform>",
                        "</PropertyGroup>",
                    ]

                p = f"{project_name}.{godot_platform}.{godot_target}.{godot_arch}.generated.props"
                imports += [
                    f'<Import Project="$(MSBuildProjectDirectory)\\{p}" Condition="Exists(\'$(MSBuildProjectDirectory)\\{p}\')"/>'
                ]

                section1 += [f"{godot_target}|{sln_plat} = {godot_target}|{sln_plat}"]

                section2 += [
                    f"{{{proj_uuid}}}.{godot_target}|{sln_plat}.ActiveCfg = {godot_target}|{proj_plat}",
                    f"{{{proj_uuid}}}.{godot_target}|{sln_plat}.Build.0 = {godot_target}|{proj_plat}",
                ]

    # Add an extra import for a local user props file at the end, so users can add more overrides.
    imports += [
        f'<Import Project="$(MSBuildProjectDirectory)\\{project_name}.vs.user.props" Condition="Exists(\'$(MSBuildProjectDirectory)\\{project_name}.vs.user.props\')"/>'
    ]
    section1 = sorted(section1)
    section2 = sorted(section2)

    if not get_bool(original_args, "vsproj_props_only", False):
        with open("misc/msvs/vcxproj.template", "r", encoding="utf-8") as file:
            proj_template = file.read()
        proj_template = proj_template.replace("%%UUID%%", proj_uuid)
        proj_template = proj_template.replace("%%CONFS%%", "\n    ".join(configurations))
        proj_template = proj_template.replace("%%IMPORTS%%", "\n  ".join(imports))
        proj_template = proj_template.replace("%%DEFAULT_ITEMS%%", "\n    ".join(all_items))
        proj_template = proj_template.replace("%%PROPERTIES%%", "\n  ".join(properties))

        with open(f"{project_name}.vcxproj", "w", encoding="utf-8", newline="\r\n") as f:
            f.write(proj_template)

    if not get_bool(original_args, "vsproj_props_only", False):
        with open("misc/msvs/sln.template", "r", encoding="utf-8") as file:
            sln_template = file.read()
        sln_template = sln_template.replace("%%NAME%%", project_name)
        sln_template = sln_template.replace("%%UUID%%", proj_uuid)
        sln_template = sln_template.replace("%%SLNUUID%%", sln_uuid)
        sln_template = sln_template.replace("%%SECTION1%%", "\n\t\t".join(section1))
        sln_template = sln_template.replace("%%SECTION2%%", "\n\t\t".join(section2))

        with open(f"{project_name}.sln", "w", encoding="utf-8", newline="\r\n") as f:
            f.write(sln_template)

    if get_bool(original_args, "vsproj_gen_only", True):
        sys.exit()


def generate_copyright_header(filename: str) -> str:
    MARGIN = 70
    TEMPLATE = """\
/**************************************************************************/
/*  %s*/
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/
"""
    filename = filename.split("/")[-1].ljust(MARGIN)
    if len(filename) > MARGIN:
        print(f'WARNING: Filename "{filename}" too large for copyright header.')
    return TEMPLATE % filename


@contextlib.contextmanager
def generated_wrapper(
    path,  # FIXME: type with `Union[str, Node, List[Node]]` when pytest conflicts are resolved
    guard: Optional[bool] = None,
    prefix: str = "",
    suffix: str = "",
) -> Generator[TextIOWrapper, None, None]:
    """
    Wrapper class to automatically handle copyright headers and header guards
    for generated scripts. Meant to be invoked via `with` statement similar to
    creating a file.

    - `path`: The path of the file to be created. Can be passed a raw string, an
    isolated SCons target, or a full SCons target list. If a target list contains
    multiple entries, produces a warning & only creates the first entry.
    - `guard`: Optional bool to determine if a header guard should be added. If
    unassigned, header guards are determined by the file extension.
    - `prefix`: Custom prefix to prepend to a header guard. Produces a warning if
    provided a value when `guard` evaluates to `False`.
    - `suffix`: Custom suffix to append to a header guard. Produces a warning if
    provided a value when `guard` evaluates to `False`.
    """

    # Handle unfiltered SCons target[s] passed as path.
    if not isinstance(path, str):
        if isinstance(path, list):
            if len(path) > 1:
                print_warning(
                    "Attempting to use generated wrapper with multiple targets; "
                    f"will only use first entry: {path[0]}"
                )
            path = path[0]
        if not hasattr(path, "get_abspath"):
            raise TypeError(f'Expected type "str", "Node" or "List[Node]"; was passed {type(path)}.')
        path = path.get_abspath()

    path = str(path).replace("\\", "/")
    if guard is None:
        guard = path.endswith((".h", ".hh", ".hpp", ".inc"))
    if not guard and (prefix or suffix):
        print_warning(f'Trying to assign header guard prefix/suffix while `guard` is disabled: "{path}".')

    header_guard = ""
    if guard:
        if prefix:
            prefix += "_"
        if suffix:
            suffix = f"_{suffix}"
        split = path.split("/")[-1].split(".")
        header_guard = (f"{prefix}{split[0]}{suffix}.{'.'.join(split[1:])}".upper()
                .replace(".", "_").replace("-", "_").replace(" ", "_").replace("__", "_"))  # fmt: skip

    with open(path, "wt", encoding="utf-8", newline="\n") as file:
        file.write(generate_copyright_header(path))
        file.write("\n/* THIS FILE IS GENERATED. EDITS WILL BE LOST. */\n\n")

        if guard:
            file.write(f"#ifndef {header_guard}\n")
            file.write(f"#define {header_guard}\n\n")

        with StringIO(newline="\n") as str_io:
            yield str_io
            file.write(str_io.getvalue().strip() or "/* NO CONTENT */")

        if guard:
            file.write(f"\n\n#endif // {header_guard}")

        file.write("\n")


def to_raw_cstring(value: Union[str, List[str]]) -> str:
    MAX_LITERAL = 16 * 1024

    if isinstance(value, list):
        value = "\n".join(value) + "\n"

    split: List[bytes] = []
    offset = 0
    encoded = value.encode()

    while offset <= len(encoded):
        segment = encoded[offset : offset + MAX_LITERAL]
        offset += MAX_LITERAL
        if len(segment) == MAX_LITERAL:
            # Try to segment raw strings at double newlines to keep readable.
            pretty_break = segment.rfind(b"\n\n")
            if pretty_break != -1:
                segment = segment[: pretty_break + 1]
                offset -= MAX_LITERAL - pretty_break - 1
            # If none found, ensure we end with valid utf8.
            # https://github.com/halloleo/unicut/blob/master/truncate.py
            elif segment[-1] & 0b10000000:
                last_11xxxxxx_index = [i for i in range(-1, -5, -1) if segment[i] & 0b11000000 == 0b11000000][0]
                last_11xxxxxx = segment[last_11xxxxxx_index]
                if not last_11xxxxxx & 0b00100000:
                    last_char_length = 2
                elif not last_11xxxxxx & 0b0010000:
                    last_char_length = 3
                elif not last_11xxxxxx & 0b0001000:
                    last_char_length = 4

                if last_char_length > -last_11xxxxxx_index:
                    segment = segment[:last_11xxxxxx_index]
                    offset += last_11xxxxxx_index

        split += [segment]

    return " ".join(f'R"<!>({x.decode()})<!>"' for x in split)
