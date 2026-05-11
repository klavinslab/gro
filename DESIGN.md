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
API surface listed in this document (`Program`, `State`, `when`,
`always`, `rate`, `signal`, `ecoli`, `set_param`, `get_param`,
`set_signal`,
`get_signal`, `emit_signal`, `absorb_signal`, `dt`, `rate`, `rand`,
`compose`, `Composed`, `Preserved`, `Halved`, `on_tick`, …) plus the
exception types (`GroError`, `GroLoadError`).

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
    state = State(t: float = 2.4)

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
| `x := 0;` (initializer)                    | field in `state = State(x: int = 0)`    |
| `condition : { actions }` (guarded cmd)    | `@when(lambda self: ...)`               |
| `true : { … }`                             | `@always`                               |
| `rate(0.1) : { … }`                        | `@rate(0.1)`                            |
| `program main() := { … };`                 | `on_tick(fn)` or a `Main` class         |
| `program r(x) := p(x+1) + p(x+2);`         | `r = compose(P.with_args(x+1), …)`      |
| `program h(x) := p(x) + g() sharing t;`    | `h = compose(P, G, share=["t"])`        |
| `needs t;`                                 | `requires = ["t"]` class attr           |
| `ecoli([x:=0,y:=0], program p())`          | `ecoli(x=0, y=0, program=P)`            |

### State schema

```python
state = State(
    t:        float             = 2.4,    # numeric → halved on divide
    gfp:      int               = 0,      # numeric → halved
    mode:     int               = 0,      # numeric → halved (probably wrong default)
    history:  list              = (),     # non-numeric → deep-copied
    last_msg: str               = "",
    period:   Preserved[float]  = 5.0,    # explicit: not halved
    count:    Halved[int]       = 100,    # explicit: halved (redundant with default but fine)
)
```

- `State(...)` builds a per-class frozen-shape dataclass.
- Numeric fields are halved on cell division by default.
- Annotate with `Preserved[T]` to override and keep value across
  division; `Halved[T]` for explicit-halved (default).
- List/tuple/dict defaults must be immutable (`()`, `frozenset()`, or
  `field(list, default=[])`). Strict mode rejects `list = []`.

### Rules and guards

```python
@when(lambda self: self.state.mode == 0 and self.get_signal(ahl) > 0.01)
def relay(self):
    self.emit_signal(ahl, 100)
    self.state.mode = 1
```

The predicate passed to `@when(...)` is **AST-inspected at class
definition time**.
Allowed nodes:

- `Compare`, `BoolOp`, `UnaryOp`, `BinOp` (math/logic operators)
- `Attribute` accesses on `self`, on the `gro` API surface
- `Subscript` for lists/dicts
- `Constant`, `Name` (must resolve to module-level constants only)
- `Call` to a closed allowlist: `self.get_signal`, `self.get_param`,
  `get_signal`, `get_param`, `rate`, `min`, `max`, `abs`, `len`,
  `math.*` (the safe subset).

Forbidden:

- `Assign`, `AugAssign` (no mutation in guards)
- Calls to anything else (no `print`, no user functions, no
  `self.emit_signal`, etc.)
- `Lambda` inside the guard, `Yield`, comprehensions with side-effecty
  generators.

The sandbox treats both `gro.foo` (qualified) and `foo` (post `from gro
import *`) identically — the AST walker resolves names against
`gro.__all__` rather than against the user's imports.

A violation is a load-time error with file/line/column pointing into
the user's source. The user gets ~the same property CCL gave them:
guards can't have side effects.

Rule body restrictions are looser — bodies can call anything in the
`gro` API, mutate `self.state.*`, etc. The body's only structural
restriction is "must not raise from a rule fired by the simulator"; an
unhandled exception in a body halts the simulation and prints the
traceback to the console.

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

Parametric composition:

```python
def Repeater(x):
    return compose(Pulser.with_args(period=x+1),
                   Pulser.with_args(period=x+2))
```

`Program.with_args(...)` returns a thin subclass that injects the args
into `setup()` and (if any of the args are fields) into the initial
state.

Class-style sugar:

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

```python
def divide(mother, f):              # f ≈ 0.5 with noise
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

On by default. Enforced at class-creation time by `Program.__init_subclass__`:

- `state` must be a `State(...)` (no missing schema).
- No instance attributes can be set on `self` outside of:
  - the declared `state` fields,
  - simulator-injected attributes (`volume`, `id`, `just_divided`,
    `daughter`, `selected`, plus the reporter aliases `gfp`/`rfp`/
    `cfp`/`yfp`).
- Every rule method must be decorated with `@when(...)` or `@always`.
- Every `@when(...)` predicate passes the AST sandbox.
- `requires` and `share` lists, if present, reference real field names.

Mutable defaults in `State` (`list = []`, `dict = {}`, etc.) are
rejected. `__slots__` on `Program` makes accidental
attribute creation raise `AttributeError`.

Permissive mode (`set_strict(False)` or `class P(Program, strict=False)`)
opts a single class out: warns instead of erroring on the above,
deep-copies non-state attributes on division with a warning. Intended
for quick scripts, not production code.

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

## Phasing / milestones

All work on `python-embedding`. We merge to `master` and tag `v1.1.0`
once milestone 6 lands.

1. **pybind11 plumbing.** `find_package(Python3)`, `FetchContent`
   pybind11, link `pybind11::embed`. Empty `gro` module. `Py_Initialize`
   on demand, `Py_Finalize` at shutdown. gro builds and runs `.gro`
   files unchanged; passing a `.py` file prints "Python support not
   implemented" to the console.
2. **Minimal API surface.** `set_param`, `get_param`, `signal`,
   `set_signal`, `get_signal`, `dt`, `ecoli`. Enough to run a trivial
   program with no behavior. A `.py` file with `ecoli(x=0, y=0)` puts a
   cell on the canvas. `__all__` is set; `from gro import *` works.
3. **Program class with state schema.** `Program`, `State`, `when`,
   `always`. AST sandbox for the `@when(...)` predicate. Strict mode.
   wave.py runs and visually matches wave.gro.
4. **Cell division semantics.** `Preserved` / `Halved`, per-field
   halving rules, `just_divided` / `daughter` plumbed.
5. **Composition.** `compose`, `Composed`, `share=[...]`, `requires`,
   parametric composition via `.with_args`.
6. **Polish.** Reporters, error overlay path for Python errors, docs,
   `.py` examples mirroring the `.gro` ones. Bump version, build DMG.

Each milestone leaves master untouched and `python-embedding` in a
state where running gro on a `.gro` file still works. We'll know we're
done when every `examples/*.gro` has a `.py` twin and both produce
visually identical simulations.
