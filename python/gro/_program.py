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
