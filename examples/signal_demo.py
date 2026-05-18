"""Python port of examples/signal_demo.gro.

A pure-visual demo: no cells, just five signal grids whose source
locations are animated in time. Demonstrates that a `.py` program can
be all-world-program and run with no ecoli call.
"""

from gro import *
import math


set_param("dt", 0.001)

N = 5
signals = [signal(diffusion=1.0, degradation=0.75) for _ in range(N)]


class Main(WorldProgram):
    """Animate each signal's source position with a parametric sin/cos
    loop. Same shape as the CCL original's `program main()`."""
    state = State(a=0.0)

    @always
    def tick(self):
        self.state.a += 0.25 * dt()
        a = self.state.a
        for i, h in enumerate(signals):
            x = 250 * math.sin((i + 1) * a + 6.28 * i / 5.0)
            y = 250 * math.cos((N + 1 - i) * a + 6.28 * i / 5.0)
            set_signal(h, x, y, 100)


set_main(Main)
