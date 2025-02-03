import os
import sys
import json
import uuid
import functools
import subprocess
import platform

# NOTE: The multiprocessing module is not compatible with SCons due to conflict on cPickle

if sys.version_info[0] < 3:
    JSON_SERIALIZABLE_TYPES = (bool, int, long, float, basestring)
else:
    JSON_SERIALIZABLE_TYPES = (bool, int, float, str)

# CPU architecture options.
architectures = ["x86_32", "x86_64", "arm32", "arm64", "rv64", "ppc32", "ppc64", "wasm32"]
architecture_aliases = {
    "x86": "x86_32",
    "x64": "x86_64",
    "amd64": "x86_64",
    "armv7": "arm32",
    "armv8": "arm64",
    "arm64v8": "arm64",
    "aarch64": "arm64",
    "rv": "rv64",
    "riscv": "rv64",
    "riscv64": "rv64",
    "ppcle": "ppc32",
    "ppc": "ppc32",
    "ppc64le": "ppc64",
}


def detect_arch():
    host_machine = platform.machine().lower()
    if host_machine in architectures:
        return host_machine
    elif host_machine in architecture_aliases.keys():
        return architecture_aliases[host_machine]
    elif "86" in host_machine:
        # Catches x86, i386, i486, i586, i686, etc.
        return "x86_32"
    else:
        print("Unsupported CPU architecture: " + host_machine)
        print("Falling back to x86_64.")
        return "x86_64"


def run_in_subprocess(builder_function):
    @functools.wraps(builder_function)
    def wrapper(target, source, env):

        # Convert SCons Node instances to absolute paths
        target = [node.srcnode().abspath for node in target]
        source = [node.srcnode().abspath for node in source]

        # Short circuit on non-Windows platforms, no need to run in subprocess
        if sys.platform not in ("win32", "cygwin"):
            return builder_function(target, source, env)

        # Identify module
        module_name = builder_function.__module__
        function_name = builder_function.__name__
        module_path = sys.modules[module_name].__file__
        if module_path.endswith(".pyc") or module_path.endswith(".pyo"):
            module_path = module_path[:-1]

        # Subprocess environment
        subprocess_env = os.environ.copy()
        subprocess_env["PYTHONPATH"] = os.pathsep.join([os.getcwd()] + sys.path)

        # Keep only JSON serializable environment items
        filtered_env = dict((key, value) for key, value in env.items() if isinstance(value, JSON_SERIALIZABLE_TYPES))

        # Save parameters
        args = (target, source, filtered_env)
        data = dict(fn=function_name, args=args)
        json_path = os.path.join(os.environ["TMP"], uuid.uuid4().hex + ".json")
        with open(json_path, "wt") as json_file:
            json.dump(data, json_file, indent=2)
        json_file_size = os.stat(json_path).st_size

        print(
            "Executing builder function in subprocess: "
            "module_path=%r, parameter_file=%r, parameter_file_size=%r, target=%r, source=%r"
            % (module_path, json_path, json_file_size, target, source)
        )
        try:
            exit_code = subprocess.call([sys.executable, module_path, json_path], env=subprocess_env)
        finally:
            try:
                os.remove(json_path)
            except (OSError, IOError) as e:
                # Do not fail the entire build if it cannot delete a temporary file
                print(
                    "WARNING: Could not delete temporary file: path=%r; [%s] %s" % (json_path, e.__class__.__name__, e)
                )

        # Must succeed
        if exit_code:
            raise RuntimeError(
                "Failed to run builder function in subprocess: module_path=%r; data=%r" % (module_path, data)
            )

    return wrapper


def subprocess_main(namespace):

    with open(sys.argv[1]) as json_file:
        data = json.load(json_file)

    fn = namespace[data["fn"]]
    fn(*data["args"])
