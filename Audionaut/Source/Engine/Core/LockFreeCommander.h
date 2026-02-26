//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include <farbot/RealtimeTraits.hpp>
#include <farbot/fifo.hpp>

namespace audium
{

/**
 * @class LockFreeCommander
 * @brief A lock-free command invoker for real-time applications.
 *
 * The `LockFreeCommander` class provides a mechanism for managing and invoking
 * commands in a lock-free manner, suitable for real-time audio applications.
 */
class LockFreeCommander
{
public:
    /**
     * @brief Constructs a `LockFreeCommander` with a specified capacity.
     * @param capacity The maximum number of commands the container can hold.
     */
    LockFreeCommander(int capacity) :
        fifo(capacity)
    {
    }

    /**
     * @brief Default destructor.
     */
    ~LockFreeCommander() = default;

    /**
     * @brief Invokes the next command in the FIFO buffer.
     *
     * This method retrieves the next command from the FIFO buffer and invokes it
     * using `juce::NullCheckedInvocation`. If no command is available, it does nothing.
     */
    void invoke()
    {
        std::function<void()> cmd;
        while (fifo.pop (cmd)) {
            juce::NullCheckedInvocation::invoke (cmd);
        }
    }

    /**
     * @brief The FIFO buffer for lock-free command storage.
     *
     * This buffer stores commands as `std::function<void()>` objects and supports
     * single-producer, multiple-consumer concurrency.
     */
    farbot::fifo<   std::function<void()>,
                    farbot::fifo_options::concurrency::single,
                    farbot::fifo_options::concurrency::multiple> fifo;

private:
    /**
     * @brief JUCE macro to prevent copying and detect memory leaks.
     */
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LockFreeCommander)
};

} // namespace audium
