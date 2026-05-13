"""Python port of examples/foreach.gro.

Seeds 100 cells at random positions, each with a different starting
GFP count. Demonstrates the parametric-program factory pattern with
a Python `for` loop replacing CCL's `foreach`.
"""

from gro import *


def p(n):
    """Cell program that initializes its GFP count to `n`."""
    class P(Program):
        state = State()
        def setup(self):
            self.gfp = n
    return P


for _ in range(100):
    ecoli(x=rand(600) - 300,
          y=rand(600) - 300,
          theta=0.01 * rand(314),
          program=p(rand(100)))
