"""End-to-end integration tests: subprocess the real gro binary
under --load + --ticks against every example file.

Exits 0 if the example ticked the requested count without halting.
Exits 1 if the sim halted early (typically a Python rule error
that called emit_python_error + set_stop_flag, or any other crash).

Skips automatically if the gro binary hasn't been built yet -- so a
plain `python3 tests/run.py` on a fresh checkout doesn't fail."""

import glob, os, subprocess, sys, tempfile, textwrap, unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
GRO = os.path.join(ROOT, "build", "gro.app", "Contents", "MacOS", "gro")
EXAMPLES = os.path.join(ROOT, "examples")


def _run(example, ticks=30, timeout=20):
    """Run gro with --load + --ticks; return (returncode, stderr)."""
    proc = subprocess.run(
        [GRO, "--load", example, "--ticks", str(ticks)],
        timeout=timeout, capture_output=True,
    )
    return proc.returncode, proc.stderr.decode("utf-8", errors="replace")


@unittest.skipUnless(os.path.exists(GRO),
                     f"gro binary not found at {GRO}; build it first "
                     "with `cmake --build build`")
class IntegrationTests(unittest.TestCase):
    """Generated test methods, one per examples/*.py. Each runs the
    real gro binary for 30 ticks and asserts a clean exit."""

    pass


def _make_test(path):
    name = os.path.splitext(os.path.basename(path))[0]
    def test(self):
        rc, err = _run(path)
        if rc != 0:
            # Surface the binary's stderr in the failure message so
            # the user doesn't have to re-run manually to see why.
            self.fail(f"{name} exit={rc}\n--- stderr ---\n{err}")
    test.__name__ = f"test_{name}"
    return test


for _path in sorted(glob.glob(os.path.join(EXAMPLES, "*.py"))):
    if "__pycache__" in _path:
        continue
    name = os.path.splitext(os.path.basename(_path))[0]
    setattr(IntegrationTests, f"test_{name}", _make_test(_path))


def _run_inline(src, ticks=20, timeout=10):
    """Write src to a temp file and run gro on it; return (rc, stderr)."""
    src = textwrap.dedent(src)
    tmp = tempfile.NamedTemporaryFile(
        mode="w", suffix=".py", delete=False, prefix="gro_edge_")
    try:
        tmp.write(src)
        tmp.close()
        return _run(tmp.name, ticks=ticks, timeout=timeout)
    finally:
        os.unlink(tmp.name)


@unittest.skipUnless(os.path.exists(GRO),
                     f"gro binary not found at {GRO}")
class EdgeCaseTests(unittest.TestCase):
    """Cell-lifecycle edge cases caught by the audit."""

    def test_reset_from_cell_rule_halts_cleanly(self):
        """reset() inside a per-cell rule used to crash on iterator
        invalidation. The binding now throws; the sim halts with a
        readable error rather than a UAF."""
        rc, err = _run_inline("""
            from gro import *
            class Bad(Program):
                state = State()
                @always
                def boom(self): reset()
            ecoli(x=0, y=0, program=Bad)
        """)
        self.assertEqual(rc, 1,
            f"expected halt-by-error (exit 1), got {rc}\nstderr:\n{err}")
        self.assertIn("reset() called from a cell rule", err)

    def test_die_and_divide_in_same_rule(self):
        """A rule that both forces division and marks for death.
        Daughter should spawn cleanly; mother is swept. No crash."""
        rc, err = _run_inline("""
            from gro import *
            class Suicidal(Program):
                state = State()
                @when(lambda self: self.volume > 1.0)
                def split_and_die(self):
                    self.divide()
                    self.die()
            ecoli(x=0, y=0, program=Suicidal)
        """, ticks=10)
        self.assertEqual(rc, 0,
            f"die + divide in same rule should not crash\nstderr:\n{err}")

    def test_setmain_chain_in_one_tick(self):
        """A world rule that swaps itself out, and the new main does
        the same. Tests the pending_prog_deletions queue under back-
        to-back set_main calls."""
        rc, err = _run_inline("""
            from gro import *
            class M3(WorldProgram):
                state = State()
                @always
                def t(self): pass
            class M2(WorldProgram):
                state = State()
                @always
                def t(self): set_main(M3)
            class M1(WorldProgram):
                state = State()
                @always
                def t(self): set_main(M2)
            set_main(M1)
        """, ticks=10)
        self.assertEqual(rc, 0,
            f"set_main chain should not crash\nstderr:\n{err}")


if __name__ == "__main__":
    unittest.main()
