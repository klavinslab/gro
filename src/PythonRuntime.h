// Embeds CPython into gro. Lazily initialized on the first .py program
// the user opens; finalized at app shutdown.
//
// Milestone 1: just plumb pybind11 + an empty `_core` module. The
// user-visible behavior for a .py file is the parse-error overlay with
// the message "Python support not implemented yet."

#ifndef GRO_PYTHON_RUNTIME_H
#define GRO_PYTHON_RUNTIME_H

#include <string>

class World;

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

    bool initialized_ = false;
    World * current_world_ = nullptr;
};

#endif // GRO_PYTHON_RUNTIME_H
