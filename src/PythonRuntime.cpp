#include "PythonRuntime.h"

#include <pybind11/embed.h>
#include <pybind11/eval.h>
#include <sstream>
#include <stdexcept>

#include "Micro.h"
#include "EColi.h"
#include "Signal.h"
#include "Defines.h"
#include <chipmunk/chipmunk.h>

namespace py = pybind11;

// Helper: pull the current World from PythonRuntime or throw a clear
// error if none is set. All bound functions go through this so the
// failure mode of "Python code called outside a load" is a Python
// RuntimeError, not a segfault.
static World * world() {
    World * w = PythonRuntime::instance().getCurrentWorld();
    if (!w)
        throw std::runtime_error(
            "gro API called with no active simulation world. "
            "This usually means you imported gro outside of File->Open.");
    return w;
}

// The user-facing `gro` Python package re-exports primitives from
// `_core` (this module) plus adds high-level decorators/classes in
// pure Python. Milestone 2 surface: parameters, signals, ecoli, dt.
PYBIND11_EMBEDDED_MODULE(_core, m) {
    m.doc() = "gro core bindings (C++ side of the gro Python module).";

    // ---- parameters ----
    m.def("set_param", [](const std::string & name, double value) {
        world()->set_param(name, static_cast<float>(value));
    }, py::arg("name"), py::arg("value"));

    m.def("get_param", [](const std::string & name) -> double {
        return world()->get_param(name);
    }, py::arg("name"));

    // ---- signals ----
    m.def("signal", [](double diffusion, double degradation) -> int {
        World * w = world();
        int gw   = w->get_param("signal_grid_width");
        int gh   = w->get_param("signal_grid_height");
        int numx = gw / w->get_param("signal_element_size");
        int numy = gh / w->get_param("signal_element_size");
        Signal * s = new Signal(
            cpv(-gw/2, -gh/2), cpv(gw/2, gh/2), numx, numy,
            static_cast<float>(diffusion),
            static_cast<float>(degradation));
        w->add_signal(s);
        return w->num_signals() - 1;
    }, py::arg("diffusion"), py::arg("degradation"));

    m.def("set_signal", [](int handle, double x, double y, double value) {
        world()->set_signal(handle,
                            static_cast<float>(x),
                            static_cast<float>(y),
                            static_cast<float>(value));
    }, py::arg("handle"), py::arg("x"), py::arg("y"), py::arg("value"));

    // get_signal at (x,y). The cell-local form (no coords; uses
    // self's position) is a method on Cell and lives in milestone 3.
    m.def("get_signal", [](int handle, double x, double y) -> double {
        // Borrow the Signal directly; World only exposes a cell-based
        // accessor. Acceptable for M2 since signals are public.
        World * w = world();
        if (handle < 0 || handle >= w->num_signals())
            throw std::runtime_error("get_signal: invalid handle");
        return w->get_signal_at(handle, static_cast<float>(x), static_cast<float>(y));
    }, py::arg("handle"), py::arg("x"), py::arg("y"));

    // ---- cells ----
    m.def("ecoli",
          [](double x, double y, double theta, double volume) {
        World * w = world();
        EColi * c = new EColi(w,
                              static_cast<float>(x),
                              static_cast<float>(y),
                              static_cast<float>(theta),
                              static_cast<float>(volume));
        // Milestone 2: no program assignment yet. Cell sits and grows
        // by physics; EColi::update gracefully no-ops when program is
        // NULL.
        w->add_cell(c);
    },
          py::arg("x")      = 0.0,
          py::arg("y")      = 0.0,
          py::arg("theta")  = 0.0,
          py::arg("volume") = DEFAULT_ECOLI_INIT_SIZE);

    // ---- time / dt ----
    // Both exposed as zero-arg functions. The Python wrapper presents
    // `dt` as just `dt()` (function call) — a property-style accessor
    // would need a module-level descriptor, which is overkill.
    m.def("dt", []() -> double {
        return world()->get_sim_dt();
    });
    m.def("time", []() -> double {
        return world()->get_time();
    });
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
        // Smoke test: importing _core must succeed.
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

    try {
        // Ensure the bundle's Python wrapper directory is on sys.path
        // so `from gro import *` resolves. applicationDirPath/../Resources/python.
        // We use Qt's path here via a small helper exposed by Gui; for
        // milestone 2 we hardcode "python" relative to cwd, which the
        // main.cpp chdir already sets to Resources/.
        py::module_::import("sys").attr("path").attr("insert")(0, "python");

        py::eval_file(path);
    } catch (py::error_already_set & e) {
        std::ostringstream oss;
        oss << "Python error in " << (path ? path : "(null)") << ":\n"
            << e.what();
        err = oss.str();
        return false;
    } catch (const std::exception & e) {
        std::ostringstream oss;
        oss << "Error loading " << (path ? path : "(null)") << ":\n"
            << e.what();
        err = oss.str();
        return false;
    }
    return true;
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
