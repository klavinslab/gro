"""Python port of examples/symbiosis.gro.

Two cell types that depend on each other: each emits a nutrient
signal for the OTHER type to consume. The internal nutrient pool
`n` drives growth; cells die if their pool depletes below a
threshold.
"""

from gro import *


chemostat(True)

# Yellow and green signal palette so the two cell types are visually
# distinct. Same idiom as CCL's `bright_theme << [signals := ...]`.
set_theme({**bright_theme, "signals": [[1, 1, 0], [0, 1, 0]]})

dif, deg = 0.1, 0.1
nutrient = [signal(diffusion=dif, degradation=deg),
            signal(diffusion=dif, degradation=deg)]

k1, k2, k3, k4 = 1.0, 1.0, 0.1, 1.0


def p(i):
    """Parametric cell program: emits nutrient[i], consumes
    nutrient[1-i], grows according to internal pool, dies if depleted."""
    other = 1 - i

    class P(Program):
        # n is the internal nutrient pool; x, y, z are derived
        # per-tick quantities (could be locals but State keeps them
        # introspectable for selected: messages).
        state = State(n=1.0, x=0.0, y=0.0, z=0.0)

        @always
        def update(self):
            self.emit_signal(nutrient[i], self.state.n)
            self.state.x = self.get_signal(nutrient[other])
            self.state.y = k1 * self.state.x / (k2 + self.state.x)
            self.state.n += dt() * self.state.y
            self.state.z = (k3 * (self.state.n / self.volume)
                            / (k4 + self.state.n / self.volume))
            self.absorb_signal(nutrient[other], 500 * self.state.y)
            set_param("ecoli_growth_rate", self.state.z)
            self.state.n -= dt() * self.state.z

        # Color by cell type: yfp for i=0, gfp for i=1.
        @always
        def color(self):
            level = int(50 * self.volume * (0.5 + self.state.n))
            if i == 0:
                self.yfp = level
            else:
                self.gfp = level

        @when(lambda self: self.selected)
        def report(self):
            self.message(1, f"n/volume = {self.state.n / self.volume}")

        @when(lambda self: self.state.n / self.volume < 0.0001)
        def starve(self):
            self.die()

    P.__name__ = f"Symbiont({i})"
    return P


ecoli(x=0, y=-5, program=p(0))
ecoli(x=0, y= 5, program=p(1))
