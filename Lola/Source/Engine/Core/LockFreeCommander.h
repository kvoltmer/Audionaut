//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include <farbot/RealtimeTraits.hpp>
#include <farbot/fifo.hpp>

namespace audium
{

class LockFreeCommander
{
    
public:
    LockFreeCommander(int capacity) :
        fifo(capacity)
    {
    }
    
    ~LockFreeCommander() = default;

    void invoke()
    {
        std::function<void()> cmd;
        if (fifo.pop (cmd)) {
            // std::cout << "LockFreeCommander::invoke" << std::endl;
            juce::NullCheckedInvocation::invoke (cmd);
        }
    }

    farbot::fifo<   std::function<void()>,
                    farbot::fifo_options::concurrency::single,
                    farbot::fifo_options::concurrency::multiple> fifo;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LockFreeCommander)        

};

} // namespace audium
