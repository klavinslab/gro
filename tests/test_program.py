"""Tests for Program: init, rule dispatch, division semantics,
with_args, and cell-local API forwarding."""

import os, sys, unittest
sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))
from tests import _stub
_core = _stub.install()

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(__file__)), "python"))
from gro._program import (
    Program, State, Preserved, field, when, always, rate, GroLoadError
)


class ProgramInitTests(unittest.TestCase):

    def test_no_state_gets_empty_namespace(self):
        class P(Program):
            pass
        p = P()
        self.assertEqual(vars(p.state), {})

    def test_state_initialized_from_factory(self):
        class P(Program):
            state = State(t=2.4, mode=0)
        p = P()
        self.assertEqual(p.state.t, 2.4)
        self.assertEqual(p.state.mode, 0)

    def test_state_per_instance(self):
        class P(Program):
            state = State(t=0)
        a, b = P(), P()
        a.state.t = 99
        self.assertEqual(b.state.t, 0)


class RuleCollectionTests(unittest.TestCase):

    def test_when_decorator_registers_rule(self):
        class P(Program):
            state = State(t=0)
            @when(lambda self: self.state.t > 1)
            def fire(self): pass
        self.assertEqual(len(P._gro_rules), 1)
        name, kind, _, _ = P._gro_rules[0]
        self.assertEqual(name, "fire")
        self.assertEqual(kind, "when")

    def test_always_registers_with_true_predicate(self):
        class P(Program):
            state = State()
            @always
            def tick(self): pass
        self.assertEqual(len(P._gro_rules), 1)
        self.assertTrue(P._gro_rules[0][2](None))  # predicate(self) → True

    def test_rate_predicate_is_probabilistic(self):
        class P(Program):
            state = State()
            @rate(2.5)
            def maybe(self): pass
        pred = P._gro_rules[0][2]
        # With dt=0.1 and rate=2.5, threshold = 2.5 * 0.1 * 100000 = 25000.
        # rand stub returns 0, so 0 < 25000 → True.
        self.assertTrue(pred(None))

    def test_declaration_order_preserved(self):
        class P(Program):
            state = State()
            @always
            def a(self): pass
            @always
            def b(self): pass
            @always
            def c(self): pass
        names = [r[0] for r in P._gro_rules]
        self.assertEqual(names, ["a", "b", "c"])

    def test_subclass_with_no_own_rules_inherits(self):
        class Parent(Program):
            state = State()
            @always
            def t(self): pass
        class Child(Parent):
            pass
        self.assertEqual(Child._gro_rules, Parent._gro_rules)

    def test_subclass_with_own_rules_replaces(self):
        class Parent(Program):
            state = State()
            @always
            def parent_rule(self): pass
        class Child(Parent):
            @always
            def child_rule(self): pass
        names = [r[0] for r in Child._gro_rules]
        self.assertEqual(names, ["child_rule"])


class TickDispatchTests(unittest.TestCase):

    def test_tick_fires_rules_whose_predicate_is_true(self):
        fired = []
        class P(Program):
            state = State(armed=True)
            @when(lambda self: self.state.armed)
            def go(self): fired.append("go")
            @when(lambda self: not self.state.armed)
            def stop(self): fired.append("stop")
        p = P()
        p._tick()
        self.assertEqual(fired, ["go"])
        p.state.armed = False
        p._tick()
        self.assertEqual(fired, ["go", "stop"])

    def test_predicate_evaluated_each_tick(self):
        calls = [0]
        class P(Program):
            state = State()
            @when(lambda self: calls.__setitem__(0, calls[0] + 1) or False)
            def never(self): pass
        p = P()
        p._tick(); p._tick(); p._tick()
        self.assertEqual(calls[0], 3)


class SplitTests(unittest.TestCase):

    def test_split_halves_numeric_fields(self):
        class P(Program):
            state = State(n=10.0, count=20)
            @always
            def t(self): pass
        p = P()
        p.state.n = 10.0
        p.state.count = 20
        d = p._split(0.5)
        self.assertEqual(p.state.n, 5.0)
        self.assertEqual(d.state.n, 5.0)
        self.assertEqual(p.state.count, 10)
        self.assertEqual(d.state.count, 10)

    def test_split_skips_preserved_fields(self):
        class P(Program):
            state = State(mode=Preserved(7), x=4.0)
            @always
            def t(self): pass
        p = P()
        p.state.x = 4.0
        d = p._split(0.5)
        self.assertEqual(p.state.mode, 7)
        self.assertEqual(d.state.mode, 7)
        self.assertEqual(p.state.x, 2.0)

    def test_split_skips_non_numeric_fields(self):
        class P(Program):
            state = State(label="hello", items=field(list))
            @always
            def t(self): pass
        p = P()
        p.state.label = "hello"
        p.state.items = [1, 2, 3]
        d = p._split(0.5)
        # Strings + lists are deep-copied unchanged.
        self.assertEqual(p.state.label, "hello")
        self.assertEqual(d.state.label, "hello")
        self.assertEqual(d.state.items, [1, 2, 3])
        # And the daughter's list is independent.
        d.state.items.append(4)
        self.assertEqual(p.state.items, [1, 2, 3])

    def test_split_skips_booleans(self):
        # bool is a subclass of int, but halving True/False is wrong.
        class P(Program):
            state = State(flag=True)
            @always
            def t(self): pass
        p = P()
        d = p._split(0.5)
        self.assertEqual(p.state.flag, True)
        self.assertEqual(d.state.flag, True)


class WithArgsTests(unittest.TestCase):

    def test_overrides_named_default(self):
        class Pulser(Program):
            state = State(period=2.0)
            @always
            def t(self): pass
        P3 = Pulser.with_args(period=3.0)
        self.assertEqual(P3().state.period, 3.0)
        self.assertEqual(Pulser().state.period, 2.0)  # parent untouched

    def test_unknown_field_rejected(self):
        class Pulser(Program):
            state = State(period=2.0)
            @always
            def t(self): pass
        with self.assertRaises(GroLoadError):
            Pulser.with_args(nonsense=1)

    def test_inherits_parent_rules(self):
        fired = [False]
        class Pulser(Program):
            state = State(period=2.0)
            @always
            def fire(self): fired[0] = True
        P3 = Pulser.with_args(period=3.0)
        P3()._tick()
        self.assertTrue(fired[0])

    def test_preserved_wrapper_marks_field(self):
        class Pulser(Program):
            state = State(period=2.0)
            @always
            def t(self): pass
        P3 = Pulser.with_args(period=Preserved(3.0))
        self.assertIn("period", P3.state._preserved)

    def test_program_without_state_rejected(self):
        class Pulser(Program):
            pass  # no State
        with self.assertRaises(GroLoadError):
            Pulser.with_args(period=3.0)


class CellLocalAPIForwardingTests(unittest.TestCase):
    """Sanity-check that Program's cell-local methods just forward to
    `_core.*`. We use the MagicMock stub to record call args."""

    def setUp(self):
        for m in (_core.current_emit_signal, _core.current_absorb_signal,
                  _core.current_die, _core.current_force_divide,
                  _core.current_run, _core.current_tumble,
                  _core.current_set_rep, _core.message):
            m.reset_mock()

    def test_emit_signal_forwards(self):
        class P(Program):
            state = State()
        P().emit_signal(2, 100)
        _core.current_emit_signal.assert_called_once_with(2, 100)

    def test_die_forwards(self):
        class P(Program):
            state = State()
        P().die()
        _core.current_die.assert_called_once_with()

    def test_divide_forwards_to_force_divide(self):
        class P(Program):
            state = State()
        P().divide()
        _core.current_force_divide.assert_called_once_with()

    def test_run_tumble_forward(self):
        class P(Program):
            state = State()
        p = P()
        p.run(180)
        p.tumble(50)
        _core.current_run.assert_called_once_with(180)
        _core.current_tumble.assert_called_once_with(50)

    def test_message_casts_args(self):
        class P(Program):
            state = State()
        P().message(1, 42)  # int channel + non-string text
        _core.message.assert_called_once_with(1, "42")

    def test_gfp_setter_forwards(self):
        class P(Program):
            state = State()
        P().gfp = 100
        _core.current_set_rep.assert_called_once_with(0, 100)


if __name__ == "__main__":
    unittest.main()
