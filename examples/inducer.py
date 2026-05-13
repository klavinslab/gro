"""Python port of examples/inducer.gro.

A cell that produces GFP at a rate gated by the world-level IPTG
concentration. A world program toggles IPTG between 0 and 1 µM/L
every 50 time units, replacing the console message each time so
the user can see the current induction state.
"""

from gro import *


chemostat(True)

# Module-level mutable carrying the current IPTG concentration. Rule
# bodies inside Cells reference it by name (closure-captured at
# class-creation time), but assignment happens in Main below.
iptg = 0.0


class P(Program):
    state = State()

    @when(lambda self: rand(100000)
                       < (1 + 10 * iptg / (1 + iptg)) * dt() * 100000)
    def transcribe(self):
        self.gfp += 1

    @when(lambda self: rand(100000) < 0.001 * self.gfp * dt() * 100000)
    def degrade(self):
        self.gfp -= 1


class Main(WorldProgram):
    state = State(t=0.0)

    @always
    def tick(self):
        self.state.t += dt()

    @when(lambda self: self.state.t > 50)
    def toggle(self):
        global iptg
        self.state.t = 0.0
        iptg = 1.0 - iptg
        clear_messages(1)
        message(1, f"IPTG at {iptg} uM/L")


ecoli(x=0, y=0, program=P)
set_main(Main)
