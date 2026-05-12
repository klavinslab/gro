"""Python port of examples/morphogenesis.gro.

A developmental state machine: each cell carries a state index `q`;
divisions and timers drive transitions through a tree of states,
producing a spatial pattern of cell types (rendered as four colors).

This file is a *single-Program fusion* of the CCL composed form. The
original uses a parametric sub-program (`program state(this, m_next,
d_next, t_next, tf, gr, sigs)`) instantiated eight times and stitched
together with `+ sharing q, t, event`. The authentic compose()-based
form returns in M5; for M4 we collapse the state table into a dict
and dispatch in one class.

Depends on (still landing):
- `self.just_divided`, `self.daughter` — M4 core, in progress.
- `self.gfp` / `self.rfp` / `self.cfp` / `self.yfp` setters — deferred.
- `reset()` + WorldProgram main loop (the periodic re-seeding every
  120 time units in morphogenesis.gro's `program main`) — M5.
"""

from gro import *

set_param("dt", 0.1)
set_param("ecoli_growth_rate", 0.1)

s0 = signal(diffusion=1.0, degradation=0.2)
s1 = signal(diffusion=1.0, degradation=0.2)


# Growth-rate functions of two signal levels (a = s0, b = s1).
def on (a, b): return 0.1
def off(a, b): return 0.0
def both(a, b): return 0.1 if (a > 0.1 and b > 0.1) else 0.0


# State table: q -> (m_next, d_next, t_next, tf, gr, s0_rate, s1_rate)
#   m_next: state to go to on division if this cell is the mother
#   d_next: state to go to on division if this cell is the daughter
#   t_next: state to go to when timer t exceeds tf
#   tf:     timer length (negative = no time-based transition)
#   gr:     growth-rate function of (s0_level, s1_level)
#   *_rate: signal-emission rates while active in this state
STATES = {
    0: (1, 2, 0, -1, on,    0,  0),
    1: (1, 3, 5, 60, on,   50,  0),
    2: (2, 3, 6, 60, on,    0, 50),
    3: (3, 3, 3, -1, both,  0,  0),
    5: (5, 5, 7, 40, on,    0,  0),
    6: (6, 6, 8, 40, on,    0,  0),
    7: (7, 7, 7,  0, off,   0,  0),
    8: (8, 8, 8,  0, off,   0,  0),
}


class Morpho(Program):

    # q is the state index, t a timer, event a single-tick latch that
    # prevents two transitions in the same tick. None is a count, so
    # all three are Preserved across division.
    state = State(q=Preserved(0), t=Preserved(0.0), event=Preserved(False))

    @always
    def clock(self):
        self.state.t += dt()
        self.state.event = False

    # On division, the mother and daughter cells transition along
    # different edges in the state graph (m_next vs d_next).
    # `q` is always a key of STATES — it starts at 0 (a key) and is
    # only ever reassigned from STATES[...] entries (also keys) — so
    # no `q in STATES` guard is needed in these predicates.

    @when(lambda self: not self.state.event
                       and self.just_divided
                       and not self.daughter
                       and self.state.q != STATES[self.state.q][0])
    def transition_mother(self):
        self.state.q = STATES[self.state.q][0]
        self.state.t = 0.0
        self.state.event = True

    @when(lambda self: not self.state.event
                       and self.just_divided
                       and self.daughter
                       and self.state.q != STATES[self.state.q][1])
    def transition_daughter(self):
        self.state.q = STATES[self.state.q][1]
        self.state.t = 0.0
        self.state.event = True

    @when(lambda self: not self.state.event
                       and STATES[self.state.q][3] > 0
                       and self.state.t > STATES[self.state.q][3]
                       and self.state.q != STATES[self.state.q][2])
    def transition_timer(self):
        self.state.q = STATES[self.state.q][2]
        self.state.t = 0.0
        self.state.event = True

    @always
    def active(self):
        q = self.state.q
        _m, _d, _tn, _tf, gr, r0, r1 = STATES[q]
        a, b = self.get_signal(s0), self.get_signal(s1)
        set_param("ecoli_growth_rate", gr(a, b))
        self.emit_signal(s0, r0)
        self.emit_signal(s1, r1)
        self.rfp = 100 if q == 3        else 0
        self.yfp = 100 if q in (1, 2)   else 0
        self.cfp = 100 if q in (5, 7)   else 0
        self.gfp = 100 if q in (6, 8)   else 0


ecoli(x=0, y=0, program=Morpho)
