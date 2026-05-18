gro
===

Bacterial micro-colony growth simulator. Programs are written in
either CCL (`.gro`) or Python (`.py`); both load through `File →
Open` and share the same C++ simulator.

Documentation
---

- [`docs/gro.md`](docs/gro.md) — CCL reference.
- [`docs/python.md`](docs/python.md) — Python reference.
- [`DESIGN.md`](DESIGN.md) — Python integration internals.
- [`CHANGELOG.md`](CHANGELOG.md) — release history.

Build
---

v1.1.0+ is source-only — build it yourself. Binary releases are
deferred. (v1.0.0 DMGs may still be floating around but are missing
the Python frontend and recent fixes.)

Requires Qt 6 and CMake 3.24+. CCL, Chipmunk2D, and pybind11 are
fetched automatically.

### macOS

```bash
brew install qt cmake bison flex m4 python@3.13
git clone https://github.com/klavinslab/gro.git
cd gro
cmake -S . -B build
cmake --build build -j
open gro.app
```

The build drops a `gro.app` symlink at the project root pointing at
`build/gro.app`. To produce a DMG:

```bash
cmake --build build --target package   # → build/gro-<version>-mac.dmg
```

### Linux

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

```bash
python3 tests/run.py        # unit + integration (~6s, 124 tests)
python3 tests/run.py -v     # verbose
```

Unit tests stub `_core` via `tests/_stub.py`; integration tests
subprocess the gro binary with `--load PATH --ticks N` (auto-
skipped if the binary isn't built).

Layout
---

- `examples/` — `.gro` and `.py` programs.
- `include/` — CCL standard library.
- `python/gro/` — Python stdlib mirror.
- `src/` — C++ simulator, GUI, bindings.
- `docs/`, `DESIGN.md`, `CHANGELOG.md`.

At launch the binary walks up from its executable directory
looking for `examples/`, `include/`, and `python/` as siblings.
Production installs ship `gro.app` + those three folders together.
