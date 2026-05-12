"""gro — the cell programming language, Python interface.

A gro program in Python begins:

    from gro import *

The simulator-touching primitives are implemented in C++ and exposed
via the embedded `_core` module. Pure-Python idioms (Program, State,
decorators, compose) live here and combine with the C++ primitives to
form the user-facing API.

This module is the Python analogue of `include/gro.gro` — it owns the
default world parameters, the standard helpers, and the high-level
classes. The simulator calls `_setup_world()` once per .py load so a
fresh World gets the defaults regardless of Python's module-import
cache.
"""

import _core
from _core import (
    set_param,
    get_param,
    signal,
    set_signal,
    get_signal_at,
    dt,
    time,
    rand,
    message,
)
from _core import ecoli as _core_ecoli

from gro._program import (
    Program,
    State,
    field,
    Preserved,
    when,
    always,
    rate,
    compose,
    Composed,
    WorldProgram,
    set_main,
    reset,
    GroLoadError,
)


def ecoli(x=0.0, y=0.0, theta=0.0, volume=None, program=None):
    """Spawn an E. coli cell at (x, y) with orientation `theta`.

    If `program` is a `Program` subclass, an instance is created and
    attached to the cell; its `setup()` runs once with the new cell
    installed as the active cell (so any `set_param(...)` in setup
    is per-cell, mirroring CCL).

    Volume defaults to the same compile-time constant CCL uses
    (`DEFAULT_ECOLI_INIT_SIZE`); we forward `None` and let the C++
    binding apply it.
    """
    if program is not None and isinstance(program, type):
        program = program()
    _core_ecoli(x=x, y=y, theta=theta, volume=volume, program=program)


# Cell-local signal API (`emit_signal`, `absorb_signal`, `get_signal`)
# is exposed as methods on `Program`; the user writes
# `self.emit_signal(...)` inside a rule, which forwards to the C++
# binding that uses the current_cell context.


# Theme presets, mirroring those in include/gro.gro.

bright_theme = {
    "background":     "#ffffff",
    "ecoli_edge":     "#777777",
    "ecoli_selected": "#ff0000",
    "chemostat_edge": "#999999",
    "message":        "#999999",
    "mouse":          "#000000",
    "signals": [
        [1.0, 0.0, 1.0],
        [0.0, 1.0, 1.0],
        [1.0, 1.0, 0.0],
        [1.0, 0.0, 0.0],
        [0.0, 1.0, 0.0],
        [0.0, 0.0, 1.0],
    ],
}

dark_theme = {
    "background":     "#000000",
    "ecoli_edge":     "#444444",
    "ecoli_selected": "#880000",
    "chemostat_edge": "#444499",
    "message":        "#ffffff",
    "mouse":          "#ffffff",
    "signals": [
        [1.0, 0.0, 1.0],
        [0.0, 1.0, 1.0],
        [1.0, 1.0, 0.0],
        [1.0, 0.0, 0.0],
        [0.0, 1.0, 0.0],
        [0.0, 0.0, 1.0],
    ],
}


def set_theme(theme):
    """Apply a theme to the active World. Theme is a dict with keys
    matching `bright_theme`/`dark_theme`."""
    _core.set_theme(
        background     = theme["background"],
        ecoli_edge     = theme["ecoli_edge"],
        ecoli_selected = theme["ecoli_selected"],
        chemostat_edge = theme["chemostat_edge"],
        message        = theme["message"],
        mouse          = theme["mouse"],
        signals        = theme["signals"],
    )


def _setup_world():
    """Apply gro's default world parameters to the active World.

    Called by PythonRuntime once at the start of each .py program
    load, before the user's file runs. Mirrors the `set(...)` block
    in include/gro.gro.
    """
    set_param("dt", 0.02)

    set_param("chemostat_width",     200)
    set_param("chemostat_height",    200)
    set_param("population_max",      1000)
    set_param("throttle",              0.0)

    set_param("signal_grid_width",   800)
    set_param("signal_grid_height",  800)
    set_param("signal_element_size",   5)

    for stem in ("gfp", "rfp", "yfp", "cfp"):
        set_param(f"{stem}_saturation_min",  0.0)
        set_param(f"{stem}_saturation_max", 50.0)

    set_param("ecoli_growth_rate",          0.0346574)
    set_param("ecoli_init_size",            1.57)
    set_param("ecoli_division_size_mean",   3.14)
    set_param("ecoli_division_size_var",    0.005)
    set_param("ecoli_diameter",             1.0)
    set_param("ecoli_scale",               10.0)

    set_param("yeast_growth_rate",          0.015)
    set_param("yeast_division_size_mean",   1.0)
    set_param("yeast_division_size_variance", 0.0001)

    # Match gro.gro's `set_theme(bright_theme)` so .py programs start
    # with the same white background.
    set_theme(bright_theme)


__all__ = [
    # Spawning
    "ecoli",
    # Programs & rules
    "Program",
    "State",
    "field",
    "Preserved",
    "when",
    "always",
    "rate",
    "compose",
    "Composed",
    "WorldProgram",
    "set_main",
    "reset",
    # Signals (world-coordinate API; cell-local forms are Program methods)
    "signal",
    "set_signal",
    "get_signal_at",
    # World
    "set_param",
    "get_param",
    "dt",
    "time",
    "message",
    # Themes
    "set_theme",
    "bright_theme",
    "dark_theme",
    # Misc
    "rand",
    "GroLoadError",
]
