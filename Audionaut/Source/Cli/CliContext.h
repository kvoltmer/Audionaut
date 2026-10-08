//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <functional>

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
    bool progressJson = false; ///< Progress as one JSON object per stderr line; not silenced by quiet.

    /** The GUI app's live preferences in in-app CLI mode; null in the
        standalone binary, which opens its own instance when it needs one. */
    Preferences* preferences = nullptr;

    /**
     * Set by a host running a verb on someone else's behalf: the envelope and
     * the log are handed over instead of written to this process's streams,
     * so they can travel back to the client that asked. The client then
     * renders them through its own context, which is what keeps hosted output
     * identical to a local run.
     */
    std::function<void (const nlohmann::json& envelope)> envelopeSink;
    std::function<void (const juce::String& line)> logSink;
    std::function<void (double fraction, const juce::String& message)> progressSink;

    /** Emits a success envelope (or nothing in human mode) and returns exitOk. */
    int ok (const nlohmann::json& result)
    {
        if (envelopeSink) {
            envelopeSink ({ { "ok", true }, { "result", result } });
            return exitOk;
        }

        if (json)
            resultStream() << nlohmann::json ({ { "ok", true }, { "result", result } }).dump (2) << std::endl;
        return exitOk;
    }

    /** Emits an error envelope (json mode) or a stderr message, returns exitCode. */
    int fail (int exitCode, const std::string& code, const std::string& message)
    {
        if (envelopeSink) {
            envelopeSink ({ { "ok", false },
                            { "error", { { "code", code }, { "message", message } } } });
            return exitCode;
        }

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
        if (logSink) {
            logSink (message);
            return;
        }

        if (! quiet)
            std::cerr << message << std::endl;
    }

    /**
     * How far a long verb has got, @p fraction in [0, 1]. Verbs throttle their
     * own calls; this only picks the rendering.
     *
     * With --progress-json the line is `{"progress":0.35,"message":"..."}` on
     * stderr - stdout stays the envelope alone - and it is written even with
     * --quiet, which is how a wrapper asks for progress without the chatter.
     * Other stderr lines may sit between them, so readers must skip anything
     * that is not such an object.
     */
    void progress (double fraction, const juce::String& message)
    {
        if (progressSink) {
            progressSink (fraction, message);
            return;
        }

        if (progressJson) {
            std::cerr << nlohmann::json ({ { "progress", fraction },
                                           { "message", message.toStdString() } }).dump()
                      << std::endl;
            return;
        }

        log (message + " " + juce::String (static_cast<int> (fraction * 100.0)) + "%");
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
