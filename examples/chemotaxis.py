"""Python port of examples/chemotaxis.gro.

Cells move toward a signal source at the origin using a classic
run-and-tumble strategy: in mode 0 they swim forward; if the signal
is decreasing they switch to tumble (mode 1) with high probability,
if increasing then with low probability. After a refractory period
they return to running.
"""

from gro import *


set_param("dt", 0.1)

s = signal(diffusion=1.0, degradation=0.1)


class Swimmer(Program):
    state = State(m1=0.0, m2=0.0, t=0.0, mode=Preserved(0))

    def setup(self):
        set_param("ecoli_growth_rate", 0.0)

    # Sample the local signal once every 0.25 time units. m1 is the
    # previous sample, m2 the current.
    @when(lambda self: self.state.t > 0.25)
    def sample(self):
        self.state.t = 0.0
        self.state.m1 = self.state.m2
        self.state.m2 = self.get_signal(s)

    @when(lambda self: self.state.mode == 0)
    def run_forward(self):
        self.run(180)

    @when(lambda self: self.state.mode == 1)
    def tumble_in_place(self):
        self.tumble(180)

    # Mode transitions: switch to tumble if signal is dropping (fast
    # response) or stagnant (slow response); leave tumble at a low
    # rate.
    @when(lambda self: self.state.mode == 0 and self.state.m2 < self.state.m1
                       and rand(100000) < 0.5 * dt() * 100000)
    def tumble_on_drop(self):
        self.state.mode = 1

    @when(lambda self: self.state.mode == 0 and self.state.m2 > self.state.m1
                       and rand(100000) < 0.01 * dt() * 100000)
    def tumble_on_rise(self):
        self.state.mode = 1

    @when(lambda self: self.state.mode == 1
                       and rand(100000) < 0.01 * dt() * 100000)
    def run_again(self):
        self.state.mode = 0

    @always
    def clock(self):
        self.state.t += dt()


class Main(WorldProgram):
    """Continuous signal source at the origin."""
    state = State()

    @always
    def emit(self):
        set_signal(s, 0, 0, 100)


for _ in range(50):
    ecoli(x=rand(600) - 300,
          y=rand(600) - 300,
          theta=0.01 * rand(628),
          program=Swimmer)

set_main(Main)
