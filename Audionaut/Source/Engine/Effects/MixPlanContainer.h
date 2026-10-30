//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <memory>
#include <vector>

#include "Engine/Core/LockFreeContainer.h"
#include "MixPlan.h"

namespace audium
{

/**
 * Hands mix plans from the message thread to the audio thread without
 * locks, modelled on VoiceSourceContainer: commit() publishes a plan, the
 * renderer pull()s the latest at the top of each block and reads it until
 * its next pull. The previous plan is retired, not deleted: it goes once
 * the audio thread has pulled the plan that replaced it, which a timer
 * checks on the message thread, since nothing is ever freed on the audio
 * thread. The effects a retired plan kept alive go with it.
 */
class MixPlanContainer : private juce::Timer
{
public:
    MixPlanContainer() = default;
    ~MixPlanContainer() override { stopTimer(); }

    /// Message thread: publishes the plan; the previous one is retired.
    void commit (std::unique_ptr<MixPlan> plan);

    /// Render thread: the latest published plan (nullptr before the first commit), valid until the next pull().
    const MixPlan* pull() noexcept;

    /// Message thread: frees retired plans the audio thread has let go of; otherwise the timer does.
    void releaseRetired();

    /// The plan the message thread published last (not necessarily pulled yet).
    const MixPlan* getCurrent() const noexcept { return current.get(); }

    size_t getNumRetired() const noexcept { return retired.size(); }

private:
    void timerCallback() override;

    LockFreeContainer<MixPlan*> snapshot { 1 };
    std::unique_ptr<MixPlan> current;
    std::vector<std::unique_ptr<MixPlan>> retired;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixPlanContainer)
};

} // namespace audium
