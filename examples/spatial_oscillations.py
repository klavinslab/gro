"""Python port of examples/spatial_oscillations.gro.

A Lotka-Volterra-style signal-only simulation: two signals X and Y
diffuse, X catalyzes its own production, X+Y produces more Y, and Y
decays. Seeds 100 random initial sources of each. No cells.
"""

from gro import *


set_theme({**bright_theme, "signals": [[1, 0, 1], [0, 0, 1]]})

X = signal(diffusion=0.5, degradation=0.0)
Y = signal(diffusion=0.5, degradation=0.0)

reaction([X, Y], [Y, Y], 0.5)
reaction([X],    [X, X], 0.5)
reaction([Y],    [],     0.5)

for _ in range(50):
    set_signal(X, rand(800) - 400, rand(800) - 400, 1)
    set_signal(Y, rand(800) - 400, rand(800) - 400, 1)
