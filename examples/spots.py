"""Python port of examples/spots.gro.

A simple cell state machine driven by two signals (rS, gS). Cells
cycle between three states: idle (waiting for green signal to
decline), broadcasting (emitting both signals, glowing green), and
responding (accumulating rfp triggered by red signal).
"""

from gro import *


set_param("dt", 0.05)

rS = signal(diffusion=0.3, degradation=1.0)
gS = signal(diffusion=3.0, degradation=0.5)


class G1(Program):
    """Three-state cell:
       state=0 -- idle. Transition to 1 when gS drops below thGS, or
                  to 3 when rS exceeds thRS.
       state=1 -- broadcasting. Emit rS+gS, grow slowly, glow green.
       state=3 -- responding. Accumulate rfp; bounce back to 0 when
                  rS drops below thRS again."""
    state = State(state=Preserved(0), thRS=Preserved(1.0), thGS=Preserved(0.5))

    # Slow rfp decay always running.
    @when(lambda self: rand(100000) < 0.05 * self.rfp * dt() * 100000)
    def rfp_decay(self):
        self.rfp -= 1

    # state 0 → 1 (low green signal triggers broadcast).
    @when(lambda self: self.state.state == 0
                       and rand(100000) < 0.01 * dt() * 100000
                       and self.get_signal(gS) < self.state.thGS)
    def begin_broadcast(self):
        self.state.state = 1

    # state 1 -- broadcasting.
    @when(lambda self: self.state.state == 1)
    def broadcast(self):
        self.emit_signal(rS, 70)
        self.emit_signal(gS, 100)
        set_param("ecoli_growth_rate", 0.001)
        self.gfp = 100

    # state 0 → 3 (high red signal triggers response).
    @when(lambda self: self.state.state == 0
                       and self.get_signal(rS) > self.state.thRS)
    def begin_respond(self):
        self.state.state = 3

    # state 3 -- responding (accumulate rfp; bounce back when red drops).
    @when(lambda self: self.state.state == 3)
    def respond(self):
        self.rfp += 1
        self.state.state = 3 if self.get_signal(rS) >= self.state.thRS else 0


class Report(Program):
    state = State()

    @when(lambda self: self.selected)
    def report(self):
        self.message(
            1,
            f"{self.id} rfp: {self.rfp} "
            f"rS:{self.get_signal(rS)} gS:{self.get_signal(gS)}"
        )


def movie(period, path_prefix):
    """Write a snapshot to `<path_prefix><n>.tif` every `period` time
    units. The CCL original spelled the filename inline; in Python
    we take the prefix as a parameter and append a sequence number."""
    class Movie(Program):
        state = State(t=Preserved(0.0), n=Preserved(0))

        @always
        def tick(self):
            self.state.t += dt()

        @when(lambda self: self.state.t > period)
        def shoot(self):
            snapshot(f"{path_prefix}{self.state.n}.tif")
            self.state.n += 1
            self.state.t = 0.0
    return Movie


# Two programs available: P just runs the state machine + report,
# PP also writes snapshots every 5 time units to ./cheetahstat3_<n>.tif.
P  = compose(G1, Report)
PP = compose(P, movie(5, "./cheetahstat3_"))

ecoli(x=0, y=0, program=P)  # swap to PP to write snapshots

start()  # ensure the sim isn't paused at load
