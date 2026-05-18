"""Headless smoke test: every examples/*.py must load through the
stubbed `_core` without raising. Catches paste-in corruption,
syntax errors, strict-mode rejections, etc."""

import glob, os, sys, unittest
sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))
from tests import _stub
_stub.install()

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(__file__)), "python"))
import gro

EXAMPLES_DIR = os.path.join(os.path.dirname(os.path.dirname(__file__)), "examples")


def _run_example(path):
    """Exec the file as if loaded by gro's py::eval_file. Returns the
    namespace it left behind."""
    with open(path) as fp:
        src = fp.read()
    ns = {"__file__": path, "__name__": "__main__"}
    exec(compile(src, path, "exec"), ns)
    return ns


class ExampleLoadTests(unittest.TestCase):
    """One generated test method per example file. unittest will list
    them individually so a single failure points at the offending file."""

    pass


def _make_test(path):
    name = os.path.splitext(os.path.basename(path))[0]
    def test(self):
        try:
            _run_example(path)
        except gro.GroLoadError as e:
            self.fail(f"{name}: GroLoadError at load time -- {e}")
    test.__name__ = f"test_load_{name}"
    return test


for _path in sorted(glob.glob(os.path.join(EXAMPLES_DIR, "*.py"))):
    if "__pycache__" in _path:
        continue
    setattr(ExampleLoadTests, f"test_load_{os.path.splitext(os.path.basename(_path))[0]}",
            _make_test(_path))


if __name__ == "__main__":
    unittest.main()
