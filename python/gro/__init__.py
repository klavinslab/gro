"""gro — the cell programming language, Python interface.

A gro program in Python begins:

    from gro import *

The simulator-touching primitives are implemented in C++ and exposed
via the embedded `_core` module. Pure-Python idioms (Program, State,
decorators, compose) live here and combine with the C++ primitives to
form the user-facing API.

This module is the Python analogue of `include/gro.gro` — it owns the
default world parameters, the standard types/themes (later milestones),
and the high-level helpers. The simulator calls `_setup_world()` once
per .py load so a fresh World gets the defaults regardless of Python's
module-import cache.
"""

from _core import (
    set_param,
    get_param,
    signal,
    set_signal,
    get_signal,
    ecoli,
    dt,
    time,
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
    set_param("signal_area_width",   800)
    set_param("signal_num_divisions", 160)
    set_param("population_max",      1000)
    set_param("throttle",              0.0)

    set_param("signal_grid_width",   800)
    set_param("signal_grid_height",  800)
    set_param("signal_element_size",   5)

    # Reporters
    for stem in ("gfp", "rfp", "yfp", "cfp"):
        set_param(f"{stem}_saturation_min",  0.0)
        set_param(f"{stem}_saturation_max", 50.0)

    # E. coli
    set_param("ecoli_growth_rate",          0.0346574)  # reactions/min
    set_param("ecoli_init_size",            1.57)       # fL
    set_param("ecoli_division_size_mean",   3.14)       # fL
    set_param("ecoli_division_size_var",    0.005)      # fL
    set_param("ecoli_diameter",             1.0)
    set_param("ecoli_scale",               10.0)        # pixels/um

    # Yeast
    set_param("yeast_growth_rate",          0.015)
    set_param("yeast_division_size_mean",   1.0)
    set_param("yeast_division_size_variance", 0.0001)


__all__ = [
    "set_param",
    "get_param",
    "signal",
    "set_signal",
    "get_signal",
    "ecoli",
    "dt",
    "time",
]
