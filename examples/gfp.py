"""Python port of examples/gfp.gro.

Models GFP expression with explicit mRNA and protein dynamics. The
rate constants are computed from order-of-magnitude bionumbers for
E. coli (transcription speed, mRNA half-life, etc.) and logged to the
console at module load.

Composed program: `GFPProd` carries mRNA in state and runs the four
production/degradation reactions; `Report` prints per-cell stats when
the user selects a cell. `mRNA` is shared between the two parts.
The GFP reporter is per-cell at the C++ level (rep[GFP]), so it
doesn't need to be in `share=[...]`.
"""

from gro import *
import math


set_param("dt", 0.01)


# RNA production: ~9.4 mRNA/s for a 2.35 fL average bacterium, so
# alpha_r per minute per fL is:
alpha_r = 69.4 / 2.35
message(0, f"alpha_r = {alpha_r} mRNA / min / fL")

# RNA degradation: ~3.69 minute half-life, treated as per-RNA (not
# per-volume), so:
beta_r = -math.log(0.5) / 3.69
message(0, f"beta_r = {beta_r} / min")

# Protein production: ~3 GFP / min / mRNA.
alpha_p = 3.0
message(0, f"alpha_p = {alpha_p} / min")

# Protein degradation: GFP is famously long-lived, so a small number.
beta_p = 0.01
message(0, f"beta_p = {beta_p} / min")

# Render saturation tuned around the steady-state concentration.
set_param("gfp_saturation_max", 1000)
set_param("gfp_saturation_min",  800)


class GFPProd(Program):
    """Transcription/translation of GFP with explicit mRNA."""
    state = State(mRNA=0)

    @when(lambda self: rand(100000) < alpha_r * self.volume * dt() * 100000)
    def transcribe(self):
        self.state.mRNA += 1

    @when(lambda self: rand(100000) < beta_r * self.state.mRNA * dt() * 100000)
    def degrade_mRNA(self):
        self.state.mRNA -= 1

    @when(lambda self: rand(100000) < alpha_p * self.state.mRNA * dt() * 100000)
    def translate(self):
        self.gfp += 1

    @when(lambda self: rand(100000) < beta_p * self.gfp * dt() * 100000)
    def degrade_protein(self):
        self.gfp -= 1


class Report(Program):
    """When the user selects a cell, print its mRNA + GFP stats."""
    state = State()
    requires = ["mRNA"]

    @when(lambda self: self.selected)
    def report(self):
        self.message(
            1,
            f"cell {self.id}: mRNA={self.state.mRNA}, "
            f"GFP={self.gfp}, [GFP]={self.gfp / self.volume:.2f}",
        )


GFP = compose(GFPProd, Report, share=["mRNA"])

ecoli(x=0, y=0, program=GFP)
