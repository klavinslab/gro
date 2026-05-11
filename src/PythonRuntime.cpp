#include "PythonRuntime.h"

#include <pybind11/embed.h>
#include <sstream>

namespace py = pybind11;

// The user-facing `gro` Python module will eventually be a Python
// package on disk (Contents/Resources/python/gro/__init__.py) that
// re-exports primitives from this C++-defined `_core` module. For
// milestone 1, `_core` is empty — its only job is to prove the
// pybind11 plumbing works end to end.
PYBIND11_EMBEDDED_MODULE(_core, m) {
    m.doc() = "gro core bindings (milestone 1: empty placeholder).";
}

PythonRuntime & PythonRuntime::instance() {
    static PythonRuntime r;
    return r;
}

bool PythonRuntime::ensureInitialized(std::string & err) {
    if (initialized_)
        return true;

    try {
        py::initialize_interpreter();
        // Smoke test: importing _core must succeed. If pybind11 isn't
        // wired up correctly this throws and the user gets a useful
        // error rather than the generic "not implemented" message.
        py::module_::import("_core");
        initialized_ = true;
        return true;
    } catch (const std::exception & e) {
        std::ostringstream oss;
        oss << "Could not initialize embedded Python: " << e.what();
        err = oss.str();
        return false;
    }
}

bool PythonRuntime::loadProgram(const char * path, std::string & err) {
    if (!ensureInitialized(err))
        return false;

    // Milestone 1: the rest of the loader is unimplemented. We've
    // proven Python initializes and our _core module imports; that's
    // enough for this milestone. Surface a clear "not yet" message.
    std::ostringstream oss;
    oss << "Python support not implemented yet (milestone 1). "
        << "Tried to load: " << (path ? path : "(null)");
    err = oss.str();
    return false;
}

void PythonRuntime::shutdown() {
    if (!initialized_)
        return;
    py::finalize_interpreter();
    initialized_ = false;
}

PythonRuntime::~PythonRuntime() {
    shutdown();
}
