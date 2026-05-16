"""Stubbed `_core` for headless testing.

`python/gro/_program.py` and `python/gro/__init__.py` both
`import _core` at top-level. In the live binary `_core` is the
pybind11-embedded C++ module; under unittest we synthesize an
in-memory `_core` whose functions are MagicMocks so the tests can:

- assert that a Python rule called `_core.current_emit_signal(...)`,
- swap a function for a fake to drive state into rules,
- inspect call counts/args after the fact.

Call `install()` once at the top of a test module BEFORE
`import gro`. Returns the fake `_core` module so the test can poke
at it.
"""

import sys
import types
from unittest.mock import MagicMock


# Names of every binding `_program.py` and `__init__.py` reference.
_CORE_NAMES = [
    # World queries
    "rand", "srand", "dt", "time",
    # World param / signal API
    "set_param", "get_param",
    "signal", "set_signal", "get_signal_at", "set_signal_rect",
    "get_signal_matrix", "reaction",
    # Cell-context API
    "current_emit_signal", "current_absorb_signal", "current_get_signal",
    "current_volume", "current_id",
    "current_x", "current_y", "current_theta",
    "current_just_divided", "current_is_daughter", "current_selected",
    "current_get_rep", "current_set_rep",
    "current_die", "current_force_divide",
    "current_run", "current_tumble",
    # Cell spawn / world program
    "ecoli", "yeast",
    "set_main_program", "reset_world",
    # World env / IO
    "set_chemostat_mode", "add_barrier",
    "message", "clear_messages", "snapshot",
    "stop", "start",
    "set_theme", "stats",
]


def install():
    """Synthesize and register a fake `_core` module. Returns it.

    Idempotent: subsequent calls return the same module object so
    that `gro._program._core` (cached at import time) and each test
    file's local `_core` reference all point at the same thing.
    Otherwise per-test-file `install()` calls would create fresh
    MagicMocks that the under-test Program methods never reach."""
    existing = sys.modules.get("_core")
    if existing is not None and getattr(existing, "_gro_stub", False):
        return existing

    mod = types.ModuleType("_core")
    mod._gro_stub = True
    for name in _CORE_NAMES:
        setattr(mod, name, MagicMock(return_value=0, name=f"_core.{name}"))

    # A few specific defaults that affect test semantics:
    mod.dt.return_value = 0.1
    mod.rand.return_value = 0
    mod.current_volume.return_value = 1.0
    mod.current_id.return_value = 0
    mod.current_x.return_value = 0.0
    mod.current_y.return_value = 0.0
    mod.current_theta.return_value = 0.0
    mod.current_just_divided.return_value = False
    mod.current_is_daughter.return_value = False
    mod.current_selected.return_value = False
    mod.current_get_rep.return_value = 0
    mod.current_get_signal.return_value = 0.0
    mod.time.return_value = 0.0
    mod.get_param.return_value = 0.0
    mod.signal.return_value = 0     # signal handle
    mod.stats.return_value = 0
    mod.get_signal_at.return_value = 0.0
    mod.get_signal_matrix.return_value = []

    # Reporter index constants, sourced live by _program.py at import.
    mod.REP_GFP = 0
    mod.REP_RFP = 1
    mod.REP_YFP = 2
    mod.REP_CFP = 3

    sys.modules["_core"] = mod
    return mod
