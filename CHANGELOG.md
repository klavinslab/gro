# Changelog

## v1.1.0-rc.1

Source-only release candidate. Binary distribution is deferred;
advanced users can `git checkout v1.1.0-rc.1` and build per the
README. The final `v1.1.0` tag will land when binaries ship.

Adds a Python frontend alongside the existing CCL one. A gro program
can now be written as either a `.gro` file (CCL syntax) or a `.py`
file (Python class subclassing `Program`); both load through
`File → Open` and share the same C++ simulator.

### Python integration

- **`Program` base class** with `State(...)` schema, `@when` /
  `@always` / `@rate(k)` rule decorators, and `setup()` hook. Each
  decorated method registers a rule the simulator fires per tick.
- **Cell built-ins**: `self.volume`, `self.id`, `self.x`, `self.y`,
  `self.theta`, `self.just_divided`, `self.daughter`, `self.selected`,
  and reporters `self.gfp` / `rfp` / `yfp` / `cfp`.
- **Cell-local API**: `self.emit_signal`, `self.absorb_signal`,
  `self.get_signal`, `self.die`, `self.divide`, `self.run`,
  `self.tumble`, `self.message`.
- **World API** (module level): `signal`, `set_signal`,
  `get_signal_at`, `set_signal_rect`, `set_param`, `get_param`,
  `message`, `clear_messages`, `dt`, `time`, `rand`, `srand`,
  `chemostat`, `barrier`, `snapshot`, `stop`, `start`, `reset`,
  `set_theme`, `stats`, `reaction`, `get_signal_matrix`.
- **Cell types**: `ecoli(...)` and `yeast(...)` spawn at world
  coords with an optional Program attached.
- **Composition**: `compose(*parts, share=[...])` merges several
  Program subclasses; `Composed` class-body sugar provides the
  equivalent declarative form. `Program.with_args(**overrides)`
  returns a thin subclass for parametric programs whose differences
  are in `State` defaults; closure-factory functions handle the
  rest.
- **Cell division semantics**: numeric state fields are halved
  between mother and daughter by default; wrap a value in
  `Preserved(...)` to opt out. Reporter counters use the same
  halve-on-divide behavior as CCL.
- **World programs**: `WorldProgram` base class plus `set_main(...)`
  installs a rule set that runs once per simulation tick at world
  scope, mirroring CCL's `program main()`.
- **Strict mode** (enforced at class-creation time): `state` must
  be a `State(...)`; method bodies may only write to reporter
  setters or underscore-prefixed internals; `@when` predicates
  pass an AST sandbox (walrus, nested lambda, yield, await
  rejected); `requires` lists must reference shared names;
  `set_main` requires a `WorldProgram` subclass. `GroLoadError`
  surfaces all of these at load time.
- **Example coverage**: 21 of 23 `examples/*.gro` files have `.py`
  twins (`wave`, `skin`, `morphogenesis`, `growth`, `gfp`,
  `dilution`, `bandpass`, `signal_demo`, `inducer`,
  `coupled_oscillator`, `symbiosis`, `spots`, `chemotaxis`,
  `foreach`, `edge`, `barriers`, `signal_grid`, `game`,
  `spatial_oscillations`, `signal_dump`, `yeast_example`).

### Error UX

Python rule errors now surface in the bottom console panel (with
the user's traceback preserved via `<pre>` formatting) and halt the
simulation, instead of being silently written to `stderr`.

### Build / packaging

- `include/`, `examples/`, and `python/` live as siblings of
  `gro.app` rather than copied inside the bundle. At launch the
  binary walks up from its executable directory looking for those
  three folders, so the dev build and the production install share
  the same logic with no build-tree symlinks.
- The build drops a `gro.app` symlink at the source-tree root for
  double-click testing.
- `File → Open` now remembers the last-browsed directory across
  sessions (persisted via `QSettings` to
  `~/Library/Preferences/edu.washington.klavinslab.gro.plist`).

### Internal

- `Yeast.cpp` is now compiled into the gro target (was previously
  excluded). A latent `Yeast::render` shadowing bug was fixed in
  the process.
- `Cell::run` / `Cell::tumble` extracted from CCL's `run` / `tumble`
  bindings so the CCL and Python frontends share one C++
  implementation. Same pattern applied to `World::add_barrier`,
  `World::dispatch_set_param`, `World::stats`,
  `World::add_reaction`, `World::get_signal_matrix`'s bounds check,
  and `Theme::set` (now delegates to `Theme::set_colors`).
- `set_main(...)`'s re-installation queues the old prog for
  deferred deletion via `World::pending_prog_deletions`, drained
  after `prog->world_update` returns so a rule body that calls
  `set_main(NewMain)` doesn't free the prog whose `world_update`
  is still on the C++ stack.

## v1.0.0

Qt 6 + CMake 3.24+ port of the original gro. Apple-Silicon-native
build; `find_package(Python3)` and `FetchContent` for `ccl` and
`Chipmunk2D`. CCL-only frontend.
