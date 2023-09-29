/*
  ==============================================================================

    AudioRegionView.h
    Created: 19 Sep 2023 2:20:32pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <memory>

class AudioResource;
class ZoomHandler;
class AudioRegion;

class AudioRegionView : public juce::Component,
                        public juce::ChangeListener
{
public:
    AudioRegionView(std::shared_ptr<AudioResource> audioResource,
                    std::shared_ptr<ZoomHandler> zoomHandler,
                    std::shared_ptr<AudioRegion> audioRegion);
    ~AudioRegionView() override;

    void paint (juce::Graphics&) override;
    
    void resized() override;
    
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    
    void setAudioResource (std::shared_ptr<AudioResource> audioResource);

    std::shared_ptr<AudioRegion> getAudioRegion() const { return audioRegion; }
    
private:
    std::shared_ptr<AudioResource> audioResource;
    std::shared_ptr<ZoomHandler> zoomHandler;
    /// TODO: change to playlistitem
    std::shared_ptr<AudioRegion> audioRegion;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRegionView)
};
