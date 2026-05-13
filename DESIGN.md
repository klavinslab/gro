# Python embedding — design doc

Living document for the `python-embedding` branch. Subject to revision as
we implement and learn.

## Goal

Let gro users write cell programs in Python in addition to the existing
CCL `.gro` syntax. A `.py` file dropped into File → Open simulates
identically (in semantics, not byte-for-byte) to its `.gro` equivalent.

CCL is not deprecated. `.gro` files keep working, unchanged, forever.

## Non-goals (v1)

- Replicating CCL's Hindley–Milner type inference. Python is dynamic and
  we accept the trade.
- Pure-functional guarantees for guards. We sandbox via AST inspection
  (see below), but the surface is Python, not a custom DSL.
- Running CCL programs *as* Python (or vice versa). Each file uses one
  language end-to-end.
- Cross-language composition. `gro.compose(LeaderPy, LeaderCcl)` is out.

## Architecture

```
                 .gro file → CCL lexer/parser/eval (existing path)
File → Open →  ┌
               └ .py file  → embedded CPython → user code calls
                              into the `gro` Python module → C++
```

CPython is **embedded** in `gro.app` via [pybind11]. The user does not
need a Python install on their machine; we ship Python 3.X (the matching
ABI) inside the bundle. macdeployqt won't handle this for us — see
*CMake plumbing* below.

[pybind11]: https://pybind11.readthedocs.io

`GroThread::parse(path)` switches on extension:

- `.gro` → `readOrganismProgram` (unchanged).
- `.py`  → new `readPythonProgram` which:
  1. Lazily `Py_Initialize()`s on first call (process-wide, one shot).
  2. Imports the `gro` module (registered via `PyImport_AppendInittab`
     **before** `Py_Initialize`).
  3. Resets the simulator's program registry.
  4. `PyRun_SimpleFile`'s the user's script. Top-level calls to
     `gro.set_param`, `gro.signal`, `gro.ecoli`, `@gro.program` etc.
     register state with the simulator.
  5. Returns success / a `PythonException` containing the formatted
     traceback for the error console.

Subsequent ticks call the registered Python program callbacks from
`GroThread::run()` with the GIL held for the duration of that cell's
update. See *Threading* below.

## User-facing API

### Imports

`.py` gro programs canonically start with:

```python
from gro import *
```

This mirrors CCL's `include gro` one-for-one. The `gro` module curates
`__all__` so the wildcard import is well-defined; it exposes only the
API surface listed in this document (`Program`, `WorldProgram`,
`State`, `field`, `when`, `always`, `rate`, `signal`, `ecoli`,
`set_main`, `set_param`, `get_param`, `set_signal`, `get_signal`,
`emit_signal`, `absorb_signal`, `dt`, `rand`, `compose`, `Composed`,
`Preserved`, …) plus the exception types (`GroLoadError`).

Programs that prefer a tighter namespace can `import gro` and use
qualified names (`gro.rule`, `gro.always`, …) instead. The simulator
doesn't care which style is used.

### Program definition

Programs are subclasses of `Program`. State lives in a declared,
typed schema; rules are methods decorated with one of:

- `@when(predicate)` — fires when `predicate(self)` is true. Reads
  line-for-line as CCL's `predicate : { ... }`.
- `@always` — fires every tick. Sugar for `@when(lambda self: True)`.
- `@rate(p)` — fires probabilistically with rate `p` per simulated
  time unit. Sugar for `@when(lambda self: random.random() < p * dt)`.

```python
from gro import *

ahl = signal(diffusion=1.0, degradation=1.0)
set_param("dt", 0.075)

class Leader(Program):
    state = State(t=2.4)

    def setup(self):
        set_param("ecoli_growth_rate", 0.0)

    @always
    def tick(self):
        self.state.t += dt

    @when(lambda self: self.state.t > 10)
    def fire(self):
        self.emit_signal(ahl, 100)
        self.state.t = 0

ecoli(x=0, y=0, program=Leader)
```

Semantic correspondence with CCL:

| CCL                                        | Python                                  |
| ------------------------------------------ | --------------------------------------- |
| `include gro` (standard library)           | `from gro import *`                     |
| `program p() := { … };`                    | `class P(Program): …`                   |
| `x := 0;` (initializer)                    | field in `state = State(x=0)`           |
| `condition : { actions }` (guarded cmd)    | `@when(lambda self: ...)`               |
| `true : { … }`                             | `@always`                               |
| `rate(0.1) : { … }`                        | `@rate(0.1)`                            |
| `program main() := { … };`                 | `class Main(WorldProgram): … ; set_main(Main)` |
| `program r(x) := p(x+1) + p(x+2);`         | `r = compose(P.with_args(x+1), …)`      |
| `program h(x) := p(x) + g() sharing t;`    | `h = compose(P, G, share=["t"])`        |
| `needs t;`                                 | `requires = ["t"]` class attr           |
| `ecoli([x:=0,y:=0], program p())`          | `ecoli(x=0, y=0, program=P)`            |

### State schema

```python
state = State(
    t=2.4,                          # numeric → halved on divide
    gfp=0,                          # numeric → halved
    mode=Preserved(0),              # numeric, opt-out of halving
    history=field(list),            # each cell gets a fresh []
    counters=field(dict),           # each cell gets a fresh {}
    last_msg="",
    period=Preserved(5.0),          # numeric, preserved across divide
)
```

- `State(...)` builds a per-class state schema. At spawn time each
  cell gets its own `state` namespace populated from the defaults.
- Numeric fields are halved on cell division by default; non-numeric
  fields are deep-copied.
- Wrap a value in `Preserved(value)` to opt it out of halving — for
  state that represents a flag or timer rather than a molecular count.
  (Halving is already the default for numerics, so there's no
  corresponding `Halved(...)` wrapper.)
- Mutable types (`list`, `dict`, `set`, user-defined classes) are
  perfectly valid state — CCL has lists and records, and we keep
  parity. The only restriction is on **how the default is spelled**:
  a literal `list = []` or `dict = {}` would be shared across every
  cell (Python's class-attribute footgun), so use `field(callable)`
  instead, e.g. `list = field(list)`, `dict = field(dict)`, or
  `MyClass = field(lambda: MyClass(args))`. Strict mode rejects bare
  mutable literals at class-creation with a helpful error pointing to
  the `field(...)` fix.
- `gro.field(factory)` is just `dataclasses.field(default_factory=factory)`
  re-exported for convenience.

### Rules and guards

```python
@when(lambda self: self.state.mode == 0 and self.get_signal(ahl) > 0.01)
def relay(self):
    self.emit_signal(ahl, 100)
    self.state.mode = 1
```

The predicate passed to `@when(...)` is **AST-inspected at class
definition time**.

The current implementation enforces these foot-gun rejections:

- **Walrus operator** (`:=`) inside the lambda — Python lambdas can't
  syntactically contain `=` or `+=`, so walrus is the only in-lambda
  assignment form.
- **Nested `Lambda`** inside the guard (move the helper to a named
  method or module function).
- **`Yield` / `YieldFrom`** — predicates run synchronously per-tick
  and aren't generators.
- **`Await`** — rules aren't coroutines.

A violation raises `GroLoadError` at class-creation time. This catches
the obvious foot-guns; the user still has the freedom to call
arbitrary functions from a guard, which is a tradeoff — looser than
CCL's pure-expression guards, but enforceable without a full
call-allowlist sandbox.

A fuller sandbox is planned for a follow-up milestone:

- `Compare`, `BoolOp`, `UnaryOp`, `BinOp` (math/logic operators)
- `Attribute` accesses on `self`, on the `gro` API surface
- `Subscript` for lists/dicts
- `Constant`, `Name` (must resolve to module-level constants only)
- `Call` to a closed allowlist: `self.get_signal`, `self.get_param`,
  `get_signal`, `get_param`, `rate`, `min`, `max`, `abs`, `len`,
  `math.*` (the safe subset).
- Forbidden in the fuller form: calls to anything else (no `print`,
  no user functions, no `self.emit_signal`, etc.) and comprehensions
  with side-effecty generators.

The full sandbox would treat both `gro.foo` (qualified) and `foo`
(post `from gro import *`) identically by resolving names against
`gro.__all__`, and report file/line/column on violation — giving the
user the same property CCL gave them: guards can't have side effects.
Today's blocker for shipping it is that the natural Python idiom for
rate-style guards (`rand(N) < k * dt() * N`) calls functions CCL
hides inside its `rate(k)` keyword; landing the full sandbox cleanly
requires exposing a `rate(k) -> bool` callable for in-predicate use.

Rule body restrictions are looser — bodies can call anything in the
`gro` API, mutate `self.state.*`, etc. The body's only structural
restriction is "must not raise from a rule fired by the simulator"; an
unhandled exception in a body halts the simulation and prints the
traceback to the console.

#### Firing semantics: which rules run each tick

v1 uses **standard scheduling**: at each simulation tick, every rule's
predicate is evaluated in source-declaration order, and every rule
whose predicate is true fires (its body runs), in that same order. This
matches gro's current behavior — `gro_Program::update` calls
`AtomicProgram::step`, which is exactly the body of CCL's
`standard_scheduler`. Multiple rules can fire in one tick.

CCL's library actually defines three scheduling strategies in
`ccl/Schedulers.cpp` — `standard_scheduler`, `random_epoch_scheduler`
(one randomly-chosen rule per step; every rule fires exactly once per
"epoch" through the rule list), and `debugger_scheduler` (round-robin,
one rule per step). gro only ever wires up the standard one; the other
two are dormant.

We don't expose a scheduler choice in v1 to avoid API surface for a
feature that's never been used. If `random_epoch` semantics become
useful later, the planned extension point is a class kwarg:

```python
class Async(Program, scheduler="random_epoch"):  # not in v1
    ...
```

Adding that kwarg later is fully backward-compatible — existing
programs all behave as `scheduler="standard"`.

### Composition

The canonical primitive:

```python
Composite = compose(P1, P2, share=["t", "gfp"])
```

- Returns a new `Program` subclass.
- Each part is instantiated independently per cell; they have separate
  `state` namespaces.
- `share=[...]` lists field names aliased between parts. Every name in
  the list must exist in every part's `State` with the same type, or
  load fails.
- Sub-program rules are registered in part-list order, then in
  rule-declaration order within a part. Matches CCL's "guarded commands
  union, in order" semantics.

Parametric composition has two forms. For the common case where the
parameters override `State` defaults, use `Program.with_args(...)`:

```python
def Repeater(x):
    return compose(Pulser.with_args(period=x+1),
                   Pulser.with_args(period=x+2))
```

`Program.with_args(**overrides)` returns a thin subclass with the
named `State` defaults overridden. Wrap a value in `Preserved(...)`
to mark it preserved across division.

For parameters that shape rules (predicates, action bodies) rather
than just state defaults, use a plain factory function that closes
over the parameters and returns a `Program` subclass:

```python
def state_node(this, m_next, d_next, tf, gr, sigs):
    class S(Program):
        state = State(active=False)
        requires = ["q", "t"]
        @when(lambda self: self.state.q == this and tf > 0
                           and self.state.t > tf)
        def expire(self): ...
    return S
```

Class-style sugar via the `Composed` base class:

```python
class Wave(Composed):
    parts = [Leader, Follower]
    share = ["t"]
```

Equivalent to `Wave = compose(Leader, Follower, share=["t"])`.

### `needs` / `requires`

```python
class Printer(Program):
    requires = ["t"]

    @always
    def show(self):
        print(self.state.t)
```

`compose(Leader, Printer, share=["t"])` validates that every
name in `Printer.requires` is either:

- declared in some part's `State`, **and**
- listed in `share`,

so that all `requires` resolve to actual storage. Otherwise load fails
with a clear message.

### Cell division

CCL halves all numeric locals when a cell divides. With the declared
schema, gro knows exactly what to do.

From the user's perspective, division is automatic — they declare
state via `State(...)` (with optional `Preserved` wrappers) and
don't write any division code themselves. The
behavior below is the *simulator's* job, shown here as pseudocode so
the implementation can be specified unambiguously:

```python
# INTERNAL — what the simulator does on cell division. Not user code.
def _divide(mother, f):              # f ≈ 0.5 with noise
    daughter = mother.__class__.__new_uninitialized__()
    for name, type_ in mother.__schema__.items():
        v = getattr(mother.state, name)
        if mother.__halve_on_divide__[name]:
            setattr(mother.state,   name, type_(v * f))
            setattr(daughter.state, name, type_(v * (1 - f)))
        else:
            setattr(daughter.state, name, copy.deepcopy(v))
    # setup() is NOT re-run on the daughter — same as CCL initializers
    return daughter
```

For composed programs, each sub-part's state divides independently.
Shared cells (from `share=[...]`) divide exactly once.

### Strict mode

On by default. Enforced at class-creation time by the `_ProgramMeta`
metaclass:

- **State must be a `State(...)`.** Assigning anything else (`state =
  {"t": 0}`, `state = SimpleNamespace(t=0)`, ...) raises
  `GroLoadError` immediately, instead of failing mysteriously when
  the simulator tries to introspect it.
- **Method bodies may only write `self.<name>`** where name is a
  reporter setter (`gfp`, `rfp`, `yfp`, `cfp`) or starts with `_`
  (private/internal). Anything else — `self.tagged = True`,
  `self.counter += 1` — is rejected with a "did you mean
  self.state.tagged?" hint. The check is an AST walk of each
  decorated rule method plus any user-defined `setup`, so the
  typo fails at load time instead of silently inventing an
  instance attribute. Writing through `self.state.X = ...` is
  always fine.
- **Every `@when(...)` predicate passes the AST sandbox** (walrus,
  nested lambda, `yield`, `await` rejected; see above).
- **`requires` lists, if present, must reference names in `share`**
  (transitively: in some part's `State`).
- **`set_main(...)` requires a `WorldProgram` subclass**, not a
  bare `Program`. Cell-context built-ins like `self.volume` and
  `self.emit_signal` make no sense at world scope; the
  `WorldProgram` marker makes that boundary explicit.

Method roles inside a `Program` subclass:

- **`setup(self)`** — optional init hook. Runs once when each cell
  is spawned (not on division). Use it for per-cell parameter
  changes (`set_param(...)`), one-time computations, etc.
- **`@when(...)` / `@always` / `@rate(p)` decorated methods** — rules
  the simulator fires each tick.
- **Any other method** — plain helper. Not a rule. Not called
  automatically. Use it however you like; rules and `setup()` can
  invoke it. Helper methods are NOT body-validated (escape hatch).
- Dunder methods (`__init__`, `__repr__`, …) are allowed but rarely
  needed; the framework manages instance lifecycle.

Literal mutable defaults in `State(...)` (`list = []`, `dict = {}`, …)
are rejected at class creation with an error that points the user at
`field(factory)` — see the *State schema* section for the rationale.

Permissive mode (a `set_strict(False)` toggle, or class-level
`strict=False` keyword) is **not yet implemented**. Today strict mode
is unconditional.

### Cell built-ins

These read-only attributes live on `self` and are written by the
simulator each tick:

- `self.volume` — current cell volume (femtoliters)
- `self.id` — unique cell id
- `self.just_divided` — true the tick after a division
- `self.daughter` — true on exactly one of the two cells just after
  division
- `self.selected` — true when the user has selected this cell in the
  GUI

Plus the world constant `dt` (i.e. `gro.dt`). Same set as CCL.

### Reporters

The four fluorescent-protein counts (`gfp`, `rfp`, `cfp`, `yfp`) are
properties on the base class backed by per-cell counters that the
renderer reads. They behave like `int`s; `self.gfp += 1` works.

Reporters do NOT live in `state`; they're separate counters that the
simulator owns. They are halved on division (same as numeric state).

### World programs (`main`)

CCL's `program main()` is special: it runs once per simulation tick at
the world level, not per cell. The Python equivalent is a subclass of
`WorldProgram`, registered with `set_main(...)`.

```python
class Main(WorldProgram):
    state = State(t=0.0)

    @always
    def tick(self):
        self.state.t += dt

    @when(lambda self: self.state.t > 5)
    def pin(self):
        set_signal(ahl, 0, 0, 10)

set_main(Main)
```

`WorldProgram` inherits from `Program` and uses the same machinery:
`state` schema, `@when` / `@always` / `@rate` decorators, scheduler
dispatch, `setup()`, `compose(...)` for combining several into one.
Multiple guarded commands are dispatched by the same scheduler that
runs per-cell rules — no `if`-chains in the user's tick code.

`set_main(...)` enforces that its argument is a `WorldProgram`
subclass (not a bare `Program`); passing the wrong kind of class
raises `GroLoadError` at load time. See *Strict mode* above.

Differences from `Program`:

- No cell built-ins (`volume`, `id`, `just_divided`, `daughter`,
  `selected`). Accessing them is a class-creation error in strict mode.
- No reporters (`gfp`/`rfp`/`cfp`/`yfp`).
- No per-cell signal API (`self.emit_signal`, `self.absorb_signal`).
  World-level signal control still works via the global functions
  (`set_signal`, `get_signal`).
- Singleton — one instance per world. Division semantics don't apply,
  so `Preserved` markers on the state schema are
  no-ops.

Composition still works:

```python
class Logger(WorldProgram):  ...
class Pinger(WorldProgram):  ...
set_main(compose(Logger, Pinger, share=["t"]))
```

A `.py` program with no `set_main(...)` call simply has no world
program (same as omitting `program main` in CCL — perfectly fine).

### Standard library

| CCL                          | Python                                |
| ---------------------------- | ------------------------------------- |
| `print(x, y, ...)`           | builtin `print`                       |
| `skip()`                     | `pass`                                |
| `exit()`                     | `exit()` (raises a clean shutdown)    |
| `atoi`/`atof`/`tostring`     | builtins `int`, `float`, `str`        |
| `length L`                   | `len(L)`                              |
| `sin`/`cos`/`tan`/…          | `math.sin` etc.                       |
| `rand(x)`                    | `rand(x)`                             |
| `range n`                    | `range(n)`                            |
| `cross A B`                  | `itertools.product(A, B)`             |
| `zip A B`                    | `zip(A, B)`                           |
| `rev`/`sumlist`/`member`/…   | builtin reversals, `sum`, `in`        |
| `makelist n default`         | `[default] * n`                       |
| `table f n m`                | `[f(i) for i in range(n, m+1)]`       |

## Extension model

The CCL stdlib pattern — write a C++ function, register it with a
string name, then declare it in a `.gro` interface file with
`internal real time() "time";` — collapses in pybind11-land to one
line per primitive. The interface file goes away; the C++ binding
both registers and declares simultaneously.

### C++ core + Python wrapper

The user-facing `gro` module is built from two layers:

```
gro/                            ← user-facing package (`from gro import *`)
├── __init__.py                 ← module entry: re-exports from _core
│                                 and _program; defines world defaults
│                                 (_setup_world), themes, the ecoli
│                                 wrapper, and the public __all__.
│                                 Plays the role of include/gro.gro.
├── _program.py                 ← program-model machinery: Program,
│                                 State, Preserved, field, @when /
│                                 @always / @rate, _tick, _split,
│                                 strict-mode + AST checks. No
│                                 direct CCL analogue (CCL's program
│                                 syntax lives in the parser, not the
│                                 stdlib).
└── _core   (built-in module)   ← C++ via pybind11: simulator hooks,
                                  cell-local API, signal/parameter
                                  primitives, world queries.
```

`gro/__init__.py` imports from `_core` (C++ primitives) and from
`gro._program` (program-model machinery), defines the world-defaults
and theme stdlib functions, and curates `__all__` so `from gro
import *` brings in exactly the documented surface.

`_core` is registered via `PyImport_AppendInittab("_core", &init_core)`
before `Py_Initialize`; the user never sees it as a separate import.

### How a developer adds a new primitive

**Low-level (new C++ functionality exposed to Python):**

```cpp
// In gro_python_module.cpp:
PYBIND11_EMBEDDED_MODULE(_core, m) {
    m.def("time",       &gro_time);
    m.def("set_param",  &gro_set_param);
    m.def("get_signal", &gro_get_signal);
    m.def("ecoli",      &gro_ecoli);
    // ... one line per function

    py::class_<Cell>(m, "Cell")
        .def_readonly("id", &Cell::id)
        .def_readonly("volume", &Cell::volume)
        .def_property_readonly("gfp", &Cell::get_gfp)
        .def("die",    &Cell::die)
        .def("divide", &Cell::divide)
        .def("run",    &Cell::run)
        .def("tumble", &Cell::tumble);
}
```

Adding a new C++-backed primitive is a one-liner: add `m.def("foo",
&gro_foo)`, recompile, done. pybind11 introspects the C++ types to
build the Python signature — no separate `internal real foo(real)
"foo";` declaration needed.

To make the new name visible to `from gro import *`, also add it to
the `__all__` list in `gro/__init__.py`.

**High-level (a new decorator or helper, written in Python):**

Edit `gro/__init__.py`, add the function or class, list it in
`__all__`. No C++ recompile.

### Why split it this way

Some things are better in C++:

- Tight inner loops the simulator calls every tick (signal lookup, cell
  position queries). One pybind11 call per cell-tick is fine; ten is
  noticeable.
- Anything that touches the gro core directly (the chipmunk space, the
  signal grid, the cell list).

Some things are better in Python:

- The `Program` metaclass with `__init_subclass__` strict-mode checks.
- The `State(...)` schema builder.
- The `@when` / `@always` / `@rate` decorators.
- AST-walking the predicate sandbox.
- `compose(...)` / `Composed`.

Trying to do the second list in C++ via pybind11 is masochism; trying
to do the first list in pure Python is slow. The split is exactly
where most embedded-Python projects (numpy, pandas, mypy's mypyc-built
core, etc.) put it.

### Bundle layout on macOS

```
gro.app/Contents/
├── MacOS/
│   └── gro                       ← the Qt app; embeds CPython
├── Frameworks/
│   ├── QtWidgets.framework
│   ├── QtSvg.framework
│   └── Python.framework          ← bundled by macdeployqt successor
├── Resources/
│   ├── examples/                 ← .gro and .py example files
│   ├── include/
│   │   ├── gro.gro               ← CCL stdlib (unchanged)
│   │   └── standard.gro
│   └── python/                   ← Python user-facing wrapper
│       └── gro/
│           └── __init__.py
└── PlugIns/
    ├── platforms/
    └── imageformats/
```

At `Py_Initialize` time, gro prepends `Contents/Resources/python/` to
`sys.path` so `from gro import *` resolves to the bundled wrapper.
`_core` is already a builtin module, so it imports without going
through `sys.path` at all.

## Threading

The simulation runs in a `QThread` (`GroThread::run`). All Python calls
from C++ acquire the GIL (`pybind11::gil_scoped_acquire`) for the
duration of one tick of one cell. The GIL is released between cells
inside one tick. Total wall-clock overhead is ~1 µs per call; for a
1000-cell colony with 4 rules each running at 50 Hz, that's roughly
200 ms/sec of GIL churn — measurable but acceptable.

No user-spawned Python threads. We may revisit if a use case appears.

## CMake plumbing

```cmake
find_package(Python3 3.11 REQUIRED COMPONENTS Development Interpreter)
include(FetchContent)
FetchContent_Declare(pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG        v2.13.6
)
FetchContent_MakeAvailable(pybind11)

target_link_libraries(gro PRIVATE pybind11::embed)
```

Plus a POST_BUILD step that bundles the matching Python framework into
`gro.app/Contents/Frameworks/` and rewrites the interpreter's rpath. On
macOS this is roughly:

```cmake
# Copy Python.framework (slim copy — only the .dylib + std lib zip) into
# Contents/Frameworks/, then install_name_tool the gro binary so it loads
# the bundled Python instead of the build host's.
```

Bundle size impact: +25 to +35 MB depending on which stdlib modules we
prune. DMG goes from 25 MB → ~55 MB.

The user-facing `gro` Python module is implemented in C++ (single
`pybind11::module_` named `gro`) and registered via
`PyImport_AppendInittab` before `Py_Initialize`. It is a built-in
module of the embedded interpreter; the user never sees a `gro.so` on
disk. The module's `__all__` is set explicitly to the API surface
documented here, so `from gro import *` brings in exactly the
documented names — nothing more, nothing accidentally re-exported.

## Loading & error reporting

A `.py` parse error or top-level exception is caught and formatted via
`traceback.format_exception`. The error overlay (red Material Symbols
"error" we just added) appears on the canvas; the formatted traceback
prints to the console panel.

Runtime exceptions inside a rule body have the same path: catch in C++,
format with `traceback`, halt the simulation, show overlay + traceback.

## Production readiness (post-M6, pre-merge-to-master)

All milestones M1–M6 landed on `python-embedding`. The branch is
behaviorally complete (Python frontend mirrors the CCL one; 21 of
23 example ports done). Before merging to `master` and tagging
`v1.1.0`, the items below need attention. Grouped by risk-of-
shipping rather than by polish; items marked **load-bearing** would
block release if skipped.

### Correctness / robustness — load-bearing

- **Crash-on-stop investigation.** A user-reported crash after
  selecting a cell + stopping the sim hasn't reproduced, but a
  crash silently dropping the lab is unacceptable. Add a crash
  logger or wire up the macOS report-on-crash path so the next
  occurrence captures a stack.
- **Cell-lifecycle edge audit.** Cell dying mid-divide; reload
  while a Python rule is mid-execution; `pending_prog_deletions`
  under unusual conditions; whether a cell's Python program can
  hold a reference that outlives the World.
- **Error-recovery story.** After `set_stop_flag(true)` from a
  Python rule error, document and test the minimum sequence to
  return to a clean state (Reload? close + reopen? edit + reload?).
- **`PythonRuntime` thread safety.** The static `current_cell_`
  pointer is single-window by convention. If multi-window or
  parallel-sim ever lands, the pointer becomes a race. Document
  the constraint or push to a thread-local.

### Testing — load-bearing for production

- **Headless test runner** that loads each `examples/*.py`, ticks
  for N steps, asserts no Python exception. Catches the class of
  regression we kept finding by "open and watch".
- **Unit tests** for `compose()`, `with_args`, the strict-mode
  validators, `_PartScope` proxy. They were tested manually
  mid-session; a CI-runnable suite freezes the behavior.

### Build / distribution — load-bearing for shipping

- **DMG staging.** Verify `cmake --build build --target package`
  stages `gro.app + examples/ + include/ + python/` at the DMG top
  level, not just `gro.app` alone. Customize CPack DMG config
  otherwise.
- **Code signing + notarization.** Ad-hoc signing works locally;
  anyone outside the lab will hit Gatekeeper. Decide if v1.1.0 is
  lab-internal or share-able; the latter needs real signing +
  notarization.
- **Universal binary.** Currently arm64 only; Intel Macs need a
  rebuild on x86_64. Decide whether to support.
- **Python 3.13 hard dependency.** `CMakeLists.txt` pins to
  Homebrew's `python@3.13`; minor-version bumps shift the path.
  Either document the dep or relax to "Python 3.10+".

### UX

- **Long traceback rendering** in the bottom console with the
  `<pre>` wrapper — verify on a deliberately deep stack.
- **Reload after error.** Does the stale traceback clear, or stick
  in the console?
- **Selection message persistence.** When the user clicks away
  from a selected cell, the channel-2 quadrant message should
  clear; verify it does.

### Documentation

- **User-facing Python tutorial** ("your first .py gro program in
  60 seconds"). This DESIGN doc is internal; users need an entry
  point.
- **API reference.** Either Sphinx/MkDocs site from the docstrings
  or a single user-guide markdown.
- **Migration note** for CCL-`.gro` users moving to `.py`: what
  changes, what doesn't.

### Known gaps / debt

- `examples/maptocells.py` port pending (needs a `for_each_cell`-
  style iterate-over-cells primitive).
- `examples/chemotaxis.gro` has uncommitted user edits
  (`run(180)`/`tumble(180)`) — decide whether they go into v1.1.0
  or revert.
- `Cell::run`/`Cell::tumble` are concrete on `Cell` so Yeast
  inherits them silently. Either virtualize with a no-op on Yeast,
  or document as "physics works on any cell type".
- Batch reporter setter and per-tick caching of
  `just_divided`/`daughter` — only after a profile points there.

### API stability — fix-before-v1.1.0-tag

- `_core.current_*` naming on the C++ embedded API: stable for
  any future C++ extensions or alternate Python frontends.
- `Preserved(value)` vs alternatives (`Pinned`, `KeepAcrossDivision`)
  — cheaper to bikeshed before a tagged release than after.
- `set_main(...)` strictness: today rejects bare `Program`
  subclasses. Lock in (currently right call) or loosen.

### Ship steps (in order, once the above is settled)

1. Address any load-bearing items judged blocking.
2. Final DESIGN.md / README / CHANGELOG pass.
3. `git tag v1.1.0`; build + verify DMG.
4. Merge `python-embedding` → `master` (probably a merge commit,
   not squash, to preserve the per-milestone history).
5. Update GitHub release notes from CHANGELOG.md.
