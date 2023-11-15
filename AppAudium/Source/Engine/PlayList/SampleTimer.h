/*
  ==============================================================================

    SampleTimer.h
    Created: 9 Jul 2023 1:19:51pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <atomic>
#include <cassert>

// This call not used anywhere.
class SampleTimer {
    
    
public:
    SampleTimer() = default;
    
    bool process(int numSamples, int& offset)
    {
        if (active)
        {
            if (sampleCounter - numSamples < 0)
            {
                offset = sampleCounter;
                sampleCounter = 0;
                active = false;
                return true;
            }
            else
            {
                sampleCounter -= numSamples;
            }
        }
        return false;
    }

    void schedule(int numSamples = 0)
    {
        sampleCounter = numSamples;
        active = true;
    }
    
    void invalidate()
    {
        active = false;
    }
    
    bool isActive() const noexcept
    {
        return active;
    }
    
private:
    int sampleCounter = 0;
    std::atomic<bool> active = false;
};
