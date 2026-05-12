#include "PythonRuntime.h"

#include <pybind11/embed.h>
#include <pybind11/eval.h>
#include <sstream>
#include <stdexcept>
#include <iostream>

#include <pybind11/stl.h>

#include "Micro.h"
#include "Cell.h"
#include "EColi.h"
#include "Theme.h"
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

// Pulls the cell currently being ticked. Used by emit_signal /
// get_signal / volume / id etc. — cell-local bindings that need to
// know which cell `self` refers to without the user passing it.
static Cell * current_cell() {
    Cell * c = PythonRuntime::instance().getCurrentCell();
    if (!c)
        throw std::runtime_error(
            "Cell-local gro API called outside a cell rule. "
            "Functions like emit_signal can only be called from "
            "inside a Program rule (a @when/@always/@rate method).");
    return c;
}

// RAII guard: installs `c` as the active cell on entry, restores
// whatever was there on exit (so nested invocations from a rule —
// e.g. spawning a new cell from inside another's setup — see the
// right context).
struct CellScope {
    Cell * prev;
    explicit CellScope(Cell * c) : prev(PythonRuntime::instance().getCurrentCell()) {
        PythonRuntime::instance().setCurrentCell(c);
    }
    ~CellScope() {
        PythonRuntime::instance().setCurrentCell(prev);
    }
};

// Adapter that lets a CCL Cell drive a Python Program. EColi::update
// calls program->update(world, this); we forward that to the Python
// instance's _tick() with the cell installed as the current_cell.
class PythonMicroProgram : public MicroProgram {
public:
    explicit PythonMicroProgram(py::object instance)
        : instance_(std::move(instance)) {}

    // py::object's destructor decrements a Python refcount; must hold
    // the GIL when that happens. Cell deletion can happen on either
    // thread. If Py_Finalize has already run (e.g. during static
    // teardown), there's no interpreter to acquire — leak the
    // refcount silently rather than crash.
    ~PythonMicroProgram() override {
        if (!Py_IsInitialized()) {
            instance_.release();
            return;
        }
        py::gil_scoped_acquire gil;
        instance_ = py::object();
    }

    void update(World * w, Cell * c) override {
        py::gil_scoped_acquire gil;
        CellScope scope(c);
        try {
            instance_.attr("_tick")();
        } catch (py::error_already_set & e) {
            // Logged to stderr; doesn't stop the simulation. The
            // overlay-on-rule-error path is a future improvement.
            std::cerr << "Python rule error in cell " << c->get_id() << ":\n"
                      << e.what() << std::endl;
        }
        // Clear the division built-ins after the tick. EColi::divide
        // set them; rules read them via self.just_divided /
        // self.daughter during _tick. Clearing here (outside the
        // try/catch so a raising rule still resets the flag) gives
        // the same one-tick lifetime CCL has in Gro.cpp.
        if (c->just_divided()) {
            c->set_division_indicator(false);
            c->set_daughter_indicator(false);
        }
    }

    MicroProgram * split(float mother_frac) override {
        // Delegate state-splitting to Program._split in Python. On
        // any Python error we return nullptr; the caller in
        // EColi::divide skips set_prog for the daughter, leaving
        // her programless rather than crashing. Better than nothing
        // until the error-overlay path is wired up.
        py::gil_scoped_acquire gil;
        try {
            py::object new_instance = instance_.attr("_split")(mother_frac);
            return new PythonMicroProgram(new_instance);
        } catch (py::error_already_set & e) {
            std::cerr << "Python program split error: " << e.what() << std::endl;
            return nullptr;
        }
    }

private:
    py::object instance_;
};

PYBIND11_EMBEDDED_MODULE(_core, m) {
    m.doc() = "gro core bindings (C++ side of the gro Python module).";

    // ---- parameters ----
    // set_param mirrors CCL's set_param dispatch: if called inside a
    // cell context (during setup() or a rule body), the param is
    // written to that cell's local map; otherwise it goes to the
    // World. This is how `Leader.setup()` can lower its own growth
    // rate without affecting other cells.
    m.def("set_param", [](const std::string & name, double value) {
        Cell * cc = PythonRuntime::instance().getCurrentCell();
        if (cc) {
            cc->set_param(name, static_cast<float>(value));
            cc->compute_parameter_derivatives();
        } else {
            World * w = world();
            // CCL refuses to change signal grid sizes after any
            // signal has been declared; mirror that guard.
            if (w->num_signals() == 0 ||
                (name != "signal_grid_width" &&
                 name != "signal_grid_height" &&
                 name != "signal_element_size")) {
                w->set_param(name, static_cast<float>(value));
            }
        }
    }, py::arg("name"), py::arg("value"));

    m.def("get_param", [](const std::string & name) -> double {
        Cell * cc = PythonRuntime::instance().getCurrentCell();
        return cc ? cc->get_param(name) : world()->get_param(name);
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

    m.def("get_signal_at", [](int handle, double x, double y) -> double {
        World * w = world();
        if (handle < 0 || handle >= w->num_signals())
            throw std::runtime_error("get_signal_at: invalid handle");
        return w->signal_value(handle,
                               static_cast<float>(x),
                               static_cast<float>(y));
    }, py::arg("handle"), py::arg("x"), py::arg("y"));

    // ---- cell-local signal API (uses current_cell context) ----
    m.def("emit_signal_cell", [](int handle, double amount) {
        world()->emit_signal(current_cell(), handle,
                             static_cast<float>(amount));
    }, py::arg("handle"), py::arg("amount"));

    m.def("absorb_signal_cell", [](int handle, double amount) {
        world()->absorb_signal(current_cell(), handle,
                               static_cast<float>(amount));
    }, py::arg("handle"), py::arg("amount"));

    m.def("get_signal_cell", [](int handle) -> double {
        return world()->get_signal_value(current_cell(), handle);
    }, py::arg("handle"));

    // ---- current cell properties ----
    m.def("current_volume",        []() -> double { return current_cell()->get_volume(); });
    m.def("current_id",            []() -> int    { return current_cell()->get_id(); });
    m.def("current_x",             []() -> double { return current_cell()->get_x(); });
    m.def("current_y",             []() -> double { return current_cell()->get_y(); });
    m.def("current_theta",         []() -> double { return current_cell()->get_theta(); });
    m.def("current_just_divided",  []() -> bool   { return current_cell()->just_divided(); });
    m.def("current_is_daughter",   []() -> bool   { return current_cell()->is_daughter(); });

    // ---- reporters (gfp/rfp/yfp/cfp counters) ----
    // Single index-parameterized pair; Python wraps it in four named
    // properties on Program. Indices below are re-exported so the
    // Python side never has to hard-code Defines.h's GFP/RFP/YFP/CFP
    // and silently mis-index if the C++ macros are ever renumbered.
    m.attr("REP_GFP") = py::int_(GFP);
    m.attr("REP_RFP") = py::int_(RFP);
    m.attr("REP_YFP") = py::int_(YFP);
    m.attr("REP_CFP") = py::int_(CFP);
    m.def("current_get_rep", [](int idx) -> int {
        if (idx < 0 || idx >= MAX_REP_NUM)
            throw std::runtime_error("current_get_rep: invalid index");
        return current_cell()->get_rep(idx);
    }, py::arg("idx"));
    m.def("current_set_rep", [](int idx, int value) {
        if (idx < 0 || idx >= MAX_REP_NUM)
            throw std::runtime_error("current_set_rep: invalid index");
        current_cell()->set_rep(idx, value);
    }, py::arg("idx"), py::arg("value"));

    // ---- die ----
    // Marks the current cell for removal at the end of this tick.
    // World::update sweeps marked cells out of the population after
    // the per-cell loop finishes. Same mechanism CCL's die() uses.
    m.def("die_cell", []() { current_cell()->mark_for_death(); });

    // ---- spawning ----
    m.def("ecoli",
          [](double x, double y, double theta, py::object volume,
             py::object program) {
        World * w = world();
        double v = volume.is_none()
                       ? DEFAULT_ECOLI_INIT_SIZE
                       : volume.cast<double>();
        EColi * c = new EColi(w,
                              static_cast<float>(x),
                              static_cast<float>(y),
                              static_cast<float>(theta),
                              static_cast<float>(v));
        if (!program.is_none()) {
            // The Python side passes a Program *instance* (created by
            // the gro.ecoli wrapper). Wrap it in the C++ adapter so
            // EColi::update can dispatch into it each tick.
            c->set_prog(new PythonMicroProgram(program));
        }
        w->add_cell(c);
        // Run setup() with current_cell set, mirroring CCL's
        // new_ecoli which calls prog->init under a current_cell
        // context so any set_param calls in init are cell-local.
        if (!program.is_none()) {
            CellScope scope(c);
            program.attr("setup")();
        }
    },
          py::arg("x")       = 0.0,
          py::arg("y")       = 0.0,
          py::arg("theta")   = 0.0,
          py::arg("volume")  = py::none(),
          py::arg("program") = py::none());

    // ---- time / dt ----
    m.def("dt",   []() -> double { return world()->get_sim_dt(); });
    m.def("time", []() -> double { return world()->get_time();  });

    // ---- theme ----
    m.def("set_theme", [](
        const std::string & background,
        const std::string & ecoli_edge,
        const std::string & ecoli_selected,
        const std::string & chemostat_edge,
        const std::string & message,
        const std::string & mouse,
        const std::vector< std::vector<float> > & signal_palette
    ) {
        world()->get_theme()->set_colors(
            background, ecoli_edge, ecoli_selected,
            chemostat_edge, message, mouse, signal_palette);
    },
        py::arg("background"),
        py::arg("ecoli_edge"),
        py::arg("ecoli_selected"),
        py::arg("chemostat_edge"),
        py::arg("message"),
        py::arg("mouse"),
        py::arg("signals"));

    // ---- RNG (matches CCL's `rand`) ----
    m.def("rand", [](int n) -> int {
        if (n <= 0)
            throw std::runtime_error("rand(n): n must be positive");
        return std::rand() % n;
    }, py::arg("n"));
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
        py::module_::import("_core");
        // main.cpp chdir'd to Contents/Resources at startup, so
        // relative "python" resolves to the bundled wrapper at
        // Contents/Resources/python.
        py::module_::import("sys").attr("path").attr("insert")(0, "python");

        // Release the GIL so the simulation thread (a QThread) can
        // acquire it each tick via py::gil_scoped_acquire. Without
        // this, the main thread would hold the GIL indefinitely and
        // GroThread would deadlock the first time a Python rule
        // tries to run.
        main_thread_state_ = PyEval_SaveThread();
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

    // We released the GIL in ensureInitialized; reacquire it for
    // this main-thread work.
    py::gil_scoped_acquire gil;
    try {
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
    if (main_thread_state_) {
        PyEval_RestoreThread(static_cast<PyThreadState *>(main_thread_state_));
        main_thread_state_ = nullptr;
    }
    py::finalize_interpreter();
}

PythonRuntime::~PythonRuntime() {
    shutdown();
}
