"""Functions used to generate source files during build time"""

import os


def make_debug_linuxbsd(target, source, env):
    dst = str(target[0])
    os.system('objcopy --only-keep-debug "{0}" "{0}.debugsymbols"'.format(dst))
    os.system('strip --strip-debug --strip-unneeded "{0}"'.format(dst))
    os.system('objcopy --add-gnu-debuglink="{0}.debugsymbols" "{0}"'.format(dst))
