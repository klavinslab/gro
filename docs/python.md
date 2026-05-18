# gro — the Python Reference

gro programs can also be written in Python (`.py` files) instead of
the original CCL syntax (`.gro` files). The simulator is the same;
only the frontend differs. The Python frontend is a class-based DSL
with the same surface as CCL — `State` for state, `@when` /
`@always` / `@rate` decorators for rules, `compose(...)` for
composition, and so on.

This document is the Python parallel of `gro.md`. Both share the
same outline. See `DESIGN.md` for the internal design and trade-
offs of the Python integration.

---

## Getting Started

Welcome! Python gro programs are loaded the same way as `.gro`
programs — `File → Open` and pick a `.py` file. The simulator
auto-detects from the file extension. Start with the installation
section, then go through the tutorial.

### Installation

#### macOS

Requirements: macOS 14.7 or later, Apple Silicon, and the system
Python 3.13 (Homebrew's `python@3.13` on Apple Silicon).

The Python integration runs inside the same `gro.app` bundle — no
separate Python interpreter is needed. The `python/gro/` directory
ships alongside `gro.app`; gro finds it at startup.

To write `.py` programs, use any text editor (the lab favors VS
Code). After editing, choose Reload from the menu.

You don't need to install `gro` as a Python package — gro embeds
its own interpreter and exposes the `gro` module to the running
`.py` file directly. `from gro import *` works inside a gro program
but **not** in your shell's Python.

#### Windows

The Python integration tracks the same Python version as macOS
(Python 3.13). The Python runtime is bundled in the same way as on
macOS.

### Tutorial

This section walks through gro's features via Python examples.
Each `.py` example is paired with the corresponding `.gro` example
in `examples/`; running them side by side is a good way to see the
two frontends produce identical simulations.

#### Growth

The simplest Python gro program:

```python
from gro import *

set_param("dt", 0.1)     # fast and inaccurate


class P(Program):
    state = State()


ecoli(x=0, y=0, program=P)
```

This is `examples/growth.py`. Open it via `File → Open`.

- `from gro import *` brings in the entire user surface
  (`Program`, `State`, `ecoli`, `signal`, …) — mirrors CCL's
  `include gro`.
- `set_param("dt", 0.1)` sets the simulator time step.
- `class P(Program)` declares a Program class. `state = State()`
  makes it a valid Program subclass (state schema can be empty).
- `ecoli(x=0, y=0, program=P)` spawns a cell at `(0, 0)` running
  `P`. By default, 10 pixels = 1 µm.

What happens under the hood is identical to the CCL case — the
same C++ simulator runs underneath.

##### Controlling growth rate and division volume

```python
class P(Program):
    state = State()

    def setup(self):
        set_param("ecoli_division_size_mean",     2.0)
        set_param("ecoli_division_size_variance", 0.02)
```

`setup(self)` runs once per cell at spawn time. Inside `setup`, the
`set_param` calls land on the cell-local parameter map — different
cells can have different growth/division parameters. Outside any
Program (at module top level), `set_param` sets the world default.

Manual division control:

```python
class P(Program):
    state = State()

    def setup(self):
        set_param("ecoli_division_size_mean", 1000)

    @when(lambda self: self.volume > 3.14
                       and rand(100000) < dt() * 100000)
    def divide_big(self):
        self.divide()
```

- `@when(lambda self: ...)` registers a rule that fires when the
  lambda is true. The lambda must take `self` and return a bool.
- `self.volume`, `self.id`, `self.x`, `self.y`, `self.theta`,
  `self.just_divided`, `self.daughter`, `self.selected` are
  per-cell built-ins set by the simulator each tick.
- `rand(100000) < dt() * 100000` mirrors CCL's `rate(1)` coin flip
  — fire approximately 1× per simulated minute.
- `self.divide()` requests division on the next divide check,
  bypassing size-mean / size-variance.

##### Chemostat mode

```python
chemostat()
```

`chemostat()` is a no-arg toggle (defaults to `True`). Equivalent
to CCL's `chemostat(true)`.

#### Molecules

##### Dilution

A cell loaded with 1000 GFP molecules, no production, no
degradation:

```python
from gro import *

set_param("dt", 0.1)


def dilute(m):
    """Pure initializer: set GFP to m at spawn."""
    class D(Program):
        state = State()
        def setup(self):
            self.gfp = m
    return D


ecoli(x=0, y=0, program=dilute(1000))
```

`dilute(m)` is a factory function that returns a Program subclass
with `m` closure-captured. This is the natural Python equivalent
of CCL's parametric `program p(m) := { ... }`. Use it whenever a
parameter shapes rule bodies (predicates, action code) rather than
just `State` defaults.

For parameters that only override `State` defaults, the more
idiomatic form is `Program.with_args(**overrides)`:

```python
class Dilute(Program):
    state = State(starting_gfp=1000)
    def setup(self):
        self.gfp = self.state.starting_gfp

D500 = Dilute.with_args(starting_gfp=500)
```

GFP counts dilute automatically across divisions: numeric `State`
fields and reporter counters (gfp/rfp/yfp/cfp) are halved between
mother and daughter on each cell division. Mark a field
`Preserved(...)` to opt out:

```python
state = State(t=Preserved(0.0), mode=Preserved(0))
```

To inspect GFP per cell, compose a reporter:

```python
class Report(Program):
    state = State()

    @when(lambda self: self.selected)
    def report(self):
        self.message(1, f"{self.id}: {self.gfp}")


P = compose(dilute(1000), Report)
ecoli(x=0, y=0, program=P)
```

`compose(*parts)` builds a composite Program whose rules are the
union of each part's. Reporter counters (`gfp`/`rfp`/`yfp`/`cfp`)
are per-cell at the C++ level, so the two parts see the same `gfp`
value via `self.gfp` without needing to be in `share=[...]`.

##### Production, dilution and degradation

```python
import math

def make_gfp(k1, k2, m):
    class G(Program):
        state = State()
        def setup(self):
            self.gfp = m

        @when(lambda self: rand(100000) < k1 * dt() * 100000)
        def produce(self):
            self.gfp += 1

        @when(lambda self: rand(100000) < k2 * self.gfp * dt() * 100000)
        def degrade(self):
            self.gfp -= 1
    return G


alpha = -math.log(0.5) / 20.0
k1 = 100 * alpha

ecoli(x=0, y=0, program=make_gfp(k1, 0.001, 0))
```

To collect per-cell time series, build an `output` factory and
compose it on:

```python
def output(delta):
    class O(Program):
        state = State(t=Preserved(0.0), s=Preserved(0.0))

        @always
        def tick(self):
            self.state.t += dt()
            self.state.s += dt()

        @when(lambda self: self.state.s >= delta)
        def emit(self):
            print(f"{self.id}, {self.state.t}, {self.gfp / self.volume}")
            self.state.s = 0.0
    return O


P = compose(make_gfp(k1, 0.001, 0), output(5 * dt()))
```

Note `t` and `s` are wrapped in `Preserved(...)`: they're per-cell
control state (timers), not molecular counts, so they shouldn't be
halved on division.

For real file output, just use Python's builtin `open`:

```python
fp = open("path", "w")
# inside a rule body:
fp.write(f"{self.id}, {self.state.t}, {self.gfp / self.volume}\n")
```

#### Dogma

The mRNA → GFP model with explicit rates. From `examples/gfp.py`:

```python
from gro import *
import math


set_param("dt", 0.01)


alpha_r = 69.4 / 2.35    # mRNA / min / fL
beta_r  = -math.log(0.5) / 3.69
alpha_p = 3.0
beta_p  = 0.01


class GFPProd(Program):
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
    state = State()
    requires = ["mRNA"]

    @when(lambda self: self.selected)
    def show(self):
        self.message(
            1,
            f"cell {self.id}: mRNA={self.state.mRNA}, "
            f"GFP={self.gfp}, [GFP]={self.gfp / self.volume:.2f}",
        )


set_param("gfp_saturation_max", 1000)
set_param("gfp_saturation_min",  800)


GFP = compose(GFPProd, Report, share=["mRNA"])
ecoli(x=0, y=0, program=GFP)
```

`requires = ["mRNA"]` declares that `Report` reads a field named
`mRNA`. `compose(..., share=["mRNA"])` makes one storage for mRNA
that both parts see via `self.state.mRNA`. Violating `requires`
(forgetting to share the name) is a load-time `GroLoadError`.

#### Signals

##### A concentration bandpass filter

```python
from gro import *


set_param("dt", 0.1)

ahl = signal(diffusion=1.0, degradation=0.01)


class Sensor(Program):
    state = State()

    @when(lambda self: 0.1 < self.get_signal(ahl) < 0.6)
    def detect(self):
        self.rfp += 1

    @when(lambda self: rand(100000) < 0.01 * self.rfp * dt() * 100000)
    def decay(self):
        self.rfp -= 1


set_param("rfp_saturation_max", 50)
set_param("rfp_saturation_min",  0)

ecoli(x=0, y=0, program=Sensor)


class Main(WorldProgram):
    state = State()

    @always
    def source(self):
        set_signal(ahl, 0, 0, 10)


set_main(Main)
```

- `signal(diffusion=..., degradation=...)` returns an integer
  signal handle.
- `self.get_signal(h)` reads the local-to-cell concentration.
- `set_signal(h, x, y, c)` (module-level) sets the value at a
  world coordinate.
- `WorldProgram` is the marker base class for `main()`-style
  programs that run once per tick at world scope. `set_main(M)`
  installs it; `set_main` is strict — it rejects bare `Program`
  subclasses.

##### The wave

```python
from gro import *


set_param("dt", 0.075)
ahl = signal(diffusion=1.0, degradation=1.0)


class Leader(Program):
    state = State(t=Preserved(2.4))

    def setup(self):
        set_param("ecoli_growth_rate", 0.0)

    @always
    def tick(self):
        self.state.t += dt()

    @when(lambda self: self.state.t > 10)
    def fire(self):
        self.emit_signal(ahl, 100)
        self.state.t = 0


class Follower(Program):
    state = State(mode=Preserved(0), t=Preserved(0.0))

    @when(lambda self: self.state.mode == 0 and self.get_signal(ahl) > 0.01)
    def relay(self):
        self.emit_signal(ahl, 100)
        self.state.mode = 1
        self.state.t = 0

    @when(lambda self: self.state.mode == 1)
    def grow(self):
        self.state.t += dt()

    @when(lambda self: self.state.mode == 1 and self.state.t > 9)
    def reset(self):
        self.state.mode = 0


ecoli(x=0, y=0,  program=Leader)
ecoli(x=0, y=10, program=Follower)
```

#### Evolution

Mutate a parameter on division using `self.daughter`:

```python
from gro import *


chemostat()
set_param("dt", 0.075)


nutrient = 1
kinit    = 0.25
dk       = 0.05


def cost(e, n):    return 0.2 * e * n / (50.0 + n)
def benefit(e, n): return 0.002 * e / (1.0 - 0.01 * e)
def fitness(e, n): return cost(e, n) - benefit(e, n)


class Evolver(Program):
    state = State(k=Preserved(kinit), E=25, t=0.0)

    @when(lambda self: rand(100000) < self.state.k * self.volume * dt() * 100000)
    def make_enzyme(self):
        self.state.E += 1

    @when(lambda self: rand(100000) < 0.05 * self.state.E * dt() * 100000)
    def degrade_enzyme(self):
        self.state.E -= 1

    @always
    def update(self):
        set_param("ecoli_growth_rate",
                  0.001 + fitness(self.state.E, nutrient))
        self.state.t += dt()

    @when(lambda self: self.daughter)
    def mutate(self):
        self.state.k += dk * (rand(1000) - 500) / 1000.0


ecoli(x=0, y=0, program=Evolver)
```

`self.daughter` is true on the new cell of a fresh division for
exactly one tick. Pair with `self.just_divided` if you want both
halves to mutate.

#### Global Control

A `WorldProgram` toggling an IPTG concentration:

```python
from gro import *


iptg = 0


class P(Program):
    state = State()

    @when(lambda self: rand(100000)
                       < (1 + 10 * iptg / (1 + iptg)) * dt() * 100000)
    def transcribe(self):
        self.gfp += 1

    @when(lambda self: rand(100000) < 0.001 * self.gfp * dt() * 100000)
    def degrade(self):
        self.gfp -= 1


class Main(WorldProgram):
    state = State(t=0.0)

    @always
    def tick(self):
        self.state.t += dt()

    @when(lambda self: self.state.t > 50)
    def toggle(self):
        global iptg
        self.state.t = 0.0
        iptg = 1.0 - iptg
        clear_messages(1)
        message(1, f"IPTG at {iptg} uM/L")


ecoli(x=0, y=0, program=P)
set_main(Main)
```

##### Simulation control

`reset()` restarts the world: cells removed, signal grids zeroed,
chipmunk space rebuilt. Calling `reset()` from a per-cell rule
raises (the per-cell loop would be iterating a freed population);
only call it from a `WorldProgram` rule. Stop/quit:

- `stop()` — pause the simulator (Start/Stop toolbar can resume).
- `start()` — resume.

---

## The Details

### Expressions

Python's own expression language. The user writes regular Python
in rule bodies and predicates, with one constraint on predicates
(see Strict Mode → AST sandbox below). Below are the gro-specific
shapes you'll need.

#### Values

Plain Python values: `int`, `float`, `bool`, `str`, `list`, `dict`,
`tuple`. `State` defaults can be any of these (with mutable types
wrapped in `field(factory)` — see Lists).

#### Logic, Arithmetic, Strings

All Python — `and` / `or` / `not`, `+ - * / % **`, f-strings, etc.
Use them as you would in any Python program.

#### Lists

For mutable defaults in `State(...)`, wrap a callable:

```python
state = State(items=field(list), counts=field(dict))
```

A bare `state = State(items=[])` is rejected at class-creation time
because the literal would alias across cells.

#### Records

Python dictionaries or dataclasses. No special syntax needed.

#### Lambda expressions

Python lambdas are the natural form for `@when` predicates:

```python
@when(lambda self: self.state.t > 1.0 and self.volume > 3.0)
def fire(self): ...
```

The AST sandbox (see Strict Mode) forbids walrus (`:=`), nested
lambdas, `yield`, and `await` inside `@when` predicates.

#### Functions

Plain Python `def`. No special gro keyword.

#### Conditionals

Python `if`/`elif`/`else` in rule bodies; `a if cond else b`
ternary in predicates. Both work as expected.

#### `for` loops (foreach)

Python `for ... in ...`. Procedural, not the CCL functional-`foreach`
form:

```python
for _ in range(100):
    ecoli(
        x=rand(600) - 300,
        y=rand(600) - 300,
        theta=0.01 * rand(314),
        program=p(rand(100)),
    )
```

#### Type errors

Python is dynamically typed — type errors surface at runtime, not
load time. Strict mode catches a few common typos at class
definition (see below).

#### Programs

A Program subclass:

```python
class P(Program):
    state = State(t=0.0)

    @always
    def tick(self):
        self.state.t += dt()

    @when(lambda self: self.state.t > 100)
    def reset(self):
        self.state.t = 0.0
```

Rules are methods decorated with `@when(predicate)`, `@always`, or
`@rate(k)`. They fire each tick if the predicate is true; multiple
rules can fire per tick.

##### Composition

```python
R = compose(P1, P2, share=["t"])
```

The composite has every part's rules, in part order then
declaration order. Shared fields get one storage; non-shared
fields are auto-namespaced per-part so two parts can both declare
e.g. `active = False` without collision.

Class-body sugar:

```python
class R(Composed):
    parts = [P1, P2]
    share = ["t"]
```

Equivalent to `R = compose(P1, P2, share=["t"])`.

##### Sharing and scope

Parts that read a name not declared in their own `state` must
list it via `requires = [...]`. Every name in `requires` must be
in `share`:

```python
class Reader(Program):
    state = State()
    requires = ["t"]
    @always
    def show(self): print(self.state.t)

R = compose(Writer, Reader, share=["t"])
```

### gro Control

#### E. coli

```python
ecoli(x=10, y=10, theta=1.57, program=P)
```

All four arguments default — `ecoli(program=P)` puts the cell at
the origin. `volume` is also accepted (defaults to gro's compile-
time `DEFAULT_ECOLI_INIT_SIZE`).

Multiple seedings work the same way as CCL:

```python
for _ in range(100):
    ecoli(
        x=rand(600) - 300,
        y=rand(600) - 300,
        theta=0.01 * rand(314),
        program=p(rand(100)),
    )
```

#### Variables (cell built-ins)

Read-only attributes on `self` inside a Program method:

- `self.volume` — cell volume in fL.
- `self.id` — unique integer per cell.
- `self.x`, `self.y`, `self.theta` — position + orientation.
- `self.just_divided` — true for one tick on both halves.
- `self.daughter` — true for one tick on the new cell of a
  division.
- `self.selected` — true while the GUI selection is on this cell.

`dt()` and `time()` are module-level functions; they don't take
`self`.

Don't use these inside a `WorldProgram` — they refer to a
"current cell" that doesn't exist at world scope.

#### Reporters

`self.gfp` / `self.rfp` / `self.yfp` / `self.cfp` are read-write
properties backed by the C++ per-cell reporter counters. Set them
like attributes:

```python
self.gfp = 100 * self.volume
self.rfp += 1
```

Rendering intensity and the saturation parameters
(`gfp_saturation_min` / `_max`) work the same as the CCL side.

Strict mode rejects writes to any other `self.X` attribute (it's
almost certainly a typo for `self.state.X`).

#### Parameters

```python
set_param("name", value)
```

- Called at module scope → world default.
- Called inside a Program method (cell context) → cell-local.

```python
def setup(self):
    set_param("ecoli_growth_rate", 0.1)   # cell-local
```

Common parameters are the same as CCL (`"dt"`,
`"ecoli_growth_rate"`, `"ecoli_division_size_mean"`,
`"ecoli_division_size_variance"`, `"gfp_saturation_min"`,
`"gfp_saturation_max"`, etc.). Defaults live in
`python/gro/__init__.py` (which mirrors `include/gro.gro`).

#### Signals

##### Declaration

```python
ahl = signal(diffusion=1.0, degradation=0.01)
```

Returns an integer handle.

##### Setting / sourcing

```python
set_signal(handle, x, y, value)
```

For a constant source, drive it from a `WorldProgram`:

```python
class Main(WorldProgram):
    state = State()
    @always
    def source(self):
        set_signal(ahl, 0, 0, 10)

set_main(Main)
```

##### Emitting / sensing / absorbing (cell-local)

```python
self.emit_signal(handle, amount)
self.get_signal(handle)           # returns local concentration
self.absorb_signal(handle, amount)
```

The full skin / leader-follower differentiation example
(`examples/skin.py`):

```python
from gro import *


set_param("dt", 0.2)

UNDEC, LEADER, FOLLOWER = 0, 1, 2
s = signal(diffusion=1.0, degradation=0.25)


class Skin(Program):
    state = State(m=Preserved(UNDEC), t=Preserved(0.0))

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

    @when(lambda self: self.state.m == FOLLOWER
                       and self.get_signal(s) < 0.01
                       and self.state.t > 50)
    def far_die(self):
        self.die()

    @always
    def tick(self):
        self.state.t += dt()


ecoli(x=0, y=0, program=Skin)
```

Same numerical-stability advice as CCL: high diffusion + large
`dt` produces visible Euler-integration artifacts. Reduce `dt` or
the diffusion rate.

##### Reactions

```python
reaction([X, Y], [Y, Y], 5)   # X + Y → 2Y at rate 5
reaction([X], [X, X], 5)
reaction([Y], [], 5)
```

Same mechanics as CCL's `reaction(...)`; rate constants are
applied to the discretized grid.

#### Messages

```python
self.message(channel, text)    # cell-context, e.g. inside a `selected:` rule
message(channel, text)         # module-level
clear_messages(channel)
```

Channels 0–3 map to four on-screen quadrants. The new bottom
console panel shows Python tracebacks on rule errors and uses a
separate channel from `message()`.

#### Collecting Data

Two paths:

1. `print(...)` from a rule body. gro's stdout is captured by the
   bottom console for the duration of the run; redirect by running
   the binary from a terminal:

   ```
   gro --load my_program.py --ticks 1000 > data.csv
   ```

2. Open a real file:

   ```python
   fp = open("/tmp/out.csv", "w")

   class Logger(Program):
       state = State()
       @always
       def emit(self):
           fp.write(f"{self.id}, {time()}, {self.gfp}\n")
   ```

For population-level statistics, since gro doesn't yet expose
`maptocells` in Python, the workaround is to collect into shared
state via `compose(...)`:

```python
totals = {"n": 0, "sum": 0.0}

class Tally(Program):
    state = State()
    @always
    def add(self):
        totals["n"] += 1
        totals["sum"] += self.gfp / self.volume

class Reporter(WorldProgram):
    state = State()
    @when(lambda self: True)
    def report(self):
        if totals["n"]:
            print(f"mean = {totals['sum'] / totals['n']}")
        totals["n"] = totals["sum"] = 0
```

(A first-class `cells()` iterator is on the production-readiness
list — see `DESIGN.md`.)

#### Movies

```python
def movie(period, path_prefix):
    class M(Program):
        state = State(t=Preserved(0.0), n=Preserved(0))

        @always
        def tick(self):
            self.state.t += dt()

        @when(lambda self: self.state.t > period)
        def shoot(self):
            snapshot(f"{path_prefix}{self.state.n}.tif")
            self.state.n += 1
            self.state.t = 0.0
    return M
```

`snapshot(path)` writes a PNG of the current scene.

#### Command Line

```
gro --load PATH --ticks N
```

`--load` opens `PATH` programmatically; `--ticks N` runs the
simulator until `N` World ticks have elapsed, then exits cleanly.
Used by the headless integration test suite (`tests/test_integration.py`).
Exit 0 if the target was reached; 1 if the sim halted early
(typically a Python rule error halting via `set_stop_flag`).

For runtime arguments to a `.py` program, use Python's `sys.argv`
directly — gro doesn't intercept it.

### Execution

#### 1. Load

The simulator's C++ side detects the `.py` extension, sets up the
embedded interpreter (one-time on first load), and `_setup_world()`
sets gro's world-default parameters (matching `include/gro.gro`).

#### 2. py::eval_file

gro evaluates the user's `.py` file as a script. Top-level code
runs in order:

- Module-level `set_param(...)`, `signal(...)`, etc. configure the
  world.
- `class P(Program): ...` declarations register rules (with strict-
  mode AST checks at class-creation time — see below).
- `ecoli(...)` / `yeast(...)` calls instantiate cells and run
  their `setup(self)` once each.
- `set_main(M)` installs a world program if needed.

#### 3. The Simulation Loop

Per tick:

1. The world program (`set_main`'s target) runs first if present.
2. Each live cell runs in turn: cell built-ins are set,
   `_tick()` dispatches each rule whose predicate is true.
3. Division checks fire; on division the program is propagated to
   the daughter (`_split` halves top-level numeric state, deep-
   copies non-numerics, preserves Preserved fields).
4. Physics integrate: signal diffusion + decay, cell motion.
5. Cells marked for death (`self.die()`) are swept out.

### Strict Mode

Python-side strict mode runs at class-creation time and rejects
several classes of common mistake before the simulator starts. See
`DESIGN.md` for the full spec; the rules are:

- **`state` must be a `State(...)`.** Assigning a `dict` or a
  `SimpleNamespace` raises `GroLoadError`.
- **Method bodies may only write `self.<name>`** where name is a
  reporter setter (`gfp`/`rfp`/`yfp`/`cfp`) or starts with `_`.
  `self.tagged = True` → load-time error with a "did you mean
  `self.state.tagged`?" hint.
- **`@when` predicates pass an AST sandbox**: walrus `:=`, nested
  `lambda`, `yield`, and `await` are rejected. (The full call-
  allowlist sandbox is deferred — today the rules' main constraint
  is structural, not semantic.)
- **`requires` lists must reference names in `share`** (and
  therefore in some part's `State`).
- **`set_main(...)` requires a `WorldProgram` subclass.** Bare
  `Program` subclasses are rejected.

A failing check raises `GroLoadError` (subclass of `Exception`)
naming the class + method + line.

### Standard Library

The `gro` module exposes (via `from gro import *`):

- **Spawning** — `ecoli`, `yeast`.
- **Programs & rules** — `Program`, `State`, `field`, `Preserved`,
  `when`, `always`, `rate`, `compose`, `Composed`, `WorldProgram`,
  `set_main`, `reset`, `GroLoadError`.
- **Signals (world coords)** — `signal`, `set_signal`,
  `get_signal_at`, `set_signal_rect`, `get_signal_matrix`,
  `reaction`.
- **World** — `set_param`, `get_param`, `dt`, `time`, `message`,
  `clear_messages`, `stats`, `chemostat`, `barrier`, `snapshot`,
  `stop`, `start`, `srand`.
- **Themes** — `set_theme`, `bright_theme`, `dark_theme`.
- **Misc** — `rand`.

Cell-local API is on the `Program` class (used as `self.foo(...)`):

- `self.emit_signal(handle, amount)`
- `self.absorb_signal(handle, amount)`
- `self.get_signal(handle)` (returns cell-local concentration)
- `self.die()`
- `self.divide()` (force divide on next check)
- `self.run(dvel)` / `self.tumble(vel)` (motility)
- `self.message(channel, text)`

Python's own stdlib (`math`, `random`, `os`, `json`, `itertools`,
…) is available unrestricted — the embedded interpreter is full
CPython 3.13.

---

## FAQ

### The Language

**Can `@when` predicates contain statements?**

No — they're Python lambdas, so they're expression-only. Use rule
bodies (methods) for anything with side effects. The AST sandbox
also rejects walrus `:=` inside predicates so you can't sneak in
state mutation.

**Why does strict mode reject `self.foo = 5`?**

Almost always a typo for `self.state.foo = 5`. Writing to an
undeclared `self.X` would silently invent an instance attribute and
silently fail to participate in state halving on division.
Underscore-prefixed names (`self._cache = ...`) are allowed as an
internal escape hatch.

**Can I subclass a composed program?**

Yes:

```python
C = compose(A, B)

class Tagged(C):
    @always
    def tag(self): ...
```

`Tagged`'s `@always` rule fires after the composite's rules each
tick. The subclass-rule path is rare; the canonical way to extend a
composite is `compose` again.

**Does `from gro import *` work in regular Python?**

No — `gro` is only importable inside a `.py` program loaded by the
gro binary (it's the embedded interpreter's view of the C++
bindings plus the Python stdlib at `python/gro/`). For unit tests
of the Python pieces themselves, see `tests/_stub.py` which
stubs `_core` with MagicMocks.

### Simulation

**Why do my `@when` predicates seem to fire too rarely?**

If you're emulating CCL's `rate(k) & cond`, remember the standard
translation is `rand(N) < k * dt() * N` — i.e. you need to multiply
`k * dt()` to get the per-tick probability. Missing the `dt()` is
the common bug.

**Why do signal patterns sometimes form rings?**

Same numerical-stability issue as CCL: the Euler scheme on the
finite-element grid becomes unstable for large diffusion / large
`dt` / large `signal_grid_*`. Reduce `dt` or the diffusion rate.

**Cells go through walls created by `barrier(...)`. What's wrong?**

You're probably running a stale build. The `barrier(...)` binding
was fixed in v1.1.0 to also create the chipmunk segment shape
(previously only the rendering record was added). Rebuild and
retry.

### Development

**What is the Python integration built on?**

[pybind11](https://pybind11.readthedocs.io/) embedding a CPython
interpreter into the same `gro.app` binary. The same C++ simulator
(Qt + Chipmunk2D) underlies both frontends. The shared C++
backbone is documented in `DESIGN.md`.

**Can I extend gro with my own C++ bindings?**

Yes, in principle — see `src/PythonRuntime.cpp` for the binding
patterns. The `_core.*` names are internal and not API-stable yet
(the user-facing API on the `gro` module is). Wait for v1.1.0 to
tag before depending on `_core.*` names from a third-party build.

**Is there a headless / scripted build?**

Yes: `gro --load PATH --ticks N` runs the simulator for `N` ticks
without showing a window (used by the integration test suite).
Combine with shell redirection to capture `print()` output.

**What about Linux / Windows?**

Linux builds work as of v1.1.0 (`cmake -S . -B build`). Windows
support tracks the CCL side; the Python frontend uses the same
embedded interpreter.
