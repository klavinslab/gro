"""Python port of examples/bandpass.gro.

A bandpass-detector cell: rfp accumulates while local AHL is in
[0.1, 0.6] and decays otherwise. A world program continuously emits
AHL at the origin so concentration varies with distance and time.
"""

from gro import *


set_param("dt", 0.1)

ahl = signal(diffusion=1.0, degradation=0.01)


class Sensor(Program):
    """Per-cell rfp counter that climbs while local AHL is in the
    sensitive window and decays via mass-action otherwise."""
    state = State()

    @when(lambda self: 0.1 < self.get_signal(ahl) < 0.6)
    def detect(self):
        self.rfp += 1

    @when(lambda self: rand(100000) < 0.01 * self.rfp * dt() * 100000)
    def decay(self):
        self.rfp -= 1


class Report(Program):
    """Selected-only console output. `rfp` is a reporter (per-cell at
    the C++ level) so it doesn't need to be in `share=[...]` for
    Report to see it via `self.rfp`."""
    state = State()

    @when(lambda self: self.selected)
    def report(self):
        self.message(
            1,
            f"cell {self.id}: ahl={self.get_signal(ahl)}, "
            f"rfp/vol={self.rfp / self.volume}",
        )


set_param("rfp_saturation_max", 50)
set_param("rfp_saturation_min",  0)

ecoli(x=0, y=0, program=compose(Sensor, Report))


class Main(WorldProgram):
    """Continuous AHL source at the origin."""
    state = State()

    @always
    def emit_source(self):
        set_signal(ahl, 0, 0, 10)


set_main(Main)
