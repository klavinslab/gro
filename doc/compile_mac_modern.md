# Building gro on modern macOS (CLI, Qt 5.15, arm64)

This supersedes `compile_mac.md` (which targets Qt 5.2/5.3 + Qt Creator).
Tested on macOS 14 (Sonoma), Apple Silicon, Qt 5.15.18.

## 1. Dependencies (Homebrew)

```bash
brew install qt@5 bison flex m4 pkg-config
```

All four are keg-only. Put them on PATH **and** export `M4`:

```bash
export M4=/opt/homebrew/opt/m4/bin/m4
export PATH="/opt/homebrew/opt/qt@5/bin:/opt/homebrew/opt/bison/bin:/opt/homebrew/opt/flex/bin:/opt/homebrew/opt/m4/bin:$PATH"
```

`M4` is the non-obvious one. `/usr/bin/m4` (GNU M4 1.4.6, from 2007) is too old
for flex 2.6.4 — flex fails **silently** (exit 1, zero output, empty stderr).

## 2. Directory layout

This tree expects three sibling source dirs and three sibling build dirs:

```
gro_project/
  ccl/                   (source)
  chipmunk/              (symlink → Chipmunk-5.3.5)
  gro/                   (source)
  build-ccl/             (build)
  build-chipmunk/        (build)
  build-gro/             (build)
```

One-time setup (chipmunk needs a hand-written .pro shipped in gro/useful/):

```bash
cd gro_project
ln -s Chipmunk-5.3.5 chipmunk
cp gro/useful/chipmunk.pro chipmunk/chipmunk.pro
```

## 3. Build

```bash
cd gro_project
for d in chipmunk ccl gro; do
  mkdir -p build-$d && (cd build-$d && qmake ../$d/$d.pro && make -j8)
done
open build-gro/gro.app
```

## 4. Run

`gro.app` must be able to find `include/gro.gro` relative to cwd. `main.cpp`
chdirs to `applicationDirPath()/../../..` (i.e. `build-gro/`) at startup, and
the qmake `makelinks` target creates `build-gro/{examples,include}` symlinks
into `../gro/`. As long as you launch the bundle directly, that just works.

## Landmines fixed during the upgrade (now baked into the source)

- `gro.pro` macx LIBS was `-lchip` in `../chipmunk/src` (wrong on both counts).
  Now: `-lchipmunk` in `../build-chipmunk`.
- `gro.pro` `makelinks` target made self-referential symlinks
  (`ln -s examples examples`). Now uses `$$PWD/examples`.
- `ccl.pro` had `QT -= core x86_64` (`x86_64` was never a Qt module).
- `-fast` removed from qmake flags (Apple GCC-ism; clang chokes).
- Stale auto-generated `ui_gui.h` (uic 5.0.2) was checked in and listed in
  HEADERS — deleted, regenerated from `gui.ui` into the build dir.
- Qt 5.15 stopped pulling `QPainterPath` from `<QtGui>` umbrella header —
  explicit `#include <QPainterPath>` added in `World.cpp` and `EColi.cpp`.
- `Gui::about()` did `chdir("../../..")` and was called from the `Gui`
  constructor, undoing the main-chdir before the first parse. Removed the
  chdir; the print-cwd debug line was kept.
