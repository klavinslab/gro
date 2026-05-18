"""Tests for the strict-mode AST validators and `set_main` type check.

These need real on-disk source for `inspect.getsource` to succeed,
so we write each rejection case to a temp file and import it.
"""

import os, sys, tempfile, textwrap, unittest, importlib.util
sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))
from tests import _stub
_stub.install()

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(__file__)), "python"))
import gro
from gro._program import (
    Program, State, Preserved, when, always,
    WorldProgram, set_main, GroLoadError,
)


def _load(src, name="strict_case"):
    """Write src to a temp .py file and import it; returns the module
    (or raises whatever the loader raises). Cleans up the temp file."""
    src = textwrap.dedent(src)
    tmp = tempfile.NamedTemporaryFile(
        mode="w", suffix=".py", delete=False, prefix=name + "_")
    try:
        tmp.write(src)
        tmp.close()
        spec = importlib.util.spec_from_file_location(name, tmp.name)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        return mod
    finally:
        os.unlink(tmp.name)


class PredicateSandboxTests(unittest.TestCase):

    def test_walrus_rejected(self):
        with self.assertRaises(GroLoadError) as cm:
            _load("""
                from gro import *
                class P(Program):
                    state = State(t=0)
                    @when(lambda self: (x := self.state.t) > 1)
                    def fire(self): pass
            """)
        self.assertIn("walrus", str(cm.exception).lower())

    def test_nested_lambda_rejected(self):
        with self.assertRaises(GroLoadError) as cm:
            _load("""
                from gro import *
                class P(Program):
                    state = State(t=0)
                    @when(lambda self: (lambda x: x)(self.state.t) > 1)
                    def fire(self): pass
            """)
        self.assertIn("lambda", str(cm.exception).lower())

    def test_legitimate_predicate_accepted(self):
        # Combination of comparisons, BoolOps, subscript, attribute access,
        # closure-captured constants, and function calls -- all should pass.
        mod = _load("""
                from gro import *
                THRESHOLD = 10
                class P(Program):
                    state = State(t=0.0, mode=0)
                    @when(lambda self: self.state.t > THRESHOLD
                                       and self.state.mode == 0
                                       and rand(100000) < 1000)
                    def fire(self): pass
        """)
        self.assertTrue(hasattr(mod, "P"))


class MethodBodyChecksTests(unittest.TestCase):

    def test_typo_rejected(self):
        with self.assertRaises(GroLoadError) as cm:
            _load("""
                from gro import *
                class P(Program):
                    state = State()
                    @always
                    def t(self):
                        self.tagged = True
            """)
        msg = str(cm.exception)
        self.assertIn("self.tagged", msg)
        self.assertIn("self.state.tagged", msg)

    def test_reporter_writes_accepted(self):
        _load("""
            from gro import *
            class P(Program):
                state = State()
                @always
                def t(self):
                    self.gfp = 100
                    self.rfp += 1
                    self.yfp -= 1
                    self.cfp = 0
        """)
        # No exception = pass.

    def test_state_writes_accepted(self):
        _load("""
            from gro import *
            class P(Program):
                state = State(x=0)
                @always
                def t(self):
                    self.state.x = 5
                    self.state.x += 1
        """)

    def test_underscore_prefix_accepted(self):
        _load("""
            from gro import *
            class P(Program):
                state = State()
                @always
                def t(self):
                    self._cache = 42
        """)

    def test_tuple_unpack_typo_caught(self):
        with self.assertRaises(GroLoadError) as cm:
            _load("""
                from gro import *
                class P(Program):
                    state = State()
                    @always
                    def t(self):
                        self.gfp, self.bogus = 1, 2
            """)
        self.assertIn("self.bogus", str(cm.exception))

    def test_setup_body_validated(self):
        with self.assertRaises(GroLoadError) as cm:
            _load("""
                from gro import *
                class P(Program):
                    state = State()
                    def setup(self):
                        self.typoed_field = 1
                    @always
                    def t(self): pass
            """)
        self.assertIn("self.typoed_field", str(cm.exception))


class StateClassShapeTests(unittest.TestCase):

    def test_non_state_factory_rejected(self):
        with self.assertRaises(GroLoadError) as cm:
            _load("""
                from gro import *
                class P(Program):
                    state = {"t": 0}  # dict, not State()
                    @always
                    def t(self): pass
            """)
        self.assertIn("State(...)", str(cm.exception))


class SetMainTypeTests(unittest.TestCase):

    def test_program_subclass_rejected(self):
        class NotW(Program):
            state = State()
            @always
            def t(self): pass
        with self.assertRaises(GroLoadError) as cm:
            set_main(NotW)
        self.assertIn("WorldProgram", str(cm.exception))

    def test_worldprogram_accepted(self):
        class M(WorldProgram):
            state = State()
            @always
            def t(self): pass
        set_main(M)

    def test_non_class_non_instance_rejected(self):
        with self.assertRaises(GroLoadError):
            set_main(42)

    def test_returns_instance(self):
        class M(WorldProgram):
            state = State(tag=0)
            @always
            def t(self): pass
        inst = set_main(M)
        self.assertIsInstance(inst, M)

    def test_instance_argument_passed_through(self):
        class M(WorldProgram):
            state = State()
            @always
            def t(self): pass
        pre_built = M()
        returned = set_main(pre_built)
        self.assertIs(returned, pre_built)


class ResetTests(unittest.TestCase):

    def test_reset_forwards_to_core(self):
        import gro
        gro._core_chemostat_mode = lambda on: None  # ensure exists
        # The actual binding:
        import _core
        _core.reset_world.reset_mock()
        gro.reset()
        _core.reset_world.assert_called_once_with()


if __name__ == "__main__":
    unittest.main()
