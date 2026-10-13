//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <atomic>
#include <cassert>

namespace audium
{

/**
 * \class SampleTimer
 * \brief A utility class for managing sample-based timing in audio processing.
 *
 * The `SampleTimer` class provides functionality to schedule and process
 * sample-based timers, which are commonly used in audio applications to
 * trigger events after a specified number of samples.
 */
class SampleTimer {
    
public:
    SampleTimer() = default;
    
    // returns true once timer is due
    bool process(int numSamples, int& offset) {
        if (active) {
            if (sampleCounter - numSamples <= 0.0) {
                offset = static_cast<int>(sampleCounter);
                sampleCounter = 0.0;
                active = false;
                return true;
            }
            else {
                sampleCounter -= numSamples;
            }
        }
        return false;
    }
    
    void schedule(int numSamples = 0) {
        sampleCounter = numSamples;
        active = true;
    }

    /**
     * Scales the remaining time by @p factor - for a voice whose playback
     * speed changed mid-clip (a tempo-locked clip following a tempo change):
     * the source material left is the same, it now takes factor times as
     * long. Fractional so a stream of small changes does not drift.
     */
    void rescale(double factor) {
        if (active && factor > 0.0)
            sampleCounter *= factor;
    }
    
    void invalidate() {
        active = false;
    }
    
    bool isActive() const noexcept {
        return active;
    }
    
private:
    double sampleCounter = 0.0;
    std::atomic<bool> active = false;
};

} // namespace audium
