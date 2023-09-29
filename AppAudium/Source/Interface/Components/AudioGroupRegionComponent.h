/*
  ==============================================================================

    AudioGroupRegionComponent.h
    Created: 28 Sep 2023 12:07:58pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class AudioResourceGroup;
class AudioRegion;
class ZoomHandler;
class AudioRegionView;

//==============================================================================
/*
Display all AudioRegionViews within a AudioResourceGroup
*/
class AudioGroupRegionComponent  : public juce::Component
{
public:
    AudioGroupRegionComponent(std::shared_ptr<AudioResourceGroup> audioResourceGroup,
                              std::shared_ptr<AudioRegion> audioRegion,
                              std::shared_ptr<ZoomHandler> zoomHandler);
    ~AudioGroupRegionComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    std::shared_ptr<AudioResourceGroup> audioResourceGroup;
    
    std::vector<std::shared_ptr<AudioRegionView>> children;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioGroupRegionComponent)
};
