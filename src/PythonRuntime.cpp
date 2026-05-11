#include "PythonRuntime.h"

#include <pybind11/embed.h>
#include <pybind11/eval.h>
#include <sstream>
#include <stdexcept>

#include "Micro.h"
#include "EColi.h"
#include "Defines.h"

namespace py = pybind11;

// Pulls the active World from PythonRuntime or throws a clear Python
// RuntimeError if none is set. The throw turns "gro API called with no
// active simulation" into a readable error in the gro console instead
// of a segfault when the binding dereferences a null world pointer.
static World * world() {
    World * w = PythonRuntime::instance().getCurrentWorld();
    if (!w)
        throw std::runtime_error(
            "gro API called with no active simulation world. "
            "This usually means you imported gro outside of File->Open.");
    return w;
}

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
        return world()->add_new_signal(
            static_cast<float>(diffusion),
            static_cast<float>(degradation));
    }, py::arg("diffusion"), py::arg("degradation"));

    m.def("set_signal", [](int handle, double x, double y, double value) {
        world()->set_signal(handle,
                            static_cast<float>(x),
                            static_cast<float>(y),
                            static_cast<float>(value));
    }, py::arg("handle"), py::arg("x"), py::arg("y"), py::arg("value"));

    // get_signal at (x,y). The cell-local form (no coords; uses self's
    // position) is a method on Cell, added when the Cell binding lands.
    m.def("get_signal", [](int handle, double x, double y) -> double {
        World * w = world();
        if (handle < 0 || handle >= w->num_signals())
            throw std::runtime_error("get_signal: invalid handle");
        return w->signal_value(handle,
                               static_cast<float>(x),
                               static_cast<float>(y));
    }, py::arg("handle"), py::arg("x"), py::arg("y"));

    // ---- cells ----
    m.def("ecoli",
          [](double x, double y, double theta, double volume) {
        // No program assignment yet. Cell grows under physics;
        // EColi::update no-ops when program is NULL.
        World * w = world();
        EColi * c = new EColi(w,
                              static_cast<float>(x),
                              static_cast<float>(y),
                              static_cast<float>(theta),
                              static_cast<float>(volume));
        w->add_cell(c);
    },
          py::arg("x")      = 0.0,
          py::arg("y")      = 0.0,
          py::arg("theta")  = 0.0,
          py::arg("volume") = DEFAULT_ECOLI_INIT_SIZE);

    // ---- time / dt ----
    // Exposed as zero-arg functions. dt() is read each call so it
    // tracks set_param("dt", …) at any point during the run.
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
    if (Py_IsInitialized())
        return true;

    try {
        py::initialize_interpreter();
        // Smoke test that the embedded module is registered correctly;
        // a missing _core means pybind11 wiring is wrong, and the
        // user gets that as a clean error instead of an obscure
        // ImportError on first `from gro import *`.
        py::module_::import("_core");
        // The user's .py file does `from gro import *`. Make the
        // bundled wrapper at Contents/Resources/python/gro/__init__.py
        // findable. main.cpp chdir'd to Contents/Resources at startup,
        // so the relative "python" path resolves there. Inserted once
        // per process; subsequent loadProgram calls reuse it.
        py::module_::import("sys").attr("path").attr("insert")(0, "python");
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
        // Apply the gro stdlib's default world parameters to this
        // fresh World. Python's module-import cache means the
        // top-level code of gro/__init__.py only runs once per
        // process, so we can't rely on module load to do this —
        // we call the setup function explicitly each load.
        py::module_::import("gro").attr("_setup_world")();
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
    if (!Py_IsInitialized())
        return;
    py::finalize_interpreter();
}

PythonRuntime::~PythonRuntime() {
    shutdown();
}
