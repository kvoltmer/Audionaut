//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <memory>
#include <JuceHeader.h>

namespace audium {
class AudioTrack;
class PlayListItem;
}

class ZoomHandler;

/**
 * Lane-wide overlay above a track's clip components. Draws the parts of a
 * clip's fade ramps that lie outside the clip rect (negative fade-in start /
 * fade-out end offsets - the fade extends the audible material), which the
 * clip-local FadeInOutView cannot paint because children are clipped to
 * their parent. Also hosts the lane-parented bottom fade handles as
 * children; it is transparent to the mouse itself.
 */
class ClipFadeOverlay : public juce::Component
{
public:
    ClipFadeOverlay(std::shared_ptr<audium::AudioTrack> audioTrack,
                    std::shared_ptr<ZoomHandler> zoomHandler);

    void paint (juce::Graphics&) override;

private:
    void paintItemExtensions (juce::Graphics& g,
                              const audium::PlayListItem& item,
                              juce::Range<double> clipRange);

    std::shared_ptr<audium::AudioTrack> audioTrack;
    std::shared_ptr<ZoomHandler> zoomHandler;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipFadeOverlay)
};
