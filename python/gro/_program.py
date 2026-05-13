"""Python equivalent of CCL's `program p() := {...}` syntax: the
`Program` base class, the `State` schema, the `@when`/`@always`/
`@rate` rule decorators, and the `compose` / `Composed` composition
machinery. The simulator drives instances through `_tick()` and
`_split()` -- everything else here is class-creation-time setup.
"""

from __future__ import annotations

import ast
import copy as _copy
import inspect
import textwrap
from types import SimpleNamespace

import _core

# Reporter indices re-exported from C++ so we don't drift if
# src/Defines.h ever renumbers GFP/RFP/YFP/CFP.
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
    """Marker: this field is preserved on division (not halved)."""
    __slots__ = ("value",)
    def __init__(self, value): self.value = value


def Preserved(value):
    """`State(t=Preserved(2.4))` -- opt this field out of the default
    numeric-halving on division. Use for flags, timers, anything
    that isn't a molecular count."""
    return _Preserved(value)


class _StateFactory:
    """Built by `State(...)`. Stored on the class; `make()` produces
    a per-cell state namespace from it."""

    __slots__ = ("_defaults", "_preserved")

    def __init__(self, defaults):
        self._defaults = {}
        self._preserved = set()
        for name, default in defaults.items():
            if isinstance(default, _Preserved):
                self._defaults[name] = default.value
                self._preserved.add(name)
            elif isinstance(default, (list, dict, set)):
                # Bare mutable defaults would alias across cells.
                tn = type(default).__name__
                raise TypeError(
                    f"State field '{name}' has a mutable literal "
                    f"default ({tn}). Use field({tn}) instead.")
            else:
                self._defaults[name] = default

    def make(self):
        ns = SimpleNamespace()
        for name, default in self._defaults.items():
            value = default() if isinstance(default, _Field) else _copy.copy(default)
            setattr(ns, name, value)
        return ns

    @classmethod
    def _from_validated(cls, defaults, preserved):
        """Skip __init__'s mutable-literal check; for compose() where
        each per-part factory has already been validated."""
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
    """`field(factory)` -- defer construction so mutable defaults
    aren't shared across cells."""
    __slots__ = ("factory",)
    def __init__(self, factory):
        if not callable(factory):
            raise TypeError("field(x): x must be callable, e.g. field(list)")
        self.factory = factory
    def __call__(self):
        return self.factory()


def State(**defaults):
    """Declare a Program's per-cell state schema. Numeric fields are
    halved on division; wrap a value in `Preserved(...)` to opt out.
    For mutable types use `field(list)` etc. so each cell gets its
    own instance."""
    return _StateFactory(defaults)


def field(factory):
    """Factory wrapper for mutable State defaults: `field(list)`,
    `field(dict)`, `field(lambda: MyClass(args))`."""
    return _Field(factory)


# ----------------------------------------------------------------------------
# Rule decorators
# ----------------------------------------------------------------------------

def _get_ast(obj):
    """Source + parse for a callable, or None if unreachable (REPL,
    dynamically-constructed). The production path (py::eval_file)
    always has source, so silent-skip is acceptable."""
    try:
        src = textwrap.dedent(inspect.getsource(obj)).strip()
    except (OSError, TypeError):
        return None
    try:
        return ast.parse(src, mode="exec")
    except SyntaxError:
        return None


def _flatten_targets(target):
    """Walk Tuple/List/Starred LHS structures so tuple-unpacked
    `self.gfp, self.bogus = ...` writes are checked individually."""
    if isinstance(target, (ast.Tuple, ast.List)):
        for elt in target.elts:
            yield from _flatten_targets(elt)
    elif isinstance(target, ast.Starred):
        yield from _flatten_targets(target.value)
    else:
        yield target


def _validate_predicate(predicate, cls_name, rule_name):
    """Reject walrus, nested lambda, yield, and await inside an @when
    predicate. DESIGN.md's full call-allowlist is deferred -- today's
    predicates legitimately call rand()/dt() to implement rate-style
    guards (CCL's `rate(k)` does the same internally)."""
    if not callable(predicate):
        return
    parsed = _get_ast(predicate)
    if parsed is None:
        return
    lambda_node = next(
        (n for n in ast.walk(parsed) if isinstance(n, ast.Lambda)), None
    )
    if lambda_node is None:
        return
    where = f"{cls_name}.{rule_name}"
    for n in ast.walk(lambda_node.body):
        if isinstance(n, ast.NamedExpr):
            raise GroLoadError(
                f"@when predicate on {where} (line {n.lineno}): "
                f"walrus operator `:=` is forbidden -- predicates may "
                f"only read state.")
        if isinstance(n, ast.Lambda):
            raise GroLoadError(
                f"@when predicate on {where} (line {n.lineno}): "
                f"nested `lambda` forbidden -- move the helper to a "
                f"named method.")
        if isinstance(n, (ast.Yield, ast.YieldFrom)):
            raise GroLoadError(
                f"@when predicate on {where} (line {n.lineno}): "
                f"`yield` forbidden -- predicates aren't generators.")
        if isinstance(n, ast.Await):
            raise GroLoadError(
                f"@when predicate on {where} (line {n.lineno}): "
                f"`await` forbidden -- rules aren't coroutines.")


# Reporters (gfp/rfp/yfp/cfp) live as per-cell counters on the C++
# side, so writes to them bypass `self.state` (see DESIGN.md
# "Reporters"). Underscore-prefixed names are the private-internals
# escape hatch.
_ALLOWED_SELF_WRITES = {"gfp", "rfp", "yfp", "cfp"}


def _validate_method_body(method, cls_name):
    """Reject `self.X = ...` writes where X isn't a reporter or
    underscore-prefixed internal -- the typical typo is `self.foo =
    5` instead of `self.state.foo = 5`. Doesn't catch setattr,
    vars(self)[...], or typos nested in `self.state.X`."""
    tree = _get_ast(method)
    if tree is None:
        return

    for node in ast.walk(tree):
        if isinstance(node, ast.Assign):
            targets = node.targets
        elif isinstance(node, ast.AugAssign):
            targets = [node.target]
        else:
            continue
        for top in targets:
            for tgt in _flatten_targets(top):
                if not (isinstance(tgt, ast.Attribute)
                        and isinstance(tgt.value, ast.Name)
                        and tgt.value.id == "self"):
                    continue
                attr = tgt.attr
                if attr in _ALLOWED_SELF_WRITES or attr.startswith("_"):
                    continue
                raise GroLoadError(
                    f"{cls_name}.{method.__name__} (line {tgt.lineno}): "
                    f"writes to `self.{attr}` -- `self` has no attribute "
                    f"'{attr}'. Did you mean `self.state.{attr}`? "
                    f"Direct writes to `self` are limited to reporter "
                    f"counters ({', '.join(sorted(_ALLOWED_SELF_WRITES))}) "
                    f"and underscore-prefixed internals.")


def when(predicate):
    """`@when(lambda self: <expr>)` -- fire when the predicate is
    true. Mirrors CCL's `predicate : { ... }`."""
    if not callable(predicate):
        raise TypeError(
            "@when(pred): pred must be a callable taking (self). "
            "Did you write @when(self.state.t > 10) instead of "
            "@when(lambda self: self.state.t > 10)?")
    def decorate(fn):
        fn._gro_rule = ("when", predicate)
        return fn
    return decorate


def always(fn):
    """`@always` -- fire every tick. Sugar for `@when(lambda self: True)`."""
    fn._gro_rule = ("always", lambda self: True)
    return fn


def rate(k):
    """`@rate(k)` -- fire probabilistically with rate `k` per simulated
    time unit. Sugar for `@when(lambda self: rand(...) < k*dt()*...)`."""
    def predicate(_self):
        return _core.rand(100000) < k * _core.dt() * 100000
    def decorate(fn):
        fn._gro_rule = ("rate", predicate)
        return fn
    return decorate


# ----------------------------------------------------------------------------
# Program base class
# ----------------------------------------------------------------------------

_BASE_CLASSES = ("Program", "Composed", "WorldProgram")


class _ProgramMeta(type):
    """Builds `_gro_rules` from decorated methods and enforces strict-
    mode invariants on each user subclass."""

    def __new__(mcs, name, bases, ns):
        cls = super().__new__(mcs, name, bases, ns)

        if "state" in ns and not isinstance(ns["state"], _StateFactory):
            raise GroLoadError(
                f"Program subclass {name!r}: `state` must be assigned "
                f"State(...), got {type(ns['state']).__name__}.")

        rules = []
        for method_name, attr in cls.__dict__.items():
            rule_info = getattr(attr, "_gro_rule", None)
            if rule_info is None:
                continue
            kind, predicate = rule_info
            rules.append((method_name, kind, predicate, attr))

        # Inherit parent rules when the subclass adds none of its own.
        # Makes `class Tagged(Pulser): pass` and `Pulser.with_args(...)`
        # work without manual re-linking.
        if not rules and bases:
            for base in bases:
                base_rules = getattr(base, "_gro_rules", None)
                if base_rules:
                    rules = list(base_rules)
                    break
        cls._gro_rules = rules

        # AST checks on user subclasses only -- base classes are
        # framework code.
        if name not in _BASE_CLASSES:
            for rule_name, _kind, predicate, action in rules:
                _validate_predicate(predicate, name, rule_name)
                _validate_method_body(action, name)
            user_setup = ns.get("setup")
            if callable(user_setup):
                _validate_method_body(user_setup, name)

        factory = ns.get("state")
        cls._gro_preserved = (
            factory.preserved_names if isinstance(factory, _StateFactory) else set()
        )
        return cls


class Program(metaclass=_ProgramMeta):
    """Base class for Python gro programs. Subclasses declare
    `state = State(...)`, override `setup()` for one-time per-cell
    init, and decorate methods with `@when`/`@always`/`@rate`."""

    def __init__(self):
        factory = getattr(type(self), "state", None)
        if isinstance(factory, _StateFactory):
            self.state = factory.make()
        else:
            self.state = SimpleNamespace()

    def setup(self):
        """Override to run once per cell at spawn time."""

    @classmethod
    def with_args(cls, **overrides):
        """Return a thin subclass with the given `state` defaults
        overridden. Wrap a value in `Preserved(...)` to mark it
        preserved on division; otherwise the original Preserved
        status is retained."""
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
                    f"'{name}' (declared fields: {sorted(new_defaults)})")
            if isinstance(value, _Preserved):
                new_defaults[name] = value.value
                new_preserved.add(name)
            else:
                new_defaults[name] = value
        new_factory = _StateFactory._from_validated(new_defaults, new_preserved)
        return type(f"{cls.__name__}.with_args", (cls,), {"state": new_factory})

    def _tick(self):
        """Simulator entry: fire every rule whose predicate is true."""
        for _name, _kind, predicate, action in type(self)._gro_rules:
            if predicate(self):
                action(self)

    def _split(self, mother_frac):
        """Simulator entry: build the daughter on division. Top-level
        int/float fields are split mother/daughter; Preserved fields
        and non-numerics are deep-copied unchanged."""
        new_instance = type(self).__new__(type(self))
        new_instance.state = _copy.deepcopy(self.state)
        daughter_frac = 1.0 - mother_frac
        preserved = type(self)._gro_preserved
        for name, value in vars(self.state).items():
            if name in preserved:
                continue
            # Exact-type match: bool subclasses int (we don't halve
            # booleans), and avoiding isinstance skips numpy scalars
            # and complex (separate thought required).
            if type(value) not in (int, float):
                continue
            setattr(self.state,         name, value * mother_frac)
            setattr(new_instance.state, name, value * daughter_frac)
        return new_instance

    # ------------------------------------------------------------------
    # Cell-local API: thin forwarders to _core, which uses the
    # current_cell pointer set by PythonMicroProgram::update.
    # ------------------------------------------------------------------

    def die(self):
        """Mark this cell for removal at the end of the current tick."""
        _core.current_die()

    def divide(self):
        """Force-divide on the next divide check, bypassing the
        size-mean/variance machinery. Mirrors CCL's `divide()`."""
        _core.current_force_divide()

    def run(self, dvel):
        """Thrust forward toward velocity `dvel`; damp angular rotation."""
        _core.current_run(dvel)

    def tumble(self, vel):
        """Apply torque `vel`; damp translation. Pair with `run()`."""
        _core.current_tumble(vel)

    def emit_signal(self, handle, amount):
        _core.current_emit_signal(handle, amount)

    def absorb_signal(self, handle, amount):
        _core.current_absorb_signal(handle, amount)

    def get_signal(self, handle):
        return _core.current_get_signal(handle)

    @property
    def volume(self): return _core.current_volume()
    @property
    def id(self):     return _core.current_id()
    @property
    def x(self):      return _core.current_x()
    @property
    def y(self):      return _core.current_y()
    @property
    def theta(self):  return _core.current_theta()

    @property
    def just_divided(self):
        """True for one tick on both halves of a fresh division."""
        return _core.current_just_divided()

    @property
    def daughter(self):
        """True for one tick on the new cell of a division (false on
        the mother). Pair with `just_divided` to break symmetry."""
        return _core.current_is_daughter()

    @property
    def selected(self):
        """True while the user has this cell highlighted in the GUI."""
        return _core.current_selected()

    def message(self, channel, text):
        """Print to the GUI console on the given channel."""
        _core.message(int(channel), str(text))

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
    """One part's view of a composite's raw state. Shared fields pass
    through; local fields go to a part-prefixed key in the raw
    namespace, so two parts can both declare `active=False` without
    collision. `_aliases` is the local-to-prefixed map, built once at
    compose() time."""
    __slots__ = ("_raw", "_aliases")

    def __init__(self, raw_ns, aliases):
        object.__setattr__(self, "_raw",     raw_ns)
        object.__setattr__(self, "_aliases", aliases)

    def __getattr__(self, name):
        return getattr(self._raw, self._aliases.get(name, name))

    def __setattr__(self, name, value):
        setattr(self._raw, self._aliases.get(name, name), value)


def compose(*parts, share=None):
    """Combine Program subclasses into one composite. Shared fields
    get one storage (right-most part's default wins, matching CCL);
    non-shared fields are auto-namespaced per-part. Rules run in
    part-list order under a per-part scope view. Equivalent to CCL's
    `p() := q() + r() sharing x, y, ...`."""
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

    # `requires` must be in `share`. Combined with the share-must-be-
    # declared check above, transitively gives "declared by some part
    # AND shared" (DESIGN.md's requires semantics).
    for p in parts:
        for name in getattr(p, "requires", None) or ():
            if name not in share:
                raise GroLoadError(
                    f"compose(): {p.__name__}.requires = [..., {name!r}, ...] "
                    f"but '{name}' is not in share=[...].")

    merged_factory = _StateFactory._from_validated(merged_defaults, merged_preserved)

    composed_rules = [
        (idx, rule)
        for idx, p in enumerate(parts)
        for rule in p._gro_rules
    ]

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
        # Cache scopes once per instance -- otherwise we'd allocate
        # one per rule-fire per cell per tick.
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
        for part_idx, (_name, _kind, predicate, action) in self._gro_composed_rules:
            self.state = scopes[part_idx]
            try:
                if predicate(self):
                    action(self)
            finally:
                self.state = raw
        # Subclass-of-composite rules run against the raw state. Rare
        # path; lets the user add rules on top of a composite.
        for _name, _kind, predicate, action in type(self)._gro_rules:
            if predicate(self):
                action(self)
    Composite._tick = _tick

    def _split(self, mother_frac):
        # Base _split handles the merged namespace correctly (every
        # field lives on the raw SimpleNamespace; _gro_preserved spans
        # both shared and namespaced names). We only need to rebuild
        # the daughter's scopes around her fresh deepcopied state.
        daughter = Program._split(self, mother_frac)
        daughter._scopes = [_PartScope(daughter.state, parts_aliases[i])
                            for i in range(len(parts))]
        return daughter
    Composite._split = _split

    return Composite


# ----------------------------------------------------------------------------
# Composed: class-body sugar for compose(...)
# ----------------------------------------------------------------------------

class _ComposedMeta(_ProgramMeta):
    """Metaclass that runs `compose(*parts, share=share)` on a user's
    class-body declaration and copies the result onto the new class."""

    _COPIED_ATTRS = (
        "state", "_gro_preserved",
        "_gro_composed_rules", "_gro_part_aliases", "_gro_parts",
        "__init__", "setup", "_tick", "_split",
    )

    def __new__(mcs, name, bases, ns):
        cls = super().__new__(mcs, name, bases, ns)
        if name == "Composed" or "parts" not in ns:
            return cls
        merged = compose(*ns["parts"], share=list(ns.get("share", [])))
        for attr in mcs._COPIED_ATTRS:
            setattr(cls, attr, getattr(merged, attr))
        return cls


class Composed(Program, metaclass=_ComposedMeta):
    """Class-body sugar for `compose(...)`:

        class Wave(Composed):
            parts = [Leader, Follower]
            share = ["t"]
    """


# ----------------------------------------------------------------------------
# WorldProgram + set_main / reset
# ----------------------------------------------------------------------------

class WorldProgram(Program):
    """Marker base for a `program main()`-style world-level program.
    Rules run once per tick at world scope, not per cell, so the
    cell-local API (self.volume, self.emit_signal, ...) doesn't apply
    here -- use the module-level signal/param functions instead."""


def set_main(cls_or_instance):
    """Install a `WorldProgram` as the world's per-tick main program.
    Accepts a class (built once) or a pre-built instance; returns the
    live instance. Equivalent to CCL's `program main() := { ... }`."""
    if isinstance(cls_or_instance, type):
        if not issubclass(cls_or_instance, WorldProgram):
            raise GroLoadError(
                f"set_main({cls_or_instance.__name__}): must be a "
                f"WorldProgram subclass. Cell-context built-ins "
                f"like self.volume / self.emit_signal don't apply "
                f"at world scope.")
        instance = cls_or_instance()
    elif isinstance(cls_or_instance, WorldProgram):
        instance = cls_or_instance
    else:
        raise GroLoadError(
            f"set_main(...): argument must be a WorldProgram subclass "
            f"or instance, got {type(cls_or_instance).__name__}.")
    _core.set_main_program(instance)
    return instance


def reset():
    """Restart the world: cells removed, signal grids zeroed,
    chipmunk space rebuilt. Params, signal registrations, and the
    installed `set_main(...)` persist across the restart."""
    _core.reset_world()
