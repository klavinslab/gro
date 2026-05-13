"""Python port of examples/yeast_example.gro.

A single yeast cell that starts with a large GFP count. Demonstrates
that the `yeast(...)` spawn function works the same way as
`ecoli(...)`. Yeast cells bud and the daughters inherit the program.
"""

from gro import *


class P(Program):
    state = State()
    def setup(self):
        self.gfp = 10000


yeast(x=0, y=0, program=P)
