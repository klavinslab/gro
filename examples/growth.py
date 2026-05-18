"""Python port of examples/growth.gro.

Four variants of growth behavior. Swap which one is passed to
`ecoli(..., program=...)` to compare. Mirrors the four CCL programs
`p`, `q`, `r`, `s` in growth.gro.
"""

from gro import *

set_param("dt", 0.1)
set_param("population_max", 2000)


# p: empty program — cells grow with whatever the world's defaults
# say. CCL's `skip();` — Python doesn't need a marker, just an empty
# class.
class P(Program):
    state = State()


# q: per-cell override of growth + division parameters via set_param
# in setup(). Each cell starts with its own tuned parameters.
class Q(Program):
    state = State()

    def setup(self):
        set_param("ecoli_growth_rate",           0.1)
        set_param("ecoli_division_size_mean",    2.0)
        set_param("ecoli_division_size_variance", 0.2)


# r: when the user clicks a cell, print its volume to console
# channel 1.
class R(Program):
    state = State()

    @when(lambda self: self.selected)
    def report(self):
        self.message(1, str(self.volume))


# s: turn off automatic size-based division (huge division_size_mean)
# and divide manually at rate 1 / time-unit once volume crosses pi.
# CCL writes this as `rate(1) & volume > 3.14`; in Python we inline
# the rate coin flip into the @when predicate.
class S(Program):
    state = State()

    def setup(self):
        set_param("ecoli_division_size_mean", 1000)

    @when(lambda self: self.volume > 3.14
                       and rand(100000) < dt() * 100000)
    def divide_when_big(self):
        self.divide()


# Try P, Q, R, or S here. They are all a bit different.
ecoli(x=0, y=0, program=Q)
