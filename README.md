gro
===

The gro bacterial micro-colony growth specification and simulation software.

Documentation
===

http://depts.washington.edu/soslab/gro/docview.html.

Building
===

gro builds against **Qt 6** and **CMake 3.24+**. Dependencies (`ccl`,
`Chipmunk2D`) are fetched automatically — no manual setup required.

macOS
---

```bash
brew install qt cmake bison flex m4

git clone https://github.com/klavinslab/gro.git
cmake -S gro -B build \
    -DCMAKE_PREFIX_PATH=$(brew --prefix qtbase)
cmake --build build -j

open build/gro.app
```

The `bison`, `flex`, and `m4` formulas are needed to build the bundled
parser. `cmake` resolves them automatically once they are installed; you
don't need to put them on `PATH` manually.

The first `cmake -S ... -B ...` invocation clones `ccl` and `Chipmunk2D`
into `build/_deps/`. Subsequent configures are cached.

To build a distributable DMG on macOS:

```bash
cmake --build build --target package
# → build/gro-<version>-mac.dmg
```

Linux
---

```bash
sudo apt install qt6-base-dev cmake bison flex
git clone https://github.com/klavinslab/gro.git
cmake -S gro -B build
cmake --build build -j
./build/gro
```

Notes
---

- Examples live in `examples/`. Open one via `File → Open` in the GUI.
- `main.cpp` `chdir`s to the build directory at startup so the `examples/`
  and `include/` symlinks (created by `CMakeLists.txt` as a post-build
  step) resolve correctly.
