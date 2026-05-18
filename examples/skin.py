"""Python port of examples/skin.gro.

A single undifferentiated cell divides; on the first division, the
mother becomes a LEADER (emits a control signal, stops growing) and
the daughter becomes a FOLLOWER (colors itself by local signal
strength, dies if it drifts too far from the leader).

Demonstrates the M4 division built-ins: `self.just_divided` and
`self.daughter`.

Depends on (still landing):
- `self.just_divided`, `self.daughter` — M4 core, in progress.
- `self.die()` — deferred CCL primitive.
- `self.gfp`, `self.rfp` reporter setters — deferred CCL primitive.
"""

from gro import *

set_param("dt", 0.2)

UNDEC, LEADER, FOLLOWER = 0, 1, 2

s = signal(diffusion=1.0, degradation=0.25)


class Skin(Program):

    # `m` is a role flag, `t` a timer — neither is a molecular count,
    # so both are Preserved across division.
    state = State(m=Preserved(UNDEC), t=Preserved(0.0))

    # Break symmetry on the first division: mother becomes LEADER,
    # daughter becomes FOLLOWER. Both rules fire on the same tick.
    @when(lambda self: self.state.m == UNDEC and self.just_divided and not self.daughter)
    def become_leader(self):
        self.state.m = LEADER

    @when(lambda self: self.state.m == UNDEC and self.daughter)
    def become_follower(self):
        self.state.m = FOLLOWER

    @when(lambda self: self.state.m == LEADER)
    def lead(self):
        set_param("ecoli_growth_rate", 0.0)
        self.emit_signal(s, 100)
        self.gfp = 100

    @when(lambda self: self.state.m == FOLLOWER)
    def follow(self):
        self.rfp = 50 * self.volume / (1 + self.get_signal(s))

    # Followers die if they drift too far from the leader's signal.
    # The `t > 50` guard delays death so the leader has time to start
    # emitting before its first daughter checks.
    @when(lambda self: self.state.m == FOLLOWER
                       and self.get_signal(s) < 0.01
                       and self.state.t > 50)
    def far_die(self):
        self.die()

    @always
    def tick(self):
        self.state.t += dt()


ecoli(x=0, y=0, program=Skin)
