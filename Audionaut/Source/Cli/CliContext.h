//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <iostream>
#include <string>
#include <JuceHeader.h>
#include <nlohmann/json.hpp>

namespace audium {

class Preferences;

namespace cli {

/** Exit codes shared by every command (documented in the CLI help text). */
constexpr int exitOk          = 0; ///< Success.
constexpr int exitFailure     = 1; ///< The operation ran and failed.
constexpr int exitUsage       = 2; ///< Bad arguments.
constexpr int exitUnavailable = 3; ///< Feature not available in this build (e.g. Essentia off).

/**
 * @class CliContext
 * @brief Per-invocation output state for the CLI commands.
 *
 * In --json mode stdout carries exactly one result envelope so agents can
 * pipe it straight into a JSON parser; everything else (logs, progress,
 * human-readable output) goes to stderr.
 */
class CliContext {
public:
    bool json = false;  ///< Emit a machine-readable envelope on stdout.
    bool quiet = false; ///< Suppress log output.

    /** The GUI app's live preferences in in-app CLI mode; null in the
        standalone binary, which opens its own instance when it needs one. */
    Preferences* preferences = nullptr;

    /** Emits a success envelope (or nothing in human mode) and returns exitOk. */
    int ok (const nlohmann::json& result)
    {
        if (json)
            resultStream() << nlohmann::json ({ { "ok", true }, { "result", result } }).dump (2) << std::endl;
        return exitOk;
    }

    /** Emits an error envelope (json mode) or a stderr message, returns exitCode. */
    int fail (int exitCode, const std::string& code, const std::string& message)
    {
        if (json)
            resultStream() << nlohmann::json ({ { "ok", false },
                                                { "error", { { "code", code }, { "message", message } } } }).dump (2)
                           << std::endl;
        else
            std::cerr << "error (" << code << "): " << message << std::endl;
        return exitCode;
    }

    /** Human-facing progress/log line; never lands on the JSON stdout. */
    void log (const juce::String& message)
    {
        if (! quiet)
            std::cerr << message << std::endl;
    }

private:
    /**
     * The envelope always goes to the *real* stdout, whose buffer is captured
     * when the context is constructed (in main, before any ScopedCoutToStderr
     * redirect) - so a guard active around an early return cannot misroute
     * the result.
     */
    std::ostream& resultStream() { return resultOut; }

    std::ostream resultOut { std::cout.rdbuf() };
};

/**
 * @class ScopedCoutToStderr
 * @brief Redirects std::cout to stderr for its lifetime.
 *
 * The engine prints the odd status line to std::cout (e.g. openFile's
 * "loading:"); in --json mode that would corrupt the stdout envelope, so
 * commands wrap their engine calls in this guard instead of editing the
 * engine.
 */
class ScopedCoutToStderr {
public:
    explicit ScopedCoutToStderr (bool active)
    {
        if (active)
            saved = std::cout.rdbuf (std::cerr.rdbuf());
    }

    ~ScopedCoutToStderr()
    {
        if (saved != nullptr)
            std::cout.rdbuf (saved);
    }

private:
    std::streambuf* saved = nullptr;

    JUCE_DECLARE_NON_COPYABLE (ScopedCoutToStderr)
};

} // namespace cli
} // namespace audium
