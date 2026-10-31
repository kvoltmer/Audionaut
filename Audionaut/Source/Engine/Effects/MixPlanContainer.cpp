//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "MixPlanContainer.h"

#include <algorithm>

namespace audium
{

const MixPlan* MixPlanContainer::commit (std::unique_ptr<MixPlan> plan)
{
    releaseRetired();

    if (current != nullptr)
        retired.push_back (std::move (current));
    current = std::move (plan);
    current->serial = ++lastSerial;

    if (! retired.empty())
        startTimer (50);

    return current.get();
}

void MixPlanContainer::adopt (const MixPlan* plan) noexcept
{
    adopted.store (plan, std::memory_order_relaxed);
    adoptedSerial.store (plan != nullptr ? plan->serial : 0, std::memory_order_release);
}

bool MixPlanContainer::releaseRetired()
{
    if (retired.empty())
        return false;

    // a plan older than the adopted one is read by nobody: the render
    // thread adopts at the top of a block and renders from the new one
    const auto done = adoptedSerial.load (std::memory_order_acquire);
    const auto before = retired.size();
    retired.erase (std::remove_if (retired.begin(), retired.end(),
                                   [done] (const std::unique_ptr<MixPlan>& plan) { return plan->serial < done; }),
                   retired.end());
    return retired.size() < before;
}

void MixPlanContainer::timerCallback()
{
    const auto released = releaseRetired();
    if (retired.empty())
        stopTimer();
    if (released && onRetiredReleased != nullptr)
        onRetiredReleased();
}

} // namespace audium
