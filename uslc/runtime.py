"""Explicit free-threaded runtime verification and conservative worker defaults."""

import os
import sys
import sysconfig


def runtime_info() -> dict:
    return {"version": sys.version.splitlines()[0], "executable": sys.executable,
            "free_threaded": bool(sysconfig.get_config_var("Py_GIL_DISABLED")),
            "gil_enabled": sys._is_gil_enabled(), "cpu_count": os.process_cpu_count() or 1}


def require_runtime() -> None:
    if (sys.implementation.name != "cpython" or sys.version_info[:3] != (3, 14, 5)
            or not sysconfig.get_config_var("Py_GIL_DISABLED") or sys._is_gil_enabled()):
        raise RuntimeError("Python 3.14.5+freethreaded with the GIL disabled is required")


def default_workers() -> int:
    return min(4, os.process_cpu_count() or 1) if not sys._is_gil_enabled() else 1
