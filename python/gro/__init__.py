"""gro — the cell programming language, Python interface.

This is the user-facing module. For a gro program in Python, do:

    from gro import *

Milestone 2: only enough surface to put a cell on the canvas. Higher-
level constructs (Program, State, @when, @always, @rate, compose) land
in later milestones.

Implementation: pure-Python idioms live here; the simulator-touching
primitives are implemented in C++ and exposed via the embedded `_core`
module that ships built into gro itself.
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
