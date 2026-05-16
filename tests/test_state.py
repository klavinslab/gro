"""Tests for State(...), field(...), Preserved(...), and _StateFactory."""

import os, sys, unittest
sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))
from tests import _stub
_stub.install()

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(__file__)), "python"))
from gro._program import State, field, Preserved, _StateFactory


class StateBasicTests(unittest.TestCase):

    def test_state_returns_factory(self):
        s = State(t=2.4)
        self.assertIsInstance(s, _StateFactory)

    def test_make_produces_namespace_with_defaults(self):
        s = State(t=2.4, mode=0)
        ns = s.make()
        self.assertEqual(ns.t, 2.4)
        self.assertEqual(ns.mode, 0)

    def test_each_make_is_independent(self):
        s = State(t=2.4)
        a, b = s.make(), s.make()
        a.t = 10
        self.assertEqual(b.t, 2.4)


class PreservedTests(unittest.TestCase):

    def test_preserved_unwraps_to_value(self):
        s = State(t=Preserved(2.4))
        ns = s.make()
        self.assertEqual(ns.t, 2.4)

    def test_preserved_marks_field(self):
        s = State(t=Preserved(2.4), v=1.0)
        self.assertIn("t", s._preserved)
        self.assertNotIn("v", s._preserved)


class MutableLiteralRejectionTests(unittest.TestCase):

    def test_bare_list_rejected(self):
        with self.assertRaises(TypeError) as cm:
            State(history=[])
        self.assertIn("mutable literal", str(cm.exception))
        self.assertIn("field(list)", str(cm.exception))

    def test_bare_dict_rejected(self):
        with self.assertRaises(TypeError):
            State(seen={})

    def test_bare_set_rejected(self):
        with self.assertRaises(TypeError):
            State(seen=set())


class FieldFactoryTests(unittest.TestCase):

    def test_field_with_list_constructs_per_cell(self):
        s = State(items=field(list))
        a, b = s.make(), s.make()
        a.items.append(1)
        self.assertEqual(b.items, [])

    def test_field_with_dict_constructs_per_cell(self):
        s = State(seen=field(dict))
        a, b = s.make(), s.make()
        a.seen["x"] = 1
        self.assertEqual(b.seen, {})

    def test_field_with_lambda_constructor(self):
        class Thing:
            def __init__(self, n=0):
                self.n = n
        s = State(thing=field(lambda: Thing(7)))
        ns = s.make()
        self.assertEqual(ns.thing.n, 7)

    def test_field_rejects_non_callable(self):
        with self.assertRaises(TypeError):
            field(42)


class FromValidatedTests(unittest.TestCase):
    """The internal classmethod compose() uses to skip __init__'s
    mutable-literal check on already-validated defaults."""

    def test_round_trip(self):
        f = _StateFactory._from_validated({"a": 1, "b": 2}, {"b"})
        ns = f.make()
        self.assertEqual(ns.a, 1)
        self.assertEqual(ns.b, 2)
        self.assertEqual(f.preserved_names, {"b"})


if __name__ == "__main__":
    unittest.main()
