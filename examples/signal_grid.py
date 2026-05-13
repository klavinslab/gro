"""Python port of examples/signal_grid.gro.

Writes a smiley-face bitmap into a signal grid (16×16 superpixels)
and lets a single cell grow proportionally to the signal it sits on.
Demonstrates set_signal at arbitrary world coordinates.
"""

from gro import *
import itertools


b = 20  # superpixel size in world units

set_param("signal_grid_width",  16 * b)
set_param("signal_grid_height", 16 * b)
set_param("signal_element_size", b)

s = signal(diffusion=0.0, degradation=0.01)


# Smiley-face bitmap. Rows go top-to-bottom; columns left-to-right.
bitmap = [
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0],
    [0, 0, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1, 0, 0, 0, 0],
    [0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0],
    [0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0],
    [0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0],
    [0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
]

for col, row in itertools.product(range(16), range(16)):
    set_signal(s, col * b - 8 * b, row * b - 8 * b, 20 * bitmap[row][col])


k1 = 0.0015
k2 = 0.0003


class Grazer(Program):
    """Grow proportional to local signal; consume it."""
    state = State(x=0.0)

    @always
    def feed(self):
        self.state.x = self.get_signal(s)
        set_param("ecoli_growth_rate", k1 * self.state.x)
        self.absorb_signal(s, k2 * self.state.x)


ecoli(x=0, y=0, theta=0.78, program=Grazer)
