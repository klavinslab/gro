# gro — the CCL Reference

The gro bacterial micro-colony growth specification and simulation
software. This document covers the original `.gro` (CCL) frontend.
The Python frontend (`.py`) is documented separately in `python.md`.

Derived from https://depts.washington.edu/soslab/gro/docs.xml.

---

## Getting Started

Welcome! Thank you for your interest in gro. Start with the
installation section, then go through the tutorial. Look carefully
at the examples, and try making small changes to them to see if you
understand how they work.

### Installation

#### macOS

Requirements: macOS 14.7 or later and Apple Silicon.

You will receive gro in a compressed disk image named something like
`gro_1.1.0-mac.dmg`. Double-click to mount it; you should see the
gro program, the folders `examples/`, `include/`, `python/`,
`readme.txt`, and `changelog.txt`.

To start gro, double-click the gro icon. To install permanently,
make a folder called `gro` in your home directory and copy the disk
image's contents into it; then drag the gro icon onto your taskbar.

> Note: keep the `gro` application in the folder with `include/`,
> `examples/`, and `python/` — that's how gro finds its
> configuration files. From v1.1.0 onward, those three folders live
> as siblings of `gro.app`, not inside the bundle.

To edit `.gro` programs, use any text editor (the lab favors VS
Code). Choose Reload from the menu after each change.

#### Windows

You will receive gro in a zip file named something like
`gro_win_beta.4.zip`. Save it on your Desktop, right-click, and
choose Extract All… A folder containing the gro program plus
`examples/`, `include/`, `python/`, `readme.txt`, and
`changelog.txt` appears.

Double-click the gro icon to start. To edit `.gro` programs, use a
UNIX-friendly editor — **Notepad and Write will not work** because
of their text encoding. [Notepad++](http://notepad-plus-plus.org/)
is a good free option.

### Tutorial

This section walks through gro's features by example. Studying
these should teach you most of what you need to write your own gro
programs.

#### Growth

The simplest gro program:

```gro
include gro

set ( "dt", 0.1 );   // fast and inaccurate

program p() := {

  skip();

};

ecoli ( [ x := 0, y := 0 ], program p() );
```

This is `examples/growth.gro`. Open it via Examples menu.

- Line 1 includes the gro standard library.
- Line 3 sets the simulation time step `dt`. Larger = faster but
  less accurate.
- Lines 5–9 declare an empty program `p()`.
- Line 11 places an *E. coli* cell at `(0, 0)` running `p()`. By
  default, 10 pixels = 1 µm.

What happens under the hood:

- A cell is 1 µm in diameter and initially 2 µm long, so volume
  V = 1.57 fL.
- The cell doubles in volume in ~20 min: `dV/dt = kV` with
  k ≈ 0.034 fL/min. Each tick adds `δkV` with δ drawn from a gamma
  distribution with mean `dt`.
- Cells divide once volume ≈ 3.14 fL (Gaussian; variance ≈ 0.005).
- On division, daughter volume is ~50% of mother volume with
  random noise; one is chosen as the parent, the other the
  daughter. The program is copied; numeric local variables are
  halved.

> Note: simulation time is not real time. As the colony grows the
> simulator does exponentially more work per simulated minute.

##### Controlling growth rate and division volume

```gro
program p() := {

  set ( "ecoli_division_size_mean", 2.0 );     // fL
  set ( "ecoli_division_size_variance", 0.02 ); // fL^2

};
```

The `set` statements run when the first cell is initialized — once.

You can also override gro's division machinery entirely:

```gro
program p() := {

  set ( "ecoli_division_size_mean", 1000 );

  rate 1 & volume > 3.14 : {
    divide()
  }

};
```

- `rate(k)` is a built-in returning true with probability `k*dt` per
  tick. `rate(1)` ≈ "fire ~1× per simulated minute."
- `volume` is a per-tick global injected by gro.
- `divide()` forces division on the next divide check.

##### Chemostat mode

```gro
chemostat ( true );
```

Adds boundary walls reminiscent of Hasty-style flow chambers. At
`dt=0.1` you'll see jitter; drop to `dt=0.01` for smoother motion
(slower simulation).

#### Molecules

##### Dilution

A cell loaded with 1000 copies of GFP, no production, no
degradation. As it grows and divides, GFP gets distributed between
mother and daughter.

```gro
include gro

set ( "dt", 0.1 );

program dilute(m) := {

  gfp := m;

};

ecoli ( [ x := 0, y := 0 ], program dilute(1000) );
```

gro automatically halves numeric locals on division — that's why
GFP dilutes.

To inspect GFP per cell:

```gro
program report() := {

  needs gfp;

  selected : { message ( 1, tostring(id) <> ": " <> tostring (gfp) ) }

};

program p(m) := dilute(m) + report() sharing gfp;

ecoli ( [ x := 0, y := 0 ], program p(1000) );
```

`selected` is true while the user has highlighted the cell. `<>` is
string concatenation. `tostring` converts numeric to string. The
`+` and `sharing` keywords compose the two programs so they share
the `gfp` variable.

##### Production, dilution and degradation

Adding production at rate `k1` and decay at `k2 * gfp`:

```gro
program make_gfp ( k1, k2, m ) := {

  gfp := m;

  rate ( k1 ) :         { gfp := gfp + 1 }
  rate ( k2 * gfp ) :   { gfp := gfp - 1 }

};

alpha := - log ( 0.5 ) / 20.0;   // dilution rate
k1 := 100 * alpha;               // for ~100 copies/cell

ecoli ( [ x := 0, y := 0 ], program make_gfp ( k1, 0.001, 0 ) );
```

Average cell volume is ~2.36 fL, so 100 copies / 2.36 fL ≈ 42 /fL.

To collect per-cell time series, define an `output` program that
prints periodically:

```gro
program output(delta) := {

  needs gfp;
  p := [ t := 0, s := 0 ];

  true : {
    p.t := p.t + dt,
    p.s := p.s + dt
  }

  p.s >= delta : {
    print ( id, ", ", p.t, ", ", gfp / volume, "\n" ),
    p.s := 0
  }

};

program p() := make_gfp ( k1, 0.001, 0 ) + output(50) sharing gfp;
```

> The state `t` and `s` are wrapped in a record `p` so they survive
> division — gro halves numeric *locals* but **not** record fields.

To write directly to a file:

```gro
fp := fopen ( "path" );
...
fprint ( fp, id, ", ", p.t, ", ", gfp / volume, "\n" )
```

#### Dogma

Building on dilution, add mRNA explicitly. From `examples/gfp.gro`:

```gro
include gro

set ( "dt", 0.01 );

alpha_r := 69.4 / 2.35;         // mRNA / min / fL
beta_r := - log ( 0.5 ) / 3.69; // 1/min
alpha_p := 3.0;                 // protein/min/mRNA
beta_p := 0.01;                 // 1/min

program gfp() := {

  mRNA := 0;
  gfp  := 0;

  rate ( alpha_r * volume ) : { mRNA := mRNA + 1 };
  rate ( beta_r * mRNA )    : { mRNA := mRNA - 1 };
  rate ( alpha_p * mRNA )   : { gfp := gfp + 1 };
  rate ( beta_p * gfp )     : { gfp := gfp - 1 };

};

set ( "gfp_saturation_max", 1000 );
set ( "gfp_saturation_min", 800 );

ecoli ( [ x := 0, y := 0 ], program gfp() );
```

Rate choices (rough bionumbers): mRNA production ~9.4/sec for an
average bacterium → `alpha_r * volume`; mRNA half-life ~3.69 min
→ `beta_r`; protein production ~3/min/mRNA; protein decay slow.

Why `dt = 0.01`? With `dt=0.1`, the `rate ( alpha_r * volume )`
guard fires at most 10 times per minute — not enough to keep up
with the desired 69.4/min rate. Smaller `dt` improves the
stochastic approximation.

The `gfp_saturation_*` parameters tweak the green-channel rendering
intensity — like adjusting gamma on a microscope.

#### Signals

gro models diffusion via a 160×160 finite-element grid (0.5 µm per
element). At each tick the concentration in each grid cell is
updated via a stencil that captures diffusion and degradation.

##### A concentration bandpass filter

```gro
include gro

set ( "dt", 0.1 );

ahl := signal ( 1, 0.01 );   // diffusion rate, degradation rate

fun f a . 0.1 < a & a < 0.6;

program sensor() := {

  rfp := 0.0;

  f ( get_signal ( ahl ) ) : { rfp := rfp + 1 };
  rate ( 0.01 * rfp ) :     { rfp := rfp - 1 }

};

set ( "rfp_saturation_max", 50 );
set ( "rfp_saturation_min", 0 );

ecoli ( [ x:= 0, y:= 0 ], program sensor() );

program main() := {

  // hold the concentration at (0,0) at 10
  true : { set_signal ( ahl, 0, 0, 10 ) }

};
```

- `signal(k_diff, k_deg)` declares a new signal and returns an
  integer handle.
- `get_signal(s)` returns the signal concentration at the cell's
  location.
- The `main()` program (when present) runs once per simulator tick
  at world scope. Use it to drive sources, switch inducers, or
  control the simulation.

##### The wave

A leader cell pulses; followers relay; the wave travels.

```gro
include gro

set ( "dt", 0.075 );

ahl := signal ( 1, 1 );

program leader() := {

  p := [ t := 2.4 ];
  set ( "ecoli_growth_rate", 0.00 );

  true : { p.t := p.t + dt }

  p.t > 10 : {
    emit_signal ( ahl, 100 ),
    p.t := 0
  }

};

program follower() := {

  p := [ mode := 0, t := 0 ];
  set ( "ecoli_growth_rate", 0.04 );

  p.mode = 0 & get_signal ( ahl ) > 0.01 : {
    emit_signal ( ahl, 100 ),
    p.mode := 1,
    p.t := 0
  }

  p.mode = 1 : { p.t := p.t + dt }

  p.mode = 1 & p.t > 9 : { p.mode := 0 }

};

ecoli ( [ x:= 0, y:= 0 ], program leader() );
ecoli ( [ x:= 0, y:= 10 ], program follower() );
```

The leader's growth rate is 0 so it doesn't divide and seed more
wave initiators. Followers have two modes: waiting (0) and
refractory (1).

#### Evolution

Mutate a parameter across divisions using the `daughter` variable
(true on the daughter cell in the tick after division — exactly
one of the two halves).

```gro
include gro

chemostat ( true );
set ( "dt", 0.075 );

nutrient := 1;
kinit := 0.25;
dk := 0.05;

fun cost e n . 0.2 * e * n / ( 50.0 + n );
fun benefit e n . 0.002 * e / ( 1.0 - 0.01 * e );
fun fitness e n . cost e n - benefit e n;

program evolver() := {

  p := [ k := kinit ];
  E := 25;
  t := 0;

  rate ( p.k * volume ) : { E := E + 1 }
  rate ( 0.05 * E )     : { E := E - 1 }

  true : { set ( "ecoli_growth_rate", 0.001 + fitness E nutrient ), t := t + dt }

  daughter : {
    p.k := p.k + dk * ( rand ( 1000 ) - 500 ) / 1000.0
  }

};

ecoli ( [ x := 0, y := 0 ], program evolver() );
```

`rand(N)` returns a non-negative random integer less than `N`.
Centering `(rand(1000) - 500) / 1000.0` gives a uniform mutation
in `[-0.5, 0.5)`.

> Note: both parent and daughter could mutate after division. Use
> `just_divided` instead of `daughter` to mutate both halves.

#### Global Control

Switch a global concentration on/off via a `main()` program:

```gro
include gro

iptg := 0;

program p() := {

  gfp := 0;

  rate ( 1 + 10 * iptg / ( 1 + iptg ) ) : { gfp := gfp + 1 };
  rate ( 0.001 * gfp ) :                  { gfp := gfp - 1 };

};

program main() := {

  t := 0;

  true : { t := t + dt };

  t > 50 : {
    t := 0,
    iptg := 1.0 - iptg,
    clear_messages(1),
    message ( 1, "IPTG at " <> tostring(iptg) <> "uM/L" )
  };

};

ecoli ( [], program p() );
```

##### Simulation Control

Use `main()` to script entire campaigns of simulations:

```gro
include gro

num_cells := 1;

program p() := {
  daughter : { num_cells := num_cells + 1 }
};

program main() := {

  mode := 0;
  t := 0;
  n := 0;

  mode = 0 & n < 50 : {
    ecoli ( [], program p() ),
    num_cells := 1,
    mode := 1
  }

  mode = 0 & n = 50 : { exit() }

  mode = 1 & t > 150 : {
    print ( "simulation ended with ", num_cells, " cells\n" ),
    reset(),
    t := 0,
    n := n + 1,
    mode := 0
  }

  true : { t := t + dt }

}
```

`reset()` erases all cells and resets simulator time. `exit()`
quits gro entirely.

---

## The Details

### Expressions

Values are atomic expressions. Below are the larger expressions you
build from them.

#### Values

- `boolean` — `true`, `false`. **Not** 0 / 1.
- `integer` — `1`, `-2`, `0`.
- `real` — `3.14159`, `-0.123` (double precision).
- `string` — `"abcdefg\n"`.
- `list` — homogeneous, written `{ 1, 2, 3 }` or `{ {1,2}, {3} }`.
- `record` — `[ x := 5, y := {}, z := "I love gro!" ]`.
- `lambda` — `lambda x . x+1` or `\ x . x+1`.
- `unit` (written `.`) — return type for external functions that
  don't return values.

#### Logic

```gro
true & false;
! true;
true | false;
```

Note `&` and `|` (single chars), not `&&` / `||`.

Comparisons (mix ints + reals freely):

```gro
( 1.0 < 2 ) | ( 3.0 >= -7 & 3.0 > 5.0 ) | false;
```

Equality and inequality work on booleans, ints, reals, strings, and
lists:

```gro
1 = 2 | "a" != "b" | {1,2,3} = {4,5};
```

> **`=` is comparison.** Assignment uses `:=`.

#### Arithmetic

`+ - * / % ^` with standard precedence. If both operands are int,
result is int; otherwise real.

```gro
( 1 + 1 ) ^ 5 + 7 % 3;        // = 33
( 2.0 ^ 3 ) * ( 5.0 - 1.23E-3 ) - 1;   // = 38.990160
```

Division by zero is a runtime error.

#### Strings

Concatenation with `<>`:

```gro
( "abc" <> "def" ) = "abcdef";   // true
```

#### Lists

Cons / concat:

```gro
1 @ { 2, 3 };       // {1,2,3}
{ 0, 1 } # { 2, 3 }; // {0,1,2,3}
```

Head / tail:

```gro
head { 1, 2, 3 };   // 1
tail { 1, 2, 3 };   // {2,3}
```

Indexing:

```gro
L := { "a", "b", "c" };
L[0];           // "a"
L[0] = "d";     // updates L in place
```

#### Records

Field access via `.`:

```gro
r := [ x := 0, y := 1 ];
r.x;            // 0
```

Record update via `<<`:

```gro
[ x:= 0, y := 1 ] << [ x := -1 ];   // [ x := -1, y := 1 ]
```

In `r << s`, fields in `s` override fields in `r`; types must
agree.

#### Lambda expressions

```gro
lambda x . -x;                // negate
lambda x . lambda y . x = y;  // curried compare
```

Apply by juxtaposition:

```gro
( lambda x . -x ) 10;                            // -10
( lambda x . lambda y . x = y ) 10 11;           // false
```

gro's type inferrer reports types like `'a -> 'a -> boolean` for
polymorphic functions.

Shorthand: `\ x . x - 1` for `lambda x . x - 1`.

#### Functions

Named, possibly recursive:

```gro
fun fact n .
  if n <= 0
    then 1
    else n * fact (n-1)
  end;
```

Apply by juxtaposition:

```gro
fact 5;   // 120
f a b     // apply f to a then to b
f (a+1) (b+2)   // parenthesize expressions
```

**Internal functions** (C++-backed, e.g. `sin`, `print`) use C
syntax with comma-separated args in parens:

```gro
x := sin ( 2.3 );
print ( x, "\n" );
```

#### Conditionals

```gro
if 1 > 0 then 1 else 0 end;
```

`then` and `else` branches must have the same type; the `if`
expression must be boolean.

#### Let expressions

```gro
let x := 10, y := x/2 in
  x + y
end;          // 15
```

#### Foreach expressions

`foreach` is **functional**, not procedural — it returns a list.

```gro
foreach x in range 100 do
  print ( x )
end;

foreach p in cross (range 5) (range 5) do
  p[0]*p[1]
end;
```

#### Type errors

gro's type checker catches bad expressions at load time:

```
Type error in 'example.gro' on line 1:
  could not compare (= or !=) arguments
  1 has type integer, while
  "one" has type string
```

#### Programs

A program is a set of initializers and guarded commands inside
curly brackets:

```gro
program p(x) := {

  t := 0;

  true   : { t := t + dt }
  t >= x : { t := 0      }

};
```

Initializers like `t := 0` run once at cell creation — **not** on
division.

Guarded commands run every tick: if the guard evaluates true, the
body executes.

> **Guarded commands cannot be nested.** Use `&` to combine
> conditions on the guard side.

##### Composition

```gro
program r(x) := p(x+1) + p(x+2);
```

`r` is the union of `p(x+1)`'s rules and `p(x+2)`'s rules.
Variables are **local to each part** — `r` has two copies of `t`.

`compose` over a list:

```gro
program s() := compose x in {100, 200, 300, 400} : p(x);
```

##### Sharing and scope

To share a variable across composed parts:

```gro
program g() := {
  needs t;
  true : { print ( t, "\n" ) }
};

program h(x) := p(x) + g() sharing t;
```

`needs` declares a dependency; `sharing` resolves it. Sharing
applies per `+`, not across a chain:

```gro
program f() := g1()
             + g2() sharing w
             + g3() sharing w;
```

### gro Control

#### E. coli

```gro
ecoli ( [ x := 10, y := 10, theta := 1.57 ], program p() )
```

`theta` is the initial orientation in radians. Any of `x`, `y`,
`theta` may be omitted; defaults are 0.

You can call `ecoli` many times:

```gro
foreach q in range 100 do
  ecoli ( [
      x := rand(600)-300,
      y := rand(600)-300,
      theta := 0.01*rand(314) ],
    program p ( rand ( 100 ) ) )
end;
```

#### Variables (cell built-ins)

Available inside programs (top-level scope; don't shadow them):

- `dt` — simulated time step in minutes. Also valid in `main()`.
- `volume` — cell volume in fL.
- `just_divided` — true for one tick on both halves of a division.
- `daughter` — true for one tick on the daughter (one specific
  half of the pair).
- `selected` — true while the user has the cell highlighted.
- `id` — unique integer per cell.

Example:

```gro
include gro

program report() := {

  p := [ t := 0 ];

  true : { p.t := p.t + dt }

  daughter : {
    print ( p.t, " minutes: A new cell (",
            id, ") with volume ", volume, " fL\n" )
  }

};

ecoli ( [ x := 0, y := 0 ], program report() );
```

#### Reporters

`gfp`, `rfp`, `cfp`, `yfp` are interpreted as fluorescent-protein
copy counts. They must be at the top level of a program
composition.

Rendering intensity is

```
( gfp/volume - gfp_saturation_min ) / ( gfp_saturation_max - gfp_saturation_min )
```

clamped to [0, 1]. Tune via `gfp_saturation_min`, `gfp_saturation_max`
(and `rfp_`, `yfp_`, `cfp_` analogues). For a fixed-color cell:

```gro
true : { gfp := volume * 100 }
```

#### Parameters

```gro
set ( "name", value );
```

If called outside a program, sets the world-default value. If
called inside a program (cell context), sets the cell-local value
only — letting different cells run with different parameters.

Common parameters:

- `"dt"` — simulation time step (min). Smaller = better quality,
  slower.
- `"ecoli_growth_rate"` — fL/min.
- `"ecoli_division_size_mean"` — fL.
- `"ecoli_division_size_variance"` — fL².
- `"gfp_saturation_min"` / `"gfp_saturation_max"` (and rfp / yfp /
  cfp).

Defaults are in `include/gro.gro`.

#### Signals

Each signal lives on a finite-element grid; per-tick update:

```
Δc_{i,j} = - 6 kdiff c_{i,j} - kdeg c_{i,j}
          + kdiff * (sum of 8 neighbors with corner-weights 0.5)
```

##### Declaration

```gro
s := signal ( k_diff, k_deg );
```

Returns an integer handle.

##### Setting / sourcing

```gro
set_signal ( s, x, y, c );      // world coords; temporary
```

To hold a constant source, re-set every tick from `main()`:

```gro
program main() := {
  true : { set_signal ( ahl, 0, 0, 10 ) }
};
```

##### Emitting / sensing / absorbing

From inside a cell program:

```gro
emit_signal ( s, amount );     // at the cell's location
get_signal ( s );               // local concentration
absorb_signal ( s, amount );    // negative emit; clamps at 0
```

A useful pattern — leader cell emits, follower cells die without
the signal:

```gro
include gro

set ( "dt", 0.2 );

UNDEC := 0;
LEADER := 1;
FOLLOWER := 2;

s := signal ( 1, 0.25 );

program skin() := {

  p := [ m := UNDEC, t := 0 ];
  gfp := 0;
  rfp := 0;

  p.m = UNDEC & just_divided & !daughter : { p.m := LEADER }
  p.m = UNDEC & daughter :                 { p.m := FOLLOWER }

  p.m = LEADER : {
    set ( "ecoli_growth_rate", 0 ),
    emit_signal ( s, 100 ),
    gfp := 100*volume
  }

  p.m = FOLLOWER : {
    rfp := 50*volume / ( 1 + get_signal(s) )
  }

  p.m = FOLLOWER & get_signal ( s ) < 0.01 & p.t > 50 : {
    die()
  }

  true : { p.t := p.t + dt }

};

ecoli ( [], program skin() );
```

(See `examples/skin.gro`.)

> If the diffusion rate is set too high, the Euler integration
> becomes unstable and produces obvious ring artifacts. Reduce
> `dt`, the rate, or the grid extent.

#### Messages

`message(channel, text)` prints a string on one of four on-screen
quadrants (channels 0–2 typically). `clear_messages(channel)`
clears one channel. Common idiom:

```gro
selected : { message ( 1, "id=" <> tostring(id) ) }
```

#### Collecting Data

The CCL way: `print` to stdout, redirect to a file.

```gro
include gro

t := 0;

program p() := {
  just_divided : {
    print ( id, ", ", t, ", ", volume, "\n" );
  }
};

program main() := {
  true : { t := t + dt }
};

ecoli ( [], program p() );
```

```
gro division.gro > division.csv
```

For population statistics, use `maptocells`:

```gro
include gro

fun statistics L .
  let n := length(L), mu := ( sumlist L ) / n in
    [
      num := n,
      mean := mu,
      std := sqrt ( sumlist ( map ( \ x . (x-mu)^2 ) L ) / n )
    ]
  end;

program p() := { gfp := 1000; };

program main() := {

  t := 0;
  s := 0;
  L := {};
  stats := [ num := 0, mean := 0, std := 0 ];

  s >= 1 : {
    s := 0,
    L := maptocells gfp/volume end,
    stats := statistics L ,
    print ( t, ", ", stats.num, ", ", stats.mean, ", ", stats.std, "\n" )
  }

  true : {
    t := t + dt,
    s := s + dt
  }

};

ecoli ( [], program p() );
```

`maptocells expr end` evaluates `expr` in every cell's context and
returns a list.

#### Movies

```gro
program movie ( T, path ) := {

  t := 0;
  n := 0;

  true : { t := t + dt }

  t > T : {
    snapshot ( path <> tostring(n) <> ".tif" ),
    n := n + 1,
    t := 0
  }

};
```

`snapshot(path)` writes the current scene to an image file.

#### Command Line

```
gro example.gro 10 20
```

The argc/argv equivalent: globals `ARGC` (integer) and `ARGV` (list
of strings; `ARGV[0]` is `".gro"`, `ARGV[1]` is the program name).

```gro
print ( "ARGC = ", ARGC, " and ARGV = ", ARGV, "\n" );

if ( ARGC = 4 )
  then ecoli ( [ x := atof ( ARGV[2] ), y := atof ( ARGV[3] ) ], program p() )
  else ecoli ( [], program p() )
end;
```

### Execution

When you load a `.gro` file, gro executes the following phases.

#### 1. Parsing

The file is parsed and type-checked; included files are also
processed. Parse errors go to stdout — fix them and Reload.

#### 2. Global Initialization

Statements outside any program (variable declarations, signal
declarations, function definitions, top-level `print`, top-level
`ecoli`) run in order. A program can be written as a pure top-level
script with no cells:

```gro
include gro

print ( "hello world\n" );
exit();
```

#### 3. Program Initialization

After parsing, programs initialize. `main()`'s initializers run.
Then each `ecoli(...)` call's program initializes for that cell.

#### 4. The Simulation Loop

Per tick, in order:

1. `main()`'s guarded commands evaluate (in order) and fire bodies
   if true.
2. Each live cell updates (unspecified order): volume integration,
   built-in variables refreshed, guarded commands evaluated.
3. Division checks fire (size-based).
4. Physics integrate: signal diffusion + decay, cell motion.

### Standard Library

Defined in `include/standard.gro`. Most are C++-backed (comma-
separated args in parens):

- `print(x, y, ...)` — write any number of args to stdout.
- `skip()` — no-op; useful as a program body placeholder.
- `exit()` — quit gro.
- `atoi(str)` / `atof(str)` — like the C functions.
- `tostring(x)` — any-type → string.
- `length(L)` — list length.
- `sin(x)`, `cos(x)`, `tan(x)`, `log(x)`, `sqrt(x)`, … — usual math.
- `rand(x)` — non-negative random integer below `x`.

List utilities (most are CCL-defined, not internal):

- `rev L` — reverse.
- `zip A B` — `{{a1,b1}, {a2,b2}, …}`.
- `makelist n default` — list of `n` copies.
- `sumlist L` — sum of numeric list.
- `table f n m` — `map f {n, n+1, …, m}`.
- `member x L` — boolean.
- `remove x L` — drop `x` from L.
- `cross A B` — Cartesian product.
- `range n` — `{0, 1, …, n-1}`.
- `tocol L` — `{ {x1}, {x2}, … }`.
- `replace L i x` — `L` with `x` at index `i` (no bounds check).

---

## FAQ

### The Language

**Can guarded commands be nested?**

No. They look like if-then but the language doesn't support
nesting. This is invalid:

```gro
condition a : {
  condition b : { statement ab }
  condition c : { statement ac }
}
```

Instead:

```gro
condition a & condition b : { statement ab }
condition a & condition c : { statement ac }
```

Guarded commands also can't go inside `let` or `foreach`.

**Why won't my `foreach` work?**

`foreach` is functional, not procedural — it returns a list. This
works:

```gro
s := foreach x in range 10 do
  signal(1,1)
end;
```

This does not, because assignment doesn't return a value:

```gro
foreach x in range 10 do
  s[i] := signal(1,1)
end;
```

### Simulation

**Why do signals sometimes make strange concentric ring patterns?**

The reaction-diffusion PDEs are discretized for simulation. If the
diffusion rate, degradation rate, or step size is too large, the
numerical scheme becomes unstable. Reduce the rates, reduce `dt`,
or reduce the signal grid extent via `signal_grid_width`,
`signal_grid_height`, `signal_element_size` (all set with the
`set` command).

### Development

**What is gro programmed in?**

C++ with [Qt](https://www.qt.io/) for the GUI and threading. The
CCL language is tokenized with lex / bison-yacc. 2D physics is
[Chipmunk2D](https://chipmunk-physics.net/). Python integration
(v1.1.0+) uses [pybind11](https://pybind11.readthedocs.io/).

**Can I get the source?**

Yes — gro is open source. See the GitHub repository.
