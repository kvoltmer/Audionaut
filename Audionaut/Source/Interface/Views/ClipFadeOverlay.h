//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <map>
#include <memory>
#include <JuceHeader.h>

namespace audium {
class AudiumEngine;
class AudioTrack;
class AudioResource;
class AudioThumbnail;
class PlayListItem;
}

class ZoomHandler;

/**
 * Lane-wide overlay above a track's clip components. Draws the parts of a
 * clip's fade ramps that lie outside the clip rect (negative fade-in start /
 * fade-out end offsets - the fade extends the audible material), plus a
 * dimmed ghost waveform of the extended source material, which the
 * clip-local FadeInOutView cannot paint because children are clipped to
 * their parent. Also hosts the lane-parented bottom fade handles as
 * children; it is transparent to the mouse itself.
 */
class ClipFadeOverlay : public juce::Component, public juce::ChangeListener
{
public:
    ClipFadeOverlay(std::shared_ptr<audium::AudioTrack> audioTrack,
                    std::shared_ptr<audium::AudiumEngine> audiumEngine,
                    std::shared_ptr<ZoomHandler> zoomHandler);
    ~ClipFadeOverlay() override;

    void paint (juce::Graphics&) override;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;

private:
    void paintItemExtensions (juce::Graphics& g,
                              const audium::PlayListItem& item,
                              juce::Range<double> clipRange);

    // Thumbnails for the ghost waveforms, one per source file (keyed by URL;
    // the heavy data is shared through the engine's AudioThumbnailCache).
    std::shared_ptr<audium::AudioThumbnail> getOrCreateThumbnail(
        const std::shared_ptr<audium::AudioResource>& resource);

    std::shared_ptr<audium::AudioTrack> audioTrack;
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::shared_ptr<ZoomHandler> zoomHandler;

    std::map<juce::String, std::shared_ptr<audium::AudioThumbnail>> thumbnails;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipFadeOverlay)
};
