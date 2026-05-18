"""Python port of examples/barriers.gro.

Builds an asterisk-shaped channel system: a free central square of
side 2*B, with four open arms along the cardinal axes flanked by
walls. Cells seeded at the origin grow outward through the four
channels but can't spread sideways.
"""

from gro import *


A = 200
B = 20

# Top-left and top-right horizontal walls at y = +B, plus the two
# vertical walls bounding the north arm at x = +/-B going up.
barrier(-A,  B, -B,  B)
barrier(-B,  B, -B,  A)
barrier( A,  B,  B,  B)
barrier( B,  B,  B,  A)

# Bottom-right and bottom-left horizontal walls at y = -B, plus the
# two vertical walls bounding the south arm going down.
barrier( A, -B,  B, -B)
barrier( B, -B,  B, -A)
barrier(-A, -B, -B, -B)
barrier(-B, -B, -B, -A)


class P(Program):
    state = State()
    def setup(self):
        self.gfp = 1000


ecoli(x=0, y=0, program=P)
