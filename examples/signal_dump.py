"""Python port of examples/signal_dump.gro.

Spatial X/Y reaction-diffusion (same as spatial_oscillations.py),
plus a world program that dumps the X and Y grids to text files
after 5 simulated time units and stops the simulation. Uses Python's
built-in file I/O for the dump.
"""

from gro import *


set_theme({**bright_theme, "signals": [[1, 0, 0], [0, 1, 0]]})

X = signal(diffusion=1.0, degradation=0.0)
Y = signal(diffusion=1.0, degradation=0.0)

reaction([X, Y], [Y, Y], 5)
reaction([X],    [X, X], 5)
reaction([Y],    [],     5)

for _ in range(100):
    set_signal(X, rand(800) - 400, rand(800) - 400, 1)
    set_signal(Y, rand(800) - 400, rand(800) - 400, 1)


class Main(WorldProgram):
    state = State(s=0.0)

    @always
    def tick(self):
        self.state.s += dt()

    @when(lambda self: self.state.s > 5)
    def dump(self):
        with open("/tmp/signalX.txt", "w") as fx:
            fx.write(repr(get_signal_matrix(X)))
        with open("/tmp/signalY.txt", "w") as fy:
            fy.write(repr(get_signal_matrix(Y)))
        stop()


set_main(Main)
