def can_build(env, platform):
    import sys

    # Supported architectures and platforms depend on the Embree library.
    if env["arch"] == "arm64" and platform == "windows" and env.msvc:
        return False
    # OpenBSD doesn't have the required headers.
    if sys.platform.startswith("openbsd"):
        return False
    if env["arch"] in ["x86_64", "arm64", "wasm32"]:
        return True
    if env["arch"] == "x86_32" and platform == "windows":
        return True
    return False


def configure(env):
    pass
