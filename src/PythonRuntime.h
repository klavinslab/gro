// Embeds CPython into gro. Lazily initialized on the first .py program
// the user opens; finalized at app shutdown.

#ifndef GRO_PYTHON_RUNTIME_H
#define GRO_PYTHON_RUNTIME_H

#include <string>

class World;
class Cell;

class PythonRuntime {
public:
    static PythonRuntime & instance();

    // Lazily initializes the interpreter on first call. Safe to call
    // many times. Returns true if Python is ready to use.
    bool ensureInitialized(std::string & err);

    // The simulator sets the active World before loadProgram runs so
    // pybind11-bound functions like ecoli() / set_param() / signal()
    // know where to add their effects. Cleared after loadProgram
    // returns, regardless of success.
    void setCurrentWorld(World * w) { current_world_ = w; }
    World * getCurrentWorld() const { return current_world_; }

    // Set just before a cell's Python program is ticked, cleared
    // after. Used by cell-local bindings (emit_signal, get_signal,
    // current_volume) to find "self" without the user passing it.
    // Mirrors CCL's current_cell static in Gro.cpp.
    void setCurrentCell(Cell * c) { current_cell_ = c; }
    Cell * getCurrentCell() const { return current_cell_; }

    // Loads and executes a .py file. On Python exceptions, formats the
    // traceback into err and returns false. Caller is responsible for
    // having called setCurrentWorld() first.
    bool loadProgram(const char * path, std::string & err);

    // Finalize on app shutdown. Idempotent.
    void shutdown();

    ~PythonRuntime();

private:
    PythonRuntime() = default;
    PythonRuntime(const PythonRuntime &) = delete;
    PythonRuntime & operator=(const PythonRuntime &) = delete;

    World * current_world_ = nullptr;
    Cell  * current_cell_  = nullptr;

    // Opaque main-thread Python state. After py::initialize_interpreter
    // the main thread holds the GIL. To let GroThread (a QThread)
    // acquire the GIL each tick, the main thread releases it. We
    // keep the saved state so we can restore it at shutdown.
    void * main_thread_state_ = nullptr;
};

#endif // GRO_PYTHON_RUNTIME_H
