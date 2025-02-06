def can_build(env, platform):
    import sys

    # Camera module not supported on BSDs
    if "bsd" in sys.platform.lower():
        return False
    return platform == "macos" or platform == "windows" or platform == "linuxbsd"


def configure(env):
    pass
