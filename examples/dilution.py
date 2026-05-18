"""Python port of examples/dilution.gro.

Three composed program variants for GFP dilution:
- P1: a one-shot loading of GFP (no production), shown to dilute over
  successive cell divisions.
- P2: production + degradation reaching a steady state.
- P3: same as P2 but also prints `(id, t, [GFP])` to stdout every
  5 * dt time units, for offline plotting.

Demonstrates the parametric-factory pattern (`make_gfp(k1, k2, m)`
closure) and nested composition.
"""

from gro import *
import math


set_param("dt", 0.1)


def dilute(m):
    """Pure initializer: set the cell's GFP counter to `m` at spawn.
    No rules — GFP just dilutes through repeated division."""
    class P(Program):
        state = State()
        def setup(self):
            self.gfp = m
    P.__name__ = f"Dilute({m})"
    return P


class Report(Program):
    """When the user selects a cell, print its [GFP] on channel 1."""
    state = State()

    @when(lambda self: self.selected)
    def report(self):
        self.message(1, f"{self.id}: {self.gfp / self.volume:.4f}")


def make_gfp(k1, k2, m):
    """Production at constant rate k1 + degradation at k2 * gfp.
    Initial GFP count is m."""
    class P(Program):
        state = State()
        def setup(self):
            self.gfp = m

        @when(lambda self: rand(100000) < k1 * dt() * 100000)
        def produce(self):
            self.gfp += 1

        @when(lambda self: rand(100000) < k2 * self.gfp * dt() * 100000)
        def degrade(self):
            self.gfp -= 1

    P.__name__ = f"MakeGFP({k1}, {k2}, {m})"
    return P


def output(delta):
    """Every `delta` time units, print `(id, t, [GFP])` to stdout
    for offline plotting. Uses local state `t` (clock) and `s`
    (period accumulator)."""
    class P(Program):
        state = State(t=Preserved(0.0), s=Preserved(0.0))

        @always
        def tick(self):
            self.state.t += dt()
            self.state.s += dt()

        @when(lambda self: self.state.s >= delta)
        def emit(self):
            print(f"{self.id}, {self.state.t}, {self.gfp / self.volume}")
            self.state.s = 0.0

    P.__name__ = f"Output({delta})"
    return P


# PROGRAM 1: pure dilution + selected-only reporting.
P1 = compose(dilute(1000), Report)

# PROGRAM 2: production + degradation + reporting.
alpha = -math.log(0.5) / 20.0
k1 = 100 * alpha
P2 = compose(make_gfp(k1, 0.001, 0), Report)

# PROGRAM 3: same as P2 but also writes data to stdout every 5*dt.
P3 = compose(make_gfp(k1, 0.001, 0), Report, output(5 * dt()))


ecoli(x=0, y=0, program=P1)  # try P2 or P3 here too
