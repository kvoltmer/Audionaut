//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/Components/MixerPanel/MixerBusStripComponent.h"

/**
 * The bus section the mixer pins right of the scrolling track strips: the
 * aux buses' strips under an "Aux" band, in the container's order (Create >
 * Create Aux Bus adds one; View > Aux Buses hides them), and the master's
 * strip under "Main" last. The strips are built from the buses once: the mixer
 * rebuilds the section when a bus comes or goes.
 */
class MixerBusGroupComponent : public juce::Component
{
public:
    MixerBusGroupComponent (MixerContext context, bool withAuxBuses);
    ~MixerBusGroupComponent() override;

    int getPreferredWidth() const;

    void setSections (int sections);
    void refresh();
    void refreshLive();

    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    MixerContext context;
    std::vector<std::unique_ptr<MixerBusStripComponent>> strips;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerBusGroupComponent)
};
