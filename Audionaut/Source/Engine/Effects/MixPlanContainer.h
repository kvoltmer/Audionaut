//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include <JuceHeader.h>

#include "MixPlan.h"

namespace audium
{

/**
 * Owns the mix plans and hands them from the message thread to the render
 * thread without locks. commit() takes a plan as the current one and
 * retires the previous; the planner then posts a command that adopt()s it
 * on the render thread, in the same queue as the channel data, so a block
 * runs the plan together with the layout it was built for. A retired plan
 * is freed, on the message thread, once the render thread has adopted a
 * later one: adoption happens at the top of a block, before anything
 * renders, so nothing is inside the old plan by then. The effects a
 * retired plan kept alive go with it. A timer checks while the device
 * runs; where no device runs, the commands are pumped as they are posted
 * and the next commit frees what the last adoption let go of.
 */
class MixPlanContainer : private juce::Timer
{
public:
    MixPlanContainer() = default;
    ~MixPlanContainer() override { stopTimer(); }

    /// Message thread: the plan becomes the current one, the previous is retired. Returns what to adopt().
    const MixPlan* commit (std::unique_ptr<MixPlan> plan);

    /// Render thread (a posted command): the plan the renderer runs from now on.
    void adopt (const MixPlan* plan) noexcept;

    /// Render thread: the adopted plan, nullptr before the first adoption.
    const MixPlan* getAdopted() const noexcept { return adopted.load (std::memory_order_relaxed); }

    /// Message thread: frees the retired plans the render thread has moved past; otherwise the timer does. True when any went.
    bool releaseRetired();

    /// Called from the timer when it freed a retired plan: the slots only that plan ran are free again.
    std::function<void()> onRetiredReleased;

    /// The plan the message thread committed last (not necessarily adopted yet).
    const MixPlan* getCurrent() const noexcept { return current.get(); }

    size_t getNumRetired() const noexcept { return retired.size(); }

private:
    void timerCallback() override;

    std::unique_ptr<MixPlan> current;
    std::vector<std::unique_ptr<MixPlan>> retired;
    std::uint64_t lastSerial = 0;

    std::atomic<const MixPlan*> adopted { nullptr };
    std::atomic<std::uint64_t> adoptedSerial { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixPlanContainer)
};

} // namespace audium
