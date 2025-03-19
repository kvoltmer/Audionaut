//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <atomic>
#include <cassert>

namespace audium
{

class SampleTimer {
    
public:
    SampleTimer() = default;
    
    // returns true once timer is due
    bool process(int numSamples, int& offset) {
        if (active) {
            if (sampleCounter - numSamples <= 0) {
                offset = sampleCounter;
                sampleCounter = 0;
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
    
    void invalidate() {
        active = false;
    }
    
    bool isActive() const noexcept {
        return active;
    }
    
private:
    int sampleCounter = 0;
    std::atomic<bool> active = false;
};

} // namespace audium
