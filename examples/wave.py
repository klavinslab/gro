"""Python port of examples/wave.gro.

A leader cell periodically emits an AHL signal; follower cells relay
the pulse and then enter a refractory mode so they don't respond to
their own emission. Visually: a wave of activity travels across the
colony.
"""

from gro import *

set_param("dt", 0.075)
ahl = signal(diffusion=1.0, degradation=1.0)


# Numeric state is halved by default on cell division (cells split
# molecular counts between mother and daughter). The `t` clock and
# `mode` flag here aren't counts — they're per-cell control state —
# so we mark them Preserved.

class Leader(Program):

    state = State(t=Preserved(2.4))

    def setup(self):
        set_param("ecoli_growth_rate", 0.0)

    @always
    def tick(self):
        self.state.t += dt()

    @when(lambda self: self.state.t > 10)
    def fire(self):
        self.emit_signal(ahl, 100)
        self.state.t = 0


class Follower(Program):

    state = State(mode=Preserved(0), t=Preserved(0.0))

    @when(lambda self: self.state.mode == 0 and self.get_signal(ahl) > 0.01)
    def relay(self):
        self.emit_signal(ahl, 100)
        self.state.mode = 1
        self.state.t = 0

    @when(lambda self: self.state.mode == 1)
    def grow(self):
        self.state.t += dt()

    @when(lambda self: self.state.mode == 1 and self.state.t > 9)
    def reset(self):
        self.state.mode = 0


ecoli(x=0, y=0,  program=Leader)
ecoli(x=0, y=10, program=Follower)
