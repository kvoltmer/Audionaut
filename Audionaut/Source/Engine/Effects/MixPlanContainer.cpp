//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "MixPlanContainer.h"

namespace audium
{

void MixPlanContainer::commit (std::unique_ptr<MixPlan> plan)
{
    releaseRetired();

    if (current != nullptr)
        retired.push_back (std::move (current));
    current = std::move (plan);

    auto& producer = snapshot.getProducerObjects();
    producer.clear();
    producer.push_back (current.get());
    snapshot.commit();

    if (! retired.empty())
        startTimer (50);
}

const MixPlan* MixPlanContainer::pull() noexcept
{
    snapshot.pull();
    const auto& objects = snapshot.getConsumerObjects();
    return objects.empty() ? nullptr : objects.front();
}

void MixPlanContainer::releaseRetired()
{
    // once the audio thread has pulled the latest plan, no retired one is read any more
    if (retired.empty() || ! snapshot.isPulled())
        return;
    retired.clear();
}

void MixPlanContainer::timerCallback()
{
    releaseRetired();
    if (retired.empty())
        stopTimer();
}

} // namespace audium
