"""Program base class, State schema, and rule decorators.

This file is the Python equivalent of CCL's `program p() := {...}`
syntax. Every gro `.py` program defines one or more subclasses of
`Program`, attaches state via `State(...)`, and declares rules with
the `@when` / `@always` / `@rate` decorators. The simulator calls
`Program._tick()` on each cell each simulation tick, which fires
every rule whose predicate is true.
"""

from __future__ import annotations

import ast
import copy as _copy
import inspect
import textwrap
from types import SimpleNamespace

import _core

# Reporter indices, sourced from the C++ side so we can't drift away
# from src/Defines.h's GFP/RFP/YFP/CFP macros if those are ever
# renumbered.
_REP_GFP = _core.REP_GFP
_REP_RFP = _core.REP_RFP
_REP_YFP = _core.REP_YFP
_REP_CFP = _core.REP_CFP


class GroLoadError(Exception):
    """Raised when a gro Python program is structurally invalid."""


# ----------------------------------------------------------------------------
# State schema
# ----------------------------------------------------------------------------

class _Preserved:
    """Wrapper marker: field's value is preserved on cell division."""
    __slots__ = ("value",)
    def __init__(self, value): self.value = value


def Preserved(value):
    """`state = State(t=Preserved(2.4))` — don't halve this field on
    cell division. Use for state that represents something other than
    a molecular count (mode flags, timers, etc.). Numeric fields are
    halved on division by default."""
    return _Preserved(value)


class _StateFactory:
    """Result of `State(...)`. Building a Program subclass stashes it
    on the class; each cell instance builds its own state namespace
    from it.
    """

    __slots__ = ("_defaults", "_preserved")

    def __init__(self, defaults):
        self._defaults = {}
        self._preserved = set()
        for name, default in defaults.items():
            if isinstance(default, _Preserved):
                self._defaults[name] = default.value
                self._preserved.add(name)
            elif isinstance(default, (list, dict, set)):
                raise TypeError(
                    f"State field '{name}' has a mutable literal "
                    f"default ({type(default).__name__}). Each cell "
                    f"would share the same object. Use field({type(default).__name__}) "
                    f"instead: e.g. {name}=field({type(default).__name__})."
                )
            else:
                self._defaults[name] = default

    def make(self):
        """Build a fresh state namespace for one cell."""
        ns = SimpleNamespace()
        for name, default in self._defaults.items():
            value = default() if isinstance(default, _Field) else _copy.copy(default)
            setattr(ns, name, value)
        return ns

    @classmethod
    def _from_validated(cls, defaults, preserved):
        """Build a factory from already-validated defaults+preserved
        sets. Skips __init__ (which would re-run the mutable-literal
        check); used by compose() where each per-part factory has
        already been validated."""
        f = cls.__new__(cls)
        f._defaults = defaults
        f._preserved = preserved
        return f

    @property
    def field_names(self):
        return list(self._defaults.keys())

    @property
    def preserved_names(self):
        return self._preserved


class _Field:
    """`field(factory)` — defer construction of mutable defaults until
    each cell is built so they don't share state.
    """
    __slots__ = ("factory",)
    def __init__(self, factory):
        if not callable(factory):
            raise TypeError("field(x): x must be callable, e.g. field(list)")
        self.factory = factory
    def __call__(self):
        return self.factory()


def State(**defaults):
    """Declare a Program's per-cell state schema.

    Usage:

        state = State(t=2.4, mode=0, history=field(list))

    Numeric fields are halved on cell division (milestone 4+).
    Non-numeric fields are deep-copied. Literal mutable defaults
    (`list = []`, etc.) are rejected; use `field(list)` instead.
    """
    return _StateFactory(defaults)


def field(factory):
    """`field(list)` / `field(dict)` / `field(lambda: MyClass(...))` —
    factory for State fields whose default needs to be constructed
    per cell (mutable types).
    """
    return _Field(factory)


# ----------------------------------------------------------------------------
# Rule decorators
# ----------------------------------------------------------------------------

def _validate_predicate(predicate):
    """Best-effort check that an @when predicate doesn't mutate state.

    Python lambdas can't syntactically contain `=` or `+=`, so the
    only in-lambda mutation is the walrus operator (`:=`). We reject
    that at class-creation time so it fails loudly instead of silently
    changing state during simulation. A fuller call-allowlist sandbox
    (rejecting `self.emit_signal(...)`, etc., in predicates) is
    deferred to a follow-up; today the rule is "no walrus and no
    nested lambda" plus the runtime promise that a careless predicate
    will at worst slow the simulator, not corrupt other cells' state.
    """
    if not callable(predicate):
        return
    try:
        src = textwrap.dedent(inspect.getsource(predicate))
    except (OSError, TypeError):
        return  # dynamic / no source — skip silently
    src = src.strip()
    # The source is often the `@when(lambda self: …)` decorator line.
    # ast.parse needs balanced trailing punctuation; trim until it
    # accepts.
    parsed = None
    for trim in range(6):
        try:
            parsed = ast.parse(src[: len(src) - trim] if trim else src,
                               mode="exec")
            break
        except SyntaxError:
            continue
    if parsed is None:
        return
    lambda_node = next(
        (n for n in ast.walk(parsed) if isinstance(n, ast.Lambda)), None
    )
    if lambda_node is None:
        return
    for n in ast.walk(lambda_node.body):
        if isinstance(n, ast.NamedExpr):
            raise GroLoadError(
                "@when predicate must be a pure expression — found a "
                "walrus operator (:=) inside the lambda. Rule guards "
                "may only read state, not assign to it."
            )
        if isinstance(n, ast.Lambda):
            raise GroLoadError(
                "@when predicate may not contain a nested lambda. "
                "Move the helper to a named method or module function."
            )


def when(predicate):
    """`@when(lambda self: <expr>)` — fire when the predicate is true.

    Reads line-for-line as CCL's `predicate : { ... }`.
    """
    if not callable(predicate):
        raise TypeError(
            "@when(pred): pred must be a callable taking (self). "
            "Did you write @when(self.state.t > 10) instead of "
            "@when(lambda self: self.state.t > 10)?"
        )
    _validate_predicate(predicate)
    def decorate(fn):
        fn._gro_rule = ("when", predicate)
        return fn
    return decorate


def always(fn):
    """`@always` — fire every tick. Sugar for `@when(lambda self: True)`."""
    fn._gro_rule = ("always", lambda self: True)
    return fn


def rate(k):
    """`@rate(k)` — fire probabilistically with rate `k` per
    simulated time unit. Sugar for
    `@when(lambda self: rand(100000) < k * dt() * 100000)`.
    """
    def predicate(_self):
        return _core.rand(100000) < k * _core.dt() * 100000
    def decorate(fn):
        fn._gro_rule = ("rate", predicate)
        return fn
    return decorate


# ----------------------------------------------------------------------------
# Program base class
# ----------------------------------------------------------------------------

class _ProgramMeta(type):
    """Walks decorated methods at class definition and builds the
    rule list `_gro_rules` that `_tick` iterates each step. Also
    enforces strict-mode invariants on the class shape.
    """
    def __new__(mcs, name, bases, ns):
        cls = super().__new__(mcs, name, bases, ns)
        # Strict-mode class shape check: if `state` is set on the
        # subclass, it must be a State(...) factory (catches the
        # "state = SimpleNamespace(t=0)" / "state = {...}" mistake).
        # The base Program class itself doesn't have state, so skip.
        if "state" in ns and not isinstance(ns["state"], _StateFactory):
            raise GroLoadError(
                f"Program subclass {name!r}: `state` must be assigned "
                f"State(...), got {type(ns['state']).__name__}."
            )
        rules = []
        # Preserve declaration order: Python 3.7+ guarantees
        # __dict__ insertion order.
        for method_name, attr in cls.__dict__.items():
            rule_info = getattr(attr, "_gro_rule", None)
            if rule_info is None:
                continue
            kind, predicate = rule_info
            rules.append((method_name, kind, predicate, attr))
        # If the subclass declared no rules of its own, inherit the
        # parent's. Lets `class Tagged(Pulser): pass` and the result
        # of `Pulser.with_args(...)` automatically pick up their
        # parent's rules without a manual re-link.
        if not rules and bases:
            for base in bases:
                base_rules = getattr(base, "_gro_rules", None)
                if base_rules:
                    rules = list(base_rules)
                    break
        cls._gro_rules = rules
        # Capture the State factory's preservation set so _split can
        # consult it without re-introspecting.
        factory = ns.get("state")
        cls._gro_preserved = (
            factory.preserved_names if isinstance(factory, _StateFactory) else set()
        )
        return cls


class Program(metaclass=_ProgramMeta):
    """Base class for Python gro programs.

    Subclasses declare `state = State(...)`, override `setup()` for
    one-time per-cell initialization, and decorate methods with
    `@when` / `@always` / `@rate` to register them as rules.
    """

    def __init__(self):
        factory = getattr(type(self), "state", None)
        if isinstance(factory, _StateFactory):
            self.state = factory.make()
        else:
            self.state = SimpleNamespace()

    def setup(self):
        """Override to run once per cell at spawn time."""
        pass

    @classmethod
    def with_args(cls, **overrides):
        """Return a thin subclass with the given fields' defaults
        overridden in the new class's `state`. Equivalent to CCL's
        `Pulser(period=3)` where the program takes parameters.

        Wrap an override in `Preserved(...)` to mark it preserved
        across division; otherwise the original Preserved status is
        retained.
        """
        if not isinstance(cls.state, _StateFactory):
            raise GroLoadError(
                f"{cls.__name__}.with_args(): {cls.__name__} has no "
                f"State(...) to override")
        new_defaults = dict(cls.state._defaults)
        new_preserved = set(cls.state._preserved)
        for name, value in overrides.items():
            if name not in new_defaults:
                raise GroLoadError(
                    f"{cls.__name__}.with_args(): unknown field "
                    f"'{name}' (declared fields: "
                    f"{sorted(new_defaults)})")
            if isinstance(value, _Preserved):
                new_defaults[name] = value.value
                new_preserved.add(name)
            else:
                new_defaults[name] = value
                # Caller's literal default; don't change Preserved
                # status from what the original class declared.
        new_factory = _StateFactory._from_validated(new_defaults, new_preserved)
        # _ProgramMeta sees no decorated methods on the subclass and
        # inherits the parent's _gro_rules automatically.
        return type(f"{cls.__name__}.with_args", (cls,), {"state": new_factory})

    def _tick(self):
        """Called by the simulator each tick. Evaluates every rule
        predicate; fires the action for every one that is true.
        """
        for _name, _kind, predicate, action in type(self)._gro_rules:
            if predicate(self):
                action(self)

    def _split(self, mother_frac):
        """Called by the simulator on cell division. `mother_frac` is
        the fraction the mother retains; the daughter gets the rest.
        Returns a new Program instance for the daughter cell.

        Default rule (matches CCL): each top-level int/float field
        is split between mother and daughter. Non-numeric fields
        (lists, dicts, …) are deep-copied unchanged. Fields wrapped
        in Preserved(...) at State declaration time skip halving
        entirely.
        """
        new_instance = type(self).__new__(type(self))
        new_instance.state = _copy.deepcopy(self.state)
        daughter_frac = 1.0 - mother_frac
        preserved = type(self)._gro_preserved
        for name, value in vars(self.state).items():
            if name in preserved:
                continue
            # Exact-type match: bool is a subclass of int (we don't
            # want to halve booleans), and avoiding isinstance also
            # skips numpy scalars / complex which need separate thought.
            if type(value) not in (int, float):
                continue
            setattr(self.state,         name, value * mother_frac)
            setattr(new_instance.state, name, value * daughter_frac)
        return new_instance

    # ------------------------------------------------------------------
    # Cell-local API. These forward to _core which reads the
    # current_cell pointer set by PythonMicroProgram::update.
    # ------------------------------------------------------------------

    def die(self):
        """Mark this cell for removal at the end of the current tick.
        The cell finishes the tick (later rules in the same tick still
        run); World sweeps it out before the next tick. Mirrors CCL's
        `die()`."""
        _core.die_cell()

    def emit_signal(self, handle, amount):
        _core.emit_signal_cell(handle, amount)

    def absorb_signal(self, handle, amount):
        _core.absorb_signal_cell(handle, amount)

    def get_signal(self, handle):
        return _core.get_signal_cell(handle)

    @property
    def volume(self):
        return _core.current_volume()

    @property
    def id(self):
        return _core.current_id()

    @property
    def x(self):
        return _core.current_x()

    @property
    def y(self):
        return _core.current_y()

    @property
    def theta(self):
        return _core.current_theta()

    @property
    def just_divided(self):
        """True on exactly one tick after this cell divides (on both
        mother and daughter). Cleared by the simulator at the end of
        that tick. Mirrors CCL's `just_divided` keyword."""
        return _core.current_just_divided()

    @property
    def daughter(self):
        """True for one tick on the daughter side of a division (the
        new cell). False on the mother. Used together with
        `just_divided` to break symmetry between the two halves at
        division time. Mirrors CCL's `daughter` keyword."""
        return _core.current_is_daughter()

    @property
    def gfp(self): return _core.current_get_rep(_REP_GFP)
    @gfp.setter
    def gfp(self, value): _core.current_set_rep(_REP_GFP, int(value))

    @property
    def rfp(self): return _core.current_get_rep(_REP_RFP)
    @rfp.setter
    def rfp(self, value): _core.current_set_rep(_REP_RFP, int(value))

    @property
    def yfp(self): return _core.current_get_rep(_REP_YFP)
    @yfp.setter
    def yfp(self, value): _core.current_set_rep(_REP_YFP, int(value))

    @property
    def cfp(self): return _core.current_get_rep(_REP_CFP)
    @cfp.setter
    def cfp(self, value): _core.current_set_rep(_REP_CFP, int(value))


# ----------------------------------------------------------------------------
# Composition
# ----------------------------------------------------------------------------

class _PartScope:
    """View onto one part's slice of a composite's raw state.

    Shared fields pass through to the raw namespace unchanged; local
    fields are remapped to a part-prefixed key in the raw namespace
    so multiple parts can declare the same local name (e.g. `active`)
    without colliding. CCL gets the same effect by giving each
    sub-program its own SymbolTable.

    `_aliases` is a `local_name -> prefixed_key` dict built once at
    compose() time, so attribute access is one dict.get() + one
    underlying getattr, with no per-tick string concat.
    """
    __slots__ = ("_raw", "_aliases")

    def __init__(self, raw_ns, aliases):
        object.__setattr__(self, "_raw",     raw_ns)
        object.__setattr__(self, "_aliases", aliases)

    def __getattr__(self, name):
        return getattr(self._raw, self._aliases.get(name, name))

    def __setattr__(self, name, value):
        setattr(self._raw, self._aliases.get(name, name), value)


def compose(*parts, share=None):
    """Combine several `Program` subclasses into one composite program.

    The composite has the union of every part's rules (in part-list
    order, then declaration order within a part) and a state namespace
    that contains every field declared by any part:

    - Fields listed in `share=[...]` get a single shared storage that
      every part sees through `self.state.<name>`.
    - All other fields are auto-namespaced per-part, so two parts can
      both declare `active=False` without conflict (CCL achieves the
      same via per-sub-program symbol tables).

    Each part's rules are invoked under a `_PartScope` view that
    rewrites `self.state.<local>` to the prefixed key but leaves
    shared-field accesses unchanged.

    Equivalent to CCL's `p() := q() + r() sharing x, y, ...`.
    """
    share = set(share or ())

    if not parts:
        raise GroLoadError("compose() requires at least one Program part")
    for i, p in enumerate(parts):
        if not (isinstance(p, type) and issubclass(p, Program)):
            raise GroLoadError(
                f"compose() arg #{i}: {p!r} is not a Program subclass")

    declared_anywhere = set()
    for p in parts:
        f = getattr(p, "state", None)
        if isinstance(f, _StateFactory):
            declared_anywhere.update(f._defaults.keys())

    for name in share:
        if name not in declared_anywhere:
            raise GroLoadError(
                f"compose(share=[...]): '{name}' is not declared in "
                f"any part's State()")

    merged_defaults = {}
    merged_preserved = set()
    parts_aliases = []   # parts_aliases[i] = {local_name: prefixed_key}

    for idx, p in enumerate(parts):
        aliases = {}
        f = getattr(p, "state", None)
        if isinstance(f, _StateFactory):
            for name, default in f._defaults.items():
                if name in share:
                    # Right (later parts) takes precedence on shared
                    # defaults — matches CCL's CompositeProgram which
                    # uses the right-hand value when both define it.
                    merged_defaults[name] = default
                    if name in f._preserved:
                        merged_preserved.add(name)
                else:
                    key = f"_p{idx}_{name}"
                    merged_defaults[key] = default
                    aliases[name] = key
                    if name in f._preserved:
                        merged_preserved.add(key)
        parts_aliases.append(aliases)

    # Validate `requires` on each part: every required name must be in
    # `share`. The earlier "share name declared anywhere" check makes
    # this transitively equivalent to DESIGN.md's "declared in some
    # part's State AND listed in share".
    for p in parts:
        for name in getattr(p, "requires", None) or ():
            if name not in share:
                raise GroLoadError(
                    f"compose(): {p.__name__}.requires = [..., {name!r}, ...] "
                    f"but '{name}' is not in share=[...] — required "
                    f"names must be shared so all parts see one storage")

    merged_factory = _StateFactory._from_validated(merged_defaults, merged_preserved)

    # Each rule carries its source part index so _tick can pick the
    # right pre-built scope on dispatch.
    composed_rules = []
    for idx, p in enumerate(parts):
        for rule_tuple in p._gro_rules:
            composed_rules.append((idx, rule_tuple))

    # Build a fresh subclass via the metaclass so it goes through the
    # normal class-creation pipeline (lets a future strict-mode check
    # see composites without special casing). We then write the
    # composite-specific class attributes onto it; we can't put them
    # into the class body because they depend on captured locals.
    class Composite(Program):
        pass

    Composite.state                = merged_factory
    Composite._gro_preserved       = merged_preserved
    Composite._gro_composed_rules  = composed_rules
    Composite._gro_part_aliases    = parts_aliases
    Composite._gro_parts           = parts
    Composite.__name__ = "Composed(" + ",".join(p.__name__ for p in parts) + ")"

    def __init__(self):
        self.state = merged_factory.make()
        # One PartScope per part, reused on every rule fire. Built
        # here (not per-rule in _tick) to avoid ~rule_count * cell_count
        # * tick_rate allocations per second.
        self._scopes = [_PartScope(self.state, parts_aliases[i])
                        for i in range(len(parts))]
    Composite.__init__ = __init__

    def setup(self):
        raw = self.state
        for idx, p in enumerate(parts):
            self.state = self._scopes[idx]
            try:
                p.setup(self)
            finally:
                self.state = raw
    Composite.setup = setup

    def _tick(self):
        raw = self.state
        scopes = self._scopes
        # First: parts' rules under their scoped state views.
        for part_idx, (_name, _kind, predicate, action) in self._gro_composed_rules:
            self.state = scopes[part_idx]
            try:
                if predicate(self):
                    action(self)
            finally:
                self.state = raw
        # Then: any rules added on a subclass of this composite. They
        # run against the raw (merged) state. Rare path; lets the user
        # post-compose tweak the composite if they want.
        for _name, _kind, predicate, action in type(self)._gro_rules:
            if predicate(self):
                action(self)
    Composite._tick = _tick

    def _split(self, mother_frac):
        # Delegate halving to the base implementation; it already
        # handles the merged namespace correctly because every field
        # (shared or namespaced-local) lives directly on the raw
        # SimpleNamespace and _gro_preserved spans both.
        daughter = Program._split(self, mother_frac)
        # Mother's _scopes still point at her (mutated-in-place) raw
        # state — no rebuild needed for her. Daughter has a fresh raw
        # state from deepcopy, so her scopes need to point at it.
        daughter._scopes = [_PartScope(daughter.state, parts_aliases[i])
                            for i in range(len(parts))]
        return daughter
    Composite._split = _split

    return Composite


# ----------------------------------------------------------------------------
# Composed: class-body sugar for compose(...)
# ----------------------------------------------------------------------------

class _ComposedMeta(_ProgramMeta):
    """Metaclass for `class X(Composed): parts = [...]; share = [...]`.

    Runs after `_ProgramMeta.__new__` has built `cls` with its
    `_gro_rules` list, then if the class has a `parts` list,
    delegates to `compose(...)` and copies its result onto `cls` so
    the user gets the same instance behavior as a `compose(...)`
    return.
    """

    def __new__(mcs, name, bases, ns):
        cls = super().__new__(mcs, name, bases, ns)
        # `Composed` itself is the marker base; nothing to merge for it.
        if name == "Composed":
            return cls
        parts = ns.get("parts", None)
        if parts is None:
            # Subclass of a Composed-defined class that doesn't re-declare
            # parts (e.g. a tagged subclass). Leave alone.
            return cls
        share = list(ns.get("share", []))
        merged = compose(*parts, share=share)
        for attr in (
            "state", "_gro_preserved",
            "_gro_composed_rules", "_gro_part_aliases", "_gro_parts",
            "__init__", "setup", "_tick", "_split",
        ):
            setattr(cls, attr, getattr(merged, attr))
        return cls


class Composed(Program, metaclass=_ComposedMeta):
    """Class-body sugar for `compose(...)`.

    Usage:

        class Wave(Composed):
            parts = [Leader, Follower]
            share = ["t"]

    Equivalent to `Wave = compose(Leader, Follower, share=["t"])`.
    """
    pass


# ----------------------------------------------------------------------------
# WorldProgram: a Program that runs once per simulation tick at the world
# level, not per cell. The Python equivalent of CCL's `program main()`.
# ----------------------------------------------------------------------------

class WorldProgram(Program):
    """Base class for a `main()`-style world-level program.

    Same decorator surface as `Program` (`@when`, `@always`, `@rate`,
    `State(...)`), but rules run once per simulation tick at the world
    level — not per cell. Cell built-ins (`self.volume`, `self.gfp`,
    `self.emit_signal`, etc.) are nonsensical here and will raise if
    invoked; use `set_signal(...)`, `get_signal_at(...)`, `set_param`,
    and the module-level `reset()` / `ecoli(...)` from world rules.
    """
    pass


def set_main(cls_or_instance):
    """Install a Program as the world's per-tick main program.
    Convention is to subclass `WorldProgram` for clarity, but any
    `Program` subclass works. Accepts either the class (built once)
    or a pre-constructed instance, and returns the live instance so
    the caller can keep a handle for introspection.

    Equivalent to CCL's `program main() := { ... }` declaration.
    """
    if isinstance(cls_or_instance, type) and issubclass(cls_or_instance, Program):
        instance = cls_or_instance()
    else:
        instance = cls_or_instance
    _core.set_main_program(instance)
    return instance


def reset():
    """Restart the world: all cells removed, signal grids zeroed,
    chipmunk space rebuilt. World parameters and signal registrations
    are kept, and the installed `set_main(...)` program continues to
    run on the new world. Equivalent to CCL's `reset()`.
    """
    _core.reset_world()
