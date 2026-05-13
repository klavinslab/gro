"""Python port of examples/coupled_oscillator.gro (by Kevin Oishi).

Each cell is a phase oscillator with two modes (GO/WAIT). In GO it
advances its phase `x` at a base rate plus a cell-cell feedback
proportional to the local signal. When `x` crosses 100 the cell
emits a pulse, jumps to WAIT for a refractory period, then resumes.
Coupling synchronizes the population.
"""

from gro import *


chemostat(True)
set_theme(dark_theme)

set_param("dt", 0.1)

s = signal(diffusion=1.0, degradation=1.05)

k0 = 2.5    # base rate of oscillation
kb = 5      # cell-cell feedback strength
tr = 10     # refractory period length
se = 650    # signal emit magnitude

GO, WAIT = 0, 1


def oscillator(g0):
    """One cell's phase oscillator, parameterized by the initial
    phase `g0` so a heterogeneous population can be created."""
    class Osc(Program):
        # mode is a flag and t a timer -- Preserved across division;
        # x is the per-cell phase, also Preserved.
        state = State(mode=Preserved(GO), t=Preserved(0.0), x=Preserved(g0))

        @always
        def update_gfp(self):
            self.gfp = int(0.5 * self.volume * self.state.x)

        # Phase advances at base rate k0 plus feedback kb * local signal.
        @when(lambda self: self.state.mode == GO
                           and rand(100000) < k0 * dt() * 100000)
        def step_base(self):
            self.state.x += 0.01 * (150 - self.state.x)

        @when(lambda self: self.state.mode == GO
                           and rand(100000) < kb * self.get_signal(s) * dt() * 100000)
        def step_feedback(self):
            self.state.x += 0.01 * (150 - self.state.x)

        # Phase reset: emit a pulse and enter refractory.
        @when(lambda self: self.state.x > 100)
        def fire(self):
            self.state.x = 0.0
            self.state.mode = WAIT
            self.emit_signal(s, se)

        # Refractory timer.
        @when(lambda self: self.state.mode == WAIT)
        def refractory_tick(self):
            self.state.t += dt()

        @when(lambda self: self.state.mode == WAIT and self.state.t > tr)
        def refractory_end(self):
            self.state.mode = GO
            self.state.t = 0.0

    Osc.__name__ = f"Oscillator({g0})"
    return Osc


# Seed a single oscillator at the origin; cells inherit the program
# on division and the population synchronizes via signal coupling.
ecoli(x=0, y=0, program=oscillator(0))
