//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <memory>
#include <optional>
#include <string>

#include "EffectRegistry.h"
#include "EffectSlot.h"
#include "MixPlanContainer.h"

namespace audium
{

class AudioTrackContainer;
class InsertChain;

/**
 * Builds the mix plan from the model and publishes it. Structure changes
 * (an insert added, removed or moved, a channel or track added or removed,
 * a project loaded) call requestRebuild(); the rebuilds of one message are
 * coalesced through an AsyncUpdater, or run at once where no message loop
 * turns (headless engines) or when asked to.
 *
 * The planner also owns the format: prepare() records the sample rate and
 * block size the renderer was prepared with, and every slot enters a plan
 * prepared for that format and its chain's channel count. A slot already
 * referenced by a published plan is never re-prepared under the audio
 * thread; when its format changes it is replaced by a fresh instance with
 * the same state, and the old one goes with the retired plan.
 */
class MixerPlanner : private juce::AsyncUpdater
{
public:
    MixerPlanner (std::shared_ptr<EffectRegistry> registry_, std::shared_ptr<MixPlanContainer> plans_);
    ~MixerPlanner() override;

    /// The model the plans are built from; nullptr detaches (the container does that when it goes).
    void setSource (AudioTrackContainer* container) noexcept;

    EffectRegistry& getRegistry() noexcept { return *registry; }
    MixPlanContainer& getPlans() noexcept { return *plans; }

    /// A slot with a new effect of the type, nullptr for an unknown id. Prepared when it enters a plan.
    std::shared_ptr<EffectSlot> createSlot (const std::string& typeId) const;

    /// Asks for a new plan: now, or coalesced on the message thread.
    void requestRebuild();

    /// Builds and publishes a plan at once (also runs a pending request).
    void rebuildNow();

    /// Runs a pending request, for a render driven off the message thread (a bounce) before it starts.
    void flushPendingRebuild();

    /**
     * Message thread, with the device stopped or bypassed: the format the
     * renderer was prepared with. Slots keep their instance when the sample
     * rate is unchanged and the block size did not grow (a bounce's shorter
     * last block), and are replaced otherwise. Publishes a plan.
     */
    void prepare (double sampleRate, int maxBlockSize);

    double getSampleRate() const noexcept { return sampleRate; }
    int getMaxBlockSize() const noexcept { return maxBlockSize; }

    /**
     * Whether requestRebuild() rebuilds inline instead of on the message
     * loop. Unset = follow HeadlessMode (the process-wide default); set it
     * on a scratch engine inside the GUI, which has no device driving it.
     */
    void setRebuildsSynchronously (bool shouldRebuildInline) noexcept { rebuildsSynchronously = shouldRebuildInline; }

    /// How many plans were published (tests).
    int getNumRebuilds() const noexcept { return numRebuilds; }

private:
    void handleAsyncUpdate() override;
    bool rebuildsInline() const noexcept;

    /// The chain's slots for a plan, each prepared for numChannels; a live slot of another format is replaced.
    void collect (InsertChain& chain, int numChannels, std::vector<EffectSlot*>& into, MixPlan& plan);
    std::shared_ptr<EffectSlot> replaceSlot (const EffectSlot& old) const;

    std::shared_ptr<EffectRegistry> registry;
    std::shared_ptr<MixPlanContainer> plans;
    AudioTrackContainer* source = nullptr;

    double sampleRate = 0.0;
    int maxBlockSize = 0;
    std::optional<bool> rebuildsSynchronously;
    int numRebuilds = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerPlanner)
};

} // namespace audium
