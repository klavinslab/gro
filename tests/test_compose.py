"""Tests for compose(), _PartScope, and Composed."""

import os, sys, unittest
sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))
from tests import _stub
_stub.install()

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(__file__)), "python"))
from gro._program import (
    Program, State, Preserved, when, always,
    compose, Composed, GroLoadError, _PartScope,
)


class ComposeBasicsTests(unittest.TestCase):

    def test_empty_parts_rejected(self):
        with self.assertRaises(GroLoadError):
            compose()

    def test_non_program_arg_rejected(self):
        with self.assertRaises(GroLoadError):
            compose(42)

    def test_simple_compose_succeeds(self):
        class A(Program):
            state = State(x=1)
            @always
            def t(self): pass
        class B(Program):
            state = State(y=2)
            @always
            def t(self): pass
        C = compose(A, B)
        c = C()
        # Both fields present, namespaced per part (different class names).
        keys = set(vars(c.state).keys())
        self.assertEqual(len(keys), 2)

    def test_share_must_be_declared(self):
        class A(Program):
            state = State(x=1)
            @always
            def t(self): pass
        with self.assertRaises(GroLoadError) as cm:
            compose(A, share=["nonexistent"])
        self.assertIn("nonexistent", str(cm.exception))

    def test_requires_must_be_in_share(self):
        class HasZ(Program):
            state = State(z=1)
            @always
            def t(self): pass
        class WantsZ(Program):
            state = State()
            requires = ["z"]
            @always
            def t(self): pass
        # Without share=["z"]: fail.
        with self.assertRaises(GroLoadError):
            compose(WantsZ, HasZ)
        # With share=["z"]: succeed.
        compose(WantsZ, HasZ, share=["z"])


class ShareSemanticsTests(unittest.TestCase):

    def test_shared_field_right_takes_precedence(self):
        class A(Program):
            state = State(shared=10)
            @always
            def t(self): pass
        class B(Program):
            state = State(shared=99)
            @always
            def t(self): pass
        C = compose(A, B, share=["shared"])
        c = C()
        # CCL's CompositeProgram convention: right-most part's default wins.
        self.assertEqual(c.state.shared, 99)

    def test_local_fields_autonamespace_per_part(self):
        # Two parts both declare `active=False`; without share they
        # must coexist as separate fields, not collide.
        class A(Program):
            state = State(active=False)
            @always
            def t(self): pass
        class B(Program):
            state = State(active=True)
            @always
            def t(self): pass
        C = compose(A, B)
        c = C()
        keys = sorted(vars(c.state).keys())
        # Both prefixed; values preserved.
        self.assertEqual(len(keys), 2)
        # One should be False (A's), one True (B's).
        vals = [getattr(c.state, k) for k in keys]
        self.assertEqual(sorted(vals), [False, True])


class CompositeDivisionTests(unittest.TestCase):

    def test_shared_field_halved_once(self):
        class A(Program):
            state = State(n=10.0)
            @always
            def t(self): pass
        class B(Program):
            state = State()
            @always
            def t(self): pass
        C = compose(A, B, share=["n"])
        c = C()
        c.state.n = 10.0
        d = c._split(0.5)
        self.assertEqual(c.state.n, 5.0)
        self.assertEqual(d.state.n, 5.0)

    def test_per_part_local_field_halved_independently(self):
        class A(Program):
            state = State(local=8.0)
            @always
            def t(self): pass
        class B(Program):
            state = State(local=4.0)
            @always
            def t(self): pass
        C = compose(A, B)
        c = C()
        d = c._split(0.5)
        # Each part's local halved separately.
        for inst in (c, d):
            vals = [getattr(inst.state, k) for k in vars(inst.state)]
            self.assertEqual(sorted(vals), [2.0, 4.0])

    def test_preserved_field_skipped(self):
        class A(Program):
            state = State(mode=Preserved(1))
            @always
            def t(self): pass
        class B(Program):
            state = State()
            @always
            def t(self): pass
        C = compose(A, B, share=["mode"])
        c = C()
        c.state.mode = 1
        d = c._split(0.5)
        self.assertEqual(c.state.mode, 1)
        self.assertEqual(d.state.mode, 1)


class PartScopeTests(unittest.TestCase):

    def test_unknown_attr_raises_attribute_error(self):
        from types import SimpleNamespace
        scope = _PartScope(SimpleNamespace(), {"x": "_p0_x"})
        with self.assertRaises(AttributeError):
            scope.y

    def test_shared_attr_passes_through(self):
        from types import SimpleNamespace
        raw = SimpleNamespace(shared=42)
        scope = _PartScope(raw, {})  # no aliases, so all reads go raw
        self.assertEqual(scope.shared, 42)

    def test_local_attr_remapped_to_prefixed_key(self):
        from types import SimpleNamespace
        raw = SimpleNamespace(_State_0_0_active=True)
        scope = _PartScope(raw, {"active": "_State_0_0_active"})
        self.assertEqual(scope.active, True)
        scope.active = False
        self.assertEqual(raw._State_0_0_active, False)


class ScopeCachingTests(unittest.TestCase):

    def test_scopes_cached_per_instance(self):
        class A(Program):
            state = State(x=0)
            @always
            def t(self): pass
        class B(Program):
            state = State(y=0)
            @always
            def t(self): pass
        C = compose(A, B)
        c = C()
        first = id(c._scopes[0])
        c._tick()
        c._tick()
        self.assertEqual(id(c._scopes[0]), first)

    def test_daughter_scopes_rebuilt_after_split(self):
        class A(Program):
            state = State(x=4.0)
            @always
            def t(self): pass
        C = compose(A)
        c = C()
        d = c._split(0.5)
        self.assertTrue(hasattr(d, "_scopes"))
        # Daughter's scope points at her own raw state, not the mother's.
        self.assertIs(d._scopes[0]._raw, d.state)


class WorldProgramTests(unittest.TestCase):

    def test_worldprogram_collects_rules_normally(self):
        from gro._program import WorldProgram
        class M(WorldProgram):
            state = State(t=0.0)
            @always
            def tick(self): pass
            @when(lambda self: self.state.t > 5)
            def boom(self): pass
        names = [r[0] for r in M._gro_rules]
        self.assertEqual(names, ["tick", "boom"])

    def test_worldprogram_is_program_subclass(self):
        # Programs and WorldPrograms share the rule-dispatch surface.
        from gro._program import WorldProgram
        self.assertTrue(issubclass(WorldProgram, Program))


class ComposedSugarTests(unittest.TestCase):

    def test_class_body_form_equivalent_to_compose(self):
        class A(Program):
            state = State(x=0)
            @always
            def ta(self): pass
        class B(Program):
            state = State(y=0)
            @always
            def tb(self): pass

        class Sugar(Composed):
            parts = [A, B]
            share = []

        Explicit = compose(A, B, share=[])
        # Same rule count, same parts.
        self.assertEqual(
            len(Sugar._gro_composed_rules),
            len(Explicit._gro_composed_rules))
        self.assertEqual(Sugar._gro_parts, Explicit._gro_parts)

    def test_subclass_of_composite_runs_added_rules(self):
        class A(Program):
            state = State(x=0)
            @always
            def ta(self): pass
        C = compose(A)
        fired = []
        class Tagged(C):
            @always
            def tag(self): fired.append(1)
        Tagged()._tick()
        self.assertEqual(fired, [1])


class CompositeNameTests(unittest.TestCase):

    def test_composite_name_lists_parts(self):
        class A(Program):
            state = State()
            @always
            def t(self): pass
        class B(Program):
            state = State()
            @always
            def t(self): pass
        C = compose(A, B)
        self.assertEqual(C.__name__, "Composed(A,B)")


class PartScopePrefixTests(unittest.TestCase):
    """Section-6 UX: aliased keys include the part's class name when
    it's a valid identifier."""

    def test_valid_identifier_name_used(self):
        class MyPart(Program):
            state = State(active=False)
            @always
            def t(self): pass
        C = compose(MyPart)
        c = C()
        keys = list(vars(c.state).keys())
        self.assertEqual(keys, ["_MyPart_0_active"])

    def test_non_identifier_falls_back_to_index_only(self):
        class A(Program):
            state = State(x=1)
            @always
            def t(self): pass
        class B(Program):
            state = State(x=2)
            @always
            def t(self): pass
        # Compose A and B, then compose the result with another part --
        # the inner composite's name has parens, not a valid identifier.
        Inner = compose(A, B)
        class C(Program):
            state = State()
            @always
            def t(self): pass
        Outer = compose(Inner, C)
        oi = Outer()
        # No assertion failure during compose; aliases use fallback form.
        # Check that the inner-composite's prefix used the fallback.
        inner_keys = [k for k in vars(oi.state) if "_p0_" in k]
        self.assertTrue(len(inner_keys) >= 1)


if __name__ == "__main__":
    unittest.main()
