//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <JuceHeader.h>

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
 * a project loaded) call requestRebuild(), which builds and publishes at
 * once - on the message thread, like the edit itself - so the plan is
 * posted to the render thread right behind the channel data of the same
 * edit and a block never runs one without the other. An edit made of many
 * steps (a project load, a track with its channels) opens a ScopedBatch:
 * the requests inside it fold into one rebuild when the batch closes,
 * after the layout has been committed.
 *
 * The planner also owns the format: prepare() records the sample rate and
 * block size the renderer was prepared with, and every slot enters a plan
 * prepared for that format and its chain's channel count. A slot keeps its
 * instance through every format change - an effect, like a plugin, is
 * prepared again rather than made anew - but never while a plan the render
 * thread may be inside points at it. prepare() runs with the device
 * stopped or bypassed, so there it is done in place; a chain whose channel
 * count changes while the device runs leaves its slots out of the plan
 * until the plans that ran them are freed, then prepares them and builds
 * once more.
 */
class MixerPlanner
{
public:
    MixerPlanner (std::shared_ptr<EffectRegistry> registry_, std::shared_ptr<MixPlanContainer> plans_);
    ~MixerPlanner();

    /// The model the plans are built from; nullptr detaches (the container does that when it goes).
    void setSource (AudioTrackContainer* container) noexcept;

    EffectRegistry& getRegistry() noexcept { return *registry; }
    MixPlanContainer& getPlans() noexcept { return *plans; }

    /// A slot with a new effect of the type, nullptr for an unknown id. Prepared when it enters a plan.
    std::shared_ptr<EffectSlot> createSlot (const std::string& typeId) const;

    /// Message thread: a new plan, built and published now - or when the enclosing batch closes.
    void requestRebuild();

    /// Message thread: builds and publishes a plan at once, batch or not.
    void rebuildNow();

    /**
     * Folds the rebuild requests of a multi-step edit into one, made when
     * the outermost batch closes. A null planner makes the batch a no-op,
     * so a container without a planner (tests) needs no special case.
     */
    class ScopedBatch
    {
    public:
        explicit ScopedBatch (MixerPlanner* planner_) noexcept : planner (planner_)
        {
            if (planner != nullptr)
                planner->beginBatch();
        }

        ~ScopedBatch()
        {
            if (planner != nullptr)
                planner->endBatch();
        }

    private:
        MixerPlanner* planner;
        JUCE_DECLARE_NON_COPYABLE (ScopedBatch)
    };

    /**
     * Message thread, with the device stopped or bypassed (nothing renders):
     * the format the renderer was prepared with, no more and no less. The
     * same rate and block size as before, with a plan published, needs
     * nothing; any other block size - smaller as well as larger (a bounce's,
     * the device's own after it) - or another rate prepares every slot
     * again in place and publishes a plan, as is done when there is none
     * yet. (A block shorter than the prepared one renders fine; it is the
     * prepare that keeps the effects' buffers the size of the renderer's.)
     */
    void prepare (double sampleRate, int maxBlockSize);

    double getSampleRate() const noexcept { return sampleRate; }
    int getMaxBlockSize() const noexcept { return maxBlockSize; }

    /// How many plans were published (tests).
    int getNumRebuilds() const noexcept { return numRebuilds; }

private:
    void beginBatch() noexcept;
    void endBatch();

    /// Builds and publishes a plan; nothingRenders lets a slot in a live plan be prepared in place.
    void rebuild (bool nothingRenders);

    /// The chain's slots for a plan, each prepared for numChannels; a slot in a live plan whose format changed waits outside.
    void collect (InsertChain& chain, int numChannels, std::vector<EffectSlot*>& into, MixPlan& plan, bool nothingRenders);

    static constexpr double gainRampSeconds = 0.01;   ///< send levels and aux returns ramp like the channel gains

    std::shared_ptr<EffectRegistry> registry;
    std::shared_ptr<MixPlanContainer> plans;
    AudioTrackContainer* audioTrackContainer = nullptr;

    double sampleRate = 0.0;
    int maxBlockSize = 0;
    int batchDepth = 0;
    bool rebuildPending = false;
    bool prepareDeferred = false;   // a slot waits outside the plan for its new format
    int numRebuilds = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerPlanner)
};

} // namespace audium
