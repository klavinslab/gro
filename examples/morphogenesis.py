"""Python port of examples/morphogenesis.gro.

A developmental state machine: each cell carries a state index `q`;
divisions and timers drive transitions through a tree of states,
producing a spatial pattern of cell types (rendered as four colors).

Composed form, mirroring the CCL original:

    program state(this, m_next, d_next, t_next, tf, gr, sigs) := {
      needs q, t, event;
      active := false;
      true : { active := (q = this); }
      !event & active & just_divided & !daughter & q != m_next : ...
      ...
    };

    program p() := sm() + state(0, 1, 2, ...) sharing q, t, event
                       + state(1, 1, 3, ...) sharing q, t, event
                       + ...

In Python we get the same shape via `compose(...)` with a `share`
list: one shared (q, t, event) storage, and each `state_node(...)`
gets its own auto-namespaced `active` local.

Depends on (still landing):
- WorldProgram + set_main (the periodic `program main()` re-seed
  every 120 time units in morphogenesis.gro) — M5b.
"""

from gro import *

set_param("dt", 0.1)
set_param("ecoli_growth_rate", 0.1)

s0 = signal(diffusion=1.0, degradation=0.2)
s1 = signal(diffusion=1.0, degradation=0.2)


# Growth-rate functions of two signal levels.
def on  (a, b): return 0.1
def off (a, b): return 0.0
def both(a, b): return 0.1 if (a > 0.1 and b > 0.1) else 0.0


def state_node(this, m_next, d_next, t_next, tf, gr, sigs):
    """One node of the developmental FSM.

    Args mirror CCL's `state(this, m_next, d_next, t_next, tf, gr,
    sigs)`: state index, mother-next on division, daughter-next on
    division, t-next on timer expiry, timer length (-1 or 0 ⇒ no
    timer), growth-rate function, signal-emission rates per tick.
    All closure-captured into the returned subclass's rules.
    """
    class S(Program):
        state = State(active=False)
        requires = ["q", "t", "event"]

        @always
        def update_active(self):
            self.state.active = (self.state.q == this)

        @when(lambda self: not self.state.event and self.state.active
                           and self.just_divided and not self.daughter
                           and self.state.q != m_next)
        def transition_mother(self):
            self.state.q = m_next
            self.state.t = 0.0
            self.state.event = True

        @when(lambda self: not self.state.event and self.state.active
                           and self.just_divided and self.daughter
                           and self.state.q != d_next)
        def transition_daughter(self):
            self.state.q = d_next
            self.state.t = 0.0
            self.state.event = True

        @when(lambda self: self.state.active)
        def active_body(self):
            a, b = self.get_signal(s0), self.get_signal(s1)
            set_param("ecoli_growth_rate", gr(a, b))
            self.emit_signal(s0, sigs[0])
            self.emit_signal(s1, sigs[1])

    # Only emit the timer-driven transition for states that have a
    # positive tf — states with tf <= 0 (here: 0, 3) are terminal /
    # event-only, and a dead `tf > 0` predicate would just burn
    # cycles each tick.
    if tf > 0:
        @when(lambda self: not self.state.event and self.state.active
                           and self.state.t > tf
                           and self.state.q != t_next)
        def transition_timer(self):
            self.state.q = t_next
            self.state.t = 0.0
            self.state.event = True
        S.transition_timer = transition_timer
        # Re-walk the class so _ProgramMeta picks up the new method.
        S._gro_rules.append(("transition_timer", *transition_timer._gro_rule, transition_timer))

    S.__name__ = f"State_{this}"
    return S


class SM(Program):
    """Holds the shared state and ticks the clock + event latch.
    CCL's `program sm()`."""
    state = State(q=Preserved(0), t=Preserved(0.0), event=Preserved(False))

    @always
    def clock(self):
        self.state.t += dt()
        self.state.event = False


class Reporter(Program):
    """Reads `q` from shared state and paints the cell. CCL's
    `program report()` minus the `selected : message(...)` rule
    (message() is deferred)."""
    state = State()
    requires = ["q"]

    @always
    def colorize(self):
        q = self.state.q
        self.rfp = 100 if q == 3      else 0
        self.yfp = 100 if q in (1, 2) else 0
        self.cfp = 100 if q in (5, 7) else 0
        self.gfp = 100 if q in (6, 8) else 0


Morpho = compose(
    SM,
    state_node(0, 1, 2, 0, -1, on,    (0,  0)),
    state_node(1, 1, 3, 5, 60, on,    (50, 0)),
    state_node(2, 2, 3, 6, 60, on,    (0, 50)),
    state_node(3, 3, 3, 3, -1, both,  (0,  0)),
    state_node(5, 5, 5, 7, 40, on,    (0,  0)),
    state_node(6, 6, 6, 8, 40, on,    (0,  0)),
    state_node(7, 7, 7, 7,  0, off,   (0,  0)),
    state_node(8, 8, 8, 8,  0, off,   (0,  0)),
    Reporter,
    share=["q", "t", "event"],
)


ecoli(x=0, y=0, program=Morpho)
