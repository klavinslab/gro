"""Python port of examples/edge.gro.

Edge-detection by wave propagation: cells stochastically initiate an
AHL pulse; the pulse spreads to neighbors, which relay it. After a
fixed detection delay, each cell checks whether its local AHL has
already decayed below threshold -- which only happens for cells near
the colony boundary, since interior cells stay surrounded by relays.
Boundary cells fluoresce.

Devised by Rehana Rodrigues, refined by Kevin Oishi.
"""

from gro import *


set_param("dt", 0.1)

k1 = 0.001    # wave initiation rate
tr = 6        # refraction period
td = 3.3      # detection time
se = 100      # signal emit magnitude
sd = 0.1      # signal threshold for relay
st = 0.2     # signal threshold for edge classification

ahl = signal(diffusion=1.0, degradation=1.0)


def edge(k1, tr, td):
    """Parametric edge-detector program."""
    class E(Program):
        # t timer is per-pulse, not per-cell-lifetime, but we still
        # want Preserved (it's not a molecular count). edge_state is
        # the same.
        state = State(t=Preserved(tr * 1.0), edge_state=Preserved(False))

        # Randomly initiate a wave.
        @when(lambda self: rand(100000) < k1 * dt() * 100000)
        def initiate(self):
            self.emit_signal(ahl, se)

        # Propagate when the refractory period is over and signal is
        # detected.
        @when(lambda self: self.state.t > tr and self.get_signal(ahl) > sd)
        def propagate(self):
            self.emit_signal(ahl, se)
            self.state.t = 0.0

        # Check edge condition exactly at td time after the last
        # pulse: if signal has decayed away, we're on the edge.
        @when(lambda self: td - dt() < self.state.t <= td)
        def check_edge(self):
            self.state.edge_state = self.get_signal(ahl) < st

        # Report edge condition via rfp (rises when on edge, decays
        # otherwise).
        @when(lambda self: self.state.edge_state)
        def report_edge(self):
            self.rfp += 1

        @when(lambda self: not self.state.edge_state and self.rfp > 0)
        def decay(self):
            self.rfp -= 1

        @always
        def tick(self):
            self.state.t += dt()
    return E


ecoli(x=0, y=0, program=edge(k1, tr, td))
