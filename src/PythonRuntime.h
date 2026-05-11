// Embeds CPython into gro. Lazily initialized on the first .py program
// the user opens; finalized at app shutdown.
//
// Milestone 1: just plumb pybind11 + an empty `_core` module. The
// user-visible behavior for a .py file is the parse-error overlay with
// the message "Python support not implemented yet."

#ifndef GRO_PYTHON_RUNTIME_H
#define GRO_PYTHON_RUNTIME_H

#include <string>

class PythonRuntime {
public:
    static PythonRuntime & instance();

    // Lazily initializes the interpreter on first call. Safe to call
    // many times. Returns true if Python is ready to use.
    bool ensureInitialized(std::string & err);

    // Loads a .py file as a gro program. Milestone 1: always fails
    // with a "not implemented" message after confirming the embedded
    // Python actually runs and the _core module imports cleanly.
    bool loadProgram(const char * path, std::string & err);

    // Finalize on app shutdown. Idempotent.
    void shutdown();

    ~PythonRuntime();

private:
    PythonRuntime() = default;
    PythonRuntime(const PythonRuntime &) = delete;
    PythonRuntime & operator=(const PythonRuntime &) = delete;

    bool initialized_ = false;
};

#endif // GRO_PYTHON_RUNTIME_H
