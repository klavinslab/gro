"""Test runner. From the project root:

    python3 tests/run.py
    python3 tests/run.py -v       # verbose
    python3 tests/run.py test_state test_compose   # specific modules

Discovers `tests/test_*.py` and runs each as a unittest TestLoader
module. Zero external dependencies (stdlib unittest only)."""

import os, sys, unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


def main():
    sys.path.insert(0, ROOT)

    verbose = "-v" in sys.argv
    args = [a for a in sys.argv[1:] if not a.startswith("-")]

    loader = unittest.TestLoader()
    if args:
        suite = unittest.TestSuite()
        for name in args:
            suite.addTests(loader.loadTestsFromName(f"tests.{name}"))
    else:
        suite = loader.discover(HERE, pattern="test_*.py", top_level_dir=ROOT)

    runner = unittest.TextTestRunner(verbosity=2 if verbose else 1)
    result = runner.run(suite)
    sys.exit(0 if result.wasSuccessful() else 1)


if __name__ == "__main__":
    main()
