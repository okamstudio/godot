from __future__ import annotations

import os
import sys
from enum import Enum
from typing import Final

# Colors are disabled in non-TTY environments such as pipes. This means if output is redirected
# to a file, it won't contain color codes. Colors are always enabled on continuous integration.

IS_CI: Final[bool] = bool(os.environ.get("CI"))
STDOUT_TTY: Final[bool] = bool(sys.stdout.isatty())
STDERR_TTY: Final[bool] = bool(sys.stderr.isatty())


def _color_supported(stdout: bool) -> bool:
    """
    Validates if the current environment supports colored output. Attempts to enable ANSI escape
    code support on Windows 10 and later.
    """
    if IS_CI:
        return True

    if sys.platform != "win32":
        return STDOUT_TTY if stdout else STDERR_TTY
    else:
        from ctypes import POINTER, WINFUNCTYPE, WinError, windll
        from ctypes.wintypes import BOOL, DWORD, HANDLE

        STD_HANDLE = -11 if stdout else -12
        ENABLE_VIRTUAL_TERMINAL_PROCESSING = 4

        def err_handler(result, func, args):
            if not result:
                raise WinError()
            return args

        GetStdHandle = WINFUNCTYPE(HANDLE, DWORD)(("GetStdHandle", windll.kernel32), ((1, "nStdHandle"),))
        GetConsoleMode = WINFUNCTYPE(BOOL, HANDLE, POINTER(DWORD))(
            ("GetConsoleMode", windll.kernel32),
            ((1, "hConsoleHandle"), (2, "lpMode")),
        )
        GetConsoleMode.errcheck = err_handler
        SetConsoleMode = WINFUNCTYPE(BOOL, HANDLE, DWORD)(
            ("SetConsoleMode", windll.kernel32),
            ((1, "hConsoleHandle"), (1, "dwMode")),
        )
        SetConsoleMode.errcheck = err_handler

        try:
            handle = GetStdHandle(STD_HANDLE)
            flags = GetConsoleMode(handle)
            SetConsoleMode(handle, flags | ENABLE_VIRTUAL_TERMINAL_PROCESSING)
            return True
        except OSError:
            return False


STDOUT_COLOR: Final[bool] = _color_supported(True)
STDERR_COLOR: Final[bool] = _color_supported(False)
_stdout_override: bool = STDOUT_COLOR
_stderr_override: bool = STDERR_COLOR


def toggle_color(stdout: bool, value: bool | None = None) -> None:
    """
    Explicitly toggle color codes, regardless of support.

    - `stdout`: A boolean to choose the output stream. `True` for stdout, `False` for stderr.
    - `value`: An optional boolean to explicitly set the color state instead of toggling.
    """
    if stdout:
        global _stdout_override
        _stdout_override = value if value is not None else not _stdout_override
    else:
        global _stderr_override
        _stderr_override = value if value is not None else not _stderr_override


class Ansi(Enum):
    """
    Enum class for adding ansi codepoints directly into strings. Automatically converts values to
    strings representing their internal value.
    """

    RESET = "\x1b[0m"

    BOLD = "\x1b[1m"
    DIM = "\x1b[2m"
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
    GRAY = "\x1b[90m"

    def __str__(self) -> str:
        return self.value


def print_info(*values: object) -> None:
    """Prints a informational message with formatting."""
    if _stdout_override:
        print(f"{Ansi.GRAY}{Ansi.BOLD}INFO:{Ansi.REGULAR}", *values, Ansi.RESET)
    else:
        print("INFO:", *values)


def print_warning(*values: object) -> None:
    """Prints a warning message with formatting."""
    if _stderr_override:
        print(f"{Ansi.YELLOW}{Ansi.BOLD}WARNING:{Ansi.REGULAR}", *values, Ansi.RESET, file=sys.stderr)
    else:
        print("WARNING:", *values, file=sys.stderr)


def print_error(*values: object) -> None:
    """Prints an error message with formatting."""
    if _stderr_override:
        print(f"{Ansi.RED}{Ansi.BOLD}ERROR:{Ansi.REGULAR}", *values, Ansi.RESET, file=sys.stderr)
    else:
        print("ERROR:", *values, file=sys.stderr)
