"""Python port of examples/game.gro.

A tiny game: a single cell at the origin grows; the user clicks
cells to kill them. Win by keeping the population alive but below
100; lose if it exceeds 100 or hits zero. Reload (cmd-R) to restart.
"""

from gro import *


set_param("dt", 0.01)

message(2, "Select cells to kill them.")
message(2, "Keep the population alive but under 100 cells.")
message(2, "Use Reload and Start/Stop to restart the game.")


class P(Program):
    state = State()

    @when(lambda self: self.selected)
    def kill(self):
        self.die()


class Main(WorldProgram):
    state = State()

    @when(lambda self: stats("pop_size") > 100)
    def overrun(self):
        message(0, "Unable to contain outbreak. You lose!")
        stop()

    @when(lambda self: stats("pop_size") == 0)
    def extinct(self):
        message(0, "Your cells all died! You lose!")
        stop()


ecoli(x=0, y=0, program=P)
set_main(Main)
