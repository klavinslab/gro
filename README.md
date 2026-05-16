gro
===

The gro bacterial micro-colony growth specification and simulation
software. Programs can be written in either CCL (`.gro`) or Python
(`.py`); both load through `File → Open` and share the same
underlying simulator.

Documentation
===

http://depts.washington.edu/soslab/gro/docview.html

`DESIGN.md` in this repo covers the Python integration.

Building
===

gro builds against **Qt 6** and **CMake 3.24+**. Dependencies (`ccl`,
`Chipmunk2D`, `pybind11`) are fetched automatically — no manual setup
required. The embedded Python interpreter uses the system `python3`
(Homebrew on macOS).

macOS
---

```bash
brew install qt cmake bison flex m4 python@3.13

git clone https://github.com/klavinslab/gro.git
cd gro

cmake -S . -B build
cmake --build build -j

open gro.app
```

`-S .` says "configure from the current directory"; `-B build` says
"put the build output in `./build/`". Every `cmake` invocation after
the first reuses that same `build/` directory.

The build drops a `gro.app` symlink at the project root pointing at
the freshly-built bundle, so `open gro.app` works from the source
tree without descending into `build/`.

The `bison`, `flex`, and `m4` formulas are needed to build the
bundled parser. `cmake` finds them automatically.

The first `cmake -S . -B build` clones `ccl`, `Chipmunk2D`, and
`pybind11` into `build/_deps/`. Subsequent configures are cached.

To produce a distributable disk image:

```bash
cmake --build build --target package
# → build/gro-<version>-mac.dmg
```

Linux
---

```bash
sudo apt install qt6-base-dev cmake bison flex python3-dev
git clone https://github.com/klavinslab/gro.git
cd gro
cmake -S . -B build
cmake --build build -j
./build/gro
```

Tests
---

The Python integration has a headless test suite (stdlib `unittest`,
zero external dependencies):

```bash
python3 tests/run.py        # run everything
python3 tests/run.py -v     # verbose
python3 tests/run.py test_compose test_strict   # specific modules
```

`tests/_stub.py` substitutes a MagicMock `_core` so the suite runs
without launching gro. Coverage: state schema, rule decorators,
strict-mode AST validators, `Program.with_args`, `compose` and
`_PartScope`, `Composed` sugar, `WorldProgram`/`set_main`/`reset`,
and a one-test-per-file smoke check that every `examples/*.py`
loads cleanly.


Layout
---

- `examples/` — sample `.gro` and `.py` programs. Open via `File →
  Open` in the GUI.
- `include/` — CCL standard library (`gro.gro`).
- `python/gro/` — Python stdlib equivalent of `include/gro.gro`.
- `src/` — C++ simulator, GUI, and bindings.
- `DESIGN.md` — Python integration design.

At launch the binary walks up from its executable directory looking
for the three resource folders (`include/`, `examples/`, `python/`)
as siblings; production installs ship `gro.app` + those three
folders together, and the dev build finds them at the project root.
