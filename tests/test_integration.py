"""End-to-end integration tests: subprocess the real gro binary
under --load + --ticks against every example file.

Exits 0 if the example ticked the requested count without halting.
Exits 1 if the sim halted early (typically a Python rule error
that called emit_python_error + set_stop_flag, or any other crash).

Skips automatically if the gro binary hasn't been built yet -- so a
plain `python3 tests/run.py` on a fresh checkout doesn't fail."""

import glob, os, subprocess, sys, unittest

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


if __name__ == "__main__":
    unittest.main()
