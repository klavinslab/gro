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
#include "Yeast.h"
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

// Route a pybind11 Python error to the gro bottom console (via
// World::emit_message -> Qt signal -> Gui::displayMessage), then
// halt the simulation. Deduplicated so a rule that raises every
// tick within the same update pass doesn't flood the console
// before set_stop_flag actually takes effect.
static void emit_python_error(const std::string & where,
                              py::error_already_set & e) {
    std::string text = "Python error in " + where + ":\n" + e.what();
    std::cerr << text << std::endl;
    static std::string last;
    if (text == last) return;
    last = text;
    if (World * w = PythonRuntime::instance().getCurrentWorld()) {
        // <pre> preserves the traceback's whitespace + line breaks
        // in the QTextEdit console; without it the message ends up
        // on a single wrapped line.
        std::string html = "<pre>" + text + "</pre>";
        w->emit_message(html, false);
        w->set_stop_flag(true);
    }
}

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
            emit_python_error("cell " + std::to_string(c->get_id())
                              + " rule", e);
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
            emit_python_error("cell division", e);
            return nullptr;
        }
    }

private:
    py::object instance_;
};

// World-level analogue of PythonMicroProgram. Stored on World::prog
// for Python-loaded worlds that call gro.set_main(...). The World
// calls world_update() once per simulation tick.
class PythonWorldProgram : public MicroProgram {
public:
    explicit PythonWorldProgram(py::object instance)
        : instance_(std::move(instance)) {}

    ~PythonWorldProgram() override {
        if (!Py_IsInitialized()) {
            instance_.release();
            return;
        }
        py::gil_scoped_acquire gil;
        instance_ = py::object();
    }

    void world_update(World * /*w*/) override {
        py::gil_scoped_acquire gil;
        try {
            instance_.attr("_tick")();
        } catch (py::error_already_set & e) {
            emit_python_error("main()", e);
        }
    }

    bool owned_by_world() const override { return true; }

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
        world()->dispatch_set_param(
            PythonRuntime::instance().getCurrentCell(),
            name, static_cast<float>(value));
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

    // ---- force-divide ----
    // Asks the current cell to divide on its next divide() check
    // regardless of size. Mirrors CCL's divide(): used inside rules
    // that want to override the size-mean / size-variance machinery.
    m.def("force_divide_cell", []() { current_cell()->force_divide(); });

    // ---- motility: run / tumble ----
    // Same Cell methods CCL's run()/tumble() call (Gro.cpp:717).
    m.def("run_cell",    [](double dvel) { current_cell()->run   (static_cast<float>(dvel)); }, py::arg("dvel"));
    m.def("tumble_cell", [](double vel)  { current_cell()->tumble(static_cast<float>(vel));  }, py::arg("vel"));

    // ---- selected ----
    // True while the user has the current cell selected in the GUI.
    m.def("current_selected", []() -> bool { return current_cell()->is_selected(); });

    // ---- console message ----
    // Adds a string to the gro console on the given channel.
    // CCL idiom is `message(1, ...)` for per-cell messages from a
    // `selected:` rule, `message(0, ...)` for setup logging.
    m.def("message", [](int channel, const std::string & text) {
        world()->message(channel, text);
    }, py::arg("channel"), py::arg("text"));

    // Clear all messages on a channel. Typically called right before
    // `message(channel, ...)` from a periodic world rule so the
    // console shows only the current value, not the history.
    m.def("clear_messages", [](int channel) {
        world()->clear_messages(channel);
    }, py::arg("channel"));

    // ---- world program (`main()` analogue) ----
    // Installs a Python program instance as the world's per-tick
    // main program. The World takes ownership (see
    // MicroProgram::owned_by_world). If a previous Python-owned
    // main was installed, it's stashed for deferred deletion --
    // calling set_main(...) from inside a rule body means the old
    // prog's world_update is on the C++ stack and a direct delete
    // would be use-after-free. World::update drains the queue
    // after world_update returns.
    m.def("set_main_program", [](py::object instance) {
        World * w = world();
        MicroProgram * existing = w->get_program();
        if (existing && existing->owned_by_world()) {
            w->schedule_prog_deletion(existing);
        }
        w->set_program(new PythonWorldProgram(instance));
    });

    // Restart the world: kill all cells, zero signals, rebuild the
    // chipmunk space. Equivalent to CCL's reset().
    m.def("reset_world", []() { world()->restart(); });

    // ---- environment / runtime control ----
    // Toggle chemostat-mode boundary walls.
    m.def("set_chemostat_mode", [](bool on) { world()->set_chemostat_mode(on); },
          py::arg("on"));

    // Add a static wall between two world-coordinate points. The
    // physics shape and the render record are both set up inside
    // World::add_barrier, so the binding is just a forwarder.
    m.def("add_barrier", [](double x1, double y1, double x2, double y2) {
        world()->add_barrier(static_cast<float>(x1), static_cast<float>(y1),
                             static_cast<float>(x2), static_cast<float>(y2));
    }, py::arg("x1"), py::arg("y1"), py::arg("x2"), py::arg("y2"));

    // Set a signal value across a rectangle (corners of the rect).
    m.def("set_signal_rect",
          [](int handle, double x1, double y1, double x2, double y2, double value) {
        World * w = world();
        if (handle < 0 || handle >= w->num_signals())
            throw std::runtime_error("set_signal_rect: invalid handle");
        w->set_signal_rect(handle,
                           static_cast<float>(x1), static_cast<float>(y1),
                           static_cast<float>(x2), static_cast<float>(y2),
                           static_cast<float>(value));
    }, py::arg("handle"), py::arg("x1"), py::arg("y1"),
       py::arg("x2"), py::arg("y2"), py::arg("value"));

    // Write the current scene to a PNG. Returns success.
    m.def("snapshot", [](const std::string & path) -> bool {
        return world()->snapshot(path.c_str());
    }, py::arg("path"));

    // Pause / resume simulation ticking. start() clears the stop
    // flag; stop() sets it. Mirrors CCL's stop()/start().
    m.def("stop",  []() { world()->set_stop_flag(true);  });
    m.def("start", []() { world()->set_stop_flag(false); });

    // Seed the global RNG. Mirrors CCL's srand().
    m.def("srand", [](unsigned int seed) { std::srand(seed); },
          py::arg("seed"));

    // ---- spawning ----
    // Attach a Python program (if any) to a freshly-constructed
    // cell, register it with the world, and run its setup() under
    // a CellScope so set_param calls in setup land on the cell-
    // local parameter map. Used by ecoli() and yeast() and any
    // future cell-spawn binding.
    auto spawn_python_cell = [](Cell * c, py::object program) {
        World * w = world();
        if (!program.is_none()) {
            c->set_prog(new PythonMicroProgram(program));
        }
        w->add_cell(c);
        if (!program.is_none()) {
            CellScope scope(c);
            program.attr("setup")();
        }
    };

    m.def("ecoli",
          [spawn_python_cell](double x, double y, double theta,
                              py::object volume, py::object program) {
        double v = volume.is_none() ? DEFAULT_ECOLI_INIT_SIZE
                                    : volume.cast<double>();
        spawn_python_cell(new EColi(world(),
                                    static_cast<float>(x),
                                    static_cast<float>(y),
                                    static_cast<float>(theta),
                                    static_cast<float>(v)),
                          program);
    },
          py::arg("x")       = 0.0,
          py::arg("y")       = 0.0,
          py::arg("theta")   = 0.0,
          py::arg("volume")  = py::none(),
          py::arg("program") = py::none());

    m.def("yeast",
          [spawn_python_cell](double x, double y, double theta,
                              py::object volume, py::object program) {
        double v = volume.is_none() ? 1.0 : volume.cast<double>();
        spawn_python_cell(new Yeast(world(),
                                    static_cast<float>(x),
                                    static_cast<float>(y),
                                    static_cast<float>(theta),
                                    static_cast<float>(v),
                                    false),
                          program);
    },
          py::arg("x")       = 0.0,
          py::arg("y")       = 0.0,
          py::arg("theta")   = 0.0,
          py::arg("volume")  = py::none(),
          py::arg("program") = py::none());

    // ---- time / dt ----
    m.def("dt",   []() -> double { return world()->get_sim_dt(); });
    m.def("time", []() -> double { return world()->get_time();  });

    // ---- stats / reaction ----
    // World-level statistics by name (only "pop_size" today).
    m.def("stats", [](const std::string & name) -> double {
        return world()->stats(name);
    }, py::arg("name"));

    // Register a reactant->product signal reaction at the given rate.
    // Same validation CCL's reaction() does, in shared World code.
    m.def("reaction",
          [](const std::vector<int> & reactants,
             const std::vector<int> & products, double rate) {
        world()->add_reaction(reactants, products, static_cast<float>(rate));
    }, py::arg("reactants"), py::arg("products"), py::arg("rate"));

    // Return a 2D matrix of a signal's grid values. World owns the
    // backing store and bounds-checks the handle; we copy and return
    // by value. NOTE: this copies the entire grid into a Python
    // list[list[float]] -- ~25MB for an 800x800 grid. Don't call
    // per-tick; intended for snapshot-style dumps. A numpy view via
    // py::array_t would be the right hot-path interface if needed.
    m.def("get_signal_matrix", [](int handle) {
        return *world()->get_signal_matrix(handle);
    }, py::arg("handle"));

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
